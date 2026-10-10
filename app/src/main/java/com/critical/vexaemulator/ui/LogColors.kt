package com.critical.vexaemulator.ui

import androidx.compose.ui.graphics.Color
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.ui.theme.Blue
import com.critical.vexaemulator.ui.theme.Cyan
import com.critical.vexaemulator.ui.theme.Error
import com.critical.vexaemulator.ui.theme.Fatal
import com.critical.vexaemulator.ui.theme.Info
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.Pink
import com.critical.vexaemulator.ui.theme.Success
import com.critical.vexaemulator.ui.theme.Teal
import com.critical.vexaemulator.ui.theme.Violet
import com.critical.vexaemulator.ui.theme.Warn

/**
 * Single source of truth for log-related colors and timestamp formatting.
 */

/** Returns the badge color and short label for a [LogLevel]. */
fun levelBadge(level: LogLevel): Pair<Color, String> = when (level) {
    LogLevel.DEBUG -> InkDim to "DBG"
    LogLevel.INFO -> Info to "INFO"
    LogLevel.WARN -> Warn to "WARN"
    LogLevel.ERROR -> Error to "ERR"
    LogLevel.FATAL -> Fatal to "FATAL"
}

/** The badge color for this level. */
fun LogLevel.toColor(): Color = levelBadge(this).first

/** A soft (low-alpha) variant of this level's color, for row backgrounds. */
fun LogLevel.toSoftColor(): Color = toColor().copy(alpha = softAlpha(this))

private fun softAlpha(level: LogLevel): Float = when (level) {
    LogLevel.DEBUG -> 0f
    LogLevel.INFO -> 0.06f
    LogLevel.WARN -> 0.06f
    LogLevel.ERROR -> 0.07f
    LogLevel.FATAL -> 0.08f
}

/** The accent color for this log category. */
fun LogCategory.toColor(): Color = when (this) {
    LogCategory.BOOT -> Violet
    LogCategory.UI -> Blue
    LogCategory.ACTIVITY -> Cyan
    LogCategory.SURFACE -> Teal
    LogCategory.RUNTIME -> Success
    LogCategory.CONFIG -> Warn
    LogCategory.FAILURE -> Error
    LogCategory.ROOT -> Pink
}

private val logTimestampFormat = java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.US)

/** Formats an epoch millisecond timestamp as wall-clock "HH:mm:ss.SSS". */
fun formatLogTimestamp(timestampMs: Long): String =
    logTimestampFormat.format(java.util.Date(timestampMs))
