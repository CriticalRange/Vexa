package com.critical.vexaemulator.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaTheme

private val RowShape = RoundedCornerShape(12.dp)

fun copyToClipboard(context: Context, text: String, label: String = "vexa") {
    context.getSystemService(ClipboardManager::class.java)
        ?.setPrimaryClip(ClipData.newPlainText(label, text))
}

fun VexaLogEntry.toPlainLine(): String =
    "${formatLogTimestamp(timestampMs)} ${level.name} ${category.name} $message" +
        if (fields.isEmpty()) "" else "  " + fields.entries.joinToString(" ") { (k, v) -> "$k=$v" }

fun VexaLogEntry.toJson(): String = org.json.JSONObject().apply {
    put("id", id)
    put("timestamp", formatLogTimestamp(timestampMs))
    put("level", level.name)
    put("category", category.name)
    put("message", message)
    put("fields", org.json.JSONObject(fields))
}.toString(2)

/**
 * Debug-console row. WARN+ rows get a tinted wash and border; FATAL rows carry a pulsing
 * left accent bar. Tapping expands a key/value inspector for the structured fields.
 */
@Composable
fun LogRow(
    entry: VexaLogEntry,
    modifier: Modifier = Modifier,
    expanded: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    val c = VexaTheme.colors
    val levelColor = entry.level.toColor()
    val severe = entry.level.ordinal >= LogLevel.WARN.ordinal
    val border by animateColorAsState(
        when {
            expanded -> levelColor.copy(alpha = 0.7f)
            severe -> levelColor.copy(alpha = 0.30f)
            else -> c.hairline
        },
        label = "rowBorder",
    )
    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RowShape)
            .background(if (severe) c.soft(levelColor).copy(alpha = if (c.isDark) 0.08f else 0.06f) else c.surface1)
            .border(1.dp, border, RowShape)
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier),
    ) {
        Row(Modifier.height(IntrinsicSize.Min)) {
            if (entry.level == LogLevel.FATAL) {
                val pulse by rememberInfiniteTransition(label = "fatalBar").animateFloat(
                    0.45f, 1f, infiniteRepeatable(tween(800, easing = LinearEasing), RepeatMode.Reverse), label = "fatalBarA",
                )
                Box(Modifier.width(3.dp).fillMaxHeight().alpha(pulse).background(c.fatal))
            }
            Column(Modifier.weight(1f).padding(horizontal = 12.dp, vertical = 9.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    LevelBadge(entry.level)
                    Spacer(Modifier.width(8.dp))
                    Text(
                        formatLogTimestamp(entry.timestampMs),
                        color = if (expanded) c.textPrimary else c.textTertiary,
                        style = MonospaceTypography.labelSmall,
                    )
                    Spacer(Modifier.width(8.dp))
                    Badge(entry.category.name, entry.category.toColor())
                    Spacer(Modifier.weight(1f))
                    if (entry.fields.isNotEmpty() && onClick != null) {
                        val rot by animateFloatAsState(if (expanded) 90f else 0f, label = "chev")
                        VexaIcon(
                            VexaGlyph.ChevronRight, c.textTertiary, size = 14.dp,
                            modifier = Modifier.rotate(rot),
                        )
                    }
                }
                Spacer(Modifier.height(5.dp))
                Text(
                    text = entry.message,
                    color = entry.level.messageColor(),
                    style = MonospaceTypography.bodySmall,
                    maxLines = if (expanded) Int.MAX_VALUE else 3,
                    overflow = TextOverflow.Ellipsis,
                )
                if (!expanded && entry.fields.isNotEmpty()) {
                    Text(
                        text = entry.fields.entries.joinToString("   ") { (k, v) -> "$k=$v" },
                        color = c.textTertiary,
                        style = MonospaceTypography.labelSmall,
                        modifier = Modifier.padding(top = 3.dp),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
                AnimatedVisibility(
                    visible = expanded && entry.fields.isNotEmpty(),
                    enter = fadeIn() + expandVertically(),
                    exit = fadeOut() + shrinkVertically(),
                ) {
                    FieldInspector(entry)
                }
            }
        }
    }
}

@Composable
private fun FieldInspector(entry: VexaLogEntry) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    Column(Modifier.padding(top = 10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                "STRUCT FIELDS",
                color = c.plasma,
                style = MonospaceTypography.labelSmall.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.1.em),
                modifier = Modifier.weight(1f),
            )
            GhostButton("Copy JSON", onClick = { copyToClipboard(context, entry.toJson()) }, icon = VexaGlyph.Copy)
        }
        Spacer(Modifier.height(8.dp))
        Column(
            Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(10.dp))
                .background(c.well)
                .border(1.dp, c.hairline, RoundedCornerShape(10.dp)),
        ) {
            Row(Modifier.fillMaxWidth().background(c.surface2).padding(horizontal = 12.dp, vertical = 6.dp)) {
                Text("KEY", color = c.textTertiary, style = MonospaceTypography.labelSmall, modifier = Modifier.weight(0.38f))
                Text("VALUE", color = c.textTertiary, style = MonospaceTypography.labelSmall, modifier = Modifier.weight(0.62f))
            }
            entry.fields.entries.forEachIndexed { i, (k, v) ->
                if (i > 0) Box(Modifier.fillMaxWidth().height(1.dp).background(c.hairline))
                Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 7.dp)) {
                    Text(k, color = c.plasma, style = MonospaceTypography.labelMedium, modifier = Modifier.weight(0.38f))
                    Text(v.ifEmpty { "—" }, color = c.textPrimary, style = MonospaceTypography.labelMedium, modifier = Modifier.weight(0.62f))
                }
            }
        }
    }
}

/** Launcher stream row: dot, time, category badge, message. */
@Composable
fun CompactLogRow(entry: VexaLogEntry, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    Row(modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.Top) {
        Box(Modifier.padding(top = 5.dp)) { StatusDot(entry.level.toColor(), size = 6.dp) }
        Spacer(Modifier.width(4.dp))
        Text(
            formatLogTimestamp(entry.timestampMs),
            color = c.textTertiary,
            style = MonospaceTypography.labelSmall,
            modifier = Modifier.padding(top = 2.dp),
        )
        Spacer(Modifier.width(8.dp))
        Badge(entry.category.name, entry.category.toColor())
        Spacer(Modifier.width(8.dp))
        Text(
            entry.message,
            color = entry.level.messageColor(),
            style = MonospaceTypography.labelMedium,
            maxLines = 3,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
fun LogList(
    entries: List<VexaLogEntry>,
    modifier: Modifier = Modifier,
    listState: LazyListState = rememberLazyListState(),
    compact: Boolean = false,
    expandable: Boolean = false,
) {
    if (entries.isEmpty()) {
        LogEmptyState(modifier)
        return
    }
    var expandedId by rememberSaveable { mutableStateOf<Long?>(null) }
    val c = VexaTheme.colors
    val scrollbarAlpha by animateFloatAsState(
        targetValue = if (listState.isScrollInProgress) 0.85f else 0.30f,
        animationSpec = tween(250),
        label = "logScrollbarAlpha",
    )
    LazyColumn(
        state = listState,
        modifier = modifier
            .fillMaxWidth()
            .verticalLogScrollbar(listState, scrollbarAlpha, c.hairline, c.plasma),
        verticalArrangement = Arrangement.spacedBy(if (compact) 0.dp else 6.dp),
        contentPadding = PaddingValues(end = 10.dp),
    ) {
        items(entries, key = { it.id }) { entry ->
            if (compact) {
                CompactLogRow(entry, Modifier.animateItem())
            } else {
                LogRow(
                    entry,
                    modifier = Modifier.animateItem(),
                    expanded = expandable && expandedId == entry.id,
                    onClick = if (expandable) {
                        { expandedId = if (expandedId == entry.id) null else entry.id }
                    } else {
                        null
                    },
                )
            }
        }
    }
}

// Estimated-height scrollbar for a LazyColumn: thumb size tracks the visible
// fraction of the list and brightens while a scroll gesture is in progress.
private fun Modifier.verticalLogScrollbar(
    state: LazyListState,
    alpha: Float,
    track: Color,
    thumb: Color,
): Modifier = drawWithContent {
    drawContent()
    val info = state.layoutInfo
    if (info.totalItemsCount == 0 || info.visibleItemsInfo.isEmpty()) return@drawWithContent
    val viewportH = info.viewportSize.height.toFloat()
    val avgItemH = info.visibleItemsInfo.map { it.size }.average().toFloat()
    val spacingPx = 2.dp.toPx()
    val contentH = info.totalItemsCount * (avgItemH + spacingPx) - spacingPx
    if (contentH <= viewportH) return@drawWithContent

    val trackW = 3.dp.toPx()
    val marginEnd = 1.dp.toPx()
    val marginV = 4.dp.toPx()
    val trackH = viewportH - marginV * 2
    val thumbH = (trackH * viewportH / contentH).coerceIn(24.dp.toPx(), trackH)
    val scrollRange = contentH - viewportH
    val scrollPos =
        state.firstVisibleItemIndex * (avgItemH + spacingPx) + state.firstVisibleItemScrollOffset
    val progress = (scrollPos / scrollRange).coerceIn(0f, 1f)
    val thumbTop = marginV + progress * (trackH - thumbH)
    val left = size.width - trackW - marginEnd

    drawRoundRect(track, Offset(left, marginV), Size(trackW, trackH), CornerRadius(trackW / 2f), alpha = alpha * 0.6f)
    drawRoundRect(thumb, Offset(left, thumbTop), Size(trackW, thumbH), CornerRadius(trackW / 2f), alpha = alpha)
}

@Composable
fun LogEmptyState(
    modifier: Modifier = Modifier,
    hint: String = "No logs yet.",
) {
    val c = VexaTheme.colors
    val blink by rememberInfiniteTransition(label = "cursor").animateFloat(
        0f, 1f, infiniteRepeatable(tween(1000, easing = LinearEasing), RepeatMode.Restart), label = "cursorP",
    )
    Column(
        modifier = modifier.fillMaxWidth().padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        VexaIcon(VexaGlyph.Terminal, tint = c.textTertiary, size = 26.dp)
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(hint, color = c.textTertiary, style = MonospaceTypography.bodySmall, textAlign = TextAlign.Center)
            Box(
                Modifier
                    .padding(start = 3.dp)
                    .width(7.dp)
                    .height(13.dp)
                    .alpha(if (blink < 0.5f) 1f else 0f)
                    .background(c.plasma),
            )
        }
    }
}

@Composable
fun LogFilterChip(
    label: String,
    color: Color,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val bg by animateColorAsState(if (selected) color else c.soft(color), label = "chipBg")
    val fg by animateColorAsState(if (selected) c.onAccent else color, label = "chipFg")
    Row(
        modifier = modifier
            .clip(PillShape)
            .background(bg)
            .border(1.dp, if (selected) Color.Transparent else color.copy(alpha = 0.35f), PillShape)
            .clickable {
                haptics.tick()
                onClick()
            }
            .padding(horizontal = 10.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, color = fg, style = MonospaceTypography.labelSmall.copy(fontWeight = FontWeight.SemiBold))
        AnimatedVisibility(selected) {
            VexaIcon(VexaGlyph.Close, fg, size = 11.dp, strokeWidth = 3f, modifier = Modifier.padding(start = 4.dp))
        }
    }
}
