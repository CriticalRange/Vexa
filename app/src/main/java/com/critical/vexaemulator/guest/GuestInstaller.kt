package com.critical.vexaemulator.guest

import android.content.Context
import com.critical.vexaemulator.runtime.LaunchRequest
import java.io.File

object GuestInstaller {
    private const val RUNTIME_STAMP = "vexa-graphics-1"

    fun install(context: Context, program: GuestProgram): LaunchRequest {
        val root = context.filesDir
        ensureRuntime(context, root)
        val guestDir = File(root, "guests/${program.id}").apply { mkdirs() }
        val executable = File(guestDir, program.fileName)
        context.assets.open(program.assetPath).use { input ->
            executable.outputStream().use { output -> input.copyTo(output) }
        }
        executable.setReadable(true, false)
        executable.setExecutable(true, false)

        val rootfs = File(root, "rootfs").apply { mkdirs() }
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
            launchEnv = program.env,
        )
    }

    private fun ensureRuntime(context: Context, root: File) {
        val stamp = File(root, "runtime.stamp")
        if (stamp.isFile && stamp.readText() == RUNTIME_STAMP) {
            return
        }
        if (context.assets.list("runtime").isNullOrEmpty()) {
            return
        }
        copyAssetTree(context, "runtime/rootfs", File(root, "rootfs"))
        copyAssetTree(context, "runtime/thunks/host", File(root, "thunks/host"))
        copyAssetTree(context, "runtime/thunks/guest", File(root, "thunks/guest"))
        stamp.writeText(RUNTIME_STAMP)
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
