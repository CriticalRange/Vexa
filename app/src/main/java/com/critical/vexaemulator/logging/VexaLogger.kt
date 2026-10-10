package com.critical.vexaemulator.logging

import android.util.Log
import java.util.concurrent.atomic.AtomicLong

object VexaLogger {
    private const val LOGCAT_TAG = "VEXA"
    private val nextId = AtomicLong(1L)
    fun log(
        level: LogLevel,
        category: LogCategory,
        message: String,
        fields: Map<String, String> = emptyMap()
    ) {
        val entry = VexaLogEntry(
            id = nextId.getAndIncrement(),
            timestampMs = System.currentTimeMillis(),
            level = level,
            category = category,
            message = message,
            fields = fields
        )
        LogStore.append(entry)
        fanOutToLogcat(entry)
    }

    private fun fanOutToLogcat(entry: VexaLogEntry) {
        val body = buildString {
            append('[').append(entry.category.name).append("] ")
            append(entry.message)
            for ((key, value) in entry.fields) {
                append(' ').append(key).append('=').append(value)
            }
        }
        val prio = when (entry.level) {
            LogLevel.DEBUG -> Log.DEBUG
            LogLevel.INFO -> Log.INFO
            LogLevel.WARN -> Log.WARN
            LogLevel.ERROR, LogLevel.FATAL -> Log.ERROR
        }
        runCatching { Log.println(prio, LOGCAT_TAG, body) }
    }
}
