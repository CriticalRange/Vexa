package com.critical.vexaemulator.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.CardShape
import com.critical.vexaemulator.ui.theme.Cyan

fun Modifier.vexaGlow(
    color: Color,
    radiusDp: Dp = 16.dp,
    shape: Shape = CardShape,
): Modifier = this.shadow(
    elevation = radiusDp,
    shape = shape,
    ambientColor = color,
    spotColor = color,
)

@Composable
fun Modifier.vexaShimmer(enabled: Boolean = true): Modifier {
    if (!enabled) return this

    val transition = rememberInfiniteTransition(label = "vexaShimmer")
    val animValue by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 1600, easing = LinearEasing),
            repeatMode = RepeatMode.Restart,
        ),
        label = "vexaShimmerAnim",
    )

    return this.drawWithContent {
        drawContent()
        val translateX = animValue * size.width * 1.4f - size.width * 0.2f
        drawRect(
            brush = Brush.linearGradient(
                colors = listOf(
                    Color.Transparent,
                    Color.White.copy(alpha = 0.06f),
                    Color.Transparent,
                ),
                start = Offset(translateX, 0f),
                end = Offset(translateX + size.width * 0.25f, size.height),
            ),
        )
    }
}

@Composable
fun ScanlineOverlay(
    modifier: Modifier = Modifier,
    color: Color = Cyan,
) {
    val transition = rememberInfiniteTransition(label = "scanline")
    val anim by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 4000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart,
        ),
        label = "scanlineAnim",
    )

    Box(modifier) {
        Canvas(Modifier.matchParentSize()) {
            val spacing = 4.dp.toPx()
            var y = 0f
            while (y < size.height) {
                drawLine(
                    color = color.copy(alpha = 0.03f),
                    start = Offset(0f, y),
                    end = Offset(size.width, y),
                    strokeWidth = 1f,
                )
                y += spacing
            }

            val movingY = anim * size.height
            drawLine(
                color = color.copy(alpha = 0.18f),
                start = Offset(0f, movingY),
                end = Offset(size.width, movingY),
                strokeWidth = 1.dp.toPx(),
            )
        }
    }
}
