package com.critical.vexaemulator

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.ActivityInfo
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.Process
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.lifecycleScope
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.LaunchRequest
import com.critical.vexaemulator.runtime.RuntimeIpc
import com.critical.vexaemulator.runtime.RuntimeService
import com.critical.vexaemulator.ui.GameDebugPanel
import com.critical.vexaemulator.ui.GameLoadingOverlay
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val WORKER_PROCESS = "com.critical.vexaemulator:runtime_worker"

class GameActivity : ComponentActivity(), SurfaceHolder.Callback {
    private lateinit var gameSurfaceView: SurfaceView

    private var serviceMessenger: Messenger? = null
    private var runtimeBound = false
    private var pendingLaunchRequest: LaunchRequest? = null
    private var lastReportedRuntimeExitKey: String? = null
    private var uiIncomingHandler = object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            when (msg.what) {
                RuntimeIpc.MSG_START_RESULT -> {
                    val code =
                        msg.data.getInt(RuntimeIpc.KEY_CODE, -1)
                    if (code != 0) {
                        VexaLogger.log(
                            level = LogLevel.ERROR,
                            category = LogCategory.FAILURE,
                            message = "Runtime service start failed",
                            fields = mapOf(
                                "code" to code.toString()
                            )
                        )
                    }
                }

                RuntimeIpc.MSG_LOG_EVENT -> {
                    if (msg.what == RuntimeIpc.MSG_LOG_EVENT) {
                        val category = msg.data.getString(RuntimeIpc.KEY_CATEGORY).orEmpty()
                        val message = msg.data.getString(RuntimeIpc.KEY_MESSAGE).orEmpty()
                        val fieldsJson = msg.data.getString(RuntimeIpc.KEY_FIELDS_JSON).orEmpty()

                        if (category == "BOOT" && message.contains("RuntimeService created")) {
                            runCatching {
                                val obj = org.json.JSONObject(fieldsJson)
                                val pid = obj.optString("runtimePid").toIntOrNull()
                                if (pid != null) lastRuntimePid = pid
                            }
                        }
                        RuntimeBridge.logFromNative(
                            level = msg.data.getString(RuntimeIpc.KEY_LEVEL).orEmpty(),
                            category = msg.data.getString(RuntimeIpc.KEY_CATEGORY).orEmpty(),
                            message = msg.data.getString(RuntimeIpc.KEY_MESSAGE).orEmpty(),
                            fieldsJson = msg.data.getString(RuntimeIpc.KEY_FIELDS_JSON).orEmpty(),
                        )
                    }
                }

                else -> super.handleMessage(msg)
            }
        }
    }

    private val uiMessenger = Messenger(uiIncomingHandler)

    private var showLoading by mutableStateOf(true)
    private var showDebugMenu by mutableStateOf(true) // For now, it's true; will be changed
    private var hideFilters by mutableStateOf(true)
    private var lastRuntimePid: Int? = null
    private var runtimeConnectedAtMs: Long = 0L
    private var lastReportedExitKey: String? = null

    private fun sendRegister() {
        val msg = Message.obtain(null, RuntimeIpc.MSG_REGISTER_CLIENT).apply {
            replyTo = uiMessenger
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun sendUnregister() {
        val msg = Message.obtain(null, RuntimeIpc.MSG_UNREGISTER_CLIENT).apply {
            replyTo = uiMessenger
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun logLatestRuntimeExitInfo(tag: String) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return

        val am = getSystemService(ActivityManager::class.java) ?: return
        val pid = lastRuntimePid ?: 0
        val exits = runCatching {
            am.getHistoricalProcessExitReasons(
                packageName,
                pid,
                20
            )
        }.getOrElse { e ->
            VexaLogger.log(
                LogLevel.ERROR,
                LogCategory.FAILURE,
                "ExitInfo query failed",
                mapOf(
                    "tag" to tag,
                    "pidFilter" to pid.toString(),
                    "error" to (e.message ?: e.javaClass.simpleName)
                )
            )
            return
        }

        val latestRuntimeProcess = exits.firstOrNull {
            it.processName == WORKER_PROCESS &&
                    it.timestamp >= runtimeConnectedAtMs &&
                    (it.reason == ApplicationExitInfo.REASON_SIGNALED ||
                            it.reason == ApplicationExitInfo.REASON_CRASH ||
                            it.reason == ApplicationExitInfo.REASON_CRASH_NATIVE)
        } ?: run {
            VexaLogger.log(
                LogLevel.WARN,
                LogCategory.FAILURE,
                "No runtime exit info yet",
                mapOf(
                    "tag" to tag,
                    "pidFilter" to pid.toString(),
                    "connectedAtMs" to runtimeConnectedAtMs.toString(),
                    "totalExits" to exits.size.toString()
                )
            )
            return
        }

        val exitKey =
            "${latestRuntimeProcess.pid}:${latestRuntimeProcess.timestamp}:${latestRuntimeProcess.reason}:${latestRuntimeProcess.status}"
        if (exitKey == lastReportedRuntimeExitKey) {
            return
        }
        lastReportedRuntimeExitKey = exitKey

        val reasonText = when (latestRuntimeProcess.reason) {
            ApplicationExitInfo.REASON_SIGNALED -> "SIGNALED"
            ApplicationExitInfo.REASON_CRASH -> "CRASH"
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "CRASH_NATIVE"
            ApplicationExitInfo.REASON_LOW_MEMORY -> "LOW_MEMORY"
            ApplicationExitInfo.REASON_ANR -> "ANR"
            else -> latestRuntimeProcess.reason.toString()
        }

        val traceSnippet = runCatching {
            latestRuntimeProcess.traceInputStream?.bufferedReader()?.use {
                it.readText().take(1200)
            }
        }.getOrNull().orEmpty()

        if (latestRuntimeProcess.status.toString() == "0") {
            return
        }

        VexaLogger.log(
            level = LogLevel.ERROR,
            category = LogCategory.FAILURE,
            message = "Runtime process exit captured",
            fields = mapOf(
                "tag" to tag,
                "pid" to latestRuntimeProcess.pid.toString(),
                "process" to (latestRuntimeProcess.processName ?: ""),
                "reason" to latestRuntimeProcess.reason.toString(),
                "status" to latestRuntimeProcess.status.toString(),
                "importance" to latestRuntimeProcess.importance.toString(),
                "timestamp" to latestRuntimeProcess.timestamp.toString(),
                "description" to (latestRuntimeProcess.description ?: "")
            )
        )
    }

    private fun sendStartRuntime(request: LaunchRequest) {
        val msg = Message.obtain(null, RuntimeIpc.MSG_START_RUNTIME).apply {
            replyTo = uiMessenger
            data = Bundle().apply {
                putString(RuntimeIpc.KEY_EXECUTABLE, request.executablePath)
                putString(RuntimeIpc.KEY_ROOTFS_PATH, request.rootfsPath)
                putString(RuntimeIpc.KEY_THUNK_HOST_PATH, request.thunkHostPath)
                putString(RuntimeIpc.KEY_THUNK_GUEST_PATH, request.thunkGuestPath)
                putString(RuntimeIpc.KEY_WORKING_DIRECTORY, request.workingDirectory)
                putString(RuntimeIpc.KEY_ARTIFACT_DIRECTORY, request.artifactDirectory)
            }
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun sendStopRuntime() {
        val msg = Message.obtain(
            null,
            RuntimeIpc.MSG_STOP_RUNTIME
        ).apply {
            replyTo = uiMessenger
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun sendSurfaceCreated() {
        val msg = Message.obtain(
            null,
            RuntimeIpc.MSG_SURFACE_CREATED
        ).apply {
            replyTo = uiMessenger
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun sendSurfaceChanged(surfaceFormat: Int, surfaceWidth: Int, surfaceHeight: Int) {
        val msg = Message.obtain(
            null,
            RuntimeIpc.MSG_SURFACE_CHANGED
        ).apply {
            replyTo = uiMessenger
            data = Bundle().apply {
                putInt(RuntimeIpc.KEY_SURFACE_FORMAT, surfaceFormat)
                putInt(RuntimeIpc.KEY_SURFACE_WIDTH, surfaceWidth)
                putInt(RuntimeIpc.KEY_SURFACE_HEIGHT, surfaceHeight)
            }
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private fun sendSurfaceDestroyed() {
        val msg = Message.obtain(
            null,
            RuntimeIpc.MSG_SURFACE_DESTROYED
        ).apply {
            replyTo = uiMessenger
        }
        runCatching {
            serviceMessenger?.send(msg)
        }
    }

    private val runtimeConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            serviceMessenger = Messenger(service)
            runtimeBound = true
            runtimeConnectedAtMs = System.currentTimeMillis()
            sendRegister()

            pendingLaunchRequest?.let {
                sendStartRuntime(it)
                pendingLaunchRequest = null
            }

            VexaLogger.log(
                level = LogLevel.INFO,
                category = LogCategory.BOOT,
                message = "Runtime service connected",
                fields = mapOf(
                    "uiPid" to Process.myPid().toString(),
                    "uiProcess" to android.app.Application.getProcessName()
                )
            )
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            runtimeBound = false
            serviceMessenger = null
            VexaLogger.log(
                level = LogLevel.WARN,
                category = LogCategory.BOOT,
                message = "Runtime service disconnected"
            )
            Handler(Looper.getMainLooper()).post {
                logLatestRuntimeExitInfo("t0")
            }
            Handler(Looper.getMainLooper()).postDelayed({
                logLatestRuntimeExitInfo("t+500ms")
            }, 500)
            Handler(Looper.getMainLooper()).postDelayed({
                logLatestRuntimeExitInfo("t+1500ms")
            }, 1500)
        }

        override fun onBindingDied(name: ComponentName?) {
            runtimeBound = false
            serviceMessenger = null
            VexaLogger.log(
                LogLevel.ERROR,
                LogCategory.BOOT,
                "Runtime service binding died"
            )

            Handler(Looper.getMainLooper()).post {
                logLatestRuntimeExitInfo("binding_died_t0")
            }
            Handler(Looper.getMainLooper()).postDelayed({
                logLatestRuntimeExitInfo("binding_died_t+500ms")
            }, 500)
            super.onBindingDied(name)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.ACTIVITY,
            message = "GameActivity onCreate started",
            fields = mapOf(
                "uiPid" to Process.myPid().toString(),
                "uiProcess" to android.app.Application.getProcessName()
            )
        )

        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        hideSystemBars()
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.ACTIVITY,
            message = "GameActivity window configured",
            fields = mapOf(
                "orientation" to "sensorLandscape",
                "keepScreenOn" to "true"
            )
        )

        gameSurfaceView = SurfaceView(this).also {
            it.setBackgroundColor(android.graphics.Color.BLACK)
            it.holder.addCallback(this)
        }
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "SurfaceView object created",
            fields = mapOf(
                "viewHash" to gameSurfaceView.hashCode().toString()
            )
        )

        setContent {
            GameScreen(
                surfaceView = gameSurfaceView,
                showLoading = showLoading,
                showDebugMenu = showDebugMenu,
                onToggleDebugMenu = { showDebugMenu = !showDebugMenu },
                hideFilters = hideFilters,
                onHideFilters = { hideFilters = !hideFilters }
            )
        }
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "SurfaceView object attached"
        )
        lifecycleScope.launch {
            delay(5000)

            showLoading = true // TODO: change this to false after setting up everything
            VexaLogger.log(
                level = LogLevel.INFO,
                category = LogCategory.UI,
                message = "Loading overlay hidden",
                fields = mapOf(
                    "showLoading" to showLoading.toString(),
                    "reason" to "timeout_5s"
                )
            )
        }
    }

    override fun onStart() {
        super.onStart()
        val intent = Intent(this, RuntimeService::class.java)
        bindService(intent, runtimeConnection, Context.BIND_AUTO_CREATE)
    }

    override fun onResume() {
        super.onResume()
        hideSystemBars()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface is being created..."
        )
        val request = LaunchRequest(
            executablePath = "/data/user/0/com.critical.vexaemulator/files/game/HytaleClient",
            rootfsPath = "/data/user/0/com.critical.vexaemulator/files/rootfs",
            thunkHostPath = "/data/user/0/com.critical.vexaemulator/files/thunks/host",
            thunkGuestPath = "/data/user/0/com.critical.vexaemulator/files/thunks/guest",
            workingDirectory = "/data/user/0/com.critical.vexaemulator/files/",
            artifactDirectory = "/data/user/0/com.critical.vexaemulator/files/artifacts/"
        )
        if (!runtimeBound || serviceMessenger == null) {
            pendingLaunchRequest = request
            VexaLogger.log(
                level = LogLevel.WARN,
                category = LogCategory.BOOT,
                message = "Runtime service not bound yet; launch queued",
                fields = mapOf(
                    "code" to "-100"
                )
            )
            return
        }
        sendStartRuntime(request)
        // TODO: Add a retry mechanism or exit later
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface successfully created"
        )
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface has changed:",
            fields = mapOf(
                "format" to format.toString(),
                "width" to width.toString(),
                "height" to height.toString()
            )
        )
        // RuntimeBridge.onRuntimeSurfaceChanged(width, height) replace this with actual sendSurfaceChanged() later from RuntimeService.kt or make the game control it.
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface destroy callback received...",
            fields = mapOf(
                "surfaceHash" to holder.surface.hashCode().toString(),
                "isValid" to holder.surface.isValid.toString()
            )
        )
        sendStopRuntime()
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface destroy after surface teardown by request",
            fields = mapOf(
                "surfaceHash" to holder.surface.hashCode().toString(),
                "isValid" to holder.surface.isValid.toString()
            )
        )
    }

    override fun onStop() {
        if (runtimeBound) {
            sendUnregister()
            unbindService(runtimeConnection)
            runtimeBound = false
        }
        super.onStop()
    }

    private fun hideSystemBars() {
        WindowCompat.setDecorFitsSystemWindows(window, false)

        val controller = WindowCompat.getInsetsController(window, window.decorView)
            ?: return
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        controller.hide(WindowInsetsCompat.Type.systemBars())
    }
}

@Composable
private fun GameScreen(
    surfaceView: SurfaceView,
    showLoading: Boolean,
    showDebugMenu: Boolean,
    onToggleDebugMenu: () -> Unit,
    hideFilters: Boolean,
    onHideFilters: () -> Unit
) {
    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView( // Shows actual game content
            factory = { surfaceView },
            modifier = Modifier.fillMaxSize()
        )

        AnimatedVisibility(
            // Fade in/out animations for showing/hiding overlay
            visible = showLoading,
            enter = fadeIn(animationSpec = tween(durationMillis = 500)),
            exit = fadeOut(animationSpec = tween(durationMillis = 500)),
        ) {
            GameLoadingOverlay(
                isDebugMenuOpen = showDebugMenu,
                onToggleDebugMenu = onToggleDebugMenu,
                onHideFilters = onHideFilters
            )
        }

        if (showDebugMenu) {
            GameDebugPanel(
                onClose = onToggleDebugMenu,
                onHideFilters = onHideFilters,
                hideFilters = hideFilters
            )
        }
    }
}