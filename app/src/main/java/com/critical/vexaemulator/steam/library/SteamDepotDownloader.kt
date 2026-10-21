package com.critical.vexaemulator.steam.library
import android.content.Context
import android.system.Os
import `in`.dragonbra.javasteam.enums.EDepotFileFlag
import `in`.dragonbra.javasteam.enums.EResult
import `in`.dragonbra.javasteam.steam.cdn.Client
import `in`.dragonbra.javasteam.steam.cdn.Server
import `in`.dragonbra.javasteam.steam.handlers.steamapps.SteamApps
import `in`.dragonbra.javasteam.steam.handlers.steamcontent.SteamContent
import `in`.dragonbra.javasteam.steam.steamclient.SteamClient
import `in`.dragonbra.javasteam.types.KeyValue
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.future.await
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import java.io.File
import java.io.RandomAccessFile
import java.util.concurrent.atomic.AtomicInteger
data class LinuxDepot(
    val depotId: Int,
    val ownerAppId: Int,
    val manifestGid: Long,
    val linuxOnly: Boolean,
)
data class DepotProgress(
    val fraction: Float,
    val label: String,
    val receivedBytes: Long = 0,
    val totalBytes: Long = 0,
    val bytesPerSecond: Long = 0,
)
// Mirrors GameNative's JavaSteam path: depot key, manifest request code, CDN manifest, chunks.
// GameNative then hands bytes to libgndownload. VEXA writes the chunks itself and does not load that library.
object SteamDepotDownloader {
    fun linuxDepots(app: KeyValue, appId: Int): List<LinuxDepot> {
        val depots = app["depots"]
        if (depots === KeyValue.INVALID || depots.children.isEmpty()) return emptyList()
        val supportsLinux = app["common"]["oslist"].asString().orEmpty().split(',').any { it.trim() == "linux" } ||
            launchExecutable(app) != null || depots.children.any { it["config"]["oslist"].asString().orEmpty().split(',').any { os -> os.trim() == "linux" } }
        if (!supportsLinux) return emptyList()
        return depots.children.mapNotNull { child ->
            val depotId = child.name?.toIntOrNull() ?: return@mapNotNull null
            val os = child["config"]["oslist"].asString().orEmpty()
            val systems = os.split(',').map { it.trim().lowercase() }.filter { it.isNotEmpty() }
            // OS-neutral depots contain shared game data and are needed by Linux too.
            if (systems.isNotEmpty() && "linux" !in systems) return@mapNotNull null
            if (child["dlcappid"].asInteger(0) > 0) return@mapNotNull null
            val arch = child["config"]["osarch"].asString().orEmpty().trim()
            if (arch.isNotEmpty() && arch != "64" && arch != "32") return@mapNotNull null
            val language = child["config"]["language"].asString().orEmpty().trim()
            if (language.isNotEmpty() && !language.equals("english", ignoreCase = true)) return@mapNotNull null
            val gidText = child["manifests"]["public"]["gid"].asString().orEmpty().trim()
            val gid = gidText.toULongOrNull()?.toLong() ?: return@mapNotNull null
            val owner = child["depotfromapp"].asInteger(appId).takeIf { it > 0 } ?: appId
            LinuxDepot(depotId, owner, gid, linuxOnly = "linux" in systems)
        }
    }
    fun launchExecutable(app: KeyValue): String? {
        val launches = app["config"]["launch"].children
        val linux = launches.firstOrNull { launch ->
            launch["config"]["oslist"].asString().orEmpty().split(',')
                .any { it.trim().equals("linux", ignoreCase = true) }
        }
        val chosen = linux ?: return null
        val executable = chosen["executable"].asString().orEmpty().trim()
        return executable.takeIf { it.isNotEmpty() }
    }
    suspend fun download(
        context: Context,
        client: SteamClient,
        appId: Int,
        title: String,
        app: KeyValue,
        allowedDepots: Set<Int>,
        onProgress: (DepotProgress) -> Unit,
    ): InstalledSteamProgram = coroutineScope {
        // Package depot lists can omit the Linux client depot (Half-Life depot 8) even when the app is owned.
        val depots = linuxDepots(app, appId).filter { it.linuxOnly || it.depotId in allowedDepots }
        if (depots.isEmpty()) {
            // TODO: Proton later. No Linux x86_64 depot, so this title is not installable yet.
            error("No Linux depot is included in this account's licenses")
        }
        val guestId = "steam-$appId"
        val relativePath = "guests/$guestId"
        val root = File(context.filesDir, relativePath)
        root.mkdirs()
        val apps = client.getHandler(SteamApps::class.java) ?: error("Steam apps handler missing")
        val content = client.getHandler(SteamContent::class.java) ?: error("Steam content handler missing")
        onProgress(DepotProgress(-1f, "Finding download servers..."))
        val servers = withTimeout(20_000) { content.getServersForSteamPipe(null, 8, this@coroutineScope).await() }
            .filter { !it.steamChinaOnly && !it.host.isNullOrBlank() }
            .sortedWith(compareBy<Server> { if (it.protocol == Server.ConnectionProtocol.HTTPS) 0 else 1 }.thenBy { it.weightedLoad })
        if (servers.isEmpty()) error("Steam returned no content servers")
        val cdn = Client(client)
        val tokens = TokenCache()
        try {
            val completedBytes = AtomicLong(0)
            val lastUpdate = AtomicLong(0)
            // Steam's own client keeps two dozen chunk fetches in flight; 6
            // connections cannot saturate a fast link.
            val chunkGate = Semaphore(16)
            val fileGate = Semaphore(8)
            var totalBytes = 0L
            val manifests = depots.map { depot ->
                onProgress(DepotProgress(-1f, "Authorizing game files..."))
                val keyCallback = withTimeout(15_000) { apps.getDepotDecryptionKey(depot.depotId, depot.ownerAppId).toFuture().await() }
                if (keyCallback.result != EResult.OK || keyCallback.depotKey.isEmpty()) {
                    error("Depot ${depot.depotId} key: ${keyCallback.result}")
                }
                val requestCode = withTimeout(15_000) { content.getManifestRequestCode(
                    depot.depotId,
                    depot.ownerAppId,
                    depot.manifestGid,
                    "public",
                    null,
                    this,
                ).await() }
                onProgress(DepotProgress(-1f, "Reading download manifest..."))
                val manifest = downloadManifest(cdn, depot, requestCode, keyCallback.depotKey, servers, content, depot.ownerAppId, this, tokens)
                    ?: error("Depot ${depot.depotId} manifest was not on the content servers")
                if (manifest.filenamesEncrypted && !manifest.decryptFilenames(keyCallback.depotKey)) {
                    error("Depot ${depot.depotId} filenames did not decrypt")
                }
                totalBytes += manifest.files.sumOf { file -> file.chunks.sumOf { it.uncompressedLength.toLong() } }
                Triple(depot, manifest, keyCallback.depotKey)
            }
            val transferStarted = System.currentTimeMillis()
            manifests.forEach { (depot, manifest, key) ->
                // No batch barrier: files start as workers free up, so a slow
                // file no longer stalls the next group of small files.
                coroutineScope { manifest.files.map { file -> async {
                    fileGate.withPermit {
                    val relative = file.fileName.orEmpty().replace('\\', '/')
                    if (relative.isBlank()) return@async
                    val dest = childFile(root, relative)
                    if (file.flags.contains(EDepotFileFlag.Directory)) {
                        dest.mkdirs()
                        return@async
                    }
                    if (file.flags.contains(EDepotFileFlag.Symlink)) {
                        dest.parentFile?.mkdirs()
                        val target = file.linkTarget.orEmpty()
                        if (target.isNotBlank()) {
                            val linkDestination = File(dest.parentFile, target).canonicalPath
                            require(!File(target).isAbsolute && linkDestination.startsWith(root.canonicalPath + File.separator)) { "Depot symlink is outside the guest folder" }
                            runCatching { if (dest.exists()) dest.delete() }
                            runCatching { Os.symlink(target, dest.absolutePath) }
                        }
                        return@async
                    }
                    if (dest.isFile && dest.length() == file.totalSize) {
                        noteProgress(completedBytes, lastUpdate, transferStarted, totalBytes, file.totalSize, onProgress)
                        dest.setReadable(true, false)
                        if (file.flags.contains(EDepotFileFlag.Executable)) dest.setExecutable(true, false)
                        return@async
                    }
                    dest.parentFile?.mkdirs()
                    RandomAccessFile(dest, "rw").use { output ->
                        val next = AtomicInteger(0)
                        coroutineScope {
                            List(minOf(6, file.chunks.size)) {
                                async {
                                  while (true) {
                                    val index = next.getAndIncrement()
                                    if (index >= file.chunks.size) break
                                    val chunk = file.chunks[index]
                                    require(chunk.compressedLength in 1..(32 * 1024 * 1024)) { "Unexpected compressed chunk size" }
                                    val size = chunk.uncompressedLength
                                    if (size <= 0 || size > 32 * 1024 * 1024) error("Unexpected chunk size $size")
                                    // The permit covers the HTTP fetch only; decrypt/decompress is
                                    // CPU work and must not hold a network slot.
                                    val encrypted = ByteArray(chunk.compressedLength)
                                    val fetched = chunkGate.withPermit {
                                        downloadChunk(
                                            cdn, depot.depotId, chunk, encrypted, servers, content, depot.ownerAppId, this, tokens, index,
                                        )
                                    }
                                    if (!fetched) error("Incomplete chunk in $relative")
                                    val buffer = ByteArray(size)
                                    val written = `in`.dragonbra.javasteam.steam.cdn.DepotChunk.process(chunk, encrypted, buffer, key)
                                    if (written != size) error("Incomplete chunk in $relative")
                                    synchronized(output) {
                                        output.seek(chunk.offset)
                                        output.write(buffer, 0, written)
                                    }
                                    noteProgress(completedBytes, lastUpdate, transferStarted, totalBytes, written.toLong(), onProgress)
                                  }
                                }
                            }.awaitAll()
                        }
                        if (output.length() != file.totalSize) output.setLength(file.totalSize)
                    }
                    dest.setReadable(true, false)
                    if (file.flags.contains(EDepotFileFlag.Executable)) dest.setExecutable(true, false)
                    }
                } }.awaitAll() }
            }
        } finally {
            cdn.close()
        }
        val preferred = launchExecutable(app)
        val fileName = resolveExecutable(root, preferred)
            ?: error("Linux depot downloaded, but no x86 or x86_64 executable was found")
        File(root, fileName).setExecutable(true, false)
        val installed = InstalledSteamProgram(
            id = guestId,
            title = title,
            appId = appId,
            relativePath = relativePath,
            fileName = fileName,
        )
        InstalledSteamPrograms.upsert(context, installed)
        onProgress(DepotProgress(1f, "Installed"))
        installed
    }
    private suspend fun downloadManifest(
        cdn: Client,
        depot: LinuxDepot,
        requestCode: Long,
        depotKey: ByteArray,
        servers: List<Server>,
        content: SteamContent,
        appId: Int,
        scope: CoroutineScope,
        tokens: TokenCache,
    ) = servers.firstNotNullOfOrNull { server ->
        try { withTimeoutOrNull(20_000) {
            val token = tokens.get(content, depot.depotId, appId, server, scope)
            cdn.downloadManifest(
                depot.depotId,
                depot.manifestGid,
                requestCode.toULong(),
                server,
                depotKey,
                null,
                token,
            )
        } } catch (error: CancellationException) { throw error } catch (_: Exception) { null }
    }
    private suspend fun downloadChunk(
        cdn: Client,
        depotId: Int,
        chunk: `in`.dragonbra.javasteam.types.ChunkData,
        destination: ByteArray,
        servers: List<Server>,
        content: SteamContent,
        appId: Int,
        scope: CoroutineScope,
        tokens: TokenCache,
        chunkIndex: Int,
    ): Boolean {
        // Rotate the starting server per chunk: always hitting servers[0]
        // concentrates all workers on one CDN host and triggers shaping.
        for (attempt in servers.indices) {
            val server = servers[(chunkIndex + attempt) % servers.size]
            val fetched = try { withTimeoutOrNull(20_000) {
                val token = tokens.get(content, depotId, appId, server, scope)
                val received = cdn.downloadDepotChunk(depotId, chunk, server, destination, null, null, token)
                received == destination.size
            } == true } catch (error: CancellationException) { throw error } catch (_: Exception) { false }
            if (fetched) return true
        }
        return false
    }
    private class TokenCache {
        private data class Entry(val value: String?, val expires: Long)
        private val entries = ConcurrentHashMap<String, Entry>()
        private val locks = ConcurrentHashMap<String, Mutex>()
        suspend fun get(content: SteamContent, depotId: Int, appId: Int, server: Server, scope: CoroutineScope): String? {
            val host = server.host?.takeIf { it.isNotBlank() } ?: server.vHost ?: return null
            val key = "$appId/$depotId/$host"
            return locks.getOrPut(key) { Mutex() }.withLock {
                val now = System.currentTimeMillis()
                entries[key]?.takeIf { it.expires > now }?.let { return@withLock it.value }
                val response = withTimeoutOrNull(8000) { content.getCDNAuthToken(appId, depotId, host, this).await() }
                val value = response?.token?.takeIf { response.result == EResult.OK && it.isNotBlank() }
                // Public servers need no token. Cache that answer too instead of issuing an RPC per chunk.
                val expires = if (value != null) response.expiration.time - 30_000 else now + 30_000
                entries[key] = Entry(value, expires)
                value
            }
        }
    }
    private fun noteProgress(
        completedBytes: AtomicLong,
        lastUpdate: AtomicLong,
        transferStarted: Long,
        totalBytes: Long,
        added: Long,
        onProgress: (DepotProgress) -> Unit,
    ) {
        val done = completedBytes.addAndGet(added)
        val now = System.currentTimeMillis()
        val last = lastUpdate.get()
        if ((now - last >= 200 || done == totalBytes) && lastUpdate.compareAndSet(last, now)) {
            val seconds = ((now - transferStarted).coerceAtLeast(1) / 1000.0)
            val perSecond = (done / seconds).toLong()
            onProgress(DepotProgress(
                fraction = if (totalBytes == 0L) 0f else (done.toDouble() / totalBytes).toFloat().coerceIn(0f, 1f),
                label = "",
                receivedBytes = done,
                totalBytes = totalBytes,
                bytesPerSecond = perSecond,
            ))
        }
    }
    private fun childFile(root: File, relative: String): File {
        val parts = relative.split('/').filter { it.isNotEmpty() && it != "." }
        if (parts.isEmpty() || parts.any { it == ".." }) error("Depot path is outside the guest folder")
        val dest = parts.fold(root) { parent, part -> File(parent, part) }
        val rootPath = root.canonicalPath
        val destPath = dest.canonicalPath
        if (destPath != rootPath && !destPath.startsWith(rootPath + File.separator)) {
            error("Depot path is outside the guest folder")
        }
        return dest
    }
    private fun resolveExecutable(root: File, preferred: String?): String? {
        if (!preferred.isNullOrBlank()) {
            val relative = preferred.replace('\\', '/')
            val file = runCatching { childFile(root, relative) }.getOrNull()
            if (file != null && file.isFile && elfMachine(file) != null) return relative
        }
        return root.walkTopDown()
            .maxDepth(6)
            .filter { it.isFile && !it.name.endsWith(".so") }
            .mapNotNull { file -> elfMachine(file)?.let { file to it } }
            .minByOrNull { if (it.second == 62) 0 else 1 }
            ?.first
            ?.relativeTo(root)
            ?.path
            ?.replace('\\', '/')
    }
    /** x86_64 is 62, i386 is 3. Half-Life's Linux build is the 32-bit one. */
    private fun elfMachine(file: File): Int? {
        val header = ByteArray(20)
        val ok = runCatching {
            file.inputStream().use { input -> input.read(header) == header.size }
        }.getOrDefault(false)
        if (!ok || header[0] != 0x7f.toByte() || header[1] != 'E'.code.toByte() ||
            header[2] != 'L'.code.toByte() || header[3] != 'F'.code.toByte()
        ) return null
        val machine = (header[18].toInt() and 0xff) or ((header[19].toInt() and 0xff) shl 8)
        val clazz = header[4].toInt()
        return when {
            clazz == 2 && machine == 62 -> 62
            clazz == 1 && machine == 3 -> 3
            else -> null
        }
    }
}
