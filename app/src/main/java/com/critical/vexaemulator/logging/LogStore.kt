package com.critical.vexaemulator.logging

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

object LogStore {
    private const val MAX_ENTRIES = 4000
    private const val FLUSH_MS = 120L

    private val _entries =
        MutableStateFlow<List<VexaLogEntry>>(emptyList())
    val entries: StateFlow<List<VexaLogEntry>> = _entries

    private val _lastFatal = MutableStateFlow<VexaLogEntry?>(null)
    val lastFatal: StateFlow<VexaLogEntry?> = _lastFatal

    private val lock = Any()
    private val pending = ArrayList<VexaLogEntry>()

    @Volatile
    private var flushScheduled = false

    fun append(entry: VexaLogEntry) {
        synchronized(lock) { pending.add(entry) }

        if (entry.level == LogLevel.FATAL) {
            _lastFatal.value = entry
        }
        scheduleFlush()
    }

    // Appends are O(1); snapshots publish at most ~8x/sec so heavy guest log
    // bursts don't turn every line into an O(n) list copy on the main thread.
    private fun scheduleFlush() {
        if (flushScheduled) return
        flushScheduled = true
        Thread {
            Thread.sleep(FLUSH_MS)
            val drained: List<VexaLogEntry>
            synchronized(lock) {
                drained = ArrayList(pending)
                pending.clear()
            }
            flushScheduled = false
            if (drained.isNotEmpty()) {
                _entries.value = mergeCapped(_entries.value, drained)
            }
            synchronized(lock) {
                if (pending.isNotEmpty()) scheduleFlush()
            }
        }.apply {
            isDaemon = true
            name = "VexaLogStoreFlush"
        }.start()
    }

    private fun mergeCapped(
        current: List<VexaLogEntry>,
        add: List<VexaLogEntry>,
    ): List<VexaLogEntry> {
        if (current.isEmpty() && add.size <= MAX_ENTRIES) return add
        val merged = ArrayList<VexaLogEntry>(current.size + add.size)
        merged.addAll(current)
        merged.addAll(add)
        if (merged.size > MAX_ENTRIES) {
            return ArrayList(merged.subList(merged.size - MAX_ENTRIES, merged.size))
        }
        return merged
    }

    fun clear() {
        synchronized(lock) { pending.clear() }
        _entries.value = emptyList()
        _lastFatal.value = null
    }
}
