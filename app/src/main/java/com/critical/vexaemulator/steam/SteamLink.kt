package com.critical.vexaemulator.steam

import android.content.Context
import android.util.Base64
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import org.json.JSONObject
import `in`.dragonbra.javasteam.enums.EOSType
import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.protobufs.steamclient.SteammessagesAuthSteamclient.EAuthSessionGuardType
import `in`.dragonbra.javasteam.steam.authentication.AuthPollResult
import `in`.dragonbra.javasteam.steam.authentication.AuthSession
import `in`.dragonbra.javasteam.steam.authentication.AuthSessionDetails
import `in`.dragonbra.javasteam.steam.authentication.AuthenticationException
import `in`.dragonbra.javasteam.steam.authentication.CredentialsAuthSession
import `in`.dragonbra.javasteam.steam.authentication.IAuthenticator
import `in`.dragonbra.javasteam.steam.handlers.steamuser.LogOnDetails
import `in`.dragonbra.javasteam.steam.handlers.steamuser.SteamUser
import `in`.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback
import `in`.dragonbra.javasteam.steam.steamclient.AsyncJobFailedException
import `in`.dragonbra.javasteam.steam.steamclient.SteamClient
import `in`.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager
import `in`.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.future.await
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import java.security.Security
import java.util.concurrent.CancellationException
import java.util.concurrent.CompletableFuture
import java.util.concurrent.CompletionException
import java.util.concurrent.ExecutionException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference

sealed interface SteamUiState {
    data object SignedOut : SteamUiState
    data class Busy(val message: String) : SteamUiState
    data class NeedGuard(
        val kind: GuardKind,
        val hint: String,
        val retry: Boolean,
        val attempt: Int,
    ) : SteamUiState
    data class SignedIn(
        val accountName: String,
        val steamId: String,
        val online: Boolean,
    ) : SteamUiState
    data class Failed(val message: String) : SteamUiState
}

enum class GuardKind { Device, Email, Confirm }

// Account session for the launcher. The refresh token stays in SteamSessionStore.
// JavaSteam log lines are left without a listener so tokens are not printed.
object SteamLink {
    private const val loginId = 0x56455801
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val _state = MutableStateFlow<SteamUiState>(SteamUiState.SignedOut)
    val state: StateFlow<SteamUiState> = _state.asStateFlow()

    private val generation = AtomicInteger(0)
    private val sessionToken = AtomicInteger(0)
    private val pumping = AtomicBoolean(false)
    private val signingIn = AtomicBoolean(false)
    private val accountOnline = AtomicBoolean(false)
    private val guardCode = AtomicReference<CompletableFuture<String>?>(null)
    private val sessionMutex = Mutex()
    private val lock = Any()

    @Volatile private var appContext: Context? = null
    @Volatile private var steamClient: SteamClient? = null
    @Volatile private var steamUser: SteamUser? = null
    @Volatile private var callbacks: CallbackManager? = null
    @Volatile private var authSession: CredentialsAuthSession? = null
    @Volatile private var connected = CompletableFuture<Unit>()
    @Volatile private var loggedOn = CompletableFuture<LoggedOnCallback>()
    @Volatile private var restored = false

    private val authenticator = object : IAuthenticator {
        override fun getDeviceCode(previousCodeWasIncorrect: Boolean): CompletableFuture<String> =
            ask(GuardKind.Device, "Code from the Steam app", previousCodeWasIncorrect)

        override fun getEmailCode(email: String?, previousCodeWasIncorrect: Boolean): CompletableFuture<String> {
            val hint = email?.takeIf { it.isNotBlank() }?.let { "Code sent to $it" } ?: "Code sent to your email"
            return ask(GuardKind.Email, hint, previousCodeWasIncorrect)
        }

        override fun acceptDeviceConfirmation(): CompletableFuture<Boolean> {
            _state.value = SteamUiState.NeedGuard(
                kind = GuardKind.Confirm,
                hint = "Approve the sign-in in the Steam app, or enter the code",
                retry = false,
                attempt = 0,
            )
            return CompletableFuture.completedFuture(true)
        }
    }

    fun cloud(): `in`.dragonbra.javasteam.steam.handlers.steamcloud.SteamCloud? {
        val client = steamClient ?: return null
        if (!accountOnline.get() || !client.isConnected) return null
        return client.getHandler(`in`.dragonbra.javasteam.steam.handlers.steamcloud.SteamCloud::class.java)
    }

    fun restore(context: Context) {
        val app = context.applicationContext
        appContext = app
        SteamworksHost.refresh(app)
        if (restored) return
        restored = true
        val saved = SteamSessionStore.load(app) ?: return
        _state.value = SteamUiState.SignedIn(saved.accountName, saved.steamId, online = false)
        scope.launch { ensureOnline(app) }
    }

    // Reads session.bin and logs that account on. Game launch calls this before
    // the guest starts, and so does the launcher after a process restart.
    suspend fun ensureOnline(context: Context): StoredSteamSession? {
        val app = context.applicationContext
        appContext = app
        if (signingIn.get()) return SteamSessionStore.load(app)
        return sessionMutex.withLock {
            if (signingIn.get()) return@withLock SteamSessionStore.load(app)
            val saved = SteamSessionStore.load(app)
            if (saved == null || saved.accountName.isBlank() || saved.refreshToken.isBlank()) {
                return@withLock null
            }
            val gen = generation.get()
            if (accountOnline.get() && steamClient?.isConnected == true) {
                if (generation.get() == gen) {
                    _state.value = SteamUiState.SignedIn(saved.accountName, saved.steamId, online = true)
                }
                return@withLock saved
            }
            _state.value = SteamUiState.SignedIn(saved.accountName, saved.steamId, online = false)
            try {
                logOnSaved(saved)
                if (generation.get() != gen) return@withLock null
                accountOnline.set(true)
                _state.value = SteamUiState.SignedIn(saved.accountName, saved.steamId, online = true)
                VexaLogger.log(
                    LogLevel.INFO,
                    LogCategory.CONFIG,
                    "Steam session connected",
                    mapOf("steamId" to saved.steamId),
                )
                saved
            } catch (error: Throwable) {
                if (generation.get() != gen) return@withLock null
                accountOnline.set(false)
                val real = unwrap(error)
                if (isRejected(error)) {
                    SteamSessionStore.clear(app)
                    _state.value = SteamUiState.Failed(publicMessage(real))
                } else {
                    _state.value = SteamUiState.SignedIn(saved.accountName, saved.steamId, online = false)
                }
                logFailure(real)
                null
            }
        }
    }

    fun signIn(context: Context, account: String, password: String) {
        val app = context.applicationContext
        appContext = app
        val gen = generation.incrementAndGet()
        guardCode.getAndSet(null)?.cancel(true)
        signingIn.set(true)
        _state.value = SteamUiState.Busy("Connecting to Steam")
        scope.launch {
            var persisted: StoredSteamSession? = null
            try {
                connectWait()
                if (generation.get() != gen) return@launch
                _state.value = SteamUiState.Busy("Signing in")
                val saved = SteamSessionStore.load(app)
                val details = AuthSessionDetails().apply {
                    username = account.trim()
                    this.password = password
                    persistentSession = true
                    this.authenticator = authenticator
                    deviceFriendlyName = "VEXA"
                    clientOSType = EOSType.WinUnknown
                    guardData = saved?.guardData
                }
                val session = steamClient!!.authentication.beginAuthSessionViaCredentials(details).await()
                details.password = null
                authSession = session as? CredentialsAuthSession
                VexaLogger.log(LogLevel.INFO, LogCategory.CONFIG, "Steam credentials accepted")
                val poll = withTimeout(180_000) { awaitAuth(session, gen) } ?: return@launch
                if (generation.get() != gen) return@launch
                if (poll.refreshToken.isBlank() || poll.accountName.isBlank()) {
                    throw IllegalStateException("Steam did not return a session")
                }
                // Write the file before logon. A process restart can then read
                // it even if this logon does not finish.
                val drafted = StoredSteamSession(
                    accountName = poll.accountName,
                    steamId = steamIdFromToken(poll.refreshToken)
                        ?: steamIdFromToken(poll.accessToken)
                        ?: saved?.steamId
                        ?: "",
                    refreshToken = poll.refreshToken,
                    guardData = poll.newGuardData ?: saved?.guardData,
                )
                SteamSessionStore.save(app, drafted)
                persisted = drafted
                _state.value = SteamUiState.SignedIn(drafted.accountName, drafted.steamId, online = false)
                val steamId = logOnToken(poll.accountName, poll.refreshToken)
                if (generation.get() != gen) return@launch
                if (steamId != drafted.steamId) {
                    val updated = drafted.copy(steamId = steamId)
                    SteamSessionStore.save(app, updated)
                    persisted = updated
                }
                authSession = null
                accountOnline.set(true)
                _state.value = SteamUiState.SignedIn(poll.accountName, steamId, online = true)
                VexaLogger.log(LogLevel.INFO, LogCategory.CONFIG, "Steam signed in", mapOf("steamId" to steamId))
            } catch (error: Throwable) {
                if (generation.get() != gen) return@launch
                accountOnline.set(false)
                val real = unwrap(error)
                val kept = persisted
                if (isRejected(error)) {
                    SteamSessionStore.clear(app)
                    _state.value = SteamUiState.Failed(publicMessage(real))
                } else if (kept != null) {
                    _state.value = SteamUiState.SignedIn(kept.accountName, kept.steamId, online = false)
                } else {
                    _state.value = SteamUiState.Failed(publicMessage(real))
                }
                logFailure(real)
                runCatching { steamClient?.disconnect() }
            } finally {
                if (generation.get() == gen) signingIn.set(false)
            }
        }
    }

    fun submitGuard(code: String) {
        val trimmed = code.trim()
        if (trimmed.isEmpty()) return
        val pending = guardCode.getAndSet(null)
        if (pending != null) {
            pending.complete(trimmed)
            _state.value = SteamUiState.Busy("Checking Steam Guard")
            return
        }
        val session = authSession ?: return
        val kind = (_state.value as? SteamUiState.NeedGuard)?.kind ?: GuardKind.Device
        scope.launch {
            try {
                val type = if (kind == GuardKind.Email) {
                    EAuthSessionGuardType.k_EAuthSessionGuardType_EmailCode
                } else {
                    EAuthSessionGuardType.k_EAuthSessionGuardType_DeviceCode
                }
                session.sendSteamGuardCode(trimmed, type).await()
            } catch (error: Throwable) {
                val current = _state.value
                if (current is SteamUiState.NeedGuard) {
                    _state.value = current.copy(retry = true, attempt = current.attempt + 1)
                } else {
                    _state.value = SteamUiState.Failed(publicMessage(unwrap(error)))
                }
            }
        }
    }

    fun cancel() {
        generation.incrementAndGet()
        signingIn.set(false)
        guardCode.getAndSet(null)?.cancel(true)
        authSession = null
        accountOnline.set(false)
        runCatching { steamUser?.logOff() }
        val saved = appContext?.let { SteamSessionStore.load(it) }
        _state.value = if (saved != null) {
            SteamUiState.SignedIn(saved.accountName, saved.steamId, online = false)
        } else {
            SteamUiState.SignedOut
        }
    }

    fun signOut(context: Context) {
        generation.incrementAndGet()
        signingIn.set(false)
        guardCode.getAndSet(null)?.cancel(true)
        authSession = null
        accountOnline.set(false)
        SteamSessionStore.clear(context.applicationContext)
        runCatching { steamUser?.logOff() }
        _state.value = SteamUiState.SignedOut
        VexaLogger.log(LogLevel.INFO, LogCategory.CONFIG, "Steam signed out")
    }

    private suspend fun logOnSaved(saved: StoredSteamSession) {
        connectWait()
        logOnToken(saved.accountName, saved.refreshToken)
    }

    // JavaSteam delays by pollingInterval.toLong() milliseconds, but Steam's
    // interval is in seconds. The device-approval poll then runs about once
    // every 5ms and the CM socket dies. Code entry still uses the library,
    // because that path polls once.
    private suspend fun awaitAuth(session: AuthSession, gen: Int): AuthPollResult? {
        val preferred = session.allowedConfirmations.firstOrNull()
            ?: throw AuthenticationException("There are no allowed confirmations")
        val type = preferred.confirmationType
        if (type == EAuthSessionGuardType.k_EAuthSessionGuardType_Unknown) {
            throw AuthenticationException("There are no allowed confirmations")
        }
        if (type != EAuthSessionGuardType.k_EAuthSessionGuardType_DeviceConfirmation) {
            return session.pollingWaitForResult().await()
        }
        if (!authenticator.acceptDeviceConfirmation().await()) {
            return session.pollingWaitForResult().await()
        }
        val intervalMs = (session.pollingInterval * 1000f).toLong().coerceIn(1_000L, 30_000L)
        while (generation.get() == gen) {
            val result = session.pollAuthSessionStatus().await()
            if (result != null) return result
            delay(intervalMs)
        }
        return null
    }

    private suspend fun logOnToken(accountName: String, refreshToken: String): String {
        val user = steamUser ?: error("Steam user handler is missing")
        val wait = CompletableFuture<LoggedOnCallback>()
        loggedOn = wait
        user.logOn(
            LogOnDetails(
                username = accountName,
                accessToken = refreshToken,
                shouldRememberPassword = true,
                loginID = loginId,
                machineName = "VEXA",
                clientOSType = EOSType.WinUnknown,
            ),
        )
        val callback = withTimeout(30_000) { wait.await() }
        if (callback.result != EResult.OK) {
            throw SteamSignInException(callback.result)
        }
        val id = callback.clientSteamID ?: throw SteamSignInException(EResult.Fail)
        return id.convertToUInt64().toULong().toString()
    }

    private suspend fun connectWait() {
        // A failed password leaves the websocket up. The next attempt used to
        // send into that socket; if it had already died, Steam never answered
        // and the job failed with an empty message.
        replaceClient()
        val client = steamClient ?: error("Steam client is missing")
        val wait = CompletableFuture<Unit>()
        connected = wait
        client.connect()
        withTimeout(20_000) { wait.await() }
    }

    private fun replaceClient() {
        synchronized(lock) {
            sessionToken.incrementAndGet()
            val old = steamClient
            steamClient = null
            steamUser = null
            authSession = null
            callbacks = null
            runCatching { old?.disconnect() }
            ensureClient()
        }
    }

    private fun ensureClient() {
        if (steamClient != null) return
        synchronized(lock) {
            if (steamClient != null) return
            Class.forName("in.dragonbra.javasteam.util.crypto.CryptoHelper")
            preferAndroidTls()
            val created = SteamClient()
            val manager = CallbackManager(created)
            val user = created.getHandler(SteamUser::class.java)
                ?: error("Steam user handler is missing")
            val token = sessionToken.get()
            manager.subscribe(ConnectedCallback::class.java) {
                if (token == sessionToken.get()) connected.complete(Unit)
            }
            manager.subscribe(LoggedOnCallback::class.java) {
                if (token == sessionToken.get()) loggedOn.complete(it)
            }
            callbacks = manager
            steamClient = created
            steamUser = user
            if (pumping.compareAndSet(false, true)) {
                Thread({
                    while (pumping.get()) {
                        val current = callbacks
                        if (current == null) {
                            Thread.sleep(50)
                            continue
                        }
                        try {
                            current.runWaitCallbacks(250)
                        } catch (_: Throwable) {
                        }
                    }
                }, "steam-callbacks").apply { isDaemon = true }.start()
            }
        }
    }

    // Steam refresh and access tokens are JWTs. The sub claim is the steam id.
    // The token itself is never logged.
    private fun steamIdFromToken(token: String?): String? {
        if (token.isNullOrBlank()) return null
        val body = token.split('.').getOrNull(1) ?: return null
        return try {
            val pad = (4 - body.length % 4) % 4
            val decoded = Base64.decode(body + "=".repeat(pad), Base64.URL_SAFE or Base64.NO_WRAP)
            val sub = JSONObject(String(decoded, Charsets.UTF_8)).opt("sub")
            val text = when (sub) {
                is String -> sub
                is Number -> sub.toLong().toString()
                else -> ""
            }
            text.takeIf { value -> value.length in 16..20 && value.all { it.isDigit() } }
        } catch (_: Exception) {
            null
        }
    }

    private fun ask(kind: GuardKind, hint: String, retry: Boolean): CompletableFuture<String> {
        val future = CompletableFuture<String>()
        guardCode.set(future)
        val attempt = ((_state.value as? SteamUiState.NeedGuard)?.attempt ?: 0) + 1
        _state.value = SteamUiState.NeedGuard(kind, hint, retry, attempt)
        return future
    }

    private fun preferAndroidTls() {
        // JavaSteam inserts BouncyCastle at provider position 1 on Android.
        // Named "BC" ciphers still resolve after it is moved behind Conscrypt.
        val provider = Security.getProvider("BC") ?: return
        Security.removeProvider("BC")
        Security.addProvider(provider)
    }

    private fun isRejected(error: Throwable): Boolean {
        val name = when (val real = unwrap(error)) {
            is SteamSignInException -> real.result.name
            is AuthenticationException -> real.result?.name
            else -> null
        } ?: return false
        return name == "InvalidPassword" || name == "AccessDenied" || name == "Revoked" ||
            name.contains("Expired")
    }

    private fun unwrap(error: Throwable): Throwable {
        var current = error
        while ((current is ExecutionException || current is CompletionException) && current.cause != null) {
            current = current.cause!!
        }
        return current
    }

    private fun logFailure(error: Throwable) {
        val kind = generateSequence(error) { it.cause }
            .take(4)
            .joinToString(">") { it.javaClass.simpleName }
        VexaLogger.log(
            LogLevel.ERROR,
            LogCategory.FAILURE,
            "Steam sign-in failed",
            mapOf("error" to kind),
        )
    }

    private fun publicMessage(error: Throwable): String {
        val raw = when (error) {
            is SteamSignInException -> "Steam logon: ${error.result.name}"
            is TimeoutCancellationException -> "Steam timed out. Try again"
            is CancellationException -> "Steam did not answer. Try again"
            is AsyncJobFailedException -> "Steam closed the login attempt. Try again"
            is AuthenticationException -> error.result?.name?.let { "Steam auth: $it" }
                ?: error.message?.lineSequence()?.firstOrNull().orEmpty().ifBlank { "Steam authentication failed" }
            else -> error.message?.lineSequence()?.firstOrNull().orEmpty()
        }
        val cleaned = raw.replace(
            Regex("eyJ[A-Za-z0-9_\\-]{10,}\\.[A-Za-z0-9_\\-]+\\.[A-Za-z0-9_\\-]+"),
            "[token]",
        )
        val text = cleaned.ifBlank { "Steam sign-in failed (${error.javaClass.simpleName})" }
        return text.take(180)
    }

    private class SteamSignInException(val result: EResult) : Exception("Steam logon: ${result.name}")
}
