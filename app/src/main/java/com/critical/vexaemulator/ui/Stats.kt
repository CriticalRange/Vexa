package com.critical.vexaemulator.ui

import androidx.compose.animation.core.animateIntAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import java.text.NumberFormat
import java.util.Locale

private val countFormat = NumberFormat.getIntegerInstance(Locale.US)

/** Mono stat tile whose number counts toward its new value (320ms). */
@Composable
fun StatTile(
    label: String,
    value: Int,
    color: Color,
    modifier: Modifier = Modifier,
    horizontal: Boolean = true,
    onClick: (() -> Unit)? = null,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val shown by animateIntAsState(value, tween(VexaMotion.MEDIUM, easing = VexaMotion.Standard), label = "statCount")
    val shape = RoundedCornerShape(10.dp)
    val source = remember { MutableInteractionSource() }
    val tile = modifier
        .pressScale(source, 0.94f)
        .clip(shape)
        .background(c.soft(color).copy(alpha = if (c.isDark) 0.08f else 0.06f))
        .border(1.dp, color.copy(alpha = 0.30f), shape)
        .then(
            if (onClick != null) {
                Modifier.clickable(source, ripple(color = color)) {
                    haptics.tap()
                    onClick()
                }
            } else {
                Modifier
            },
        )
        .padding(horizontal = 12.dp, vertical = 7.dp)
    val labelText = @Composable {
        Text(
            label.uppercase(),
            style = MonospaceTypography.labelSmall.copy(color = c.textTertiary, letterSpacing = 0.1.em),
        )
    }
    val valueText = @Composable {
        Text(
            countFormat.format(shown),
            style = MonospaceTypography.titleMedium.copy(color = color, fontWeight = FontWeight.Bold),
        )
    }
    if (horizontal) {
        Row(tile, verticalAlignment = Alignment.CenterVertically) {
            labelText()
            Spacer(Modifier.width(8.dp))
            valueText()
        }
    } else {
        Column(tile, horizontalAlignment = Alignment.CenterHorizontally) {
            labelText()
            valueText()
        }
    }
}
