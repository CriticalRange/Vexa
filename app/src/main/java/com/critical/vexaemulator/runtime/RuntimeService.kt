package com.critical.vexaemulator.runtime

import android.app.Service
import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.Process
import android.view.Surface
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger

private const val MAX_PENDING_LOGS = 128

class RuntimeService : Service() {
    private var clientMessenger: Messenger? = null
    private var workerMessenger: Messenger? = null
    private var workerBound = false
    private var workerBinding = false
    private var pendingWorkerStart: LaunchRequest? = null
    private var pendingSurface: Surface? = null

    private fun getSurfaceCompat(bundle: Bundle): Surface? {
        bundle.classLoader = Surface::class.java.classLoader
        return if (Build.VERSION.SDK_INT >= 33) {
            bundle.getParcelable(RuntimeIpc.KEY_SURFACE, Surface::class.java)
        } else {
            @Suppress("DEPRECATION")
            bundle.getParcelable(RuntimeIpc.KEY_SURFACE)
        }
    }

    private fun sendSurfaceToWorker(surface: Surface?) {
        val target = workerMessenger ?: return
        val out = Message.obtain(
            null,
            RuntimeIpc.MSG_WORKER_SET_SURFACE
        ).apply {
            replyTo = workerReplyMessenger
            data = Bundle().apply {
                putParcelable(RuntimeIpc.KEY_SURFACE, surface)
            }
        }
        runCatching { target.send(out) }
    }

    private val workerReplyHandler = object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RuntimeIpc.MSG_WORKER_START_RESULT -> {
                    val code = msg.data.getInt(RuntimeIpc.KEY_CODE, -999)
                    sendStartResult(code)
                }

                RuntimeIpc.MSG_LOG_EVENT -> {
                    val d = msg.data
                    emitLog(
                        level = d.getString(RuntimeIpc.KEY_LEVEL).orEmpty().ifBlank { "INFO" },
                        category = d.getString(RuntimeIpc.KEY_CATEGORY).orEmpty()
                            .ifBlank { "RUNTIME" },
                        message = d.getString(RuntimeIpc.KEY_MESSAGE).orEmpty(),
                        fieldsJson = d.getString(RuntimeIpc.KEY_FIELDS_JSON).orEmpty()
                            .ifBlank { "{}" }
                    )
                }

                else -> super.handleMessage(msg)
            }
        }
    }

    private val workerReplyMessenger = Messenger(workerReplyHandler)
    private val workerConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            workerMessenger = Messenger(service)
            workerBinding = true
            workerBound = true
            emitLog("INFO", "BOOT", "Runtime worker connected")
            pendingSurface?.let {
                sendSurfaceToWorker(it)
            }
            pendingWorkerStart?.let {
                sendStartToWorker(it)
                pendingWorkerStart = null
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            workerBinding = false
            workerBound = false
            workerMessenger = null
            emitLog("WARN", "BOOT", "Runtime worker disconnected")
        }

        override fun onBindingDied(name: ComponentName?) {
            workerBinding = false
            workerBound = false
            workerMessenger = null
            emitLog("ERROR", "BOOT", "Runtime worker binding died")
        }
    }

    private fun bindWorker() {
        if (workerBound || workerBinding) return
        val intent = Intent(this, RuntimeWorkerService::class.java)
        val ok = bindService(intent, workerConnection, BIND_AUTO_CREATE)
        workerBinding = ok
        if (!ok) {
            emitLog("ERROR", "BOOT", "bindService to RuntimeWorkerService returned false")
        }
    }

    private fun unbindWorkerSafe() {
        if (!workerBound && !workerBinding) return
        runCatching { unbindService(workerConnection) }
        workerBinding = false
        workerBound = false
        workerMessenger = null
    }

    private fun sendStartToWorker(request: LaunchRequest) {
        val target = workerMessenger
        if (target == null) {
            pendingWorkerStart = request
            bindWorker()
            emitLog("WARN", "BOOT", "Worker not bound yet, launch queued")
            return
        }

        val out = Message.obtain(
            null,
            RuntimeIpc.MSG_WORKER_START_RUNTIME
        ).apply {
            replyTo = workerReplyMessenger
            data = Bundle().apply {
                putString(
                    RuntimeIpc.KEY_EXECUTABLE,
                    request.executablePath
                )
                putString(
                    RuntimeIpc.KEY_ROOTFS_PATH,
                    request.rootfsPath
                )
                putString(
                    RuntimeIpc.KEY_THUNK_HOST_PATH,
                    request.thunkHostPath
                )
                putString(
                    RuntimeIpc.KEY_THUNK_GUEST_PATH,
                    request.thunkGuestPath
                )
                putString(
                    RuntimeIpc.KEY_WORKING_DIRECTORY,
                    request.workingDirectory
                )
                putString(
                    RuntimeIpc.KEY_ARTIFACT_DIRECTORY,
                    request.artifactDirectory
                )
                putStringArrayList(
                    RuntimeIpc.KEY_LAUNCH_ENV,
                    ArrayList(request.launchEnv)
                )
                putStringArrayList(
                    RuntimeIpc.KEY_LAUNCH_ARGS,
                    ArrayList(request.launchArgs)
                )
            }
        }

        runCatching { target.send(out) }.onFailure {
            emitLog("ERROR", "BOOT", "Failed to send start request to worker")
            sendStartResult(-301)
        }
    }

    private fun sendStopToWorker() {
        val target = workerMessenger ?: return
        val out = Message.obtain(
            null,
            RuntimeIpc.MSG_WORKER_STOP_RUNTIME
        ).apply {
            replyTo = workerReplyMessenger
        }
        runCatching { target.send(out) }
    }

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
                        fieldsJson = """{"runtimePid":"${Process.myPid()}","runtimeProcess":"${android.app.Application.getProcessName()}"}"""
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
                        launchEnv =
                            d.getStringArrayList(RuntimeIpc.KEY_LAUNCH_ENV)?.toList().orEmpty(),
                        launchArgs =
                            d.getStringArrayList(RuntimeIpc.KEY_LAUNCH_ARGS)?.toList().orEmpty(),
                    )
                    sendStartToWorker(request)
                }

                RuntimeIpc.MSG_SURFACE_CREATED -> {
                    val surface = getSurfaceCompat(msg.data)
                    pendingSurface?.release()
                    pendingSurface = surface
                    emitLog("INFO", "SURFACE", "Surface created in UI process")
                    sendSurfaceToWorker(surface)
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
                    val current = pendingSurface
                    if (current != null && current.isValid) {
                        sendSurfaceToWorker(current)
                    } else {
                        emitLog(
                            "WARN",
                            "SURFACE",
                            "Surface change arrived but no valid pending surface"
                        )
                        // TODO: RuntimeBridge.onRuntimeSurfaceChanged(width, height)
                    }
                }

                RuntimeIpc.MSG_SURFACE_DESTROYED -> {
                    emitLog("INFO", "SURFACE", "Surface destroyed in UI process")
                    sendSurfaceToWorker(null)
                    pendingSurface?.release()
                    pendingSurface = null
                }

                RuntimeIpc.MSG_STOP_RUNTIME -> {
                    sendStopToWorker()
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
        bindWorker()
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
        sendStopToWorker()
        unbindWorkerSafe()
        pendingSurface?.release()
        pendingSurface = null
        super.onDestroy()
    }
}