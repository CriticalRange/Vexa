package com.critical.vexaemulator.runtime

import java.io.File

/**
 * Re-entry point for guest child processes.
 *
 * FEX emulates guest execve() by exec'ing /proc/self/exe — which on Android is
 * app_process64, not a FEXLoader binary. The execve handler rewrites argv to
 * `app_process <dir> com.critical.vexaemulator.runtime.FexChildMain <file>
 * <args...>` and transports the guest environment losslessly as VEXA_ENV<i>
 * entries (host exec must not see raw LD_PRELOAD — Bionic treats an unloadable
 * preload as fatal). This main() rebuilds the LaunchRequest and runs the new
 * guest ELF under FEX again, so the child lands back inside emulation.
 */
object FexChildMain {
    private const val ENV_COUNT = "VEXA_ENV_COUNT"
    private const val ENV_PREFIX = "VEXA_ENV"
    private const val ENV_FILES = "VEXA_FILES"
    private const val ENV_APK = "VEXA_APK"
    private const val ENV_PROGRAM = "VEXA_PROGRAM"
    private const val ENV_CWD = "VEXA_CHILD_CWD"

    private var logFile: File? = null

    private fun log(msg: String) {
        runCatching { logFile?.appendText("$msg\n") }
        runCatching { System.err.println("[VEXA][FEXEC] $msg") }
    }

    @JvmStatic
    fun main(args: Array<String>) {
        val env = System.getenv()

        val guestEnv = mutableListOf<String>()
        val guestMap = mutableMapOf<String, String>()
        val count = env[ENV_COUNT]?.toIntOrNull() ?: 0
        for (i in 0 until count) {
            env["$ENV_PREFIX$i"]?.let {
                guestEnv.add(it)
                val eq = it.indexOf('=')
                if (eq > 0) guestMap[it.substring(0, eq)] = it.substring(eq + 1)
            }
        }
        // Fallback: reconstruct from environ if the channel is missing.
        if (guestEnv.isEmpty()) {
            env.forEach { (k, v) ->
                if (!k.startsWith("VEXA_") && !k.startsWith("ANDROID_") &&
                    k != "CLASSPATH" && k != "BOOTCLASSPATH" &&
                    k != "SYSTEMSERVERCLASSPATH"
                ) {
                    guestEnv.add("$k=$v")
                    guestMap[k] = v
                }
            }
        }
        // VEXA_* keys travel inside the wrapped guest env — prefer those over
        // the (possibly absent) host-level copies.
        val vexaEnv = { k: String -> guestMap[k] ?: env[k] }

        val filesDir = vexaEnv(ENV_FILES) ?: "/data/data/com.critical.vexaemulator/files"
        val program = vexaEnv(ENV_PROGRAM) ?: "child"
        logFile = File("$filesDir/artifacts/$program/fexchild.log")
        runCatching { logFile?.parentFile?.mkdirs() }
        log("main pid=${android.os.Process.myPid()} args=${args.toList()} envcount=$count")
        if (args.isEmpty()) {
            log("no target")
            kotlin.system.exitProcess(2)
        }
        val target = resolveGuestPath(args[0], "$filesDir/rootfs")
        val childArgs = args.drop(1)
        log("target=$target cwd=${env[ENV_CWD]}")

        try {
            loadRuntimeLibs(vexaEnv)
            log("libs loaded")
        } catch (t: Throwable) {
            log("loadlibs failed: $t")
            kotlin.system.exitProcess(126)
        }

        val request = LaunchRequest(
            executablePath = target,
            rootfsPath = "$filesDir/rootfs",
            thunkHostPath = "$filesDir/thunks/host",
            thunkGuestPath = "$filesDir/thunks/guest",
            workingDirectory = env[ENV_CWD] ?: File(target).parent ?: "/",
            artifactDirectory = "$filesDir/artifacts/$program",
            launchArgs = childArgs,
            launchEnv = guestEnv,
        )
        log("starting $target envn=${guestEnv.size}")
        val code = try {
            com.critical.vexaemulator.RuntimeBridge.startRuntime(request)
        } catch (t: Throwable) {
            log("runtime failed: $t")
            127
        }
        log("exit code=$code")
        kotlin.system.exitProcess(code)
    }

    private fun resolveGuestPath(path: String, rootfs: String): String {
        if (File(path).exists()) return path
        val underRootfs = File(rootfs + path)
        if (underRootfs.exists()) return underRootfs.absolutePath
        return path
    }

    private fun loadRuntimeLibs(vexaEnv: (String) -> String?) {
        val apk = vexaEnv(ENV_APK)
        if (apk != null) {
            // base.apk lives in <pkgdir>/base.apk; jni libs sit alongside in lib/<abi>/.
            val abi = runCatching {
                Class.forName("android.os.Build").getField("SUPPORTED_ABIS")
                    .get(null) as Array<*>
            }.getOrNull()?.firstOrNull() as? String ?: "arm64-v8a"
            val libDir = File(apk).parent + "/lib/" + abi
            runCatching { System.load("$libDir/libvexa.so") }
                .onSuccess { return }
            log("abs load failed, dir=$libDir apk=$apk")
        } else {
            log("no VEXA_APK")
        }
        System.loadLibrary("vexa")
    }
}
