package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.ButtonShape
import com.critical.vexaemulator.ui.theme.SmallShape
import com.critical.vexaemulator.ui.theme.VexaTheme

/** Primary action with a quiet brand fill, press feedback and a tap-triggered sweep. */
@Composable
fun VexaButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = VexaGlyph.Play,
    enabled: Boolean = true,
    height: Dp = 56.dp,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    var sweep by remember { mutableIntStateOf(0) }
    Box(
        modifier = modifier
            .heightIn(min = height)
            .pressScale(source)
            .clip(ButtonShape)
            .background(c.plasma)
            .lightSweep(sweep)
            .clickable(source, ripple(color = c.onAccent), enabled = enabled) {
                sweep++
                haptics.confirm()
                onClick()
            }
            .alpha(if (enabled) 1f else 0.45f)
            .padding(horizontal = 18.dp, vertical = 14.dp),
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            icon?.let {
                VexaIcon(it, tint = c.onAccent, size = 22.dp)
                Spacer(Modifier.width(10.dp))
            }
            Text(
                text = text,
                style = MaterialTheme.typography.titleMedium.copy(color = c.onAccent, fontWeight = FontWeight.SemiBold),
            )
        }
    }
}

@Composable
private fun BaseButton(
    onClick: () -> Unit,
    modifier: Modifier,
    background: Color,
    border: Color,
    height: Dp,
    enabled: Boolean,
    shape: RoundedCornerShape = RoundedCornerShape(14.dp),
    rippleColor: Color,
    content: @Composable () -> Unit,
) {
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    Box(
        modifier = modifier
            .height(height)
            .pressScale(source)
            .clip(shape)
            .background(background)
            .border(1.dp, border, shape)
            .clickable(source, ripple(color = rippleColor), enabled = enabled) {
                haptics.tap()
                onClick()
            }
            .alpha(if (enabled) 1f else 0.45f)
            .padding(horizontal = 14.dp),
        contentAlignment = Alignment.Center,
    ) { content() }
}

@Composable
private fun ButtonLabel(text: String, color: Color, icon: VexaGlyph?) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
        icon?.let {
            VexaIcon(it, tint = color, size = 17.dp)
            Spacer(Modifier.width(7.dp))
        }
        Text(text, style = MaterialTheme.typography.labelLarge.copy(color = color, fontWeight = FontWeight.SemiBold))
    }
}

/** Tonal secondary action. */
@Composable
fun SecondaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = null,
    enabled: Boolean = true,
) {
    val c = VexaTheme.colors
    BaseButton(onClick, modifier, c.surface2, c.hairline, 48.dp, enabled, rippleColor = c.plasma) {
        ButtonLabel(text, c.textPrimary, icon)
    }
}

/** Low-emphasis action; [active] tints it with the accent (e.g. filters on). */
@Composable
fun GhostButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = null,
    active: Boolean = false,
    enabled: Boolean = true,
) {
    val c = VexaTheme.colors
    BaseButton(
        onClick, modifier,
        background = if (active) c.soft(c.plasma) else Color.Transparent,
        border = if (active) c.plasma.copy(alpha = 0.6f) else c.hairline,
        height = 38.dp, enabled = enabled, shape = RoundedCornerShape(10.dp), rippleColor = c.plasma,
    ) {
        ButtonLabel(text, if (active) c.plasma else c.textSecondary, icon)
    }
}

/** Destructive action: error wash + hairline, fills a bit stronger on press via ripple. */
@Composable
fun DangerButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = VexaGlyph.Power,
) {
    val c = VexaTheme.colors
    BaseButton(
        onClick, modifier, c.soft(c.error), c.error.copy(alpha = 0.45f), 56.dp, true,
        shape = RoundedCornerShape(16.dp), rippleColor = c.error,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            icon?.let {
                VexaIcon(it, tint = c.error, size = 20.dp)
                Spacer(Modifier.width(10.dp))
            }
            Text(text, style = MaterialTheme.typography.titleLarge.copy(color = c.error))
        }
    }
}

@Composable
fun VexaIconButton(
    glyph: VexaGlyph,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    tint: Color = VexaTheme.colors.textSecondary,
    size: Dp = 38.dp,
    container: Color = VexaTheme.colors.surface2,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    val filled = container != c.surface2
    Box(
        modifier = modifier
            .size(size)
            .pressScale(source, 0.92f)
            .clip(SmallShape)
            .background(container)
            .border(1.dp, if (filled) container else c.hairline, SmallShape)
            .clickable(source, ripple(color = tint)) {
                haptics.tap()
                onClick()
            },
        contentAlignment = Alignment.Center,
    ) {
        VexaIcon(glyph, tint = tint, size = size * 0.47f)
    }
}
