package com.critical.vexaemulator.logging

data class VexaLogEntry(
    val id: Long,
    val timestampMs: Long,
    val level: LogLevel,
    val category: LogCategory,
    val message: String,
    val fields: Map<String, String> = emptyMap()
)
