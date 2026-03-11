package com.critical.vexaemulator.runtime

import android.app.Service
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.Process
import com.critical.vexaemulator.RuntimeBridge
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger

private const val MAX_PENDING_LOGS = 128

class RuntimeService : Service() {
    private var clientMessenger: Messenger? = null

    private val pendingLogs = ArrayDeque<PendingLog>()
    private val incomingHandler = object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RuntimeIpc.MSG_REGISTER_CLIENT -> {
                    clientMessenger = msg.replyTo
                    while (pendingLogs.isNotEmpty()) {
                        val l = pendingLogs.removeFirst()
                        emitLog(l.level, l.category, l.message, l.fieldsJson)
                    }
                    emitLog(
                        level = "INFO",
                        category = "BOOT",
                        message = "Runtime client registered",
                        fieldsJson = """{"runtimePid":"${Process.myPid()}","runtimeProcess
  ":"${android.app.Application.getProcessName()}"}"""
                    )
                }

                RuntimeIpc.MSG_UNREGISTER_CLIENT -> {
                    clientMessenger = null
                }

                RuntimeIpc.MSG_START_RUNTIME -> {
                    val d = msg.data
                    val request = LaunchRequest(
                        executablePath = d.getString(RuntimeIpc.KEY_EXECUTABLE).orEmpty(),
                        rootfsPath = d.getString(RuntimeIpc.KEY_ROOTFS_PATH).orEmpty(),
                        thunkHostPath = d.getString(RuntimeIpc.KEY_THUNK_HOST_PATH).orEmpty(),
                        thunkGuestPath = d.getString(RuntimeIpc.KEY_THUNK_GUEST_PATH).orEmpty(),
                        workingDirectory = d.getString(RuntimeIpc.KEY_WORKING_DIRECTORY).orEmpty(),
                        artifactDirectory = d.getString(RuntimeIpc.KEY_ARTIFACT_DIRECTORY)
                            .orEmpty(),
                    )
                    val code = RuntimeBridge.startRuntime(request)
                    sendStartResult(code)
                }

                RuntimeIpc.MSG_SURFACE_CREATED -> {
                    emitLog("INFO", "SURFACE", "Surface created in UI process")
                    // TODO: add real surface transport,
                    //  initialize renderer binding here
                }

                RuntimeIpc.MSG_SURFACE_CHANGED -> {
                    val d = msg.data
                    val surfaceFormat = d.getInt(RuntimeIpc.KEY_SURFACE_FORMAT, -1)
                    val surfaceWidth = d.getInt(RuntimeIpc.KEY_SURFACE_WIDTH, -1)
                    val surfaceHeight = d.getInt(RuntimeIpc.KEY_SURFACE_HEIGHT, -1)
                    emitLog(
                        "INFO", "SURFACE", "Surface change detected",

                        """{"format":"$surfaceFormat","width":"$surfaceWidth","height":"$surfaceHeight"}"""
                    )
                    // TODO: RuntimeBridge.onRuntimeSurfaceChanged(width, height)
                    //  once routed in runtime process
                }

                RuntimeIpc.MSG_SURFACE_DESTROYED -> {
                    emitLog("INFO", "SURFACE", "Surface destroyed in UI process")
                    // TODO: release runtime-side surface state
                }

                RuntimeIpc.MSG_STOP_RUNTIME -> {
                    RuntimeBridge.stopRuntime()
                    emitLog("INFO", "BOOT", "Runtime stop requested")
                }

                else -> super.handleMessage(msg)
            }
        }
    }

    private val serviceMessenger = Messenger(incomingHandler)

    private data class PendingLog(
        val level: String,
        val category: String,
        val message: String,
        val fieldsJson: String,
    )

    private val nativeSink = object : NativeLogSink() {
        override fun onNativeLog(
            level: String,
            category: String,
            message: String,
            fieldsJson: String,
        ) {
            emitLog(level, category, message, fieldsJson)
        }
    }

    private fun emitLog(
        level: String,
        category: String,
        message: String,
        fieldsJson: String = "{}"
    ) {
        val target = clientMessenger
        if (target == null) {
            if (pendingLogs.size >= MAX_PENDING_LOGS) {
                pendingLogs.removeFirst()
            }
            pendingLogs.addLast(PendingLog(level, category, message, fieldsJson))
            return
        }
        val out = Message.obtain(null, RuntimeIpc.MSG_LOG_EVENT).apply {
            data = Bundle().apply {
                putString(RuntimeIpc.KEY_LEVEL, level)
                putString(RuntimeIpc.KEY_CATEGORY, category)
                putString(RuntimeIpc.KEY_MESSAGE, message)
                putString(RuntimeIpc.KEY_FIELDS_JSON, fieldsJson)
            }
        }
        runCatching {
            target?.send(out)
        }
    }

    override fun onCreate() {
        super.onCreate()
        // This runs in :runtime process
        // Don't need FEXCore loadLibrary here, native-side handles it
        System.loadLibrary("vexa")
        RuntimeBridge.setNativeLogSink(nativeSink)
        emitLog(
            "INFO",
            "BOOT",
            "RuntimeService created in :runtime",
            """{"runtimePid":"${Process.myPid()}","runtimeProcess":"${android.app.Application.getProcessName()}"}"""
        )
        VexaLogger.log(
            LogLevel.INFO,
            LogCategory.BOOT,
            "RuntimeService created in :runtime",
            fields = mapOf(
                "runtimePid" to Process.myPid().toString(),
                "runtimeProcess" to android.app.Application.getProcessName()
            )
        )
    }

    override fun onBind(intent: Intent): IBinder = serviceMessenger.binder

    private fun sendStartResult(code: Int) {
        val target = clientMessenger ?: return
        val out = Message.obtain(null, RuntimeIpc.MSG_START_RESULT).apply {
            data = Bundle().apply {
                putInt(RuntimeIpc.KEY_CODE, code)
            }
        }
        runCatching { target.send(out) }
    }

    override fun onDestroy() {
        RuntimeBridge.setNativeLogSink(null)
        super.onDestroy()
    }
}