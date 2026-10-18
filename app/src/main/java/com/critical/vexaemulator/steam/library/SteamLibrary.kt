package com.critical.vexaemulator.steam.library
import android.content.Context
import com.critical.vexaemulator.steam.SteamLink
import com.critical.vexaemulator.steam.SteamUiState
import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.protobufs.steamclient.SteammessagesPlayerSteamclient.CPlayer_GetOwnedGames_Request
import `in`.dragonbra.javasteam.protobufs.steamclient.SteammessagesPlayerSteamclient.CPlayer_GetOwnedGames_Response
import `in`.dragonbra.javasteam.steam.handlers.steamapps.PICSRequest
import `in`.dragonbra.javasteam.steam.handlers.steamapps.SteamApps
import `in`.dragonbra.javasteam.steam.handlers.steamunifiedmessages.SteamUnifiedMessages
import `in`.dragonbra.javasteam.types.KeyValue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.Job
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.delay
import `in`.dragonbra.javasteam.rpc.service.Player
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.future.await
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.ConcurrentHashMap
import kotlin.coroutines.cancellation.CancellationException
enum class CatalogKind { Owned, All }
data class OnlineGame(
    val appId: Int,
    val name: String,
    val owned: Boolean,
    val installable: Boolean,
    val installed: Boolean,
    val detailsReady: Boolean = true,
    val genre: String = "",
)
data class OnlineLibraryState(
    val loading: Boolean = false,
    val phase: String = "",
    val signedOut: Boolean = true,
    val connectionProblem: Boolean = false,
    val message: String = "",
    val games: List<OnlineGame> = emptyList(),
    val canLoadMore: Boolean = false,
    val downloads: Map<Int, DepotProgress> = emptyMap(),
)
// Owned apps come from Player.GetOwnedGames. Package licenses are only a fallback.
// "All" pages the public Steam store game search. The PICS change list from 0 is empty.
private val steamGenres = mapOf(
    "1" to "Action",
    "2" to "Strategy",
    "3" to "RPG",
    "4" to "Casual",
    "9" to "Racing",
    "18" to "Sports",
    "23" to "Indie",
    "25" to "Adventure",
    "28" to "Simulation",
    "29" to "Massively Multiplayer",
    "37" to "Free to Play",
    "51" to "Animation & Modeling",
    "52" to "Audio Production",
    "53" to "Design & Illustration",
    "54" to "Education",
    "55" to "Photo Editing",
    "56" to "Software Training",
    "57" to "Utilities",
    "58" to "Video Production",
    "59" to "Web Publishing",
    "60" to "Game Development",
    "70" to "Early Access",
)
object SteamLibrary {
    private const val pageSize = 40
    private val mutex = Mutex()
    private val catalogMutex = Mutex()
    private val downloadScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val downloads = ConcurrentHashMap<Int, Job>()
    private val names = ConcurrentHashMap<Int, String>()
    private val details = ConcurrentHashMap<Int, KeyValue>()
    private var kind = CatalogKind.Owned
    private var pending = listOf<Int>()
    private var ownedIds = setOf<Int>()
    private var ownedGames = listOf<OnlineGame>()
    private var ownedPending = listOf<Int>()
    private var ownedComplete = false
    private var storeNext = 0
    private var storeTotal = 0
    private val _state = MutableStateFlow(OnlineLibraryState())
    val state: StateFlow<OnlineLibraryState> = _state.asStateFlow()
    suspend fun refresh(context: Context, requested: CatalogKind) = catalogMutex.withLock {
        if (requested == CatalogKind.Owned && ownedComplete) {
            kind = CatalogKind.Owned
            _state.update {
                it.copy(
                    loading = false,
                    phase = "",
                    signedOut = false,
                    connectionProblem = false,
                    message = "",
                    games = ownedGames,
                    canLoadMore = false,
                )
            }
            return@withLock
        }
        kind = requested
        val app = context.applicationContext
        val started = System.currentTimeMillis()
        try {
            // Public browsing must not wait for account login or private ownership requests.
            if (requested == CatalogKind.All) {
                storeNext = 0
                storeTotal = 0
                pending = emptyList()
                _state.update { it.copy(loading = true, phase = "Loading Steam catalog...", message = "", games = emptyList(), canLoadMore = false, connectionProblem = false) }
                pending = loadStorePage()
                publishPage(app, append = false, enrich = false)
                if (SteamLink.connectedClient() != null) {
                    ownedIds = loadOwnedAppIds()
                    enrichVisible()
                }
            } else {
                val session = withTimeout(45_000) { SteamLink.ensureOnline(app) }
                if (session == null || SteamLink.connectedClient() == null) {
                    val saved = SteamLink.state.value is SteamUiState.SignedIn
                    show(
                        signedOut = !saved,
                        message = if (saved) {
                            "The Steam account is saved, but VEXA could not open a connection."
                        } else {
                            "No Steam account is connected on this device."
                        },
                        connectionProblem = true,
                    )
                    return@withLock
                }
                if (ownedGames.isEmpty()) {
                    _state.update { it.copy(loading = true, phase = "Loading your games...", message = "", games = emptyList(), canLoadMore = false, connectionProblem = false) }
                    ownedIds = loadOwnedAppIds()
                    if (ownedIds.isEmpty()) {
                        show(false, "Steam returned no owned games for this account.")
                        return@withLock
                    }
                }
                val installed = withContext(Dispatchers.IO) { InstalledSteamPrograms.read(app).map { it.appId }.toSet() }
                val rows = ownedIds.map { id -> gameRow(id, details[id], installed) }
                pending = emptyList()
                ownedPending = emptyList()
                _state.update {
                    it.copy(
                        loading = rows.any { row -> !row.detailsReady },
                        phase = if (rows.any { row -> !row.detailsReady }) "Checking Linux versions..." else "",
                        message = "",
                        games = rows,
                        canLoadMore = false,
                        connectionProblem = false,
                        signedOut = false,
                    )
                }
                enrichOwned(rows.filter { !it.detailsReady }.map { it.appId }, installed)
                rememberOwned()
            }
            Log.i("VexaSteamLibrary", "${requested.name}: ${state.value.games.size} rows in ${System.currentTimeMillis() - started}ms")
        } catch (error: TimeoutCancellationException) {
            _state.update { it.copy(connectionProblem = true, message = "Steam took too long to respond.") }
        } catch (error: CancellationException) {
            throw error
        } catch (error: Exception) {
            _state.update { it.copy(message = publicText(error)) }
            Log.w("VexaSteamLibrary", "Catalog failed: ${error.javaClass.simpleName}")
        } finally {
            _state.update { it.copy(loading = false, phase = "") }
            if (requested == CatalogKind.Owned && state.value.games.isNotEmpty()) rememberOwned()
        }
    }
    private fun rememberOwned() {
        ownedGames = state.value.games
        ownedPending = pending
        ownedComplete = ownedGames.isNotEmpty() && ownedPending.isEmpty() && ownedGames.all { it.detailsReady }
    }
    private fun show(signedOut: Boolean, message: String, connectionProblem: Boolean = false) {
        _state.update { current ->
            OnlineLibraryState(
                signedOut = signedOut,
                connectionProblem = connectionProblem,
                message = message,
                downloads = current.downloads,
            )
        }
    }
    suspend fun loadMore(context: Context) = catalogMutex.withLock {
        if (!state.value.canLoadMore) return@withLock
        _state.update { it.copy(loading = true, phase = "Loading more games...", message = "") }
        try {
            if (pending.isEmpty() && kind == CatalogKind.All && storeNext < storeTotal) pending = loadStorePage()
            publishPage(context.applicationContext, append = true)
        } catch (error: TimeoutCancellationException) {
            _state.update { it.copy(message = "Steam took too long to respond. Try again.") }
        } catch (error: CancellationException) { throw error
        } catch (error: Exception) {
            _state.update { it.copy(message = publicText(error)) }
        } finally { _state.update { it.copy(loading = false, phase = "") } }
    }
    @Synchronized
    fun install(context: Context, appId: Int) {
        if (downloads.containsKey(appId)) return
        val app = context.applicationContext
        val info = details[appId] ?: return
        val title = names[appId] ?: state.value.games.find { it.appId == appId }?.name ?: "App $appId"
        val linux = SteamDepotDownloader.linuxDepots(info, appId)
        if (appId !in ownedIds || linux.isEmpty()) return
        if (state.value.games.find { it.appId == appId }?.installed == true) return
        val accountId = SteamLink.connectedClient()?.steamID?.convertToUInt64() ?: return
        _state.update { it.copy(downloads = it.downloads + (appId to DepotProgress(-1f, "Queued"))) }
        val job = downloadScope.launch(start = CoroutineStart.LAZY) {
            try {
                mutex.withLock {
                    _state.update { it.copy(downloads = it.downloads + (appId to DepotProgress(-1f, "Connecting..."))) }
                    withTimeout(45_000) { SteamLink.ensureOnline(app) }
                    val client = SteamLink.connectedClient() ?: error("Steam is offline. Reconnect and try again.")
                    check(client.steamID?.convertToUInt64() == accountId) { "Steam account changed. Start the download again." }
                    _state.update { it.copy(downloads = it.downloads + (appId to DepotProgress(-1f, "Checking download licenses..."))) }
                    val allowedDepots = loadDepotEntitlements()
                    SteamDepotDownloader.download(app, client, appId, title, info, allowedDepots) { progress ->
                        _state.update { it.copy(downloads = it.downloads + (appId to progress)) }
                    }
                    _state.update { state -> state.copy(games = state.games.map { if (it.appId == appId) it.copy(installed = true) else it }) }
                }
            } catch (error: TimeoutCancellationException) {
                _state.update { it.copy(message = "Download timed out. Check the connection and try again.") }
            } catch (error: CancellationException) { throw error
            } catch (error: LinkageError) {
                Log.e("VexaSteamLibrary", "Download codec could not load", error)
                _state.update { it.copy(message = "The download decoder could not load. Update VEXA and retry.") }
            } catch (error: Exception) {
                android.util.Log.e("VexaSteamLibrary", "Download failed", error)
                _state.update { it.copy(message = publicText(error)) }
            } finally {
                downloads.remove(appId)
                _state.update { it.copy(downloads = it.downloads - appId) }
            }
        }
        downloads[appId] = job
        job.start()
    }
    fun cancelInstall(appId: Int) { downloads[appId]?.cancel() }
    private suspend fun loadDepotEntitlements(): Set<Int> {
        val packages = SteamLink.licenses().groupBy { it.packageID }.mapNotNull { (id, licenses) ->
            if (id <= 0) null else PICSRequest(id, licenses.firstOrNull { it.accessToken != 0L }?.accessToken ?: 0L)
        }
        check(packages.isNotEmpty()) { "Steam licenses are not ready. Try again shortly." }
        return packages.chunked(100).flatMap { chunk ->
            fetchPackageInfo(chunk).values.flatMap { it["depotids"].children }
                .mapNotNull { it.asString()?.toIntOrNull() }.filter { it > 0 }
        }.toSet()
    }
    private suspend fun publishPage(context: Context, append: Boolean, enrich: Boolean = true) {
        val slice = pending.take(pageSize)
        val installed = withContext(Dispatchers.IO) { InstalledSteamPrograms.read(context).map { it.appId }.toSet() }
        val page = slice.map { id -> gameRow(id, details[id], installed) }
        pending = pending.drop(pageSize)
        _state.update { state ->
            val shown = (if (append) state.games + page else page).distinctBy { it.appId }
            state.copy(
                loading = enrich && SteamLink.connectedClient() != null,
                phase = "Checking download availability...",
                signedOut = SteamLink.connectedClient() == null,
                games = shown,
                canLoadMore = pending.isNotEmpty() || (kind == CatalogKind.All && storeNext < storeTotal),
                message = when {
                    shown.isEmpty() && !append -> "Nothing on this page could be shown."
                    state.message.isNotBlank() -> state.message
                    else -> ""
                },
            )
        }
        if (enrich && SteamLink.connectedClient() != null) enrichVisible()
    }
    private suspend fun enrichVisible() {
        val visible = state.value.games
        enrichOwned(visible.filter { !it.detailsReady }.map { it.appId }, visible.filter { it.installed }.map { it.appId }.toSet())
    }
    /** Full app info is large, so Linux checks go out in batches and the list updates after each one. */
    private suspend fun enrichOwned(appIds: List<Int>, installed: Set<Int>) {
        val missing = appIds.filter { details[it] == null }
        for (chunk in missing.chunked(100)) {
            val infos = fetchAppInfo(chunk)
            details.putAll(infos)
            val answered = chunk.toSet()
            _state.update { state ->
                state.copy(games = state.games.map { row ->
                    if (row.appId !in answered) row
                    else gameRow(row.appId, details[row.appId], installed).let { ready ->
                        if (details[row.appId] == null) ready.copy(detailsReady = true) else ready
                    }
                })
            }
        }
    }
    private fun gameRow(appId: Int, info: KeyValue?, installed: Set<Int>): OnlineGame {
        if (info == null) return OnlineGame(appId, names[appId] ?: "App $appId", appId in ownedIds, false, appId in installed, false)
        val name = info["common"]["name"].asString().orEmpty().ifBlank { "App $appId" }
        val owned = appId in ownedIds
        val linux = SteamDepotDownloader.linuxDepots(info, appId)
        return OnlineGame(
            appId = appId,
            name = name,
            owned = owned,
            installable = owned && linux.isNotEmpty(),
            installed = appId in installed,
            genre = genreLabel(info),
        )
    }
    private suspend fun loadOwnedAppIds(): Set<Int> {
        val played = try { ownedFromPlayerService() }
        catch (_: TimeoutCancellationException) { emptySet() }
        catch (error: CancellationException) { throw error }
        catch (_: Exception) { emptySet() }
        if (played.isNotEmpty()) return played
        repeat(10) { if (SteamLink.licenses().isEmpty()) delay(200) }
        val packages = SteamLink.licenses()
            .groupBy { it.packageID }
            .mapNotNull { (packageId, licenses) ->
                if (packageId <= 0) null
                else PICSRequest(packageId, licenses.firstOrNull { it.accessToken != 0L }?.accessToken ?: 0L)
            }
        if (packages.isEmpty()) return emptySet()
        val ids = linkedSetOf<Int>()
        packages.chunked(pageSize).forEach { chunk ->
            val infos = fetchPackageInfo(chunk)
            infos.values.forEach { info ->
                info["appids"].children.forEach { child ->
                    val fromValue = child.asString().orEmpty().toIntOrNull()
                    val fromName = child.name?.toIntOrNull()
                    val appId = fromValue ?: fromName ?: return@forEach
                    if (appId > 0) ids.add(appId)
                }
            }
        }
        return ids
    }
    private suspend fun ownedFromPlayerService(): Set<Int> {
        val client = SteamLink.connectedClient() ?: return emptySet()
        val unified = client.getHandler(SteamUnifiedMessages::class.java) ?: return emptySet()
        val steamId = client.steamID?.convertToUInt64() ?: return emptySet()
        if (steamId == 0L) return emptySet()
        val request = CPlayer_GetOwnedGames_Request.newBuilder()
            .setSteamid(steamId)
            .setIncludeAppinfo(true)
            .setIncludePlayedFreeGames(true)
            .setIncludeFreeSub(false)
            .setSkipUnvettedApps(false)
            .build()
        // Register Player before sending: raw sendMessage silently drops replies for unregistered services.
        val player = unified.createService(Player::class.java)
        val response = withTimeout(15_000) { player.getOwnedGames(request).toFuture().await() }
        if (response.result != EResult.OK) error("Steam ownership request: ${response.result}")
        val games = response.body.gamesList
        games.forEach { if (it.name.isNotBlank()) names[it.appid] = it.name }
        return games
            .sortedByDescending { it.playtimeForever }
            .map { it.appid }
            .filter { it > 0 }
            .toCollection(linkedSetOf())
    }
    private suspend fun loadStorePage(): List<Int> = withContext(Dispatchers.IO) {
        if (storeTotal > 0 && storeNext >= storeTotal) return@withContext emptyList()
        val connection = URL(
            "https://store.steampowered.com/search/results/?json=1&category1=998&start=$storeNext&count=$pageSize&infinite=1",
        ).openConnection() as HttpURLConnection
        connection.connectTimeout = 8000
        connection.readTimeout = 10000
        connection.setRequestProperty("User-Agent", "VEXA")
        try { connection.inputStream.use { input ->
            val root = JSONObject(input.bufferedReader().readText())
            if (root.optInt("success") != 1) error("Steam store search failed")
            val total = root.optInt("total_count")
            val html = root.optString("results_html")
            Regex("<a\\b[^>]*data-ds-appid=\"(\\d+)\"[\\s\\S]*?</a>").findAll(html).forEach { match ->
                val id = match.groupValues[1].toIntOrNull() ?: return@forEach
                val title = Regex("<span class=\"title\">([\\s\\S]*?)</span>").find(match.value)?.groupValues?.get(1)
                if (title != null) names[id] = android.text.Html.fromHtml(title, android.text.Html.FROM_HTML_MODE_LEGACY).toString()
            }
            val ids = Regex("data-ds-appid=\"(\\d+)").findAll(html)
                .mapNotNull { it.groupValues[1].toIntOrNull() }
                .filter { it > 0 }
                .distinct()
                .toList()
            if (ids.isEmpty() && storeNext < total) error("Steam catalog format changed or returned no rows. Try Refresh.")
            storeTotal = total
            storeNext += pageSize
            ids
        } } finally { connection.disconnect() }
    }
    private fun genreLabel(info: KeyValue): String {
        val names = info["common"]["genres"].children.mapNotNull { child ->
            val raw = child.asString().orEmpty().ifBlank { child.name.orEmpty() }
            when {
                raw.isBlank() -> null
                raw.all { it.isDigit() } -> steamGenres[raw]
                else -> raw
            }
        }
        return names.distinct().take(2).joinToString(" · ")
    }
    private suspend fun fetchAppInfo(appIds: List<Int>): Map<Int, KeyValue> {
        if (appIds.isEmpty()) return emptyMap()
        return fetchProducts(appIds.map { PICSRequest(it) }, emptyList()).first
    }
    private suspend fun fetchPackageInfo(packages: List<PICSRequest>): Map<Int, KeyValue> {
        if (packages.isEmpty()) return emptyMap()
        return fetchProducts(emptyList(), packages).second
    }
    private suspend fun fetchProducts(
        apps: List<PICSRequest>,
        packages: List<PICSRequest>,
    ): Pair<Map<Int, KeyValue>, Map<Int, KeyValue>> {
        val client = SteamLink.connectedClient() ?: error("Steam is not connected")
        val handler = client.getHandler(SteamApps::class.java) ?: error("Steam apps handler missing")
        val result = withTimeout(45_000) {
            handler.picsGetProductInfo(apps, packages, false).toFuture().await()
        }
        if (result.failed) error("Steam product info failed")
        val appMap = linkedMapOf<Int, KeyValue>()
        val packageMap = linkedMapOf<Int, KeyValue>()
        result.results.forEach { callback ->
            callback.apps.values.forEach { info ->
                if (!info.isMissingToken) appMap[info.id] = info.keyValues
            }
            callback.packages.values.forEach { info ->
                if (!info.isMissingToken) packageMap[info.id] = info.keyValues
            }
        }
        return appMap to packageMap
    }
    private fun publicText(error: Throwable): String {
        val raw = generateSequence(error) { it.cause }
            .mapNotNull { it.message?.lineSequence()?.firstOrNull() }
            .firstOrNull()
            .orEmpty()
        val cleaned = raw.replace(
            Regex("eyJ[A-Za-z0-9_\\-]{10,}\\.[A-Za-z0-9_\\-]+\\.[A-Za-z0-9_\\-]+"),
            "[token]",
        )
        return cleaned.ifBlank { "Steam library request failed" }.take(180)
    }
}
