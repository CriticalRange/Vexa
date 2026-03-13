package com.critical.vexaemulator.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.logging.VexaLogEntry
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun GameDebugPanel(
    hideFilters: Boolean,
    onClose: () -> Unit,
    onHideFilters: () -> Unit
) {
    val entries = LogStore.entries.collectAsState().value

    var selectedLevels by remember { mutableStateOf(setOf<LogLevel>()) }
    var selectedCategories by remember { mutableStateOf(setOf<LogCategory>()) }

    val filteredEntries = remember(entries, selectedLevels, selectedCategories) {
        entries.filter { entry ->
            val levelMatches = selectedLevels.isEmpty() || entry.level in selectedLevels
            val categoryMatches =
                selectedCategories.isEmpty() || entry.category in selectedCategories
            levelMatches && categoryMatches
        }
    }
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0x99000000))
    ) {
        Column(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth(0.96f)
                .fillMaxHeight(0.94f)
                .background(
                    color = Color(0xFF111111),
                    shape = RoundedCornerShape(14.dp)
                )
                .border(
                    width = 1.dp,
                    color = Color(0xFF3A3A3A),
                    shape = RoundedCornerShape(14.dp)
                )
                .padding(14.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = "Debug Console",
                    color = Color.White,
                    fontSize = 20.sp
                )
                Row(
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Button(
                        onClick = onHideFilters
                    ) {
                        Text("Filters")
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(
                        onClick = onClose
                    ) {
                        Text("Close")
                    }
                }
            }

            Spacer(modifier = Modifier.height(12.dp))
            HorizontalDivider(color = Color(0xFF2E2E2E))
            Spacer(modifier = Modifier.height(12.dp))

            AnimatedVisibility(
                visible = !hideFilters,
                enter = fadeIn() + expandVertically(),
                exit = fadeOut() + shrinkVertically()
            ) {
                Column {
                    Text(
                        text = "Levels",
                        color = Color(0xFFE0E0E0),
                        fontSize = 14.sp
                    )

                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .horizontalScroll(rememberScrollState()),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        LogLevel.entries.forEach { level ->
                            FilterToggle(
                                label = level.name,
                                checked = level in selectedLevels,
                                onCheckedChange = { checked ->
                                    selectedLevels = if (checked) {
                                        selectedLevels + level
                                    } else {
                                        selectedLevels - level
                                    }
                                }
                            )
                        }
                    }

                    Spacer(modifier = Modifier.height(10.dp))

                    Text(
                        text = "Categories",
                        color = Color(0xFFE0E0E0),
                        fontSize = 14.sp
                    )

                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .horizontalScroll(rememberScrollState()),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        LogCategory.entries.forEach { category ->
                            FilterToggle(
                                label = category.name,
                                checked = category in selectedCategories,
                                onCheckedChange = { checked ->
                                    selectedCategories = if (checked) {
                                        selectedCategories + category
                                    } else {
                                        selectedCategories - category
                                    }
                                }
                            )
                        }
                    }
                    Spacer(modifier = Modifier.height(12.dp))
                    HorizontalDivider(color = Color(0xFF2E2E2E))
                    Spacer(modifier = Modifier.height(12.dp))
                }
            }

            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(
                    items = filteredEntries,
                    key = { "${it.id}-${it.timestampMs}-${it.category}-${it.message}" }
                ) { entry ->
                    LogEntryRow(entry = entry)
                }
            }

        }
    }
}

@Composable
private fun FilterToggle(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(end = 8.dp)
    ) {
        Checkbox(
            checked = checked,
            onCheckedChange = onCheckedChange
        )
        Text(
            text = label,
            color = Color.White,
            fontSize = 13.sp
        )
    }
}

@Composable
private fun LogEntryRow(
    entry: VexaLogEntry
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color(0xFF181818), RoundedCornerShape(10.dp))
            .padding(10.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = formatTimestamp(entry.timestampMs),
                color = Color(0xFF9E9E9E),
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace
            )

            Spacer(modifier = Modifier.width(8.dp))

            Text(
                text = entry.level.name,
                color = levelColor(entry.level),
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace
            )

            Spacer(modifier = Modifier.width(8.dp))

            Text(
                text = entry.message,
                color = Color.White,
                fontSize = 14.sp
            )

            Spacer(modifier = Modifier.weight(1f))

            if (entry.fields.isNotEmpty()) {
                Spacer(modifier = Modifier.height(6.dp))

                Text(
                    text = entry.fields.entries.joinToString("  ") { "${it.key}=${it.value}" },
                    color = Color(0xFFBDBDBD),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }
    }
}

private fun formatTimestamp(timestampMs: Long): String {
    val formatter = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)
    return formatter.format(Date(timestampMs))
}

private fun levelColor(level: LogLevel): Color {
    return when (level) {
        LogLevel.DEBUG -> Color(0xFF9E9E9E)
        LogLevel.INFO -> Color(0xFF64B5F6)
        LogLevel.WARN -> Color(0xFFFFB74D)
        LogLevel.ERROR -> Color(0xFFE57373)
        LogLevel.FATAL -> Color(0xFFFF5252)
    }
}
