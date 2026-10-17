package com.critical.vexaemulator.x11

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.*
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Blocking readiness belongs exclusively to the headless launch worker. */
class HeadlessX11Client(private val context: Context) {
    private val ready = CountDownLatch(1)
    @Volatile private var remote: Messenger? = null
    @Volatile private var error: String? = null
    private var registered = false
    private var rootfs = ""
    private var width = 1280
    private var height = 720
    private var captureFrames = false
    private val reply = Messenger(object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RealX11Service.READY -> ready.countDown()
                RealX11Service.FAILED -> { error = msg.data.getString(RealX11Service.ERROR); ready.countDown() }
                else -> super.handleMessage(msg)
            }
        }
    })
    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            remote = Messenger(binder)
            runCatching {
                remote!!.send(Message.obtain(null, RealX11Service.START).apply {
                    replyTo = reply
                    data = Bundle().apply {
                        putString(RealX11Service.ROOTFS, rootfs)
                        putBoolean(RealX11Service.HEADLESS, true)
                        putInt(RealX11Service.WIDTH, width)
                        putInt(RealX11Service.HEIGHT, height)
                        putBoolean(RealX11Service.CAPTURE_FRAMES, captureFrames)
                    }
                })
            }.onFailure { error = it.message; ready.countDown() }
        }
        override fun onServiceDisconnected(name: ComponentName?) { error = "X11 service disconnected"; remote = null; ready.countDown() }
        override fun onBindingDied(name: ComponentName?) = onServiceDisconnected(name)
        override fun onNullBinding(name: ComponentName?) { error = "X11 service returned no binder"; ready.countDown() }
    }
    fun start(rootfs: String, width: Int, height: Int, captureFrames: Boolean = false) {
        check(Looper.myLooper() != Looper.getMainLooper())
        this.rootfs = rootfs; this.width = width; this.height = height; this.captureFrames = captureFrames
        registered = context.bindService(Intent(context, RealX11Service::class.java), connection, Context.BIND_AUTO_CREATE)
        check(registered) { "Cannot bind real X11 service" }
        check(ready.await(25, TimeUnit.SECONDS)) { "Real X11 readiness timed out" }
        check(error == null) { error ?: "Real X11 failed" }
    }
    fun close() {
        runCatching { remote?.send(Message.obtain(null, RealX11Service.STOP)) }
        if (registered) runCatching { context.unbindService(connection) }
        registered = false; remote = null
    }
}
