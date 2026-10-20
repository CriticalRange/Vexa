package com.critical.vexaemulator.runtime

import android.content.Context
import android.os.Build
import android.os.Process
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.time.Instant

/**
 * Writes `run_manifest.json` into the run's artifact directory: the packaged build
 * manifest, device identity, the installed payloads checked against their build
 * hashes, and periodic snapshots of the files actually mapped in this process.
 * Only build-tracked payloads and APK native libraries are hashed, so snapshots do
 * not read large game files while a measurement is running.
 */
class RunManifest private constructor(
    private val context: Context,
    private val request: LaunchRequest,
    private val output: File,
) {
    private val root = JSONObject()
    private val hashCache = HashMap<String, Triple<Long, Long, String>>()
    // Installed absolute path -> build-manifest payload hash.
    private val expected = HashMap<String, String>()
    @Volatile private var finished = false
    private val lock = Any()

    private fun sha256(file: File): String? {
        if (!file.isFile) return null
        val key = file.absolutePath
        hashCache[key]?.let { (size, mtime, hash) ->
            if (size == file.length() && mtime == file.lastModified()) return hash
        }
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().buffered(1 shl 20).use { input ->
            val buffer = ByteArray(1 shl 20)
            while (true) {
                val n = input.read(buffer)
                if (n < 0) break
                digest.update(buffer, 0, n)
            }
        }
        val hash = digest.digest().joinToString("") { "%02x".format(it) }
        hashCache[key] = Triple(file.length(), file.lastModified(), hash)
        return hash
    }

    private fun installedPath(relative: String): File? = when {
        relative.startsWith("thunks/") ->
            File(request.thunkHostPath).parentFile?.let { File(it, relative.removePrefix("thunks/")) }
        relative.startsWith("rootfs/") -> File(request.rootfsPath, relative.removePrefix("rootfs/"))
        else -> null
    }

    private fun begin(): Int {
        val build = runCatching {
            JSONObject(context.assets.open("build-manifest.json").bufferedReader().use { it.readText() })
        }.getOrNull()
        root.put("schema", "vexa-run-manifest-1")
        root.put("startedAt", Instant.now().toString())
        root.put("pid", Process.myPid())
        root.put("cpuset", runCatching { File("/proc/self/cpuset").readText().trim() }.getOrNull())
        root.put("device", JSONObject().apply {
            put("manufacturer", Build.MANUFACTURER)
            put("model", Build.MODEL)
            put("hardware", Build.HARDWARE)
            put("fingerprint", Build.FINGERPRINT)
            put("sdk", Build.VERSION.SDK_INT)
            if (Build.VERSION.SDK_INT >= 31) {
                put("socManufacturer", Build.SOC_MANUFACTURER)
                put("socModel", Build.SOC_MODEL)
            }
            put("kernel", runCatching { File("/proc/version").readText().trim() }.getOrNull())
        })
        root.put("launch", JSONObject().apply {
            put("executable", request.executablePath)
            put("args", JSONArray(request.launchArgs))
            put("env", JSONObject().apply {
                request.launchEnv.filter { it.startsWith("VEXA_") || it.startsWith("FEX_") }.sorted().forEach {
                    put(it.substringBefore('='), it.substringAfter('='))
                }
            })
        })
        root.put("build", build ?: JSONObject.NULL)

        var mismatches = 0
        val payloads = JSONObject()
        build?.optJSONObject("runtime")?.optJSONObject("payloads")?.let { tracked ->
            for (relative in tracked.keys()) {
                val file = installedPath(relative) ?: continue
                val want = tracked.getString(relative)
                val have = runCatching { sha256(file) }.getOrNull()
                expected[file.absolutePath] = want
                val state = when (have) {
                    null -> "missing"
                    want -> "match"
                    else -> "mismatch"
                }
                if (state != "match") mismatches++
                payloads.put(relative, JSONObject().put("path", file.absolutePath).put("state", state).put("sha256", have))
            }
        }
        root.put("installedPayloads", payloads)
        root.put("installedMismatches", mismatches)
        root.put("nativeLibs", JSONObject().apply {
            File(context.applicationInfo.nativeLibraryDir).listFiles()?.sortedBy { it.name }?.forEach {
                put(it.name, runCatching { sha256(it) }.getOrNull())
            }
        })
        write()
        return mismatches
    }

    /** Records every file mapped into this process; tracked payloads are matched by hash. */
    fun snapshot(label: String) = synchronized(lock) {
        val mapped = linkedSetOf<String>()
        runCatching {
            File("/proc/self/maps").forEachLine { line ->
                val path = line.split(Regex("\\s+"), limit = 6).getOrNull(5)?.trim() ?: return@forEachLine
                if (path.startsWith("/") && !path.startsWith("/dev/") && !path.endsWith("(deleted)")) mapped += path
            }
        }
        val nativeDir = context.applicationInfo.nativeLibraryDir
        val files = JSONArray()
        var mismatches = 0
        for (path in mapped) {
            val file = File(path)
            val entry = JSONObject().put("path", path).put("size", file.length())
            val want = expected[path]
            if (want != null || path.startsWith(nativeDir)) {
                val have = runCatching { sha256(file) }.getOrNull()
                entry.put("sha256", have)
                if (want != null) {
                    entry.put("state", if (have == want) "match" else "mismatch")
                    if (have != want) mismatches++
                }
            }
            files.put(entry)
        }
        val snapshots = root.optJSONArray("loaded") ?: JSONArray().also { root.put("loaded", it) }
        snapshots.put(JSONObject()
            .put("label", label)
            .put("at", Instant.now().toString())
            .put("trackedMismatches", mismatches)
            .put("files", files))
        write()
        mismatches
    }

    private fun write() {
        val temp = File(output.path + ".tmp")
        temp.writeText(root.toString(1))
        temp.renameTo(output)
    }

    fun finish(exitCode: Int) {
        finished = true
        snapshot("exit")
        synchronized(lock) {
            root.put("exitCode", exitCode)
            root.put("finishedAt", Instant.now().toString())
            write()
        }
    }

    companion object {
        private val SNAPSHOT_DELAYS_MS = longArrayOf(15_000, 45_000, 120_000)

        fun start(context: Context, request: LaunchRequest, onLog: (String, String) -> Unit): RunManifest? {
            val dir = File(request.artifactDirectory)
            if (!dir.isDirectory) return null
            val manifest = RunManifest(context, request, File(dir, "run_manifest.json"))
            val mismatches = runCatching { manifest.begin() }.getOrElse {
                onLog("WARN", "Run manifest failed: ${it.message}")
                return null
            }
            if (manifest.root.isNull("build")) {
                onLog("WARN", "Run manifest: APK has no build-manifest.json; payloads unverified")
            } else {
                onLog(if (mismatches == 0) "INFO" else "WARN", "Run manifest: $mismatches installed payload(s) differ from the build")
            }
            Thread({
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
                var elapsed = 0L
                for (delay in SNAPSHOT_DELAYS_MS) {
                    runCatching { Thread.sleep(delay - elapsed) }.onFailure { return@Thread }
                    elapsed = delay
                    if (manifest.finished) return@Thread
                    val loaded = runCatching { manifest.snapshot("t+${delay / 1000}s") }.getOrDefault(-1)
                    if (loaded != 0) onLog("WARN", "Run manifest: $loaded mapped payload(s) differ from the build")
                }
            }, "vexa-run-manifest").apply { isDaemon = true }.start()
            return manifest
        }
    }
}
