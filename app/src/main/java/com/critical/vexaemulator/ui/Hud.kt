package com.critical.vexaemulator.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Slim glowing knob on the right screen edge. Horizontal drags report deltas so the
 * caller can scrub the quick panel open; a tap toggles it. Stretches slightly while held.
 */
@Composable
fun EdgeHandle(
    open: Float,
    onDrag: (Float) -> Unit,
    onDragEnd: () -> Unit,
    onTap: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    var touched by remember { mutableStateOf(false) }
    val alpha by animateFloatAsState(if (touched || open > 0f) 1f else 0.55f, tween(VexaMotion.SMALL), label = "knobA")
    val stretch by animateDpAsState(if (touched) 88.dp else 72.dp, spring(dampingRatio = 0.5f, stiffness = 400f), label = "knobH")
    Box(
        modifier
            .width(52.dp)
            .height(200.dp)
            .pointerInput(Unit) {
                detectHorizontalDragGestures(
                    onDragStart = { touched = true; haptics.tick() },
                    onDragEnd = { touched = false; onDragEnd() },
                    onDragCancel = { touched = false; onDragEnd() },
                ) { change, dx ->
                    change.consume()
                    onDrag(dx)
                }
            }
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = { touched = true; tryAwaitRelease(); touched = false },
                    onTap = { haptics.tap(); onTap() },
                )
            },
        contentAlignment = Alignment.CenterEnd,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(end = 4.dp)) {
            AnimatedVisibility(touched && open < 0.5f, enter = fadeIn() + slideInHorizontally { it / 2 }, exit = fadeOut()) {
                VexaIcon(VexaGlyph.ChevronLeft, c.plasma, size = 16.dp, strokeWidth = 2.6f)
            }
            Box(
                Modifier
                    .alpha(alpha)
                    .width(8.dp)
                    .height(stretch)
                    .shadow(if (c.isDark) 12.dp else 4.dp, PillShape, ambientColor = c.plasma, spotColor = c.plasma)
                    .clip(PillShape)
                    .background(Brush.verticalGradient(c.brandStops)),
            )
        }
    }
}

/** Right-docked quick panel with session header, mini stats, actions and hold-to-exit. */
@Composable
fun QuickPanel(
    program: GuestProgram?,
    runtimePid: Int?,
    sessionStartMs: Long,
    entries: List<VexaLogEntry>,
    onOpenDebug: () -> Unit,
    onOpenLogs: (Set<LogLevel>) -> Unit,
    onExit: () -> Unit,
    padOn: Boolean = false,
    onTogglePad: (() -> Unit)? = null,
    onOpenKeyboard: (() -> Unit)? = null,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val shape = RoundedCornerShape(topStart = 28.dp, bottomStart = 28.dp)
    val errors = remember(entries) { entries.count { it.level.ordinal >= LogLevel.ERROR.ordinal } }
    val warns = remember(entries) { entries.count { it.level == LogLevel.WARN } }
    Box(
        modifier
            .fillMaxHeight()
            .width(320.dp)
            .shadow(24.dp, shape, ambientColor = Color.Black, spotColor = Color.Black)
            .clip(shape)
            .background(c.bgElevated.copy(alpha = 0.96f))
            .border(1.dp, if (c.isDark) c.glassBorder else c.hairline, shape),
    ) {
        Box(Modifier.fillMaxHeight().width(1.5.dp).background(Brush.verticalGradient(c.brandStops)))
        Column(Modifier.fillMaxSize().padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                CircleHudButton(VexaGlyph.Terminal, c.ultraviolet, onClick = onOpenDebug, size = 42.dp, iconSize = 20.dp)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            program?.title ?: "Runtime",
                            style = MaterialTheme.typography.headlineSmall.copy(color = c.textPrimary),
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier.weight(1f, fill = false),
                        )
                        Spacer(Modifier.width(6.dp))
                        Badge("x86_64", c.plasma)
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        StatusDot(c.success, size = 6.dp, pulse = true)
                        Text(
                            "running" + (runtimePid?.let { " · pid $it" } ?: ""),
                            style = MonospaceTypography.labelSmall.copy(color = c.textSecondary),
                        )
                    }
                }
                UptimeBadge(sessionStartMs)
                Spacer(Modifier.width(8.dp))
                ThemeToggleButton(
                    dark = VexaPrefs.themeMode.resolveDark(),
                    onSelect = { mode, _ -> VexaPrefs.updateThemeMode(mode) },
                )
            }
            Spacer(Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                StatTile("logs", entries.size, c.info, Modifier.weight(1f), horizontal = false) {
                    onOpenLogs(emptySet())
                }
                StatTile("warn", warns, if (warns == 0) c.textTertiary else c.warning, Modifier.weight(1f), horizontal = false) {
                    onOpenLogs(setOf(LogLevel.WARN))
                }
                StatTile("err", errors, if (errors == 0) c.success else c.error, Modifier.weight(1f), horizontal = false) {
                    onOpenLogs(setOf(LogLevel.ERROR, LogLevel.FATAL))
                }
            }
            Spacer(Modifier.weight(1f))
            Row(
                Modifier.fillMaxWidth().staggerIn(1),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.End,
            ) {
                if (onTogglePad != null) {
                    CircleHudButton(
                        VexaGlyph.Gamepad,
                        if (padOn) c.plasma else c.textSecondary,
                        onClick = onTogglePad,
                    )
                    Spacer(Modifier.width(10.dp))
                }
                if (onOpenKeyboard != null) {
                    CircleHudButton(
                        VexaGlyph.Keyboard,
                        c.textSecondary,
                        onClick = onOpenKeyboard,
                    )
                    Spacer(Modifier.width(10.dp))
                }
                HoldToExitCircle(onExit = onExit)
            }
        }
    }
}

@Composable
private fun UptimeBadge(sessionStartMs: Long) {
    val c = VexaTheme.colors
    var now by remember { mutableStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) {
            now = System.currentTimeMillis()
            delay(1000)
        }
    }
    val s = ((now - sessionStartMs) / 1000).coerceAtLeast(0)
    Column(horizontalAlignment = Alignment.End) {
        Text("UPTIME", style = MonospaceTypography.labelSmall.copy(color = c.textTertiary, letterSpacing = 0.1.em))
        Text(
            "%02d:%02d:%02d".format(s / 3600, (s / 60) % 60, s % 60),
            style = MonospaceTypography.labelLarge.copy(color = c.plasma, fontWeight = FontWeight.Bold),
            modifier = Modifier
                .clip(RoundedCornerShape(8.dp))
                .background(c.surface2)
                .padding(horizontal = 7.dp, vertical = 2.dp),
        )
    }
}

/** Circular icon button for the quick panel footer. */
@Composable
private fun CircleHudButton(
    glyph: VexaGlyph,
    tint: Color,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = 56.dp,
    iconSize: Dp = 19.dp,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    Box(
        modifier
            .size(size)
            .pressScale(source, 0.9f)
            .clip(CircleShape)
            .background(c.surface1)
            .border(1.dp, c.hairline, CircleShape)
            .clickable(source, ripple(color = tint)) { haptics.tap(); onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Box(
            Modifier.size(size * 0.68f).clip(CircleShape).background(c.soft(tint)),
            contentAlignment = Alignment.Center,
        ) {
            VexaIcon(glyph, tint, size = iconSize)
        }
    }
}

/** Circular hold-to-exit: press and hold until the ring completes around the power glyph. */
@Composable
fun HoldToExitCircle(onExit: () -> Unit, modifier: Modifier = Modifier, holdMs: Int = 800) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val scope = rememberCoroutineScope()
    val progress = remember { Animatable(0f) }
    var done by remember { mutableStateOf(false) }
    Box(
        modifier
            .size(56.dp)
            .clip(CircleShape)
            .background(c.soft(c.error).copy(alpha = if (c.isDark) 0.10f else 0.08f))
            .border(1.dp, c.error.copy(alpha = 0.35f + 0.45f * progress.value), CircleShape)
            .pointerInput(Unit) {
                detectTapGestures(onPress = {
                    if (done) return@detectTapGestures
                    haptics.tick()
                    val job = scope.launch {
                        progress.animateTo(1f, tween((holdMs * (1f - progress.value)).toInt(), easing = LinearEasing))
                        done = true
                        haptics.confirm()
                        onExit()
                    }
                    tryAwaitRelease()
                    if (!done) {
                        job.cancel()
                        scope.launch { progress.animateTo(0f, tween(VexaMotion.SMALL)) }
                    }
                })
            },
        contentAlignment = Alignment.Center,
    ) {
        Canvas(Modifier.fillMaxSize().padding(4.dp)) {
            val stroke = 2.5.dp.toPx()
            drawArc(
                color = c.error,
                startAngle = -90f,
                sweepAngle = 360f * progress.value,
                useCenter = false,
                topLeft = Offset(stroke / 2, stroke / 2),
                size = Size(size.width - stroke, size.height - stroke),
                style = Stroke(stroke, cap = StrokeCap.Round),
            )
        }
        VexaIcon(VexaGlyph.Power, c.error, size = 20.dp, modifier = Modifier.graphicsLayer {
            val s = 1f + 0.15f * progress.value
            scaleX = s; scaleY = s
        })
    }
}

