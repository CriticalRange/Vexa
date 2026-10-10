package com.critical.vexaemulator.runtime

import java.io.File
import java.util.concurrent.TimeUnit

/**
 * Optional root-level diagnostics collector for the :runtime_worker process.
 *
 * Enabled per launch when the launch env contains VEXA_ROOT_LOGS=1, or globally
 * while the marker file files/root_logs_enabled exists.
 *
 * Requires KernelSU (or any su) to have granted this app a root profile;
 * otherwise every operation degrades to a WARN log and no-ops.
 */
object RootLogCollector {
    private const val ENV_FLAG = "VEXA_ROOT_LOGS"
    private const val FLAG_FILE = "root_logs_enabled"
    private const val DMESG_STREAM_NAME = "root_dmesg.log"
    private const val SNAPSHOT_SCRIPT_NAME = "vexa_root_snapshot.sh"

    private val suCandidates = listOf(
        "su",
        "/system/bin/su",
        "/system/xbin/su",
        "/debug_ramdisk/su",
        "/sbin/su",
    )

    fun isEnabled(launchEnv: List<String>, filesDir: File): Boolean {
        if (launchEnv.any { it == "$ENV_FLAG=1" }) return true
        return isEnabled(filesDir)
    }

    fun isEnabled(filesDir: File): Boolean = File(filesDir, FLAG_FILE).isFile

    fun setEnabled(filesDir: File, enabled: Boolean): Boolean = runCatching {
        val flag = File(filesDir, FLAG_FILE)
        if (enabled) {
            flag.parentFile?.mkdirs()
            flag.createNewFile()
            flag.isFile
        } else {
            !flag.exists() || flag.delete()
        }
    }.getOrDefault(false)

    fun probeSu(): String? {
        for (candidate in suCandidates) {
            val ok = runCatching {
                val p = ProcessBuilder(candidate, "-c", "id")
                    .redirectErrorStream(true)
                    .start()
                if (!p.waitFor(5, TimeUnit.SECONDS)) {
                    p.destroyForcibly()
                    return@runCatching false
                }
                val out = p.inputStream.bufferedReader().readText()
                p.exitValue() == 0 && out.contains("uid=0")
            }.getOrDefault(false)
            if (ok) return candidate
        }
        return null
    }

    class Session(
        private val suCmd: String,
        private val artifactDir: File,
        private val workerPid: Int,
        private val filesDir: File,
        private val onLog: (level: String, message: String) -> Unit,
    ) {
        private var dmesgProcess: Process? = null
        private var dmesgPid: Int? = null

        fun start() {
            artifactDir.mkdirs()
            val streamFile = File(artifactDir, DMESG_STREAM_NAME)
            // The su shell doubles as a watchdog: it echoes the dmesg pid, then
            // polls the worker pid and kills dmesg when the worker exits. This
            // keeps the stream bounded to the run even when the worker dies via
            // a native exit() that never reaches Kotlin teardown code.
            val cmd = "dmesg -w > '${streamFile.absolutePath}' 2>/dev/null & " +
                "echo \$!; " +
                "while kill -0 $workerPid 2>/dev/null; do sleep 2; done; " +
                "kill \$! 2>/dev/null"
            val proc = runCatching {
                ProcessBuilder(suCmd, "-c", cmd).start()
            }.getOrNull()
            dmesgPid = proc?.let {
                runCatching {
                    it.inputStream.bufferedReader().readLine()?.trim()?.toIntOrNull()
                }.getOrNull()
            }
            dmesgProcess = proc
            if (dmesgProcess == null) {
                onLog("WARN", "Root dmesg stream failed to start")
            } else {
                onLog(
                    "INFO",
                    "Root dmesg stream started -> ${streamFile.name}" +
                        (dmesgPid?.let { " (pid $it)" } ?: "")
                )
            }
            remediateCpuset()
            snapshot("start")
        }

        // The worker lands in /foreground cpuset (no big core); with root we
        // can migrate it to top-app so FEX sees all cpus.
        private fun remediateCpuset() {
            val targets = listOf(
                "/dev/cpuset/top-app/cgroup.procs",
                "/dev/cpuset/top-app/tasks",
            )
            for (target in targets) {
                val ok = runCatching {
                    val p = ProcessBuilder(suCmd, "-c", "echo $workerPid > $target")
                        .redirectErrorStream(true)
                        .start()
                    p.waitFor(5, TimeUnit.SECONDS) && p.exitValue() == 0
                }.getOrDefault(false)
                if (ok) {
                    onLog("INFO", "Worker moved to top-app cpuset via $target")
                    return
                }
            }
            val current = runCatching {
                File("/proc/self/cpuset").readText().trim()
            }.getOrDefault("?")
            onLog("WARN", "cpuset remediation failed; worker stays in $current")
        }

        fun snapshot(tag: String) {
            val safeTag = tag.replace(Regex("[^A-Za-z0-9._-]"), "_")
            val outFile = File(artifactDir, "root_snapshot_$safeTag.txt")
            val scriptFile = File(filesDir, SNAPSHOT_SCRIPT_NAME)
            val wroteScript = runCatching {
                scriptFile.writeText(snapshotScript(workerPid))
                scriptFile.setReadable(true, false)
            }.isSuccess
            if (!wroteScript) {
                onLog("WARN", "Root snapshot script write failed")
                return
            }
            val cmd = "sh '${scriptFile.absolutePath}' > '${outFile.absolutePath}' 2>&1"
            val ok = runCatching {
                val p = ProcessBuilder(suCmd, "-c", cmd).start()
                p.waitFor(30, TimeUnit.SECONDS) && p.exitValue() == 0
            }.getOrDefault(false)
            onLog(
                if (ok) "INFO" else "WARN",
                "Root snapshot '$safeTag' ${if (ok) "written" else "failed"} -> ${outFile.name}"
            )
        }

        @Synchronized
        fun stop(tag: String) {
            snapshot(tag)
            // destroy() alone orphans the dmesg child (su is only its parent);
            // kill the captured pid first, then tear down the watchdog shell.
            dmesgPid?.let { pid ->
                runCatching {
                    val p = ProcessBuilder(suCmd, "-c", "kill $pid")
                        .redirectErrorStream(true)
                        .start()
                    p.waitFor(5, TimeUnit.SECONDS)
                }
            }
            runCatching { dmesgProcess?.destroy() }
            dmesgProcess = null
            dmesgPid = null
        }

        private fun snapshotScript(pid: Int): String = """
            echo "=== root snapshot pid=$pid ==="
            date
            echo "--- getenforce ---"
            getenforce 2>/dev/null
            echo "--- cmdline ---"
            tr '\\0' ' ' < /proc/$pid/cmdline; echo
            echo "--- status ---"
            cat /proc/$pid/status
            echo "--- smaps_rollup ---"
            cat /proc/$pid/smaps_rollup 2>/dev/null
            echo "--- cpuset / affinity ---"
            cat /proc/$pid/cpuset 2>/dev/null
            taskset -p $pid 2>/dev/null
            echo "--- oom ---"
            cat /proc/$pid/oom_score /proc/$pid/oom_score_adj 2>/dev/null
            echo "--- live state (syscall + kernel stack) ---"
            cat /proc/$pid/syscall 2>/dev/null
            cat /proc/$pid/stack 2>/dev/null
            echo "--- schedstat ---"
            cat /proc/$pid/schedstat 2>/dev/null
            echo "--- tasks ---"
            for t in /proc/$pid/task/*; do
              echo "== ${'$'}t"
              cat "${'$'}t/comm" 2>/dev/null
              cat "${'$'}t/wchan" 2>/dev/null; echo
              cat "${'$'}t/stack" 2>/dev/null
            done
            echo "--- fds ---"
            ls -l /proc/$pid/fd 2>/dev/null | head -60
            echo "--- debuggerd backtrace ---"
            debuggerd -b $pid 2>&1
            echo "--- dmesg tail ---"
            dmesg 2>/dev/null | tail -100
            echo "--- dmesg filtered (avc|vexa|fex|segfault|oom) ---"
            dmesg 2>/dev/null | grep -iE 'avc|vexa|fex|segfault|sigsegv|oom|audit' | tail -100
            echo "--- tombstones ---"
            ls -la /data/tombstones/ 2>/dev/null | tail -10
            echo "--- thermal / cpufreq ---"
            for z in /sys/class/thermal/thermal_zone*; do
              echo "${'$'}z $(cat ${'$'}z/type 2>/dev/null) $(cat ${'$'}z/temp 2>/dev/null)"
            done
            for p in /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq; do
              echo "${'$'}p $(cat ${'$'}p 2>/dev/null)"
            done
            cat /proc/pressure/cpu /proc/pressure/memory 2>/dev/null
        """.trimIndent()
    }

    fun startSession(
        artifactDir: File,
        filesDir: File,
        workerPid: Int,
        onLog: (level: String, message: String) -> Unit,
    ): Session? {
        val suCmd = probeSu()
        if (suCmd == null) {
            onLog(
                "WARN",
                "Root logging requested but su unavailable " +
                    "(grant this app root in KernelSU Manager)"
            )
            return null
        }
        onLog("INFO", "Root logging enabled via $suCmd")
        val session = Session(suCmd, artifactDir, workerPid, filesDir, onLog)
        session.start()
        return session
    }
}
