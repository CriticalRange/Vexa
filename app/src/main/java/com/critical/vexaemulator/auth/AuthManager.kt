package com.critical.vexaemulator.auth

import android.app.Activity
import android.content.Context
import org.json.JSONObject
import java.nio.charset.StandardCharsets
import java.util.Base64

class AuthManager(context: Context) {
    private val store =
        SecureAuthStore(context.applicationContext)
    private val oauth = OAuthPkceClient()
    private val sessions = HytaleSessionsClient()

    companion object {
        private const val EXPIRY_SKEW_SEC = 180L
    }

    fun load(): AuthState? = store.load()

    fun logout() {
        store.delete()
    }

    suspend fun login(activity: Activity): AuthState {
        val tokenResponse = oauth.login(activity)
        val state = AuthState()
        applyTokenResponse(state, tokenResponse)
        store.save(state)
        return state
    }

    suspend fun loginAndEnsureGameTokens(
        activity:
        Activity
    ): AuthState {
        login(activity)
        return ensureGameTokens()
    }

    suspend fun refreshGameSession(): AuthState {
        val state = store.load() ?: throw IllegalStateException("Not logged in")
        if (state.gameSessionToken.isNullOrBlank()) {
            return ensureGameTokens()
        }

        val refreshed =
            sessions.refreshGameSession(
                state.gameSessionToken!!
            )
        state.gameIdentityToken =
            refreshed.identityToken
        state.gameSessionToken =
            refreshed.sessionToken
        state.gameSessionExpiresAtEpochSec =
            parseJwtExpEpochSec(refreshed.sessionToken)

        store.save(state)
        return state
    }

    suspend fun ensureFreshOAuth(): AuthState {
        val state = store.load() ?: throw IllegalStateException("Not logged in")
        val now = nowEpochSec()

        if (!state.oauthRefreshToken.isNullOrBlank()
            &&
            !state.oauthAccessToken.isNullOrBlank()
            &&

            (state.oauthAccessTokenExpiresAtEpochSec ?: 0L) >
            now + EXPIRY_SKEW_SEC
        ) {
            return state
        }

        val refresh =
            state.oauthRefreshToken ?: throw IllegalStateException("Missing OAuth refresh token")
        val refreshed = oauth.refresh(refresh)
        applyTokenResponse(state, refreshed)
        if (!refreshed.refreshToken.isNullOrBlank()) {
            state.oauthRefreshToken =
                refreshed.refreshToken
        }

        store.save(state)
        return state
    }

    suspend fun ensureGameTokens(): AuthState {
        val state = ensureFreshOAuth()
        val now = nowEpochSec()

        val identityExp =
            parseJwtExpEpochSec(state.gameIdentityToken)
        val sessionExp =
            state.gameSessionExpiresAtEpochSec ?: parseJwtExpEpochSec(state.gameSessionToken)
        val identityFresh = identityExp != null &&
                identityExp > now + EXPIRY_SKEW_SEC
        val sessionFresh = sessionExp != null &&
                sessionExp > now + EXPIRY_SKEW_SEC

        if (state.hasGameTokens && identityFresh &&
            sessionFresh
        ) {
            state.gameSessionExpiresAtEpochSec =
                sessionExp
            return state
        }

        if (!state.gameSessionToken.isNullOrBlank()) {
            runCatching {
                val refreshed =
                    sessions.refreshGameSession(
                        state.gameSessionToken!!
                    )
                state.gameIdentityToken =
                    refreshed.identityToken
                state.gameSessionToken =
                    refreshed.sessionToken
                state.gameSessionExpiresAtEpochSec =
                    parseJwtExpEpochSec(refreshed.sessionToken)
                store.save(state)
                return state
            }
        }

        if (state.gameProfileUuid.isNullOrBlank()) {
            val accessToken =
                state.oauthAccessToken ?: throw IllegalStateException("Missing OAuth access token")
            val profiles =
                sessions.getProfiles(accessToken)
            if (profiles.isEmpty()) throw IllegalStateException("No game profiles found for this account")
            state.gameProfileUuid =
                profiles.first().uuid
            state.gameProfileName =
                profiles.first().name
        }

        val accessToken =
            state.oauthAccessToken ?: throw IllegalStateException("Missing OAuth access token")
        val tokens =
            sessions.acquireGameTokens(
                accessToken,
                state.gameProfileUuid!!
            )
        state.gameIdentityToken =
            tokens.identityToken
        state.gameSessionToken = tokens.sessionToken
        state.gameSessionExpiresAtEpochSec =
            parseJwtExpEpochSec(tokens.sessionToken)

        store.save(state)
        return state
    }

    fun buildLaunchContract(state: AuthState):
            Pair<List<String>, List<String>> {
        if (!state.hasGameTokens) throw IllegalStateException("Missing game tokens")

        fun sanitize(v: String?): String = (v ?: "").replace("\r", "").replace("\n", "")

        val env = mutableListOf(

            "HYTALE_IDENTITY_TOKEN=${sanitize(state.gameIdentityToken)}",

            "HYTALE_SESSION_TOKEN=${sanitize(state.gameSessionToken)}",

            "HOME=/data/user/0/com.critical.vexaemulator/files/game/Client",

            "USER=hymobile",

            "TMPDIR=/data/user/0/com.critical.vexaemulator/files/fex-runtime/tmp",

            "XDG_RUNTIME_DIR=/data/user/0/com.critical.vexaemulator/files/fex-runtime/run",

            "PATH=/data/user/0/com.critical.vexaemulator/files/game/Client/jre/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",

            "GLIBC_TUNABLES=glibc.pthread.rseq=0",

            "DOTNET_EnableWriteXorExecute=0",

            "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1",

            "DOTNET_EnableDiagnostics=0",

            "DOTNET_gcServer=0",

            "SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt",
        )

        if (!state.gameProfileUuid.isNullOrBlank())
            env +=
                "HYTALE_PROFILE_UUID=${sanitize(state.gameProfileUuid)}"
        if (!state.gameProfileName.isNullOrBlank())
            env +=
                "HYTALE_PROFILE_NAME=${sanitize(state.gameProfileName)}"

        val args = mutableListOf<String>()
        if (!state.gameProfileUuid.isNullOrBlank()
            && !state.gameProfileName.isNullOrBlank()
        ) {
            args += "--uuid"
            args += state.gameProfileUuid!!
            args += "--name"
            args += state.gameProfileName!!
        }

        return env to args
    }

    private fun applyTokenResponse(
        state: AuthState,
        tr: TokenResponse
    ) {
        if (!tr.accessToken.isNullOrBlank())
            state.oauthAccessToken = tr.accessToken
        if (!tr.refreshToken.isNullOrBlank())
            state.oauthRefreshToken = tr.refreshToken
        if (!tr.idToken.isNullOrBlank())
            state.oauthIdToken = tr.idToken
        if (!tr.scope.isNullOrBlank())
            state.oauthScope = tr.scope
        if (tr.expiresIn != null)
            state.oauthAccessTokenExpiresAtEpochSec = nowEpochSec() + tr.expiresIn
    }

    private fun nowEpochSec(): Long =
        System.currentTimeMillis() / 1000L

    private fun parseJwtExpEpochSec(jwt: String?):
            Long? {
        if (jwt.isNullOrBlank()) return null
        return runCatching {
            val parts = jwt.split('.')
            if (parts.size < 2) return null

            var payload = parts[1].replace(
                '-',
                '+'
            ).replace('_', '/')
            when (payload.length % 4) {
                2 -> payload += "=="
                3 -> payload += "="
            }

            val decoded =
                String(
                    Base64.getDecoder().decode(payload),
                    StandardCharsets.UTF_8
                )
            val json = JSONObject(decoded)
            if (!json.has("exp")) null else
                json.optLong("exp")
        }.getOrNull()
    }
}