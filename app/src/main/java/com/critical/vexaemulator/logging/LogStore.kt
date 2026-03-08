package com.critical.vexaemulator.logging

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

object LogStore {
    private val _entries =
        MutableStateFlow<List<VexaLogEntry>>(emptyList())
    val entries: StateFlow<List<VexaLogEntry>> = _entries

    private val _lastFatal = MutableStateFlow<VexaLogEntry?>(null)
    val lastFatal: StateFlow<VexaLogEntry?> = _lastFatal
    fun append(entry: VexaLogEntry) {
        _entries.value = _entries.value + entry

        if (entry.level == LogLevel.FATAL) {
            _lastFatal.value = entry
        }
    }

    fun clear() {
        _entries.value = emptyList()
        _lastFatal.value = null
    }
}