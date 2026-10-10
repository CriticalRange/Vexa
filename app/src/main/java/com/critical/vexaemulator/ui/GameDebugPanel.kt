package com.critical.vexaemulator.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import kotlinx.coroutines.flow.first
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.ui.theme.Cyan
import com.critical.vexaemulator.ui.theme.Edge
import com.critical.vexaemulator.ui.theme.Error
import com.critical.vexaemulator.ui.theme.Info
import com.critical.vexaemulator.ui.theme.Success
import com.critical.vexaemulator.ui.theme.SurfaceBase

@Composable
fun GameDebugPanel(
    hideFilters: Boolean,
    onClose: () -> Unit,
    onHideFilters: () -> Unit
) {
    val rawEntries by LogStore.entries.collectAsState()

    var selectedLevels by remember { mutableStateOf(setOf<LogLevel>()) }
    var selectedCategories by remember { mutableStateOf(setOf<LogCategory>()) }

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

    val (filteredEntries, errorCount) = remember(entries, selectedLevels, selectedCategories) {
        var errors = 0
        val filtered = entries.filter { entry ->
            if (entry.level.ordinal >= LogLevel.ERROR.ordinal) errors++
            val levelMatches = selectedLevels.isEmpty() || entry.level in selectedLevels
            val categoryMatches =
                selectedCategories.isEmpty() || entry.category in selectedCategories
            levelMatches && categoryMatches
        }
        filtered to errors
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.6f))
    ) {
        PanelSurface(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth(0.94f)
                .fillMaxHeight(0.9f),
            color = SurfaceBase.copy(alpha = 0.98f),
            borderColor = Edge,
            accentEdge = AccentEdge.Top,
        ) {
            Column(modifier = Modifier.padding(14.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    SectionLabel(
                        text = "Debug Console",
                        modifier = Modifier.weight(1f),
                    )
                    GhostButton(text = "Filters", onClick = onHideFilters)
                    Spacer(modifier = Modifier.width(8.dp))
                    GhostButton(text = "Close", onClick = onClose)
                }

                Spacer(modifier = Modifier.height(10.dp))
                StatRow(
                    stats = listOf(
                        Triple("logs", entries.size.toString(), Info),
                        Triple(
                            "err",
                            errorCount.toString(),
                            if (errorCount == 0) Success else Error,
                        ),
                        Triple("filtered", filteredEntries.size.toString(), Cyan),
                    ),
                )

                AnimatedVisibility(
                    visible = !hideFilters,
                    enter = fadeIn() + expandVertically(),
                    exit = fadeOut() + shrinkVertically()
                ) {
                    Column {
                        Spacer(modifier = Modifier.height(12.dp))
                        SectionLabel(text = "Levels")
                        Spacer(modifier = Modifier.height(8.dp))
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            LogLevel.entries.forEach { level ->
                                LogFilterChip(
                                    label = level.name,
                                    color = level.toColor(),
                                    selected = level in selectedLevels,
                                    onClick = {
                                        selectedLevels = if (level in selectedLevels) {
                                            selectedLevels - level
                                        } else {
                                            selectedLevels + level
                                        }
                                    }
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(10.dp))
                        SectionLabel(text = "Categories")
                        Spacer(modifier = Modifier.height(8.dp))
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            LogCategory.entries.forEach { category ->
                                LogFilterChip(
                                    label = category.name,
                                    color = category.toColor(),
                                    selected = category in selectedCategories,
                                    onClick = {
                                        selectedCategories =
                                            if (category in selectedCategories) {
                                                selectedCategories - category
                                            } else {
                                                selectedCategories + category
                                            }
                                    }
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                    }
                }

                Spacer(modifier = Modifier.height(12.dp))

                LogList(
                    entries = filteredEntries,
                    modifier = Modifier.fillMaxSize(),
                    listState = listState,
                )
            }
        }
    }
}
