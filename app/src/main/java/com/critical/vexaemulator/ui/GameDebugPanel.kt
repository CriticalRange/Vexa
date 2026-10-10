package com.critical.vexaemulator.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.PanelShape
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

@Composable
fun GameDebugPanel(
    hideFilters: Boolean,
    onClose: () -> Unit,
    onHideFilters: () -> Unit,
    subtitle: String? = null,
    initialLevels: Set<LogLevel> = emptySet(),
) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val rawEntries by LogStore.entries.collectAsState()

    var selectedLevels by remember { mutableStateOf(initialLevels) }
    var selectedCategories by remember { mutableStateOf(setOf<LogCategory>()) }
    var query by remember { mutableStateOf("") }

    // Freeze the list while the user scrolls: new log bursts wait for the
    // gesture to end instead of forcing a recomposition + O(n) filter pass
    // mid-frame. The gate also covers the stats and LazyColumn relayout.
    val listState = rememberLazyListState()
    var displayedEntries by remember { mutableStateOf(rawEntries) }
    LaunchedEffect(rawEntries) {
        if (listState.isScrollInProgress) {
            snapshotFlow { listState.isScrollInProgress }.first { !it }
        }
        displayedEntries = rawEntries
    }
    val entries = displayedEntries

    val terms = remember(query) { query.split('|').map { it.trim() }.filter { it.isNotEmpty() } }
    val (filteredEntries, errorCount) = remember(entries, selectedLevels, selectedCategories, terms) {
        var errors = 0
        val filtered = entries.filter { entry ->
            if (entry.level.ordinal >= LogLevel.ERROR.ordinal) errors++
            val levelMatches = selectedLevels.isEmpty() || entry.level in selectedLevels
            val categoryMatches =
                selectedCategories.isEmpty() || entry.category in selectedCategories
            val textMatches = terms.isEmpty() || terms.any { t ->
                entry.message.contains(t, ignoreCase = true) ||
                    entry.category.name.contains(t, ignoreCase = true) ||
                    entry.fields.any { (k, v) -> k.contains(t, true) || v.contains(t, true) }
            }
            levelMatches && categoryMatches && textMatches
        }
        filtered to errors
    }
    val activeFilters = selectedLevels.size + selectedCategories.size + if (terms.isEmpty()) 0 else 1

    // Follow the tail while the user sits at the bottom; otherwise count unseen rows.
    val scope = rememberCoroutineScope()
    val atBottom by remember { derivedStateOf { !listState.canScrollForward } }
    var seenCount by remember { mutableIntStateOf(filteredEntries.size) }
    LaunchedEffect(filteredEntries.size) {
        if (atBottom || seenCount > filteredEntries.size) {
            if (filteredEntries.isNotEmpty()) listState.scrollToItem(filteredEntries.lastIndex)
            seenCount = filteredEntries.size
        }
    }
    LaunchedEffect(atBottom) { if (atBottom) seenCount = filteredEntries.size }
    val unseen = (filteredEntries.size - seenCount).coerceAtLeast(0)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(c.scrim)
            .clickable(remember { MutableInteractionSource() }, indication = null, onClick = onClose),
    ) {
        PanelSurface(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth(0.94f)
                .fillMaxHeight(0.92f)
                .clickable(remember { MutableInteractionSource() }, indication = null) {},
            shape = PanelShape,
            color = c.bgElevated.copy(alpha = 0.98f),
            accentEdge = AccentEdge.Top,
        ) {
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth().padding(start = 16.dp, end = 12.dp, top = 12.dp, bottom = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    IconTile(VexaGlyph.Terminal, c.plasma, size = 34.dp)
                    Spacer(Modifier.width(10.dp))
                    Text("Debug Console", style = MaterialTheme.typography.titleLarge.copy(color = c.textPrimary))
                    Spacer(Modifier.width(10.dp))
                    TinyPill("live", c.success, dot = true, pulse = true)
                    subtitle?.let {
                        Spacer(Modifier.width(10.dp))
                        Text(it, style = MonospaceTypography.labelSmall.copy(color = c.textTertiary), maxLines = 1)
                    }
                    Spacer(Modifier.weight(1f))
                    GhostButton(
                        text = if (activeFilters > 0) "Filters ($activeFilters)" else "Filters",
                        onClick = onHideFilters,
                        icon = VexaGlyph.Filter,
                        active = !hideFilters || activeFilters > 0,
                    )
                    Spacer(Modifier.width(8.dp))
                    GhostButton(
                        text = "Copy",
                        onClick = { copyToClipboard(context, filteredEntries.joinToString("\n") { it.toPlainLine() }) },
                        icon = VexaGlyph.Copy,
                    )
                    Spacer(Modifier.width(10.dp))
                    Box(Modifier.width(1.dp).height(22.dp).background(c.hairline))
                    Spacer(Modifier.width(10.dp))
                    VexaIconButton(VexaGlyph.Close, onClose)
                }
                Divider()
                Row(
                    Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    StatTile("logs", entries.size, c.info) {
                        selectedLevels = emptySet()
                        selectedCategories = emptySet()
                    }
                    StatTile("err", errorCount, if (errorCount == 0) c.success else c.error) {
                        val errs = setOf(LogLevel.ERROR, LogLevel.FATAL)
                        selectedLevels = if (selectedLevels == errs) emptySet() else errs
                    }
                    StatTile("filtered", filteredEntries.size, c.plasma)
                    Spacer(Modifier.weight(1f))
                    SearchField(query, { query = it }, Modifier.width(260.dp))
                }

                AnimatedVisibility(
                    visible = !hideFilters,
                    enter = fadeIn() + expandVertically(),
                    exit = fadeOut() + shrinkVertically(),
                ) {
                    Column {
                        Divider()
                        Column(Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            FilterRow("Levels") {
                                LogLevel.entries.forEach { level ->
                                    LogFilterChip(
                                        label = level.name,
                                        color = level.toColor(),
                                        selected = level in selectedLevels,
                                        onClick = {
                                            selectedLevels = if (level in selectedLevels) selectedLevels - level else selectedLevels + level
                                        },
                                    )
                                }
                            }
                            FilterRow("Categories") {
                                LogCategory.entries.forEach { category ->
                                    LogFilterChip(
                                        label = category.name,
                                        color = category.toColor(),
                                        selected = category in selectedCategories,
                                        onClick = {
                                            selectedCategories =
                                                if (category in selectedCategories) selectedCategories - category else selectedCategories + category
                                        },
                                    )
                                }
                            }
                        }
                    }
                }
                Divider()

                Box(Modifier.weight(1f).fillMaxWidth()) {
                    LogList(
                        entries = filteredEntries,
                        modifier = Modifier.fillMaxSize().padding(start = 12.dp, end = 4.dp, top = 8.dp),
                        listState = listState,
                        expandable = true,
                    )
                    UnseenPill(
                        count = unseen,
                        onClick = {
                            scope.launch { listState.animateScrollToItem(filteredEntries.lastIndex.coerceAtLeast(0)) }
                        },
                        modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 10.dp),
                    )
                }

                Divider()
                Row(
                    Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 7.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "MATCH ${filteredEntries.size} / ${entries.size}",
                        style = MonospaceTypography.labelSmall.copy(color = c.textTertiary, letterSpacing = 0.06.em),
                    )
                    if (errorCount > 0) {
                        Spacer(Modifier.width(12.dp))
                        Text(
                            "$errorCount CRITICAL",
                            style = MonospaceTypography.labelSmall.copy(color = c.error, letterSpacing = 0.06.em),
                        )
                    }
                    Spacer(Modifier.weight(1f))
                    Text(
                        "TAP A ROW TO INSPECT FIELDS",
                        style = MonospaceTypography.labelSmall.copy(color = c.textTertiary, letterSpacing = 0.06.em),
                    )
                }
            }
        }
    }
}

@Composable
private fun UnseenPill(count: Int, onClick: () -> Unit, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    AnimatedVisibility(
        visible = count > 0,
        enter = fadeIn() + slideInVertically { it } + scaleIn(initialScale = 0.9f),
        exit = fadeOut() + slideOutVertically { it } + scaleOut(targetScale = 0.9f),
        modifier = modifier,
    ) {
        Row(
            Modifier
                .vexaGlow(radius = 12.dp, shape = PillShape)
                .clip(PillShape)
                .background(c.brand())
                .clickable(onClick = onClick)
                .padding(horizontal = 14.dp, vertical = 7.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            VexaIcon(VexaGlyph.ArrowDown, c.onAccent, size = 14.dp, strokeWidth = 2.6f)
            Spacer(Modifier.width(6.dp))
            Text(
                "$count new logs",
                style = MonospaceTypography.labelMedium.copy(color = c.onAccent, fontWeight = FontWeight.Bold),
            )
        }
    }
}

@Composable
private fun Divider() {
    Box(Modifier.fillMaxWidth().height(1.dp).background(VexaTheme.colors.hairline))
}

@Composable
private fun FilterRow(label: String, chips: @Composable () -> Unit) {
    val c = VexaTheme.colors
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            label.uppercase(),
            style = MonospaceTypography.labelSmall.copy(color = c.textTertiary, letterSpacing = 0.1.em),
            modifier = Modifier.width(92.dp),
        )
        Row(
            Modifier.horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) { chips() }
    }
}

@Composable
private fun SearchField(value: String, onChange: (String) -> Unit, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    val shape = RoundedCornerShape(10.dp)
    Row(
        modifier
            .height(36.dp)
            .clip(shape)
            .background(c.well)
            .border(1.dp, if (value.isNotEmpty()) c.plasma.copy(alpha = 0.7f) else c.hairline, shape)
            .padding(horizontal = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        VexaIcon(VexaGlyph.Search, if (value.isNotEmpty()) c.plasma else c.textTertiary, size = 15.dp)
        Spacer(Modifier.width(8.dp))
        Box(Modifier.weight(1f), contentAlignment = Alignment.CenterStart) {
            if (value.isEmpty()) {
                Text("grep logs…  a|b", style = MonospaceTypography.labelMedium.copy(color = c.textTertiary))
            }
            BasicTextField(
                value = value,
                onValueChange = onChange,
                singleLine = true,
                textStyle = MonospaceTypography.labelMedium.copy(color = c.plasma),
                cursorBrush = SolidColor(c.plasma),
                modifier = Modifier.fillMaxWidth(),
            )
        }
        if (value.isNotEmpty()) {
            VexaIcon(
                VexaGlyph.Close, c.textTertiary, size = 14.dp,
                modifier = Modifier.clip(PillShape).clickable { onChange("") },
            )
        }
    }
}
