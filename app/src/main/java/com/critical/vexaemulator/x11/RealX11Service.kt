package com.critical.vexaemulator.x11

import android.app.Service
import android.content.Intent
import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.os.*
import android.system.Os
import android.graphics.Bitmap
import android.graphics.PixelFormat
import android.media.ImageReader
import com.termux.x11.LorieView
import com.termux.x11.CmdEntryPoint
import java.io.File

/** Real Xorg owns its own process: native server exit must never terminate UI/FEX. */
class RealX11Service : Service() {
    companion object {
        const val START = 1
        const val CONNECT_RENDERER = 2
        const val STOP = 3
        const val READY = 10
        const val FAILED = 11
        const val RENDERER = 12
        const val ROOTFS = "rootfs"
        const val FD = "fd"
        const val ERROR = "error"
        const val HEADLESS = "headless"
        const val WIDTH = "width"
        const val HEIGHT = "height"
        const val CAPTURE_FRAMES = "captureFrames"
    }

    private val handler = Handler(Looper.getMainLooper())
    private var entry: CmdEntryPoint? = null
    private var root: File? = null
    private var ready = false
    private var starting = false
    private val waiting = mutableListOf<Messenger>()
    private var offscreenView: LorieView? = null
    private var imageReader: ImageReader? = null
    private var captureThread: HandlerThread? = null
    private var headless = false
    private var captureFrames = false
    private var width = 1280
    private var height = 720

    private fun createOffscreenRenderer() {
        if (!headless || offscreenView != null) return
        val artifacts = File(filesDir, "artifacts/headless-x11").apply { mkdirs() }
        if (captureFrames) {
            File(artifacts, "latest.png").takeIf { it.exists() }?.let {
                check(it.renameTo(File(artifacts, "previous-${System.currentTimeMillis()}.png"))) { "Cannot archive previous headless frame" }
            }
            File(artifacts, "frames.txt").writeText("0\n")
        }
        File(artifacts, "session.txt").writeText("pid=${Process.myPid()} started=${System.currentTimeMillis()} width=$width height=$height format=RGBX_8888 capture=$captureFrames\n")
        val worker = HandlerThread("x11-frame-drain").apply { start() }
        captureThread = worker
        var frames = 0L
        var lastCapture = 0L
        // The native EGL renderer selects an opaque RGBX window configuration.
        val reader = ImageReader.newInstance(width, height, PixelFormat.RGBX_8888, 3)
        imageReader = reader
        reader.setOnImageAvailableListener({ source ->
            runCatching { source.acquireLatestImage()?.use { image ->
                frames++
                val now = SystemClock.uptimeMillis()
                if (captureFrames && now - lastCapture >= 1000) {
                    lastCapture = now
                    runCatching {
                        val plane = image.planes[0]
                        val buffer = plane.buffer
                        val pixels = IntArray(image.width * image.height)
                        // The final row may omit stride padding. Read actual pixels only;
                        // RGBX's unused alpha byte must never make the PNG transparent.
                        for (y in 0 until image.height) for (x in 0 until image.width) {
                            val offset = y * plane.rowStride + x * plane.pixelStride
                            val red = buffer.get(offset).toInt() and 255
                            val green = buffer.get(offset + 1).toInt() and 255
                            val blue = buffer.get(offset + 2).toInt() and 255
                            pixels[y * image.width + x] = (255 shl 24) or (red shl 16) or (green shl 8) or blue
                        }
                        val bitmap = Bitmap.createBitmap(pixels, image.width, image.height, Bitmap.Config.ARGB_8888)
                        try {
                            File(artifacts, "latest.png.tmp").outputStream().use { check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, it)) }
                            check(File(artifacts, "latest.png.tmp").renameTo(File(artifacts, "latest.png")))
                            File(artifacts, "frames.txt").writeText("$frames\n")
                        } finally { bitmap.recycle() }
                    }.onFailure { android.util.Log.w("VexaX11", "Headless frame capture failed", it) }
                }
            } }.onFailure { android.util.Log.w("VexaX11", "Headless image acquire/drain failed", it) }
        }, Handler(worker.looper))
        val view = LorieView(this)
        offscreenView = view
        view.apply {
            setScreenSize(this@RealX11Service.width, this@RealX11Service.height)
            setOffscreenSurface(reader.surface)
            val fd = requireNotNull(entry!!.getXConnection()) { "No headless renderer connection" }
            fd.use { connect(it) }
        }
    }

    private fun reply(target: Messenger, what: Int, values: Bundle = Bundle()) {
        runCatching { target.send(Message.obtain(null, what).apply { data = values }) }
    }

    private fun fail(reason: String) {
        starting = false
        waiting.toList().forEach { reply(it, FAILED, Bundle().apply { putString(ERROR, reason) }) }
        waiting.clear()
        // Server startup failure leaves native state unsuitable for another start.
        handler.postDelayed({ stopSelf() }, 250)
    }

    private fun awaitSocket(deadline: Long) {
        val socket = File(root, "tmp/.X11-unix/X0")
        val listening = socket.exists() && runCatching {
            LocalSocket().use {
                it.connect(LocalSocketAddress(socket.path, LocalSocketAddress.Namespace.FILESYSTEM))
            }
            true
        }.getOrDefault(false)
        if (listening) {
            try { createOffscreenRenderer() } catch (error: Throwable) {
                fail("Headless renderer: ${error.message}"); return
            }
            ready = true
            starting = false
            waiting.toList().forEach { reply(it, READY) }
            waiting.clear()
        } else if (SystemClock.uptimeMillis() < deadline) {
            handler.postDelayed({ awaitSocket(deadline) }, 50)
        } else fail("Xorg did not create its listening socket: ${socket.path}")
    }

    private fun start(msg: Message) {
        val target = msg.replyTo ?: return
        val requested = File(msg.data.getString(ROOTFS).orEmpty()).canonicalFile
        val privateRoot = filesDir.canonicalFile
        if (!requested.path.startsWith(privateRoot.path + File.separator) || !requested.isDirectory) {
            reply(target, FAILED, Bundle().apply { putString(ERROR, "X11 rootfs must be an installed app-private directory") })
            return
        }
        if (root != null && root != requested) {
            reply(target, FAILED, Bundle().apply { putString(ERROR, "A different X11 rootfs is already active") })
            return
        }
        if (ready) { reply(target, READY); return }
        waiting.add(target)
        if (starting) return
        starting = true
        root = requested
        headless = msg.data.getBoolean(HEADLESS, false)
        captureFrames = msg.data.getBoolean(CAPTURE_FRAMES, false)
        width = msg.data.getInt(WIDTH, 1280).coerceIn(1, 8192)
        height = msg.data.getInt(HEIGHT, 720).coerceIn(1, 8192)
        try {
            val temporary = File(requested, "tmp").apply { mkdirs() }
            Os.chmod(temporary.path, 448)
            val xkb = File(requested, "usr/share/X11/xkb")
            require(File(xkb, "rules/evdev").isFile) { "Install xkb-data in the real X11 rootfs" }
            Os.setenv("TMPDIR", temporary.path, true)
            Os.setenv("XKB_CONFIG_ROOT", xkb.path, true)
            // Rootfs fonts are discovered from TMPDIR's parent by upstream Xorg.
            entry = CmdEntryPoint(this)
            require(entry!!.start(arrayOf(":0", "-ac", "-nolisten", "tcp", "-noreset"))) {
                "Xorg startup rejected its configuration"
            }
            awaitSocket(SystemClock.uptimeMillis() + 15_000)
        } catch (error: Throwable) {
            fail("${error.javaClass.simpleName}: ${error.message}")
        }
    }

    private val messenger = Messenger(object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                START -> start(msg)
                CONNECT_RENDERER -> {
                    val target = msg.replyTo ?: return
                    if (!ready) { reply(target, FAILED, Bundle().apply { putString(ERROR, "Xorg is not ready") }); return }
                    try {
                        entry!!.getXConnection().use { fd ->
                            requireNotNull(fd) { "No Xorg renderer connection" }
                            reply(target, RENDERER, Bundle().apply { putParcelable(FD, fd) })
                        }
                    } catch (error: Exception) {
                        reply(target, FAILED, Bundle().apply { putString(ERROR, error.message) })
                    }
                }
                STOP -> stopSelf()
                else -> super.handleMessage(msg)
            }
        }
    })

    override fun onBind(intent: Intent): IBinder = messenger.binder
    override fun onUnbind(intent: Intent): Boolean { stopSelf(); return false }
    override fun onDestroy() {
        offscreenView?.release()
        offscreenView = null
        imageReader?.setOnImageAvailableListener(null, null)
        imageReader?.close()
        imageReader = null
        captureThread?.quitSafely()
        entry?.closeCallbacks()
        handler.removeCallbacksAndMessages(null)
        super.onDestroy()
        // Xorg is single-use and upstream exits the process on server termination.
        Process.killProcess(Process.myPid())
    }
}
