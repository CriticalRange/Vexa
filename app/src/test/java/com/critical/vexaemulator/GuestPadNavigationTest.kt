package com.critical.vexaemulator

import com.critical.vexaemulator.input.GuestPadState
import com.critical.vexaemulator.input.PadSink
import com.critical.vexaemulator.input.XKeys
import org.junit.Assert.*
import org.junit.Test

class GuestPadNavigationTest {
    private class Sink : PadSink {
        val direct = mutableListOf<Pair<Boolean, Int>>()
        override fun pointer(type: Int, x: Int, y: Int, state: Int) {}
        override fun pad(control: Int, value: Int) {}
        override fun key(down: Boolean, keysym: Int) {}
        override fun directKey(down: Boolean, keysym: Int) { direct += down to keysym }
    }

    @Test fun `stick emits one direct arrow press and releases at neutral`() {
        val pad = GuestPadState().apply { keyboardNavigationFallback = true }
        val sink = Sink()
        pad.virtLeftY = -1f
        pad.pump(1280, 720, sink)
        pad.pump(1280, 720, sink)
        pad.virtLeftY = 0f
        pad.pump(1280, 720, sink)
        assertEquals(listOf(true to XKeys.UP, false to XKeys.UP), sink.direct)
    }

    @Test fun `opposing directions cancel and release restores remaining direction`() {
        val pad = GuestPadState().apply { keyboardNavigationFallback = true }
        val sink = Sink()
        pad.setDpad(0, true)
        pad.pump(1280, 720, sink)
        pad.virtLeftY = 1f
        pad.pump(1280, 720, sink)
        pad.setDpad(0, false)
        pad.pump(1280, 720, sink)
        pad.virtLeftY = 0f
        pad.pump(1280, 720, sink)
        assertEquals(listOf(true to XKeys.UP, false to XKeys.UP,
            true to XKeys.DOWN, false to XKeys.DOWN), sink.direct)
    }

    @Test fun `other games keep native pad navigation without direct key duplicates`() {
        val pad = GuestPadState()
        val sink = Sink()
        pad.virtLeftX = 1f
        pad.setDpad(0, true)
        pad.pump(1280, 720, sink)
        assertTrue(sink.direct.isEmpty())
    }
}
