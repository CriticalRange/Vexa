package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.ui.theme.Ink
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaBlack

@Composable
fun LogRow(
    entry: VexaLogEntry,
    modifier: Modifier = Modifier,
    showMeta: Boolean = true,
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(entry.level.toSoftColor()),
    ) {
        Row(
            modifier = Modifier.padding(start = 10.dp, top = 6.dp, end = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            StatusDot(entry.level.toColor())
            Spacer(Modifier.width(8.dp))
            LevelBadge(entry.level)
            if (showMeta) {
                Spacer(Modifier.width(8.dp))
                Text(
                    text = formatLogTimestamp(entry.timestampMs),
                    color = InkDim,
                    style = MonospaceTypography.labelSmall,
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    text = entry.category.name,
                    color = entry.category.toColor(),
                    style = MonospaceTypography.labelSmall,
                )
            }
        }
        Spacer(Modifier.height(4.dp))
        Text(
            text = entry.message,
            color = Ink,
            style = MonospaceTypography.bodySmall,
            modifier = Modifier.padding(horizontal = 10.dp),
            maxLines = if (showMeta) 3 else 6,
            overflow = TextOverflow.Ellipsis,
        )
        if (showMeta && entry.fields.isNotEmpty()) {
            Text(
                text = entry.fields.entries.joinToString("   ") { (k, v) -> "$k=$v" },
                color = Color(0xFF8A90A0),
                style = MonospaceTypography.labelSmall,
                modifier = Modifier.padding(horizontal = 10.dp, vertical = 2.dp),
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
fun LogList(
    entries: List<VexaLogEntry>,
    modifier: Modifier = Modifier,
    listState: LazyListState = rememberLazyListState(),
) {
    if (entries.isEmpty()) {
        LogEmptyState()
    } else {
        val scrollbarAlpha by androidx.compose.animation.core.animateFloatAsState(
            targetValue = if (listState.isScrollInProgress) 0.85f else 0.30f,
            animationSpec = androidx.compose.animation.core.tween(250),
            label = "logScrollbarAlpha",
        )
        LazyColumn(
            state = listState,
            modifier = modifier
                .fillMaxWidth()
                .verticalLogScrollbar(listState, scrollbarAlpha),
            verticalArrangement = Arrangement.spacedBy(2.dp),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(end = 8.dp),
        ) {
            items(entries, key = { it.id }) { entry ->
                LogRow(entry, showMeta = true, modifier = Modifier.animateItem())
            }
        }
    }
}

// Estimated-height scrollbar for a LazyColumn: thumb size tracks the visible
// fraction of the list and brightens while a scroll gesture is in progress.
private fun Modifier.verticalLogScrollbar(
    state: LazyListState,
    alpha: Float,
): Modifier = drawWithContent {
    drawContent()
    val info = state.layoutInfo
    if (info.totalItemsCount == 0 || info.visibleItemsInfo.isEmpty()) return@drawWithContent
    val viewportH = info.viewportSize.height.toFloat()
    val avgItemH = info.visibleItemsInfo.map { it.size }.average().toFloat()
    val spacingPx = 2.dp.toPx()
    val contentH = info.totalItemsCount * (avgItemH + spacingPx) - spacingPx
    if (contentH <= viewportH) return@drawWithContent

    val trackW = 4.dp.toPx()
    val marginEnd = 2.dp.toPx()
    val marginV = 4.dp.toPx()
    val trackH = viewportH - marginV * 2
    val thumbH = (trackH * viewportH / contentH).coerceIn(24.dp.toPx(), trackH)
    val scrollRange = contentH - viewportH
    val scrollPos =
        state.firstVisibleItemIndex * (avgItemH + spacingPx) + state.firstVisibleItemScrollOffset
    val progress = (scrollPos / scrollRange).coerceIn(0f, 1f)
    val thumbTop = marginV + progress * (trackH - thumbH)
    val left = size.width - trackW - marginEnd

    drawRoundRect(
        color = Color(0xFF2A2E3D),
        topLeft = Offset(left, marginV),
        size = Size(trackW, trackH),
        cornerRadius = CornerRadius(trackW / 2f),
        alpha = alpha * 0.6f,
    )
    drawRoundRect(
        color = com.critical.vexaemulator.ui.theme.Cyan,
        topLeft = Offset(left, thumbTop),
        size = Size(trackW, thumbH),
        cornerRadius = CornerRadius(trackW / 2f),
        alpha = alpha,
    )
}

@Composable
fun LogEmptyState(
    modifier: Modifier = Modifier,
    hint: String = "No logs yet.",
) {
    Box(
        modifier = modifier.fillMaxWidth().padding(24.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = hint,
            color = InkDim,
            style = MonospaceTypography.bodySmall,
            textAlign = TextAlign.Center,
        )
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
    val shape = RoundedCornerShape(100.dp)
    Text(
        text = label,
        color = if (selected) VexaBlack else color,
        style = MonospaceTypography.labelSmall,
        modifier = modifier
            .clip(shape)
            .background(if (selected) color else color.copy(alpha = 0.12f))
            .then(
                if (selected) Modifier
                else Modifier.border(1.dp, color.copy(alpha = 0.4f), shape),
            )
            .clickable { onClick() }
            .padding(horizontal = 10.dp, vertical = 5.dp),
    )
}
