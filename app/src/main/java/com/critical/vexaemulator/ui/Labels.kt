package com.critical.vexaemulator.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.ui.theme.ChipShape
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.OverlineStyle
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaTheme

@Composable
fun SectionLabel(
    text: String,
    modifier: Modifier = Modifier,
    accent: Boolean = true,
    trailing: (@Composable () -> Unit)? = null,
) {
    val c = VexaTheme.colors
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = text.uppercase(),
                style = OverlineStyle.copy(color = if (accent) c.plasma else c.textTertiary),
            )
            if (accent) {
                Spacer(Modifier.width(6.dp))
                Box(Modifier.size(5.dp).clip(CircleShape).background(c.plasma))
            }
        }
        trailing?.invoke()
    }
}

/** Solid dot; [pulse] adds a radiating ring (1.6s loop) for live states. */
@Composable
fun StatusDot(color: Color, modifier: Modifier = Modifier, size: Dp = 7.dp, pulse: Boolean = false) {
    Box(modifier.size(size * 2.4f), contentAlignment = Alignment.Center) {
        if (pulse) {
            val t = rememberInfiniteTransition(label = "dotPulse")
            val p by t.animateFloat(
                0f, 1f,
                infiniteRepeatable(tween(1600, easing = LinearEasing), RepeatMode.Restart),
                label = "dotPulseP",
            )
            Canvas(Modifier.matchParentSize()) {
                val base = size.toPx() / 2f
                drawCircle(color, radius = base * (1f + 1.4f * p), alpha = 0.45f * (1f - p))
            }
        }
        Box(Modifier.size(size).clip(CircleShape).background(color))
    }
}

/** Status pill: tinted wash, optional leading dot, mono caps. */
@Composable
fun TinyPill(
    text: String,
    color: Color,
    modifier: Modifier = Modifier,
    dot: Boolean = false,
    pulse: Boolean = false,
    bordered: Boolean = true,
) {
    val c = VexaTheme.colors
    Row(
        modifier = modifier
            .clip(PillShape)
            .background(c.soft(color))
            .then(if (bordered) Modifier.border(1.dp, color.copy(alpha = 0.35f), PillShape) else Modifier)
            .padding(start = if (dot) 6.dp else 10.dp, end = 10.dp, top = 3.dp, bottom = 3.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (dot) {
            StatusDot(color, size = 6.dp, pulse = pulse)
            Spacer(Modifier.width(1.dp))
        }
        Text(
            text = text.uppercase(),
            color = color,
            style = MonospaceTypography.labelSmall.copy(fontWeight = FontWeight.SemiBold, letterSpacing = 0.08.em),
        )
    }
}

/** Compact square-ish badge used for program types and log categories. */
@Composable
fun Badge(text: String, color: Color, modifier: Modifier = Modifier, filled: Boolean = false) {
    val c = VexaTheme.colors
    Text(
        text = text.uppercase(),
        modifier = modifier
            .clip(ChipShape)
            .background(if (filled) color else c.soft(color))
            .border(1.dp, color.copy(alpha = if (filled) 0f else 0.30f), ChipShape)
            .padding(horizontal = 6.dp, vertical = 2.dp),
        color = if (filled) c.onAccent else color,
        style = MonospaceTypography.labelSmall.copy(fontWeight = FontWeight.SemiBold, letterSpacing = 0.06.em),
        maxLines = 1,
    )
}

@Composable
fun LevelBadge(level: LogLevel, modifier: Modifier = Modifier) {
    Badge(text = level.shortLabel(), color = level.toColor(), modifier = modifier, filled = level == LogLevel.FATAL)
}
