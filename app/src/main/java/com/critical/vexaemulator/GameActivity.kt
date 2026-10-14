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
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import java.nio.ByteBuffer
import java.nio.ByteOrder
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.lifecycleScope
import com.critical.vexaemulator.input.GuestKeyboard
import com.critical.vexaemulator.input.GuestPadOverlay
import com.critical.vexaemulator.input.GuestPadState
import com.critical.vexaemulator.input.PadCtl
import com.critical.vexaemulator.input.PadSink
import java.io.File as JFile
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.LaunchRequest
import com.critical.vexaemulator.runtime.RuntimeIpc
import com.critical.vexaemulator.runtime.RuntimeService
import com.critical.vexaemulator.steam.SteamLink
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.ui.EdgeHandle
import com.critical.vexaemulator.ui.GameDebugPanel
import com.critical.vexaemulator.ui.CloudConflictDialog
import com.critical.vexaemulator.ui.GameLoadingOverlay
import com.critical.vexaemulator.ui.QuickPanel
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.rememberHaptics
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.VoidColors
import com.critical.vexaemulator.ui.theme.resolveDark
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private const val WORKER_PROCESS = "com.critical.vexaemulator:runtime_worker"
private const val LOGCAT_TAG_RUNTIME_EXIT = "Vexa-RuntimeExit"

class GameActivity : ComponentActivity(), SurfaceHolder.Callback {
    companion object {
        const val EXTRA_PROGRAM_ID = "programId"
    }
    private lateinit var gameSurfaceView: SurfaceView
    private var isExiting = false

    private var latestSurface: Surface? = null
    private var serviceMessenger: Messenger? = null
    private var runtimeBound = false
    private var pendingLaunchRequest: LaunchRequest? = null
    private var guestLaunchStarted = false
    private val pad = GuestPadState()
    private var lastReportedRuntimeExitKey: String? = null
    private var runtimeStarted = false
    private var surfaceWidth = 0
    private var surfaceHeight = 0
    private var guestRootfs: String? = null
    private var pointerDown = false
    private var lastPointerX = -1
    private var lastPointerY = -1
    private var lastPointerAtMs = 0L
    private var loggedTouch = false
    private val pointerRecord = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)

    private fun closeGameActivity() {
        if (isExiting) return

        sendSurfaceDestroyed()
        sendUnregister()
        sendStopRuntime()

        if (runtimeBound) {
            runCatching {
                unbindService(runtimeConnection)
            }
            runtimeBound = false
        }

        finish()
        overridePendingTransition(0, 0)
    }

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
                            message = "Runtime service start failed ($code)",
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

                        if (category == "BOOT" &&
                            (message.contains("Runtime worker disconnected") ||
                                    message.contains("Runtime worker binding died"))
                        ) {
                            showLoading = false
                            Handler(Looper.getMainLooper()).post {
                                logLatestRuntimeExitInfo("worker_event_t0")
                            }
                            Handler(Looper.getMainLooper()).postDelayed({
                                logLatestRuntimeExitInfo("worker_event_t+500ms")
                            }, 500)
                            Handler(Looper.getMainLooper()).postDelayed({
                                logLatestRuntimeExitInfo("worker_event_t+1500ms")
                            }, 1500)
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
    private var showDebugMenu by mutableStateOf(false)
    private var hideFilters by mutableStateOf(true)
    private var lastRuntimePid by mutableStateOf<Int?>(null)

    // Boot checklist flags, in the order the loading screen lists them.
    private var bootInstalled by mutableStateOf(false)
    private var bootConnected by mutableStateOf(false)
    private var bootLandscape by mutableStateOf(false)
    private var bootPublished by mutableStateOf(false)
    private var bootStarted by mutableStateOf(false)
    private var bootCloudDone by mutableStateOf(false)
    private var cloudDetail by mutableStateOf<String?>(null)
    private var cloudConflict by mutableStateOf<List<com.critical.vexaemulator.steam.CloudConflict>?>(null)
    private var cloudChoice = kotlinx.coroutines.CompletableDeferred<com.critical.vexaemulator.steam.CloudChoice>()
    private val bootStep: Int
        get() = listOfNotNull(
            bootInstalled,
            bootCloudDone.takeIf { cloudDetail != null },
            bootConnected, bootLandscape, bootPublished, bootStarted,
        ).takeWhile { it }.size
    private var surfaceSizeLabel by mutableStateOf<String?>(null)
    private val sessionStartMs = System.currentTimeMillis()
    private var runtimeConnectedAtMs: Long = 0L
    private var lastReportedExitKey: String? = null


    private fun tryLaunchWithSurface(request: LaunchRequest): Boolean {
        if (runtimeStarted) return true
        val surface = latestSurface
        if (surface == null || !surface.isValid) return false
        // Portrait sizes are the activity before sensorLandscape applies. Publishing
        // one would lock the guest to the wrong resolution.
        if (surfaceWidth <= 1 || surfaceHeight <= 1 || surfaceHeight > surfaceWidth) {
            VexaLogger.log(
                LogLevel.INFO,
                LogCategory.SURFACE,
                "Waiting for a landscape surface",
                mapOf("width" to surfaceWidth.toString(), "height" to surfaceHeight.toString())
            )
            return false
        }
        bootLandscape = true
        surfaceSizeLabel = "$surfaceWidth×$surfaceHeight"
        guestRootfs = request.rootfsPath
        if (!publishGuestWindowSize(request.rootfsPath, surfaceWidth, surfaceHeight)) return false
        bootPublished = true
        runtimeStarted = true
        sendSurfaceCreated(surface)
        sendStartRuntime(request)
        bootStarted = true
        // The full-screen overlay sits on top of the guest surface and costs composition time every frame;
        // keep it just long enough for the last checklist tick to land.
        lifecycleScope.launch {
            delay(450)
            showLoading = false
        }
        return true
    }

    private fun publishGuestWindowSize(rootfs: String, width: Int, height: Int): Boolean {
        return try {
            val dir = java.io.File(rootfs, "tmp")
            if (!dir.isDirectory && !dir.mkdirs()) return false
            java.io.File(dir, "vexa-window-size").writeText("$width $height\n")
            java.io.File(dir, "vexa-pointer").writeBytes(ByteArray(0))
            java.io.File(dir, "vexa-present").writeText("0 0 $width $height 0 0 $width $height $width $height\n")
            VexaLogger.log(
                LogLevel.INFO,
                LogCategory.SURFACE,
                "Published guest window size",
                mapOf("width" to width.toString(), "height" to height.toString())
            )
            true
        } catch (error: Exception) {
            VexaLogger.log(
                LogLevel.ERROR,
                LogCategory.FAILURE,
                "Could not publish guest window size",
                mapOf("error" to (error.message ?: error.javaClass.simpleName))
            )
            false
        }
    }

    // 16-byte little-endian records: type, x, y, state. X11 types are
    // ButtonPress=4, ButtonRelease=5, MotionNotify=6. Button1Mask is 256.
    private fun writePointer(type: Int, x: Int, y: Int, state: Int) {
        val root = guestRootfs ?: return
        val file = java.io.File(root, "tmp/vexa-pointer")
        try {
            file.parentFile?.mkdirs()
            if (file.length() > 1_000_000L) {
                file.writeBytes(ByteArray(0))
            }
            pointerRecord.clear()
            pointerRecord.putInt(type)
            pointerRecord.putInt(x)
            pointerRecord.putInt(y)
            pointerRecord.putInt(state)
            java.io.FileOutputStream(file, true).use { it.write(pointerRecord.array()) }
            if (!loggedTouch) {
                loggedTouch = true
                VexaLogger.log(
                    LogLevel.INFO,
                    LogCategory.SURFACE,
                    "Game touch",
                    mapOf("type" to type.toString(), "x" to x.toString(), "y" to y.toString())
                )
            }
        } catch (_: Exception) {
        }
    }

    private fun handleGameTouch(event: MotionEvent) {
        if (pad.overlay) return
        val maxX = (surfaceWidth - 1).coerceAtLeast(0)
        val maxY = (surfaceHeight - 1).coerceAtLeast(0)
        val x = event.x.toInt().coerceIn(0, maxX)
        val y = event.y.toInt().coerceIn(0, maxY)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                pointerDown = true
                writePointer(6, x, y, 0)
                writePointer(4, x, y, 0)
                lastPointerX = x
                lastPointerY = y
                lastPointerAtMs = SystemClock.uptimeMillis()
            }
            MotionEvent.ACTION_MOVE -> {
                if (!pointerDown) return
                val now = SystemClock.uptimeMillis()
                val moved = kotlin.math.abs(x - lastPointerX) >= 4 || kotlin.math.abs(y - lastPointerY) >= 4
                if (!moved && now - lastPointerAtMs < 8L) return
                writePointer(6, x, y, 256)
                lastPointerX = x
                lastPointerY = y
                lastPointerAtMs = now
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (!pointerDown) return
                pointerDown = false
                writePointer(6, x, y, 256)
                writePointer(5, x, y, 256)
            }
        }
    }

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
        // Query all pids
        val pid = 0
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
                    "runtimePidHint" to (lastRuntimePid?.toString() ?: ""),
                    "error" to (e.message ?: e.javaClass.simpleName)
                )
            )
            return
        }

        val latestRuntimeProcess = exits
            .asSequence()
            .filter {
                it.processName == WORKER_PROCESS &&
                        it.timestamp >= runtimeConnectedAtMs
            }
            .maxByOrNull { it.timestamp } ?: run {
            VexaLogger.log(
                LogLevel.WARN,
                LogCategory.FAILURE,
                "No runtime exit info yet",
                mapOf(
                    "tag" to tag,
                    "pidFilter" to pid.toString(),
                    "runtimePidHint" to (lastRuntimePid?.toString() ?: ""),
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
            ApplicationExitInfo.REASON_EXIT_SELF -> "EXIT_SELF"
            ApplicationExitInfo.REASON_SIGNALED -> "SIGNALED"
            ApplicationExitInfo.REASON_LOW_MEMORY -> "LOW_MEMORY"
            ApplicationExitInfo.REASON_CRASH -> "CRASH"
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "CRASH_NATIVE"
            ApplicationExitInfo.REASON_ANR -> "ANR"
            ApplicationExitInfo.REASON_INITIALIZATION_FAILURE -> "INITIALIZATION_FAILURE"
            ApplicationExitInfo.REASON_PERMISSION_CHANGE -> "PERMISSION_CHANGE"
            ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "EXCESSIVE_RESOURCE_USAGE"
            ApplicationExitInfo.REASON_USER_REQUESTED -> "USER_REQUESTED"
            ApplicationExitInfo.REASON_USER_STOPPED -> "USER_STOPPED"
            ApplicationExitInfo.REASON_DEPENDENCY_DIED -> "DEPENDENCY_DIED"
            ApplicationExitInfo.REASON_OTHER -> "OTHER"
            // Some platform builds report additional reason codes not exposed in
            // this compileSdk's ApplicationExitInfo constants.
            14 -> "EXIT_OTHER"
            else -> latestRuntimeProcess.reason.toString()
        }

        val traceSnippet = runCatching {
            latestRuntimeProcess.traceInputStream?.bufferedReader()?.use {
                it.readText().take(1200)
            }
        }.getOrNull().orEmpty()

        val severity = when (latestRuntimeProcess.reason) {
            ApplicationExitInfo.REASON_SIGNALED,
            ApplicationExitInfo.REASON_CRASH,
            ApplicationExitInfo.REASON_CRASH_NATIVE,
            ApplicationExitInfo.REASON_ANR,
            ApplicationExitInfo.REASON_DEPENDENCY_DIED -> LogLevel.ERROR

            else -> LogLevel.WARN
        }

        val exitFields = mapOf(
            "tag" to tag,
            "pid" to latestRuntimeProcess.pid.toString(),
            "process" to (latestRuntimeProcess.processName ?: ""),
            "reason" to latestRuntimeProcess.reason.toString(),
            "reasonText" to reasonText,
            "status" to latestRuntimeProcess.status.toString(),
            "importance" to latestRuntimeProcess.importance.toString(),
            "timestamp" to latestRuntimeProcess.timestamp.toString(),
            "description" to (latestRuntimeProcess.description ?: ""),
            "traceSnippet" to traceSnippet
        )

        VexaLogger.log(
            level = severity,
            category = LogCategory.FAILURE,
            message = "Runtime process exit captured",
            fields = exitFields
        )
        val logcatPayload = "Runtime process exit captured $exitFields"
        when (severity) {
            LogLevel.ERROR -> android.util.Log.e(LOGCAT_TAG_RUNTIME_EXIT, logcatPayload)
            LogLevel.WARN -> android.util.Log.w(LOGCAT_TAG_RUNTIME_EXIT, logcatPayload)
            LogLevel.DEBUG -> android.util.Log.d(LOGCAT_TAG_RUNTIME_EXIT, logcatPayload)
            else -> android.util.Log.i(LOGCAT_TAG_RUNTIME_EXIT, logcatPayload)
        }
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
                putStringArrayList(RuntimeIpc.KEY_LAUNCH_ENV, ArrayList(request.launchEnv))
                putStringArrayList(RuntimeIpc.KEY_LAUNCH_ARGS, ArrayList(request.launchArgs))
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

    private fun sendSurfaceCreated(surface: Surface) {
        val msg = Message.obtain(
            null,
            RuntimeIpc.MSG_SURFACE_CREATED
        ).apply {
            replyTo = uiMessenger
            data = Bundle().apply {
                putParcelable(RuntimeIpc.KEY_SURFACE, surface)
            }
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
            bootConnected = true
            sendRegister()

            pendingLaunchRequest?.let { req ->
                if (tryLaunchWithSurface(req)) {
                    pendingLaunchRequest = null
                }
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
        VexaPrefs.init(this)
        startForegroundService(Intent(this, RuntimeService::class.java).apply {
            action = "com.critical.vexaemulator.HEADLESS_START"
        })

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                VexaLogger.log(
                    LogLevel.INFO,
                    LogCategory.ACTIVITY,
                    "User initiated back press",
                    mapOf("source" to "onBackPressedDispatcher")
                )
                closeGameActivity()
            }
        })
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.ACTIVITY,
            message = "GameActivity onCreate started",
            fields = mapOf(
                "uiPid" to Process.myPid().toString(),
                "uiProcess" to android.app.Application.getProcessName()
            )
        )

        pad.onFaceKey = { down, keysym -> writePointer(if (down) 2 else 3, keysym, 0, PadCtl.SYNTH_FLAG) }
        pad.onPadButton = { down, control -> writePointer(7, control, if (down) 1 else 0, 0) }
        lifecycleScope.launch {
            while (true) {
                pad.pump(surfaceWidth, surfaceHeight, padSink)
                delay(32)
            }
        }
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        requestHighestRefreshRate()
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

        gameSurfaceView = SurfaceView(this).also { sv ->
            sv.setZOrderOnTop(false)
            sv.holder.addCallback(this)
            // Opaque lets SurfaceFlinger scan the guest buffer out directly instead of blending it every frame.
            sv.holder.setFormat(android.graphics.PixelFormat.OPAQUE)
            sv.isClickable = true
            sv.isFocusable = true
            sv.setOnTouchListener { _, event ->
                handleGameTouch(event)
                true
            }
        }
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "SurfaceView object created",
            fields = mapOf(
                "viewHash" to gameSurfaceView.hashCode().toString()
            )
        )

        val program = com.critical.vexaemulator.guest.GuestCatalog.find(
            intent.getStringExtra(EXTRA_PROGRAM_ID).orEmpty()
        )
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark(), animate = true) {
                GameScreen(
                    surfaceView = gameSurfaceView,
                    program = program,
                    runtimePid = lastRuntimePid,
                    sessionStartMs = sessionStartMs,
                    bootStep = bootStep,
                    cloudDetail = cloudDetail,
                    cloudConflict = cloudConflict,
                    onCloudChoice = { picked ->
                        cloudChoice.complete(picked)
                        cloudConflict = null
                    },
                    surfaceSize = surfaceSizeLabel,
                    showLoading = showLoading,
                    showDebugMenu = showDebugMenu,
                    onToggleDebugMenu = { showDebugMenu = !showDebugMenu },
                    hideFilters = hideFilters,
                    onHideFilters = { hideFilters = !hideFilters },
                    onBack = { closeGameActivity() },
                    pad = pad,
                )
            }
        }
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "SurfaceView object attached"
        )
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (pad.onMotion(event)) return true
        return super.dispatchGenericMotionEvent(event)
    }

    private val padSink = object : PadSink {
        override fun pointer(type: Int, x: Int, y: Int, state: Int) =
            writePointer(type, x, y, state or PadCtl.SYNTH_FLAG)
        override fun key(down: Boolean, keysym: Int) =
            writePointer(if (down) 2 else 3, keysym, 0, PadCtl.SYNTH_FLAG)
        override fun pad(control: Int, value: Int) =
            writePointer(7, control, value, 0)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (pad.onKey(event, padSink)) {
            return true
        }
        if (forwardKeyboardEvent(event)) {
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    private fun forwardKeyboardEvent(event: KeyEvent): Boolean {
        if (!GuestKeyboard.isKeyboardSource(event)) return false
        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount > 0) return true
        val keysym = GuestKeyboard.keysym(event.keyCode)
        if (keysym == 0) return false
        writePointer(if (event.action == KeyEvent.ACTION_DOWN) 2 else 3, keysym, 0, 0)
        return true
    }

    // Guests present through eglSwapBuffers, so the display rate caps their frame rate.
    private fun requestHighestRefreshRate() {
        val display = display ?: return
        val current = display.mode
        val best = display.supportedModes
            .filter { it.physicalWidth == current.physicalWidth && it.physicalHeight == current.physicalHeight }
            .maxByOrNull { it.refreshRate } ?: return
        window.attributes = window.attributes.apply {
            preferredDisplayModeId = best.modeId
            preferredRefreshRate = best.refreshRate
        }
        VexaLogger.log(
            LogLevel.INFO,
            LogCategory.ACTIVITY,
            "Requested display mode",
            mapOf("modeId" to best.modeId.toString(), "refreshRate" to best.refreshRate.toString())
        )
    }

    override fun onStart() {
        super.onStart()
        if (!runtimeBound) {
            val intent = Intent(this, RuntimeService::class.java)
            bindService(intent, runtimeConnection, Context.BIND_AUTO_CREATE)
        }
    }

    override fun onResume() {
        super.onResume()
        hideSystemBars()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        latestSurface = holder.surface
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.SURFACE,
            message = "Game Surface is being created..."
        )
        // Rotation (landscape <-> reverse landscape) and returning from background
        // recreate the Surface; hand the new one to the running guest.
        if (runtimeStarted) {
            sendSurfaceCreated(holder.surface)
            return
        }
        val program = com.critical.vexaemulator.guest.GuestCatalog.find(
            intent.getStringExtra(EXTRA_PROGRAM_ID).orEmpty()
        )
        if (program == null) {
            VexaLogger.log(
                LogLevel.ERROR,
                LogCategory.FAILURE,
                "No program selected"
            )
            return
        }
        if (guestLaunchStarted) return
        guestLaunchStarted = true
        lifecycleScope.launch {
            val steamAppId = program.env.firstOrNull { it.startsWith("SteamAppId=") }
                ?.substringAfter("=")
                .orEmpty()
            if (steamAppId.isNotEmpty()) {
                cloudDetail = "signing in to Steam"
                VexaLogger.log(LogLevel.INFO, LogCategory.BOOT, "Reading Steam session")
                val session = withContext(Dispatchers.IO) { SteamLink.ensureOnline(this@GameActivity) }
                if (session == null) {
                    VexaLogger.log(
                        LogLevel.WARN,
                        LogCategory.BOOT,
                        "Steam session file is missing or the account did not connect",
                    )
                }
            }
            val request = try {
                com.critical.vexaemulator.guest.GuestInstaller.install(this@GameActivity, program)
            } catch (t: Throwable) {
                VexaLogger.log(
                    level = LogLevel.ERROR,
                    category = LogCategory.FAILURE,
                    message = "Guest install failed: ${t.message}",
                    fields = mapOf("id" to program.id)
                )
                finish()
                return@launch
            }
            bootInstalled = true
            val cloudAppId = steamAppId.toIntOrNull()
            if (cloudAppId != null) {
                val home = request.launchEnv.firstOrNull { it.startsWith("HOME=") }
                    ?.substringAfter("=")?.let(::JFile)
                    ?: JFile(request.workingDirectory)
                val result = com.critical.vexaemulator.steam.SteamCloudSync.pull(
                    this@GameActivity, cloudAppId, home, JFile(request.workingDirectory),
                    onProgress = { cloudDetail = it },
                    onConflict = { conflicts ->
                        cloudDetail = "save conflict, waiting for choice"
                        cloudConflict = conflicts
                        val picked = cloudChoice.await()
                        cloudConflict = null
                        cloudDetail = if (picked == com.critical.vexaemulator.steam.CloudChoice.Remote)
                            "using the cloud save" else "keeping the device save"
                        picked
                    },
                )
                cloudDetail = result.summary()
            }
            bootCloudDone = true
            VexaLogger.log(
                level = LogLevel.INFO,
                category = LogCategory.BOOT,
                message = "Launching ${program.title}",
                fields = mapOf(
                    "id" to program.id,
                    "executable" to request.executablePath,
                    "args" to request.launchArgs.joinToString(" "),
                )
            )
            startGuestOutputTail(java.io.File(request.artifactDirectory))
            launch {
                delay(12000)
                if (showLoading) {
                    showLoading = false
                    VexaLogger.log(
                        LogLevel.WARN,
                        LogCategory.ACTIVITY,
                        "Loading overlay dismissed by watchdog timeout"
                    )
                }
            }
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
                return@launch
            }

            if (!tryLaunchWithSurface(request)) {
                pendingLaunchRequest = request
                return@launch
            }
            // TODO: Add a retry mechanism or exit later
            VexaLogger.log(
                level = LogLevel.INFO,
                category = LogCategory.SURFACE,
                message = "Game Surface successfully created"
            )
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        latestSurface = holder.surface
        surfaceWidth = width
        surfaceHeight = height
        if (width > height) {
            requestHighestRefreshRate()
        }
        sendSurfaceChanged(format, width, height)

        pendingLaunchRequest?.let { req ->
            if (runtimeBound && tryLaunchWithSurface(req)) {
                pendingLaunchRequest = null
            }
        }
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
        latestSurface = null
        sendSurfaceDestroyed()

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

    // Stay bound while backgrounded: dropping the last binding destroys the runtime
    // service and kills the guest. The foreground service keeps the process alive.
    override fun onStop() {
        super.onStop()
    }

    override fun onDestroy() {
        if (runtimeBound) {
            sendUnregister()
            runCatching {
                unbindService(runtimeConnection)
            }
            runtimeBound = false
        }
        super.onDestroy()
    }

    private fun startGuestOutputTail(artifactDir: java.io.File) {
        lifecycleScope.launch(Dispatchers.IO) {
            val stdout = java.io.File(artifactDir, "guest_stdout.log")
            val stderr = java.io.File(artifactDir, "fex_stderr.log")
            var stdoutPos = 0L
            var stderrPos = 0L
            repeat(40) {
                stdoutPos = publishNewLines(stdout, stdoutPos, "guest stdout")
                stderrPos = publishNewLines(stderr, stderrPos, "guest stderr")
                delay(500)
            }
        }
    }

    private suspend fun publishNewLines(file: java.io.File, position: Long, label: String): Long {
        if (!file.exists()) return position
        val length = file.length()
        if (length <= position) return position
        val chunk = try {
            withContext(Dispatchers.IO) {
                java.io.RandomAccessFile(file, "r").use { raf ->
                    raf.seek(position)
                    val size = (length - position).coerceAtMost(65536L).toInt()
                    val bytes = ByteArray(size)
                    val read = raf.read(bytes)
                    if (read <= 0) null else read to String(bytes, 0, read, Charsets.UTF_8)
                }
            }
        } catch (_: Exception) {
            null
        } ?: return position
        val (read, text) = chunk
        if (showLoading) {
            withContext(Dispatchers.Main.immediate) { showLoading = false }
        }
        text.lineSequence().map { it.trimEnd() }.filter { it.isNotEmpty() }.forEach { line ->
            VexaLogger.log(LogLevel.INFO, LogCategory.RUNTIME, "$label: $line")
        }
        return position + read
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
    program: GuestProgram?,
    runtimePid: Int?,
    sessionStartMs: Long,
    bootStep: Int,
    cloudDetail: String?,
    cloudConflict: List<com.critical.vexaemulator.steam.CloudConflict>?,
    onCloudChoice: (com.critical.vexaemulator.steam.CloudChoice) -> Unit,
    surfaceSize: String?,
    showLoading: Boolean,
    showDebugMenu: Boolean,
    onToggleDebugMenu: () -> Unit,
    hideFilters: Boolean,
    onHideFilters: () -> Unit,
    onBack: () -> Unit,
    pad: GuestPadState,
) {
    val c = VexaTheme.colors
    val entries by LogStore.entries.collectAsState()

    // Quick panel: 0 = closed, 1 = open. Dragged by the edge handle, settles with a spring.
    val scope = rememberCoroutineScope()
    val haptics = rememberHaptics()
    val density = LocalDensity.current
    val panelWidthPx = with(density) { 320.dp.toPx() }
    val drawer = remember { Animatable(0f) }
    var debugLevels by remember { mutableStateOf(setOf<LogLevel>()) }
    val openDebug: (Set<LogLevel>) -> Unit = { levels ->
        scope.launch { drawer.snapTo(0f) }
        debugLevels = levels
        if (levels.isNotEmpty() && hideFilters) onHideFilters()
        onToggleDebugMenu()
    }
    val settle: (Boolean) -> Unit = { open ->
        scope.launch {
            drawer.animateTo(if (open) 1f else 0f, spring(dampingRatio = 0.82f, stiffness = 380f))
        }
        if (open) haptics.confirm()
    }

    Box(modifier = Modifier.fillMaxSize().background(VoidColors.bgBase)) {
        AndroidView(
            factory = { surfaceView },
            modifier = Modifier.fillMaxSize()
        )
        if (!showLoading) {
            GuestPadOverlay(pad)
        }

        if (!showLoading && !showDebugMenu) {
            val open = drawer.value
            if (open > 0f) {
                Box(
                    Modifier
                        .fillMaxSize()
                        .graphicsLayer { alpha = open }
                        .background(c.scrim)
                        .pointerInput(Unit) { detectTapGestures { settle(false) } },
                )
            }
            Box(
                Modifier
                    .align(Alignment.CenterEnd)
                    .graphicsLayer { translationX = (1f - drawer.value) * panelWidthPx }
                    .pointerInput(Unit) {
                        detectHorizontalDragGestures(
                            onDragEnd = { settle(drawer.value > 0.6f) },
                            onDragCancel = { settle(drawer.value > 0.6f) },
                        ) { change, dx ->
                            change.consume()
                            scope.launch { drawer.snapTo((drawer.value - dx / panelWidthPx).coerceIn(0f, 1f)) }
                        }
                    },
            ) {
                if (open > 0f) {
                    QuickPanel(
                        program = program,
                        runtimePid = runtimePid,
                        sessionStartMs = sessionStartMs,
                        entries = entries,
                        onOpenDebug = { openDebug(emptySet()) },
                        onOpenLogs = openDebug,
                        onExit = onBack,
                        padOn = pad.overlay,
                        onTogglePad = { pad.overlay = !pad.overlay },
                    )
                }
            }
            EdgeHandle(
                open = open,
                onDrag = { dx -> scope.launch { drawer.snapTo((drawer.value - dx / panelWidthPx).coerceIn(0f, 1f)) } },
                onDragEnd = { settle(drawer.value > 0.25f) },
                onTap = { settle(drawer.value < 0.5f) },
                modifier = Modifier
                    .align(Alignment.CenterEnd)
                    .graphicsLayer { translationX = -drawer.value * panelWidthPx },
            )
        }

        AnimatedVisibility(
            visible = showLoading,
            enter = fadeIn(tween(450)),
            exit = fadeOut(tween(450)) + scaleOut(targetScale = 1.04f, animationSpec = tween(450)),
        ) {
            GameLoadingOverlay(
                program = program,
                step = bootStep,
                surfaceSize = surfaceSize,
                lastMessage = entries.lastOrNull()?.message,
                cloudDetail = cloudDetail,
                onDebug = { openDebug(emptySet()) },
                onClose = onBack,
            )
        }

        cloudConflict?.let { conflicts ->
            CloudConflictDialog(conflicts, onChoice = onCloudChoice)
        }

        AnimatedVisibility(
            visible = showDebugMenu,
            enter = fadeIn(tween(VexaMotion.MEDIUM)) +
                scaleIn(initialScale = 0.4f, transformOrigin = TransformOrigin(1f, 0.5f), animationSpec = tween(VexaMotion.MEDIUM, easing = VexaMotion.Emphasized)) +
                slideInHorizontally(tween(VexaMotion.MEDIUM, easing = VexaMotion.Emphasized)) { it / 5 },
            exit = fadeOut(tween(240)) +
                scaleOut(targetScale = 0.15f, transformOrigin = TransformOrigin(1f, 0.5f), animationSpec = tween(280, easing = VexaMotion.Emphasized)) +
                slideOutHorizontally(tween(280, easing = VexaMotion.Emphasized)) { it / 3 },
        ) {
            GameDebugPanel(
                hideFilters = hideFilters,
                onClose = onToggleDebugMenu,
                onHideFilters = onHideFilters,
                subtitle = listOfNotNull(program?.id, runtimePid?.let { "pid $it" }).joinToString(" · ").ifEmpty { null },
                initialLevels = debugLevels,
            )
        }
    }
}
