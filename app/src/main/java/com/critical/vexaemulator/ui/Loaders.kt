package com.critical.vexaemulator.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.VexaAccent
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin

/**
 * Rotating arc spinner shown while the emulator core boots.
 */
@Composable
fun BootSpinner(
    modifier: Modifier = Modifier,
    color: Color = VexaAccent,
    sizeDp: Dp = 56.dp,
) {
    val transition = rememberInfiniteTransition(label = "bootSpinner")
    val rotation by transition.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 1100, easing = LinearEasing),
            repeatMode = RepeatMode.Restart,
        ),
        label = "rotation",
    )

    Canvas(modifier = modifier.size(sizeDp)) {
        val strokeWidth = 3.dp.toPx()
        val inset = strokeWidth / 2f
        drawArc(
            color = color,
            startAngle = rotation,
            sweepAngle = 270f,
            useCenter = false,
            topLeft = Offset(inset, inset),
            size = Size(size.width - strokeWidth, size.height - strokeWidth),
            style = Stroke(width = strokeWidth, cap = StrokeCap.Round),
        )
    }
}

/**
 * Row of three dots pulsing in sequence for indeterminate loading states.
 */
@Composable
fun PulsingDots(
    modifier: Modifier = Modifier,
    color: Color = VexaAccent,
) {
    val transition = rememberInfiniteTransition(label = "pulsingDots")
    val phase by transition.animateFloat(
        initialValue = 0f,
        targetValue = (2f * PI).toFloat(),
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 1200, easing = LinearEasing),
            repeatMode = RepeatMode.Restart,
        ),
        label = "phase",
    )

    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        repeat(3) { i ->
            val alpha = 0.25f + 0.75f * abs(sin(phase + i * 1.05f))
            Canvas(modifier = Modifier.size(6.dp)) {
                drawCircle(color = color, alpha = alpha)
            }
        }
    }
}
