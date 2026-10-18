package com.critical.vexaemulator.steam

import android.content.Context
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import `in`.dragonbra.javasteam.protobufs.steamclient.Enums.ECloudStoragePersistState
import `in`.dragonbra.javasteam.steam.handlers.steamcloud.AppFileInfo
import `in`.dragonbra.javasteam.steam.handlers.steamcloud.SteamCloud
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.future.await
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import java.util.zip.ZipInputStream

enum class CloudChoice { Local, Remote }

data class CloudConflict(
    val name: String,
    val localMtime: Long,
    val remoteTime: Long,
)

data class CloudSyncResult(
    val downloaded: Int = 0,
    val upToDate: Int = 0,
    val keptLocal: Int = 0,
    val unmapped: Int = 0,
    val failed: Int = 0,
    val skipped: String? = null,
) {
    fun summary(): String = skipped ?: buildString {
        append("$downloaded downloaded")
        if (upToDate > 0) append(", $upToDate current")
        if (keptLocal > 0) append(", $keptLocal local newer")
        if (unmapped > 0) append(", $unmapped unmapped")
        if (failed > 0) append(", $failed failed")
    }
}

// Pulls an app's Steam Cloud files into the guest before it starts. Files with a
// root token (%LinuxXdgConfigHome%, %GameInstall%, ...) land where the game looks
// for them. Files without one are ISteamRemoteStorage files; the Steam API stub
// reads those from $HOME/.local/share/vexa/steam/<appid>/remote.
// A local file that changed since the last sync and is newer than the cloud copy
// is kept. A local file replaced by a newer cloud copy is saved under
// files/steam/cloud/<appid>/backup first.
object SteamCloudSync {
    private const val LIST_TIMEOUT_MS = 20_000L
    private const val TOTAL_TIMEOUT_MS = 90_000L


    suspend fun pull(
        context: Context,
        appId: Int,
        home: File,
        gameDir: File,
        onProgress: (String) -> Unit,
        onConflict: suspend (List<CloudConflict>) -> CloudChoice,
    ): CloudSyncResult {
        val cloud = SteamLink.cloud() ?: return CloudSyncResult(skipped = "offline, using local saves")
        val store = File(context.filesDir, "steam/cloud/$appId").apply { mkdirs() }
        val manifestFile = File(store, "manifest.json")
        val manifest = readManifest(manifestFile)
        val roots = Roots(home, gameDir, File(home, ".local/share/vexa/steam/$appId/remote"))
        return try {
            withTimeout(TOTAL_TIMEOUT_MS) {
                withContext(Dispatchers.IO) { run(cloud, appId, roots, store, manifest, onProgress, onConflict) }
            }
        } catch (error: kotlinx.coroutines.TimeoutCancellationException) {
            VexaLogger.log(LogLevel.WARN, LogCategory.BOOT, "Steam Cloud timed out",
                mapOf("appId" to appId.toString()))
            CloudSyncResult(skipped = "cloud timed out, using local saves")
        } catch (error: kotlinx.coroutines.CancellationException) {
            throw error
        } catch (error: Throwable) {
            VexaLogger.log(
                LogLevel.WARN, LogCategory.BOOT, "Steam Cloud sync failed",
                mapOf("appId" to appId.toString(), "error" to (error.message ?: error.javaClass.simpleName)),
            )
            CloudSyncResult(skipped = "cloud unavailable, using local saves")
        } finally {
            writeManifest(manifestFile, manifest)
        }
    }

    private suspend fun run(
        cloud: SteamCloud,
        appId: Int,
        roots: Roots,
        store: File,
        manifest: MutableMap<String, String>,
        onProgress: (String) -> Unit,
        onConflict: suspend (List<CloudConflict>) -> CloudChoice,
    ): CloudSyncResult {
        onProgress("listing cloud files")
        val list = retry("list") { withTimeout(LIST_TIMEOUT_MS) { cloud.getAppFileListChange(appId, 0).await() } }
        val files = list.files.filter { it.rawFileSize >= 0 }
        VexaLogger.log(
            LogLevel.INFO, LogCategory.BOOT, "Steam Cloud listing",
            mapOf(
                "appId" to appId.toString(),
                "files" to files.size.toString(),
                "prefixes" to list.pathPrefixes.joinToString("|"),
                "sample" to files.take(4).joinToString("|") { it.filename },
            ),
        )
        var downloaded = 0
        var current = 0
        var kept = 0
        var unmapped = 0
        var failed = 0
        val conflicts = mutableListOf<CloudConflict>()
        val pending = mutableListOf<Triple<AppFileInfo, String, File>>()
        files.forEachIndexed { index, info ->
            val name = placeTruckProfiles(appId, effectiveName(info, list.pathPrefixes))
            val target = roots.resolve(name)
            if (target == null) {
                unmapped += 1
                return@forEachIndexed
            }
            // Dead Cells and other RemoteStorage games also keep a copy in the
            // install save folder and prefer it over FileRead. Compare that copy.
            if (info.persistState == ECloudStoragePersistState.k_ECloudStoragePersistStateDeleted) {
                return@forEachIndexed
            }
            onProgress("${index + 1}/${files.size} · ${target.name}")
            VexaLogger.log(
                LogLevel.INFO, LogCategory.BOOT, "Steam Cloud file",
                mapOf(
                    "file" to name,
                    "remoteTs" to info.timestamp.time.toString(),
                    "localMtime" to target.lastModified().toString(),
                    "remoteSha" to info.shaFile.toHex().take(12),
                ),
            )
            val remoteSha = info.shaFile.toHex()
            val visible = roots.gameSave(name)?.takeIf { it.isFile } ?: target
            val localSha = if (visible.isFile) sha1(visible.readBytes()) else null
            val localChanged = localSha != null && manifest[name] != localSha
            if (localSha != null && localSha != remoteSha && localChanged) {
                conflicts += CloudConflict(name, visible.lastModified(), info.timestamp.time)
            }
            pending += Triple(info, name, target)
        }
        // One popup per launch: every divergent file follows the same choice.
        val choice = if (conflicts.isEmpty()) null else onConflict(conflicts)
        for ((info, name, target) in pending) {
            val remoteSha = info.shaFile.toHex()
            val mirror = roots.gameSave(name)
            val visible = mirror?.takeIf { it.isFile } ?: target
            val localSha = if (visible.isFile) sha1(visible.readBytes()) else null
            when {
                localSha == remoteSha -> {
                    manifest[name] = remoteSha
                    current += 1
                }
                localSha != null && choice != CloudChoice.Remote &&
                    visible.lastModified() >= info.timestamp.time -> kept += 1
                choice == CloudChoice.Local && localSha != null -> kept += 1
                else -> try {
                    val bytes = download(cloud, appId, info, name)
                    writeCloudFile(store, name, target, bytes, info.timestamp.time)
                    if (choice == CloudChoice.Remote && mirror != null) {
                        writeCloudFile(store, name, mirror, bytes, info.timestamp.time)
                    }
                    manifest[name] = remoteSha
                    downloaded += 1
                } catch (error: kotlinx.coroutines.CancellationException) {
                    throw error
                } catch (error: Throwable) {
                    failed += 1
                    VexaLogger.log(
                        LogLevel.WARN, LogCategory.BOOT, "Steam Cloud download failed",
                        mapOf("file" to name, "error" to (error.message ?: error.javaClass.simpleName)),
                    )
                    // A failed transport will usually fail the remaining files too.
                    // Keep their local copies and let the game start promptly.
                    break
                }
            }
        }
        val result = CloudSyncResult(downloaded, current, kept, unmapped, failed)
        VexaLogger.log(
            LogLevel.INFO, LogCategory.BOOT, "Steam Cloud synced",
            mapOf("appId" to appId.toString(), "result" to result.summary()),
        )
        return result
    }

    private suspend fun download(cloud: SteamCloud, appId: Int, info: AppFileInfo, name: String): ByteArray {
        val meta = retry("download-url $name", attempts = 1) {
            withTimeout(LIST_TIMEOUT_MS) { cloud.clientFileDownload(appId, name).await() }
        }
        check(meta.urlHost.isNotBlank()) { "no download URL" }
        check(!meta.encrypted) { "encrypted payload" }
        val scheme = if (meta.useHttps) "https" else "http"
        val connection = URL("$scheme://${meta.urlHost}${meta.urlPath}").openConnection() as HttpURLConnection
        connection.connectTimeout = 5_000
        connection.readTimeout = 10_000
        meta.requestHeaders.forEach { connection.setRequestProperty(it.name, it.value) }
        val raw = try {
            check(connection.responseCode == 200) { "HTTP ${connection.responseCode}" }
            connection.inputStream.use { it.readBytes() }
        } finally {
            connection.disconnect()
        }
        val bytes = if (raw.size >= 4 && raw[0] == 'P'.code.toByte() && raw[1] == 'K'.code.toByte() &&
            raw[2].toInt() == 3 && raw[3].toInt() == 4 && raw.size != meta.rawFileSize
        ) {
            ZipInputStream(ByteArrayInputStream(raw)).use { zip ->
                checkNotNull(zip.nextEntry) { "empty archive" }
                zip.readBytes()
            }
        } else {
            raw
        }
        val expected = meta.shaFile.toHex()
        check(expected.isEmpty() || sha1(bytes) == expected) { "checksum mismatch" }
        return bytes
    }

    private suspend fun <T> retry(stage: String, attempts: Int = 3, block: suspend () -> T): T {
        var last: Throwable? = null
        repeat(attempts) { attempt ->
            try {
                return block()
            } catch (error: kotlinx.coroutines.CancellationException) {
                throw error
            } catch (error: Throwable) {
                last = error
                VexaLogger.log(
                    LogLevel.WARN, LogCategory.BOOT, "Steam Cloud $stage attempt ${attempt + 1} failed",
                    mapOf("error" to "${error.javaClass.simpleName}: ${error.message} ${error.cause?.message.orEmpty()}"),
                )
                kotlinx.coroutines.delay(1_500L * (attempt + 1))
            }
        }
        throw last ?: IllegalStateException(stage)
    }

    // ETS2 and ATS publish Documents-relative "profiles/..." with no root token.
    // The game reads those from its XDG directory, not from the RemoteStorage stub.
    internal fun placeTruckProfiles(appId: Int, name: String): String {
        if (name.startsWith("%")) return name
        val folder = when (appId) {
            227300 -> "Euro Truck Simulator 2"
            270880 -> "American Truck Simulator"
            else -> return name
        }
        if (!name.startsWith("profiles/") && !name.startsWith("steam_profiles/")) return name
        return "%LinuxXdgDataHome%/$folder/$name"
    }

    private fun effectiveName(info: AppFileInfo, prefixes: List<String>): String {
        val prefix = prefixes.getOrNull(info.pathPrefixIndex)?.takeIf { it.isNotBlank() }
        return (prefix ?: "") + info.filename
    }

    private fun writeCloudFile(store: File, name: String, target: File, bytes: ByteArray, modified: Long) {
        if (target.isFile) backup(store, name, target)
        target.parentFile?.mkdirs()
        val temp = File(target.parentFile, target.name + ".vexa-tmp")
        temp.writeBytes(bytes)
        check(temp.renameTo(target)) { "rename failed" }
        target.setReadable(true, false)
        target.setLastModified(modified)
    }

    private fun backup(store: File, name: String, target: File) {
        val safe = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
        val dest = File(store, "backup/$safe")
        dest.parentFile?.mkdirs()
        runCatching { target.copyTo(dest, overwrite = true) }
    }

    private class Roots(val home: File, val gameDir: File, val remote: File) {
        private val token = Regex("^%(\\w+)%/?(.*)$")

        /** Install copy used by games that read save/ instead of FileRead. Only plain RemoteStorage names. */
        fun gameSave(name: String): File? {
            if (name.startsWith("%") || name.contains('\\')) return null
            val base = File(gameDir, "save").canonicalFile
            val file = File(base, name).canonicalFile
            return file.takeIf { it.path == base.path || it.path.startsWith(base.path + File.separator) }
        }

        fun resolve(name: String): File? {
            val match = token.matchEntire(name)
            val (base, relative) = if (match == null) {
                remote to name
            } else {
                val base = when (match.groupValues[1]) {
                    "LinuxHome" -> home
                    "LinuxXdgDataHome" -> File(home, ".local/share")
                    "LinuxXdgConfigHome" -> File(home, ".config")
                    "WinAppDataLocalLow" -> File(home, ".config/unity3d")
                    "GameInstall" -> gameDir
                    "SteamCloudDocuments" -> remote
                    else -> return null
                }
                base to match.groupValues[2]
            }
            val file = File(base, relative).canonicalFile
            return file.takeIf { it.path.startsWith(base.canonicalPath + File.separator) }
        }
    }

    private fun readManifest(file: File): MutableMap<String, String> {
        val map = mutableMapOf<String, String>()
        runCatching {
            val files = JSONObject(file.readText()).getJSONObject("files")
            files.keys().forEach { map[it] = files.getString(it) }
        }
        return map
    }

    private fun writeManifest(file: File, manifest: Map<String, String>) {
        runCatching {
            file.writeText(JSONObject().put("files", JSONObject(manifest)).toString())
        }
    }

    private fun sha1(bytes: ByteArray): String = MessageDigest.getInstance("SHA-1").digest(bytes).toHex()

    private fun ByteArray?.toHex(): String = this?.joinToString("") { "%02x".format(it) }.orEmpty()
}
