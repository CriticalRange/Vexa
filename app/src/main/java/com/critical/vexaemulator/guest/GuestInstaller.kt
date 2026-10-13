package com.critical.vexaemulator.guest

import android.content.Context
import android.system.Os
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.LaunchRequest
import com.critical.vexaemulator.steam.SteamworksHost
import java.io.File

object GuestInstaller {
    private const val RUNTIME_STAMP = "vexa-graphics-1"

    fun install(context: Context, program: GuestProgram): LaunchRequest {
        val root = context.filesDir
        ensureRuntime(context, root)
        val guestDir = File(root, "guests/${program.id}").apply { mkdirs() }
        val executable = File(guestDir, program.fileName)
        if (program.preinstalled) {
            if (!executable.isFile) {
                throw IllegalStateException("Missing ${executable.absolutePath}")
            }
            guestDir.listFiles()?.forEach { file ->
                file.setReadable(true, false)
            }
        } else {
            context.assets.open(program.assetPath).use { input ->
                executable.outputStream().use { output -> input.copyTo(output) }
            }
        }
        val steamStubCount = installSteamStub(context, root, guestDir)
        if (steamStubCount > 0) {
            VexaLogger.log(
                LogLevel.INFO,
                LogCategory.BOOT,
                "Steam API stub installed",
                mapOf("program" to program.id, "libraries" to steamStubCount.toString())
            )
        }
        executable.setReadable(true, false)
        executable.setExecutable(true, false)
        val baseEnv = if (program.preinstalled && program.env.none { it.startsWith("HOME=") }) {
            program.env + "HOME=${guestDir.absolutePath}"
        } else {
            program.env
        }
        val rootfs = File(root, "rootfs").apply { mkdirs() }
        val launchEnv = withRuntimeDir(rootfs, baseEnv) + SteamworksHost.launchEnvironment(context, baseEnv)
        val thunkHost = File(root, "thunks/host").apply { mkdirs() }
        val thunkGuest = File(root, "thunks/guest").apply { mkdirs() }
        val artifacts = File(root, "artifacts/${program.id}").apply {
            mkdirs()
            File(this, "guest_stdout.log").delete()
            File(this, "fex_stderr.log").delete()
        }

        return LaunchRequest(
            executablePath = executable.absolutePath,
            rootfsPath = rootfs.absolutePath,
            thunkHostPath = thunkHost.absolutePath,
            thunkGuestPath = thunkGuest.absolutePath,
            workingDirectory = guestDir.absolutePath,
            artifactDirectory = artifacts.absolutePath,
            launchArgs = program.args,
            launchEnv = launchEnv,
        )
    }

    // Every discoverable x86_64 libsteam_api is replaced with a stub that reports a
    // running, logged-in Steam. The original library is kept next to it as .real.
    private fun installSteamStub(context: Context, root: File, guestDir: File): Int {
        val targets = buildList<File> {
            guestDir.walkTopDown()
                .filter { it.isFile && it.name == "libsteam_api.so" }
                .forEach(::add)
            add(File(root, "rootfs/usr/lib/x86_64-linux-gnu/libsteam_api.so"))
        }.distinctBy { it.absolutePath }

        var installed = 0
        targets.forEach { target ->
            if (!target.isFile) return@forEach
            val header = ByteArray(20)
            val isAmd64 = target.inputStream().use { input ->
                input.read(header) == header.size &&
                    header[0] == 0x7f.toByte() && header[1] == 'E'.code.toByte() &&
                    header[2] == 'L'.code.toByte() && header[3] == 'F'.code.toByte() &&
                    header[4] == 2.toByte() && header[5] == 1.toByte() &&
                    header[18] == 62.toByte() && header[19] == 0.toByte()
            }
            // The bundled Steam stub is amd64. Never overwrite an i386 DLL.
            if (!isAmd64) return@forEach
            val backup = File(target.parentFile, "libsteam_api.so.real")
            if (!backup.exists()) target.copyTo(backup)
            context.assets.open("guests/libsteam_api_stub.so").use { input ->
                target.outputStream().use { output -> input.copyTo(output) }
            }
            target.setReadable(true, false)
            installed += 1
        }
        return installed
    }

    private fun withRuntimeDir(rootfs: File, env: List<String>): List<String> {
        val hasDisplay = env.any { it.startsWith("DISPLAY=") && it.substringAfter("=").isNotEmpty() }
        if (!hasDisplay || env.any { it.startsWith("XDG_RUNTIME_DIR=") }) return env
        // Wayland's client library exits when this is unset or not mode 0700.
        // The directory stays empty: there is no Wayland socket here.
        val runtime = File(rootfs, "tmp/vexa-xdg")
        runtime.mkdirs()
        runCatching { Os.chmod(runtime.absolutePath, 448) }
        return env + "XDG_RUNTIME_DIR=/tmp/vexa-xdg"
    }

    @Synchronized
    fun ensureRuntime(context: Context, root: File = context.filesDir) {
        val stamp = File(root, "runtime.stamp")
        // runtime/STAMP carries a content hash, so updated thunks/rootfs replace the old copy.
        val expected = runCatching {
            context.assets.open("runtime/STAMP").bufferedReader().use { it.readText().trim() }
        }.getOrDefault(RUNTIME_STAMP)
        if (stamp.isFile && stamp.readText() == expected) {
            return
        }
        if (context.assets.list("runtime").isNullOrEmpty()) {
            return
        }
        copyAssetTree(context, "runtime/rootfs", File(root, "rootfs"))
        copyAssetTree(context, "runtime/thunks", File(root, "thunks"))
        stamp.writeText(expected)
    }

    private fun copyAssetTree(context: Context, assetPath: String, dest: File) {
        val children = context.assets.list(assetPath) ?: return
        if (children.isEmpty()) {
            try {
                context.assets.open(assetPath).use { input ->
                    dest.parentFile?.mkdirs()
                    dest.outputStream().use { output -> input.copyTo(output) }
                }
                dest.setReadable(true, false)
            } catch (_: Exception) {
                dest.mkdirs()
            }
            return
        }
        dest.mkdirs()
        for (child in children) {
            copyAssetTree(context, "$assetPath/$child", File(dest, child))
        }
    }
}
