package com.critical.vexaemulator.logging

object VexaLogger {
    fun log(
        level: LogLevel,
        category: LogCategory,
        message: String,
        fields: Map<String, String> = emptyMap()
    ) {
        val entry = VexaLogEntry(
            timestampMs = System.currentTimeMillis(),
            level = level,
            category = category,
            message = message,
            fields = fields
        )

        LogStore.append(entry)
    }
}
