package com.critical.vexaemulator.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.graphics.Color
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.ui.theme.VexaTheme

/**
 * Single source of truth for log-related colors and timestamp formatting.
 */

fun LogLevel.shortLabel(): String = when (this) {
    LogLevel.DEBUG -> "DBG"
    LogLevel.INFO -> "INFO"
    LogLevel.WARN -> "WARN"
    LogLevel.ERROR -> "ERR"
    LogLevel.FATAL -> "FATAL"
}

/** The badge color for this level in the active palette. */
@Composable
@ReadOnlyComposable
fun LogLevel.toColor(): Color {
    val c = VexaTheme.colors
    return when (this) {
        LogLevel.DEBUG -> c.textTertiary
        LogLevel.INFO -> c.info
        LogLevel.WARN -> c.warning
        LogLevel.ERROR -> c.error
        LogLevel.FATAL -> c.fatal
    }
}

/** Message tint: severities from WARN up color the text itself. */
@Composable
@ReadOnlyComposable
fun LogLevel.messageColor(): Color {
    val c = VexaTheme.colors
    return when (this) {
        LogLevel.DEBUG -> c.textSecondary
        LogLevel.INFO -> c.textPrimary
        LogLevel.WARN -> c.warning
        LogLevel.ERROR, LogLevel.FATAL -> c.error
    }
}

/** The accent color for this log category in the active palette. */
@Composable
@ReadOnlyComposable
fun LogCategory.toColor(): Color {
    val c = VexaTheme.colors
    return when (this) {
        LogCategory.BOOT -> c.ultraviolet
        LogCategory.UI -> c.ion
        LogCategory.ACTIVITY -> c.pink
        LogCategory.SURFACE -> c.teal
        LogCategory.RUNTIME -> c.success
        LogCategory.CONFIG -> c.warning
        LogCategory.FAILURE -> c.error
        LogCategory.ROOT -> c.magenta
    }
}

private val logTimestampFormat = java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.US)

/** Formats an epoch millisecond timestamp as wall-clock "HH:mm:ss.SSS". */
fun formatLogTimestamp(timestampMs: Long): String =
    logTimestampFormat.format(java.util.Date(timestampMs))
