package com.critical.vexaemulator.logging

import java.util.concurrent.atomic.AtomicLong

object VexaLogger {
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
    }
}
