package com.critical.vexaemulator.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.ClipOp
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.layer.drawLayer
import androidx.compose.ui.graphics.rememberGraphicsLayer
import com.critical.vexaemulator.ui.theme.ThemeMode
import com.critical.vexaemulator.ui.theme.VexaMotion
import kotlinx.coroutines.launch
import kotlin.math.hypot
import kotlin.math.max

/**
 * Circular theme reveal: snapshots the current frame, flips the theme, then draws the
 * old snapshot with a growing hole centred on the tapped toggle (480ms emphasized).
 * Must sit at the root so [content]'s root coordinates match this host.
 */
@Composable
fun ThemeRevealHost(content: @Composable (switchTheme: (ThemeMode, Offset) -> Unit) -> Unit) {
    val layer = rememberGraphicsLayer()
    val scope = rememberCoroutineScope()
    var snapshot by remember { mutableStateOf<ImageBitmap?>(null) }
    var origin by remember { mutableStateOf(Offset.Zero) }
    val progress = remember { Animatable(0f) }

    val switchTheme: (ThemeMode, Offset) -> Unit = { mode, at ->
        if (snapshot == null) {
            scope.launch {
                snapshot = layer.toImageBitmap()
                origin = at
                progress.snapTo(0f)
                VexaPrefs.updateThemeMode(mode)
                progress.animateTo(1f, tween(VexaMotion.LARGE + 120, easing = VexaMotion.Emphasized))
                snapshot = null
            }
        }
    }

    Box(Modifier.fillMaxSize()) {
        Box(
            Modifier
                .fillMaxSize()
                .drawWithContent {
                    layer.record { this@drawWithContent.drawContent() }
                    drawLayer(layer)
                },
        ) {
            content(switchTheme)
        }
        snapshot?.let { bitmap ->
            Canvas(Modifier.fillMaxSize()) {
                val maxR = hypot(max(origin.x, size.width - origin.x), max(origin.y, size.height - origin.y))
                val hole = Path().apply { addOval(Rect(origin, maxR * progress.value)) }
                clipPath(hole, ClipOp.Difference) { drawImage(bitmap) }
            }
        }
    }
}
