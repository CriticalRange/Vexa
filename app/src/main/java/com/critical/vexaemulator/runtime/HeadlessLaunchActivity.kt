package com.critical.vexaemulator.runtime

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.util.Log
import androidx.activity.ComponentActivity
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.guest.GuestInstaller
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * NoDisplay trampoline so `am start` can drive a guest launch with no window
 * ever appearing on screen. The activity must finish() before onResume, so
 * all long-lived work lives in [HeadlessLaunch]. The GL thunk renders into an
 * EGL pbuffer when the launch env carries VEXA_HEADLESS=1 — no UI-process or
 * worker-process surface is ever involved.
 *
 *   am start -n com.critical.vexaemulator/.runtime.HeadlessLaunchActivity \
 *     --es programId brotato
 *
 * or an explicit guest binary (for ELF tests not in GuestCatalog):
 *
 *   ... --es exe /data/data/com.critical.vexaemulator/files/guests/t32/t32 \
 *       --esa args a b --esa env DISPLAY=:0
 *
 * Stop: same activity with --es stop 1.
 */
class HeadlessLaunchActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        HeadlessLaunch.onIntent(applicationContext, intent)
        finish()
    }
}

private object HeadlessLaunch {
    private const val TAG = "VEXA-HEADLESS"
    private const val BIND_TIMEOUT_S = 10L

    private val thread = HandlerThread("headless-launch").apply { start() }
    private val handler = Handler(thread.looper)

    @Volatile private var runtimeMessenger: Messenger? = null
    @Volatile private var bound = false
    private var bindLatch = CountDownLatch(1)
    @Volatile private var binding = false
    private var expiry: Runnable? = null

    private val replyHandler = object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RuntimeIpc.MSG_LOG_EVENT -> {
                    val cat = msg.data.getString(RuntimeIpc.KEY_CATEGORY).orEmpty()
                    val text = msg.data.getString(RuntimeIpc.KEY_MESSAGE).orEmpty()
                    val fields = msg.data.getString(RuntimeIpc.KEY_FIELDS_JSON).orEmpty()
                    Log.i(TAG, "[$cat] $text $fields")
                }

                RuntimeIpc.MSG_START_RESULT -> {
                    Log.i(TAG, "start result code=${msg.data.getInt(RuntimeIpc.KEY_CODE, -1)}")
                    handler.post { unbind() }
                }

                else -> super.handleMessage(msg)
            }
        }
    }
    private val replyMessenger = Messenger(replyHandler)

    private val runtimeConnection: ServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            runtimeMessenger = Messenger(service)
            bound = true
            Log.i(TAG, "runtime service connected")
            bindLatch.countDown()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            runtimeMessenger = null
            bound = false
            // Drop the dead binding entirely so a later launch can bind fresh.
            if (binding) runCatching { appContext.unbindService(runtimeConnection) }
            binding = false
            Log.w(TAG, "runtime service disconnected")
        }

        override fun onBindingDied(name: ComponentName?) {
            runtimeMessenger = null
            bound = false
            if (binding) runCatching { appContext.unbindService(runtimeConnection) }
            binding = false
            Log.w(TAG, "runtime service binding died")
        }

        override fun onNullBinding(name: ComponentName?) {
            // A null binder still requires unbindService to release registration.
            if (binding) runCatching { appContext.unbindService(runtimeConnection) }
            runtimeMessenger = null
            bound = false
            binding = false
            Log.w(TAG, "runtime service returned null binder")
            bindLatch.countDown()
        }
    }

    private lateinit var appContext: Context

    fun onIntent(context: Context, intent: Intent?) {
        appContext = context.applicationContext
        if (intent?.getStringExtra("stop") == "1") {
            requestStop()
        } else {
            launch(context, intent)
        }
    }

    private fun send(what: Int, fill: (Bundle.() -> Unit)? = null): Boolean {
        val target = runtimeMessenger ?: return false
        val out = Message.obtain(null, what).apply {
            replyTo = replyMessenger
            fill?.let { data = Bundle().apply(it) }
        }
        return runCatching { target.send(out) }.isSuccess
    }

    private fun bind(): Boolean {
        if (bound) return true
        bindLatch = CountDownLatch(1)
        binding = appContext.bindService(
            Intent(appContext, RuntimeService::class.java),
            runtimeConnection,
            Context.BIND_AUTO_CREATE
        )
        if (!binding) return false
        if (!bindLatch.await(BIND_TIMEOUT_S, TimeUnit.SECONDS)) {
            unbind()
            return false
        }
        return bound
    }

    private fun unbind() {
        expiry?.let { handler.removeCallbacks(it) }
        expiry = null
        if (binding) runCatching { appContext.unbindService(runtimeConnection) }
        binding = false
        bound = false
        runtimeMessenger = null
    }

    private fun requestStop() {
        handler.post {
            if (bind()) {
                send(RuntimeIpc.MSG_REGISTER_CLIENT)
                send(RuntimeIpc.MSG_STOP_RUNTIME)
            }
            Thread.sleep(500)
            unbind()
        }
    }

    private fun publishGuestWindowSize(rootfs: String, width: Int, height: Int) {
        val dir = File(rootfs, "tmp").apply { mkdirs() }
        File(dir, "vexa-window-size").writeText("$width $height\n")
        File(dir, "vexa-pointer").writeBytes(ByteArray(0))
        File(dir, "vexa-present")
            .writeText("0 0 $width $height 0 0 $width $height $width $height\n")
    }

    private fun buildRequest(context: Context, intent: Intent): LaunchRequest {
        val exe = intent.getStringExtra("exe").orEmpty()
        val programId = intent.getStringExtra("programId").orEmpty()

        if (exe.isNotEmpty()) {
            GuestInstaller.ensureRuntime(context)
            val root = context.filesDir
            val rootfs = File(root, "rootfs").apply { mkdirs() }
            val thunkHost = File(root, "thunks/host").apply { mkdirs() }
            val thunkGuest = File(root, "thunks/guest").apply { mkdirs() }
            val exeFile = File(exe)
            val artifacts = File(root, "artifacts/headless").apply { mkdirs() }
            return LaunchRequest(
                executablePath = exeFile.absolutePath,
                rootfsPath = rootfs.absolutePath,
                thunkHostPath = thunkHost.absolutePath,
                thunkGuestPath = thunkGuest.absolutePath,
                workingDirectory = exeFile.parentFile?.absolutePath ?: rootfs.absolutePath,
                artifactDirectory = artifacts.absolutePath,
                launchEnv = intent.getStringArrayExtra("env")?.toList().orEmpty(),
                launchArgs = intent.getStringArrayExtra("args")?.toList().orEmpty(),
            )
        }

        val program = GuestCatalog.find(programId)
            ?: throw IllegalStateException("unknown programId: $programId")
        val request = GuestInstaller.install(context, program)
        val overrides = intent.getStringArrayExtra("env")?.toList().orEmpty()
        val keys = overrides.map { it.substringBefore('=') }.toSet()
        return request.copy(
            launchEnv = request.launchEnv.filterNot { it.substringBefore('=') in keys } + overrides,
            launchArgs = intent.getStringArrayExtra("args")?.toList() ?: request.launchArgs,
        )
    }

    private fun launch(context: Context, intent: Intent?) {
        val safeIntent = intent ?: run {
            Log.e(TAG, "null intent")
            return
        }
        val width = safeIntent.getIntExtra("width", 1280).coerceIn(1, 8192)
        val height = safeIntent.getIntExtra("height", 720).coerceIn(1, 8192)
        val ttlMinutes = safeIntent.getIntExtra("ttlMinutes", 10).coerceIn(1, 1440)

        handler.post {
            try {
                if (binding) {
                    Log.w(TAG, "a headless session is already active; stop it before launching another")
                    return@post
                }
                val request = buildRequest(context, safeIntent)
                publishGuestWindowSize(request.rootfsPath, width, height)

                if (!bind()) {
                    Log.e(TAG, "runtime service bind timed out")
                    return@post
                }

                send(RuntimeIpc.MSG_REGISTER_CLIENT)
                val env = ArrayList(request.launchEnv.filterNot {
                    it.substringBefore('=') in setOf("VEXA_HEADLESS", "VEXA_HEADLESS_W", "VEXA_HEADLESS_H")
                }).apply {
                    add("VEXA_HEADLESS=1")
                    add("VEXA_HEADLESS_W=$width")
                    add("VEXA_HEADLESS_H=$height")
                }
                val sent = send(RuntimeIpc.MSG_START_RUNTIME) {
                    putString(RuntimeIpc.KEY_EXECUTABLE, request.executablePath)
                    putString(RuntimeIpc.KEY_ROOTFS_PATH, request.rootfsPath)
                    putString(RuntimeIpc.KEY_THUNK_HOST_PATH, request.thunkHostPath)
                    putString(RuntimeIpc.KEY_THUNK_GUEST_PATH, request.thunkGuestPath)
                    putString(RuntimeIpc.KEY_WORKING_DIRECTORY, request.workingDirectory)
                    putString(RuntimeIpc.KEY_ARTIFACT_DIRECTORY, request.artifactDirectory)
                    putStringArrayList(RuntimeIpc.KEY_LAUNCH_ENV, env)
                    putStringArrayList(RuntimeIpc.KEY_LAUNCH_ARGS, ArrayList(request.launchArgs))
                }
                Log.i(TAG, "launch sent=$sent exe=${request.executablePath}")

                if (!sent) { unbind(); return@post }
                expiry = object : Runnable {
                    override fun run() {
                        // A dead worker may have been replaced before this timer fires.
                        if (expiry !== this || !bound) return
                        Log.w(TAG, "ttl expired, stopping runtime")
                        send(RuntimeIpc.MSG_STOP_RUNTIME)
                        handler.postDelayed({ if (expiry === this) unbind() }, 2000)
                    }
                }.also { handler.postDelayed(it, ttlMinutes * 60_000L) }
            } catch (t: Throwable) {
                unbind()
                Log.e(TAG, "headless launch failed", t)
            }
        }
    }
}
