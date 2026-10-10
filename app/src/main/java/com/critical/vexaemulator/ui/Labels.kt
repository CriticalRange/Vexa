package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.MonospaceTypography

@Composable
fun SectionLabel(
    text: String,
    modifier: Modifier = Modifier,
    trailing: (@Composable () -> Unit)? = null,
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = text.uppercase(),
            style = MaterialTheme.typography.labelMedium.copy(
                fontWeight = FontWeight.SemiBold,
                letterSpacing = 2.2.sp,
                color = InkMuted,
            ),
        )
        trailing?.invoke()
    }
}

@Composable
fun StatusDot(color: Color, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .size(7.dp)
            .clip(CircleShape)
            .background(color),
    )
}

@Composable
fun TinyPill(text: String, color: Color, modifier: Modifier = Modifier) {
    Text(
        text = text,
        modifier = modifier
            .clip(RoundedCornerShape(100.dp))
            .background(color.copy(alpha = 0.16f))
            .padding(horizontal = 10.dp, vertical = 4.dp),
        color = color,
        style = MonospaceTypography.labelSmall,
    )
}

@Composable
fun LevelBadge(level: LogLevel, modifier: Modifier = Modifier) {
    val (color, label) = levelBadge(level)
    Text(
        text = label,
        modifier = modifier
            .clip(RoundedCornerShape(5.dp))
            .background(color.copy(alpha = 0.16f))
            .padding(horizontal = 6.dp, vertical = 2.dp),
        color = color,
        style = MonospaceTypography.labelSmall,
    )
}
