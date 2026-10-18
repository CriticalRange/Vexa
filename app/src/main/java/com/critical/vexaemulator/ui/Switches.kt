package com.critical.vexaemulator.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.LayoutCoordinates
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.ThemeMode
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme

private val ToggleSpring = spring<Dp>(dampingRatio = 0.55f, stiffness = 380f)
private val SettingsIconRed = Color(0xFFB00000)

/** Pill switch: spring thumb with a little overshoot, crossfading track, check glyph in the thumb. */
@Composable
fun VexaSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val thumbX by animateDpAsState(if (checked) 22.dp else 0.dp, ToggleSpring, label = "thumbX")
    val track by animateColorAsState(if (checked) c.plasma else c.surface3, tween(VexaMotion.SMALL), label = "track")
    val thumb by animateColorAsState(
        if (checked) (if (c.isDark) c.onAccent else Color.White) else c.textTertiary,
        tween(VexaMotion.SMALL), label = "thumb",
    )
    val checkAlpha by animateFloatAsState(if (checked) 1f else 0f, tween(VexaMotion.SMALL), label = "check")
    Box(
        modifier = modifier
            .size(width = 50.dp, height = 28.dp)
            .alpha(if (enabled) 1f else 0.4f)
            .clip(PillShape)
            .background(track)
            .border(1.dp, if (checked) Color.Transparent else c.hairline, PillShape)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                enabled = enabled,
            ) {
                haptics.tick()
                onCheckedChange(!checked)
            }
            .padding(3.dp),
    ) {
        Box(
            Modifier
                .offset(x = thumbX)
                .size(22.dp)
                .shadow(2.dp, CircleShape)
                .clip(CircleShape)
                .background(thumb),
            contentAlignment = Alignment.Center,
        ) {
            VexaIcon(VexaGlyph.Check, tint = c.plasma.copy(alpha = checkAlpha), size = 13.dp, strokeWidth = 3f)
        }
    }
}

@Composable
fun IconTile(glyph: VexaGlyph, tint: Color = SettingsIconRed, modifier: Modifier = Modifier, size: Dp = 40.dp) {
    val c = VexaTheme.colors
    Box(
        modifier
            .size(size)
            .clip(RoundedCornerShape(12.dp))
            .background(c.soft(tint)),
        contentAlignment = Alignment.Center,
    ) {
        VexaIcon(glyph, tint = tint, size = size * 0.5f)
    }
}

/** Settings row: icon tile, label + description, arbitrary trailing control. */
@Composable
fun SettingRow(
    label: String,
    description: String,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = null,
    iconTint: Color = SettingsIconRed,
    dot: Color? = null,
    onClick: (() -> Unit)? = null,
    trailing: @Composable () -> Unit,
) {
    val c = VexaTheme.colors
    Row(
        modifier = modifier
            .fillMaxWidth()
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        icon?.let {
            IconTile(it, iconTint)
            Spacer(Modifier.width(14.dp))
        }
        Column(modifier = Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                dot?.let {
                    StatusDot(it, size = 6.dp)
                    Spacer(Modifier.width(2.dp))
                }
                Text(label, style = MaterialTheme.typography.titleLarge.copy(color = c.textPrimary))
            }
            Spacer(Modifier.height(2.dp))
            Text(description, style = MaterialTheme.typography.bodySmall.copy(color = c.textSecondary))
        }
        Spacer(Modifier.width(12.dp))
        trailing()
    }
}

@Composable
fun SwitchRow(
    label: String,
    description: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    icon: VexaGlyph? = null,
    iconTint: Color = SettingsIconRed,
    dot: Color? = null,
) {
    SettingRow(
        label = label,
        description = description,
        modifier = modifier,
        icon = icon,
        iconTint = iconTint,
        dot = dot,
        onClick = { onCheckedChange(!checked) },
    ) {
        VexaSwitch(checked = checked, onCheckedChange = onCheckedChange)
    }
}

/**
 * Light / Dark / System pill with a sliding gradient indicator. [onSelect] also receives
 * the tapped segment's center in root coordinates so callers can start a circular reveal there.
 */
@Composable
fun ThemeSegmented(
    mode: ThemeMode,
    onSelect: (ThemeMode, Offset) -> Unit,
    modifier: Modifier = Modifier,
    segment: Dp = 34.dp,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val modes = ThemeMode.entries
    val index = modes.indexOf(mode)
    val x by animateDpAsState(segment * index, ToggleSpring, label = "segX")
    val coords = remember { arrayOfNulls<LayoutCoordinates>(modes.size) }
    Box(
        modifier
            .clip(PillShape)
            .background(c.surface2)
            .border(1.dp, c.hairline, PillShape)
            .padding(3.dp),
    ) {
        Box(
            Modifier
                .offset(x = x)
                .size(segment)
                .clip(PillShape)
                .background(c.brand()),
        )
        Row {
            modes.forEachIndexed { i, m ->
                val selected = i == index
                // Sun ↔ moon morph: the newly selected glyph spins in from -90°.
                val rotation = remember { Animatable(0f) }
                LaunchedEffect(selected) {
                    if (selected) {
                        rotation.snapTo(-90f)
                        rotation.animateTo(0f, tween(VexaMotion.LARGE, easing = VexaMotion.Emphasized))
                    }
                }
                val tint by animateColorAsState(
                    if (selected) c.onAccent else c.textSecondary,
                    tween(VexaMotion.SMALL), label = "segTint",
                )
                Box(
                    Modifier
                        .size(segment)
                        .onGloballyPositioned { coords[i] = it }
                        .clip(CircleShape)
                        .clickable(
                            interactionSource = remember { MutableInteractionSource() },
                            indication = null,
                        ) {
                            if (selected) return@clickable
                            haptics.tick()
                            val center = coords[i]?.let {
                                it.positionInRoot() + Offset(it.size.width / 2f, it.size.height / 2f)
                            } ?: Offset.Zero
                            onSelect(m, center)
                        },
                    contentAlignment = Alignment.Center,
                ) {
                    VexaIcon(
                        glyph = when (m) {
                            ThemeMode.Light -> VexaGlyph.Sun
                            ThemeMode.Dark -> VexaGlyph.Moon
                            ThemeMode.System -> VexaGlyph.Monitor
                        },
                        tint = tint,
                        size = segment * 0.48f,
                        modifier = Modifier.rotate(rotation.value),
                    )
                }
            }
        }
    }
}

/** Circular button that toggles Light ↔ Dark; the glyph spins in on each change. */
@Composable
fun ThemeToggleButton(
    dark: Boolean,
    onSelect: (ThemeMode, Offset) -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = 36.dp,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val rotation = remember { Animatable(0f) }
    LaunchedEffect(dark) {
        rotation.snapTo(-90f)
        rotation.animateTo(0f, tween(VexaMotion.LARGE, easing = VexaMotion.Emphasized))
    }
    var center by remember { mutableStateOf(Offset.Zero) }
    Box(
        modifier
            .size(size)
            .onGloballyPositioned {
                center = it.positionInRoot() + Offset(it.size.width / 2f, it.size.height / 2f)
            }
            .clip(CircleShape)
            .background(c.surface2)
            .border(1.dp, c.hairline, CircleShape)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
            ) {
                haptics.tick()
                onSelect(if (dark) ThemeMode.Light else ThemeMode.Dark, center)
            },
        contentAlignment = Alignment.Center,
    ) {
        VexaIcon(
            glyph = if (dark) VexaGlyph.Moon else VexaGlyph.Sun,
            tint = c.plasma,
            size = size * 0.48f,
            modifier = Modifier.rotate(rotation.value),
        )
    }
}
