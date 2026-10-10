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
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/** Rotating arc spinner. */
@Composable
fun BootSpinner(
    modifier: Modifier = Modifier,
    color: Color = VexaTheme.colors.plasma,
    sizeDp: Dp = 56.dp,
) {
    val transition = rememberInfiniteTransition(label = "bootSpinner")
    val rotation by transition.animateFloat(
        0f, 360f,
        infiniteRepeatable(tween(1100, easing = LinearEasing), RepeatMode.Restart),
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
 * 12-segment ring: the ring slowly rotates while a lit head sweeps around it,
 * each segment tinted along the brand gradient and fading behind the head.
 */
@Composable
fun SegmentRing(modifier: Modifier = Modifier, sizeDp: Dp = 56.dp) {
    val c = VexaTheme.colors
    val t = rememberInfiniteTransition(label = "segRing")
    val head by t.animateFloat(
        0f, 12f,
        infiniteRepeatable(tween(1200, easing = LinearEasing), RepeatMode.Restart),
        label = "segHead",
    )
    val spin by t.animateFloat(
        0f, 360f,
        infiniteRepeatable(tween(6000, easing = LinearEasing), RepeatMode.Restart),
        label = "segSpin",
    )
    Canvas(modifier.size(sizeDp)) {
        val r = size.minDimension / 2f
        val stroke = 3.5.dp.toPx()
        rotate(spin) {
            for (i in 0 until 12) {
                val angle = (i / 12f) * 2f * PI.toFloat() - PI.toFloat() / 2f
                val dist = ((head - i) % 12f + 12f) % 12f
                val lit = (1f - dist / 6f).coerceIn(0f, 1f)
                val frac = i / 11f
                val tint = if (frac < 0.5f) lerp(c.plasma, c.ion, frac * 2f) else lerp(c.ion, c.ultraviolet, (frac - 0.5f) * 2f)
                val inner = r * 0.62f
                val outer = r - stroke
                drawLine(
                    color = if (lit > 0f) tint else c.hairline,
                    start = Offset(center.x + cos(angle) * inner, center.y + sin(angle) * inner),
                    end = Offset(center.x + cos(angle) * outer, center.y + sin(angle) * outer),
                    strokeWidth = stroke,
                    cap = StrokeCap.Round,
                    alpha = 0.25f + 0.75f * lit,
                )
            }
        }
    }
}

/** Three dots pulsing in sequence (150ms stagger). */
@Composable
fun PulsingDots(
    modifier: Modifier = Modifier,
    color: Color = VexaTheme.colors.plasma,
    secondary: Color = VexaTheme.colors.ultraviolet,
) {
    val transition = rememberInfiniteTransition(label = "pulsingDots")
    val phase by transition.animateFloat(
        0f, (2f * PI).toFloat(),
        infiniteRepeatable(tween(1200, easing = LinearEasing), RepeatMode.Restart),
        label = "phase",
    )
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        repeat(3) { i ->
            val alpha = 0.25f + 0.75f * abs(sin(phase - i * 0.6f))
            Canvas(modifier = Modifier.size(6.dp)) {
                drawCircle(color = lerp(color, secondary, i / 2f), alpha = alpha)
            }
        }
    }
}
