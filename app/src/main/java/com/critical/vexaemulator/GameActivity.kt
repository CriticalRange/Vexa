package com.critical.vexaemulator

import android.content.pm.ActivityInfo
import android.os.Bundle
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
import com.critical.vexaemulator.ui.GameDebugPanel
import com.critical.vexaemulator.ui.GameLoadingOverlay
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class GameActivity : ComponentActivity(), SurfaceHolder.Callback {
    private lateinit var gameSurfaceView: SurfaceView
    private var showLoading by mutableStateOf(true)
    private var showDebugMenu by mutableStateOf(true) // For now, it's true; will be changed

    private var hideFilters by mutableStateOf(true)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.ACTIVITY,
            message = "GameActivity onCreate started"
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

            showLoading = true //change this to false after setting up everything
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
        RuntimeBridge.start(holder.surface)
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
        RuntimeBridge.onSurfaceChanged(width, height)
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
        RuntimeBridge.stop()
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