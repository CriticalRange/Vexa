package com.critical.vexaemulator.logging

data class VexaLogEntry(
    val timestampMs: Long,
    val level: LogLevel,
    val category: LogCategory,
    val message: String,
    val fields: Map<String, String> = emptyMap()
)
