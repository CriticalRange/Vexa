package com.critical.vexaemulator.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.CardShape
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlinx.coroutines.delay

/** Plasma glow in Void, soft neutral shadow in Paper. */
@Composable
fun Modifier.vexaGlow(
    color: Color = VexaTheme.colors.plasma,
    radius: Dp = 18.dp,
    shape: Shape = CardShape,
): Modifier {
    val c = VexaTheme.colors
    val tint = if (c.isDark) color.copy(alpha = 0.55f) else c.shadow.copy(alpha = 0.18f)
    return shadow(if (c.isDark) radius else radius * 0.6f, shape, ambientColor = tint, spotColor = tint)
}

/** Scales to 0.97 while pressed (spring back on release). */
fun Modifier.pressScale(source: MutableInteractionSource, pressed: Float = 0.97f): Modifier = composed {
    val isPressed by source.collectIsPressedAsState()
    val scale = remember { Animatable(1f) }
    LaunchedEffect(isPressed) {
        if (isPressed) scale.animateTo(pressed, tween(VexaMotion.MICRO))
        else scale.animateTo(1f, androidx.compose.animation.core.spring(dampingRatio = 0.5f, stiffness = 600f))
    }
    graphicsLayer { scaleX = scale.value; scaleY = scale.value }
}

/** Fade + 12dp rise on first composition, staggered 40ms per [index]. */
fun Modifier.staggerIn(index: Int, enabled: Boolean = true): Modifier = composed {
    if (!enabled) return@composed this
    val progress = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        delay(40L * index)
        progress.animateTo(1f, tween(VexaMotion.LARGE, easing = VexaMotion.Emphasized))
    }
    graphicsLayer {
        alpha = progress.value
        translationY = (1f - progress.value) * 12.dp.toPx()
    }
}

@Composable
fun ScanlineOverlay(
    modifier: Modifier = Modifier,
    color: Color = VexaTheme.colors.plasma,
) {
    val transition = rememberInfiniteTransition(label = "scanline")
    val anim by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(4000, easing = LinearEasing), RepeatMode.Restart),
        label = "scanlineAnim",
    )
    val dark = VexaTheme.colors.isDark
    Box(modifier) {
        Canvas(Modifier.matchParentSize()) {
            val spacing = (if (dark) 4.dp else 28.dp).toPx()
            var y = 0f
            while (y < size.height) {
                drawLine(color.copy(alpha = if (dark) 0.03f else 0.06f), Offset(0f, y), Offset(size.width, y), 1f)
                y += spacing
            }
            if (!dark) {
                var x = 0f
                while (x < size.width) {
                    drawLine(color.copy(alpha = 0.06f), Offset(x, 0f), Offset(x, size.height), 1f)
                    x += spacing
                }
            }
            val movingY = anim * size.height
            drawLine(color.copy(alpha = 0.16f), Offset(0f, movingY), Offset(size.width, movingY), 1.dp.toPx())
        }
    }
}

/** Large blurred brand orb that breathes behind hero content (6s loop). */
@Composable
fun AmbientOrb(modifier: Modifier = Modifier, center: Offset? = null, radiusFraction: Float = 0.55f) {
    val c = VexaTheme.colors
    val t = rememberInfiniteTransition(label = "orb")
    val breathe by t.animateFloat(
        0.85f, 1.1f,
        infiniteRepeatable(tween(3000, easing = VexaMotion.Standard), RepeatMode.Reverse),
        label = "orbBreathe",
    )
    Canvas(modifier) {
        val o = center ?: Offset(size.width / 2f, size.height / 2f)
        val r = size.minDimension * radiusFraction * breathe
        val a = if (c.isDark) 0.16f else 0.10f
        drawCircle(
            brush = Brush.radialGradient(
                listOf(c.plasma.copy(alpha = a), c.ultraviolet.copy(alpha = a * 0.6f), Color.Transparent),
                center = o,
                radius = r,
            ),
            radius = r,
            center = o,
        )
    }
}

/** Dotted mesh used behind the launcher header. */
@Composable
fun DotGrid(modifier: Modifier = Modifier, spacing: Dp = 18.dp) {
    val color = VexaTheme.colors.textTertiary
    Canvas(modifier) {
        val s = spacing.toPx()
        var y = s / 2
        while (y < size.height) {
            var x = s / 2
            val fade = 1f - y / size.height
            while (x < size.width) {
                drawCircle(color, radius = 0.8.dp.toPx(), center = Offset(x, y), alpha = 0.22f * fade)
                x += s
            }
            y += s
        }
    }
}

/** One-shot light sweep across content, restarted whenever [key] changes. */
fun Modifier.lightSweep(key: Any?, color: Color = Color.White): Modifier = composed {
    val p = remember { Animatable(-0.3f) }
    LaunchedEffect(key) {
        p.snapTo(-0.3f)
        p.animateTo(1.3f, tween(900, easing = VexaMotion.Standard))
    }
    drawWithContent {
        drawContent()
        if (p.value in -0.3f..1.3f && p.isRunning) {
            val x = p.value * size.width
            drawRect(
                Brush.linearGradient(
                    listOf(Color.Transparent, color.copy(alpha = 0.35f), Color.Transparent),
                    start = Offset(x - size.width * 0.15f, 0f),
                    end = Offset(x + size.width * 0.15f, size.height),
                ),
            )
        }
    }
}
