package com.critical.vexaemulator.x11

import android.app.Activity
import android.content.*
import android.os.*
import android.view.KeyEvent
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import com.critical.vexaemulator.runtime.*
import com.termux.x11.LorieView
import java.io.File

/** Real X11 frontend shared by Wine probes and PE launches through Proton. */
class WindowsGameActivity : Activity() {
    private var view: LorieView? = null
    private lateinit var status: TextView
    private lateinit var rootfs: File
    private var xserver: Messenger? = null
    private var runtime: Messenger? = null
    private var xBound = false
    private var runtimeBound = false
    private var xReady = false
    private var launched = false
    private var rendererAttached = false
    private val ui = Handler(Looper.getMainLooper())

    private fun error(message: String) {
        android.util.Log.e("Vexa-RealX11", message)
        status.text = message
        status.visibility = android.view.View.VISIBLE
    }

    private val replies = Messenger(object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RealX11Service.READY -> {
                    xReady = true
                    requestRenderer()
                }
                RealX11Service.RENDERER -> {
                    msg.data.classLoader = ParcelFileDescriptor::class.java.classLoader
                    val fd = if (Build.VERSION.SDK_INT >= 33) msg.data.getParcelable(RealX11Service.FD, ParcelFileDescriptor::class.java)
                    else @Suppress("DEPRECATION") msg.data.getParcelable(RealX11Service.FD)
                    if (fd == null) { error("X11 renderer descriptor missing"); return }
                    view?.connect(fd) ?: fd.close()
                    rendererAttached = true
                    status.text = "Real X11 connected"
                    maybeLaunch()
                }
                RealX11Service.FAILED -> error(msg.data.getString(RealX11Service.ERROR).orEmpty())
                RuntimeIpc.MSG_START_RESULT -> {
                    val code = msg.data.getInt(RuntimeIpc.KEY_CODE)
                    status.text = "Guest exited: $code"
                    status.visibility = android.view.View.VISIBLE
                    android.util.Log.i("Vexa-RealX11", "guest-exit=$code")
                }
                RuntimeIpc.MSG_LOG_EVENT -> android.util.Log.i("Vexa-RealX11",
                    "${msg.data.getString(RuntimeIpc.KEY_CATEGORY)} ${msg.data.getString(RuntimeIpc.KEY_MESSAGE)} ${msg.data.getString(RuntimeIpc.KEY_FIELDS_JSON)}")
                else -> super.handleMessage(msg)
            }
        }
    })

    private fun send(target: Messenger?, what: Int, data: Bundle = Bundle()): Boolean {
        if (target == null) return false
        return runCatching {
            target.send(Message.obtain(null, what).apply { this.data = data; replyTo = replies })
            true
        }.getOrElse { error("Service connection failed: ${it.message}"); false }
    }

    private fun requestRenderer() {
        if (xReady && view?.isAttachedToWindow == true)
            send(xserver, RealX11Service.CONNECT_RENDERER)
    }

    private val xConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            xserver = Messenger(binder)
            send(xserver, RealX11Service.START, Bundle().apply { putString(RealX11Service.ROOTFS, rootfs.path) })
        }
        override fun onServiceDisconnected(name: ComponentName) {
            xserver = null; xReady = false; rendererAttached = false
            if (!isFinishing) error("X11 server process disconnected")
        }
    }
    private val runtimeConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            runtime = Messenger(binder)
            send(runtime, RuntimeIpc.MSG_REGISTER_CLIENT)
            maybeLaunch()
        }
        override fun onServiceDisconnected(name: ComponentName) {
            runtime = null
            if (!isFinishing) error("Runtime service disconnected")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.decorView.systemUiVisibility = (android.view.View.SYSTEM_UI_FLAG_FULLSCREEN or
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
        status = TextView(this).apply {
            text = "Starting real X11…"
            setTextColor(android.graphics.Color.WHITE)
            setBackgroundColor(0x99000000.toInt())
            setPadding(16, 12, 16, 12)
        }
        val content = FrameLayout(this)
        content.addView(status)
        setContentView(content)
        try {
            rootfs = File(intent.getStringExtra("rootfs") ?: File(filesDir, "proton-rootfs").path).canonicalFile
            require(rootfs.path.startsWith(filesDir.canonicalPath + File.separator) && rootfs.isDirectory) {
                "Install the real Linux rootfs in ${File(filesDir, "proton-rootfs")}"
            }
            val renderer = LorieView(this)
            view = renderer
            renderer.setScreenSize(intent.getIntExtra("width", 1280).coerceIn(320, 4096),
                intent.getIntExtra("height", 720).coerceIn(240, 4096))
            renderer.setConnectionListener { requestRenderer() }
            content.addView(renderer, 0, FrameLayout.LayoutParams(-1, -1))
            renderer.requestFocus()
            xBound = bindService(Intent(this, RealX11Service::class.java), xConnection, BIND_AUTO_CREATE)
            require(xBound) { "Cannot bind X11 service" }
            if (!intent.getBooleanExtra("displayOnly", false)) {
                runtimeBound = bindService(Intent(this, RuntimeService::class.java), runtimeConnection, BIND_AUTO_CREATE)
                require(runtimeBound) { "Cannot bind runtime service" }
            }
        } catch (failure: Throwable) {
            error("${failure.javaClass.simpleName}: ${failure.message}")
        }
    }

    private fun maybeLaunch() {
        if (launched || !xReady || !rendererAttached || runtime == null || intent.getBooleanExtra("displayOnly", false)) return
        try {
            val executable = File(intent.getStringExtra("exe")
                ?: File(filesDir, "wine/wine-11.19-staging-amd64/bin/wine").path)
            require(executable.isFile && executable.canRead()) { "Guest executable missing: $executable" }
            val work = File(intent.getStringExtra("cwd") ?: executable.parentFile!!.path)
            require(work.isDirectory) { "Guest working directory missing: $work" }
            val appId = intent.getStringExtra("appid") ?: "90000001"
            require(appId.matches(Regex("[0-9]{1,20}")) && appId.any { it != '0' }) { "Invalid app ID" }
            val prefix = File(filesDir, "compatdata/$appId/pfx")
            val home = File(filesDir, "compatdata/$appId/home").apply { mkdirs() }
            val temporary = File(filesDir, "wtmp").apply { mkdirs() }
            android.system.Os.chmod(temporary.path, 448)
            val defaults = listOf(
                "DISPLAY=:0", "HOME=${home.path}", "PATH=/usr/bin:/bin",
                "WINEPREFIX=${prefix.path}", "TMPDIR=${temporary.path}",
                "SteamAppId=$appId", "SteamGameId=$appId",
                "VEXA_REAL_X11=1", "VEXA_PROTON_DISPLAY_BACKEND=external-x11",
                "VEXA_PROTON_ROOTFS=${rootfs.path}",
                "VEXA_X11_SOCKET=${File(rootfs, "tmp/.X11-unix/X0").path}",
            )
            val overrides = intent.getStringArrayExtra("env")?.toList().orEmpty()
            val keys = overrides.map { it.substringBefore('=') }.toSet()
            val arguments = intent.getStringArrayExtra("args")?.toList() ?: listOf("notepad")
            val artifacts = File(filesDir, "artifacts/windows-$appId").apply { mkdirs() }
            val sent = send(runtime, RuntimeIpc.MSG_START_RUNTIME, Bundle().apply {
                putString(RuntimeIpc.KEY_EXECUTABLE, executable.path)
                putString(RuntimeIpc.KEY_ROOTFS_PATH, rootfs.path)
                putString(RuntimeIpc.KEY_THUNK_HOST_PATH, File(filesDir, "thunks/host").path)
                putString(RuntimeIpc.KEY_THUNK_GUEST_PATH, File(filesDir, "thunks/guest").path)
                putString(RuntimeIpc.KEY_WORKING_DIRECTORY, work.path)
                putString(RuntimeIpc.KEY_ARTIFACT_DIRECTORY, artifacts.path)
                putStringArrayList(RuntimeIpc.KEY_LAUNCH_ENV, ArrayList(defaults.filterNot { it.substringBefore('=') in keys } + overrides))
                putStringArrayList(RuntimeIpc.KEY_LAUNCH_ARGS, ArrayList(arguments))
            })
            if (sent) {
                launched = true
                status.visibility = android.view.View.GONE
                android.util.Log.i("Vexa-RealX11", "guest-launched exe=${executable.path}")
            }
        } catch (failure: Exception) { error("Cannot launch Windows guest: ${failure.message}") }
    }

    private var lastBackKeyTime = 0L

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        android.util.Log.i(
            "VEXA-PAD",
            "win key code=${event.keyCode} action=${event.action} " +
                "src=0x${Integer.toHexString(event.source)} " +
                "dev=0x${event.device?.sources?.let { Integer.toHexString(it) }}"
        )
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            if (event.action == KeyEvent.ACTION_DOWN) lastBackKeyTime = event.eventTime
            // The X keymap translates KEYCODE_BACK to KEY_ESC: every back event
            // reaches the guest as Escape and never navigates the activity.
            view?.dispatchKeyEvent(event)
            return true
        }
        if (view?.dispatchKeyEvent(event) == true) return true
        return super.dispatchKeyEvent(event)
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (SystemClock.uptimeMillis() - lastBackKeyTime < 1000) return
        // Gesture-style back produces no KeyEvent — deliver the same Escape.
        view?.sendKeyEvent(0, KeyEvent.KEYCODE_BACK, true)
        view?.sendKeyEvent(0, KeyEvent.KEYCODE_BACK, false)
    }

    override fun onDestroy() {
        // Stop Wine first so it releases X objects before the X server disappears.
        send(runtime, RuntimeIpc.MSG_STOP_RUNTIME)
        send(runtime, RuntimeIpc.MSG_UNREGISTER_CLIENT)
        if (runtimeBound) runCatching { unbindService(runtimeConnection) }
        view?.release(); view = null
        send(xserver, RealX11Service.STOP)
        if (xBound) runCatching { unbindService(xConnection) }
        ui.removeCallbacksAndMessages(null)
        super.onDestroy()
    }
}
