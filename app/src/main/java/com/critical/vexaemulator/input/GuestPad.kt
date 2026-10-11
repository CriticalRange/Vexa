package com.critical.vexaemulator.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.drag
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.roundToInt

private const val AIM_DEADZONE = 0.22f
private const val AIM_STEP_PX = 96f
private const val MOVE_DEADZONE = 0.28f

/** X11 keysyms. libX11_fake.c maps these onto evdev keycodes, not ASCII values. */
object XKeys {
    const val SPACE = 0x20
    const val TAB = 0xff09
    const val RETURN = 0xff0d
    const val ESCAPE = 0xff1b
    const val LEFT = 0xff51
    const val UP = 0xff52
    const val RIGHT = 0xff53
    const val DOWN = 0xff54
    const val SHIFT_L = 0xffe1
    const val CONTROL_L = 0xffe3
    const val A = 0x61
    const val C = 0x63
    const val D = 0x64
    const val E = 0x65
    const val R = 0x72
    const val S = 0x73
    const val W = 0x77
}

/**
 * Physical sticks and the on-screen pad share one stream.
 * Left stick is WASD. D-pad is the arrow keys. Right stick moves the pointer.
 * A and RT left-click. RB right-clicks. B is Escape, X is Space, Y is E.
 * LB is Shift, LT is Control, Start is Enter, Back is Tab, L3 is C, R3 is R.
 * Raw screen taps stay clicks only while the overlay is off.
 */
class GuestPadState {
    var overlay by mutableStateOf(false)
    var aimX by mutableIntStateOf(-1)
    var aimY by mutableIntStateOf(-1)
    var virtLeftX by mutableFloatStateOf(0f)
    var virtLeftY by mutableFloatStateOf(0f)
    var virtRightX by mutableFloatStateOf(0f)
    var virtRightY by mutableFloatStateOf(0f)
    var virtLeftClick by mutableStateOf(false)
    var virtRightClick by mutableStateOf(false)
    var virtLt by mutableStateOf(false)
    var virtRt by mutableStateOf(false)
    var onFaceKey: (Boolean, Int) -> Unit = { _, _ -> }

    private var physLeftX = 0f
    private var physLeftY = 0f
    private var physRightX = 0f
    private var physRightY = 0f
    private var physLeftClick = false
    private var physRightClick = false
    private var physLtKey = false
    private var physRtKey = false
    private var physLtAxis = false
    private var physRtAxis = false
    private var hatX = 0f
    private var hatY = 0f
    private val dpadKey = BooleanArray(4)
    private val dpadVirt = BooleanArray(4)
    private val held = HashSet<Int>()
    private var leftDown = false
    private var rightDown = false

    fun setDpad(index: Int, down: Boolean) {
        if (index in dpadVirt.indices) dpadVirt[index] = down
    }

    fun onMotion(event: MotionEvent): Boolean {
        if (!isController(event)) return false
        physLeftX = stickAxis(event, MotionEvent.AXIS_X)
        physLeftY = stickAxis(event, MotionEvent.AXIS_Y)
        hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        val z = stickAxis(event, MotionEvent.AXIS_Z)
        val rz = stickAxis(event, MotionEvent.AXIS_RZ)
        val rx = stickAxis(event, MotionEvent.AXIS_RX)
        val ry = stickAxis(event, MotionEvent.AXIS_RY)
        if (abs(z) + abs(rz) >= abs(rx) + abs(ry)) {
            physRightX = z
            physRightY = rz
        } else {
            physRightX = rx
            physRightY = ry
        }
        physLtAxis = triggerAxis(event, MotionEvent.AXIS_LTRIGGER) > 0.40f ||
            triggerAxis(event, MotionEvent.AXIS_BRAKE) > 0.40f
        physRtAxis = triggerAxis(event, MotionEvent.AXIS_RTRIGGER) > 0.40f ||
            triggerAxis(event, MotionEvent.AXIS_GAS) > 0.40f
        return true
    }

    fun onKey(event: KeyEvent, sink: PadSink): Boolean {
        if (!isController(event)) return false
        if (event.repeatCount > 0) return true
        val down = event.action == KeyEvent.ACTION_DOWN
        when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> physLeftClick = down
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK -> sink.key(down, XKeys.ESCAPE)
            KeyEvent.KEYCODE_BUTTON_X -> sink.key(down, XKeys.SPACE)
            KeyEvent.KEYCODE_BUTTON_Y -> sink.key(down, XKeys.E)
            KeyEvent.KEYCODE_BUTTON_L1 -> sink.key(down, XKeys.SHIFT_L)
            KeyEvent.KEYCODE_BUTTON_R1 -> physRightClick = down
            KeyEvent.KEYCODE_BUTTON_L2 -> physLtKey = down
            KeyEvent.KEYCODE_BUTTON_R2 -> physRtKey = down
            KeyEvent.KEYCODE_BUTTON_START -> sink.key(down, XKeys.RETURN)
            KeyEvent.KEYCODE_BUTTON_SELECT -> sink.key(down, XKeys.TAB)
            KeyEvent.KEYCODE_BUTTON_THUMBL -> sink.key(down, XKeys.C)
            KeyEvent.KEYCODE_BUTTON_THUMBR -> sink.key(down, XKeys.R)
            KeyEvent.KEYCODE_DPAD_UP -> dpadKey[0] = down
            KeyEvent.KEYCODE_DPAD_DOWN -> dpadKey[1] = down
            KeyEvent.KEYCODE_DPAD_LEFT -> dpadKey[2] = down
            KeyEvent.KEYCODE_DPAD_RIGHT -> dpadKey[3] = down
            else -> return false
        }
        return true
    }

    fun pump(width: Int, height: Int, sink: PadSink) {
        val lx = pick(virtLeftX, physLeftX)
        val ly = pick(virtLeftY, physLeftY)
        hold(XKeys.W, ly < -MOVE_DEADZONE, sink)
        hold(XKeys.S, ly > MOVE_DEADZONE, sink)
        hold(XKeys.A, lx < -MOVE_DEADZONE, sink)
        hold(XKeys.D, lx > MOVE_DEADZONE, sink)

        val up = dpadKey[0] || dpadVirt[0] || hatY < -0.5f
        val down = dpadKey[1] || dpadVirt[1] || hatY > 0.5f
        val left = dpadKey[2] || dpadVirt[2] || hatX < -0.5f
        val right = dpadKey[3] || dpadVirt[3] || hatX > 0.5f
        hold(XKeys.UP, up && !down, sink)
        hold(XKeys.DOWN, down && !up, sink)
        hold(XKeys.LEFT, left && !right, sink)
        hold(XKeys.RIGHT, right && !left, sink)
        hold(XKeys.CONTROL_L, virtLt || physLtKey || physLtAxis, sink)

        if (width > 1 && height > 1 && aimX < 0) {
            aimX = width / 2
            aimY = height / 2
        }
        val rx = pick(virtRightX, physRightX)
        val ry = pick(virtRightY, physRightY)
        val mag = hypot(rx, ry)
        if (width > 1 && height > 1 && mag > AIM_DEADZONE) {
            val scale = ((mag - AIM_DEADZONE) / (1f - AIM_DEADZONE)).coerceIn(0f, 1f)
            val step = AIM_STEP_PX * scale
            val x = (aimX + rx / mag * step).roundToInt().coerceIn(0, width - 1)
            val y = (aimY + ry / mag * step).roundToInt().coerceIn(0, height - 1)
            if (x != aimX || y != aimY) {
                aimX = x
                aimY = y
                sink.pointer(6, x, y, clickMask(0))
            }
        }
        val leftClick = virtLeftClick || virtRt || physLeftClick || physRtKey || physRtAxis
        val rightClick = virtRightClick || physRightClick
        leftDown = syncButton(1, leftClick, leftDown, sink)
        rightDown = syncButton(3, rightClick, rightDown, sink)
    }

    private fun hold(key: Int, want: Boolean, sink: PadSink) {
        val has = held.contains(key)
        if (want && !has) {
            held.add(key)
            sink.key(true, key)
        } else if (!want && has) {
            held.remove(key)
            sink.key(false, key)
        }
    }

    private fun syncButton(button: Int, want: Boolean, wasDown: Boolean, sink: PadSink): Boolean {
        if (want == wasDown || aimX < 0) return wasDown
        val mask = if (button == 3) 1024 else 256
        val encoded = (button shl 24) or mask
        if (want) {
            sink.pointer(6, aimX, aimY, clickMask(button))
            sink.pointer(4, aimX, aimY, encoded)
        } else {
            sink.pointer(5, aimX, aimY, encoded)
        }
        return want
    }

    private fun clickMask(excluding: Int): Int {
        var mask = 0
        if (leftDown && excluding != 1) mask = mask or 256
        if (rightDown && excluding != 3) mask = mask or 1024
        return mask
    }

    private fun stickAxis(event: MotionEvent, axis: Int): Float {
        val raw = event.getAxisValue(axis)
        val device = event.device
        val range = device?.getMotionRange(axis, event.source) ?: device?.getMotionRange(axis)
        if (range == null) {
            return if (abs(raw) < 0.15f) 0f else raw.coerceIn(-1f, 1f)
        }
        if (abs(raw) <= range.flat) return 0f
        val span = range.max - range.min
        if (span < 0.01f) return 0f
        val centered = (raw - (range.max + range.min) / 2f) / (span / 2f)
        return centered.coerceIn(-1f, 1f)
    }

    private fun triggerAxis(event: MotionEvent, axis: Int): Float {
        val raw = event.getAxisValue(axis)
        val device = event.device
        val range = device?.getMotionRange(axis, event.source) ?: device?.getMotionRange(axis)
        if (range == null) return raw.coerceIn(0f, 1f)
        if (raw <= range.min + range.flat) return 0f
        val span = (range.max - range.min).coerceAtLeast(0.01f)
        return ((raw - range.min) / span).coerceIn(0f, 1f)
    }

    private fun pick(virtual: Float, physical: Float): Float {
        return if (abs(virtual) >= abs(physical)) virtual else physical
    }

    private fun isController(event: android.view.InputEvent): Boolean {
        val sources = event.device?.sources ?: 0
        val source = event.source
        fun has(mask: Int) = (sources and mask) == mask || (source and mask) == mask
        val joystickClass = (sources and InputDevice.SOURCE_CLASS_JOYSTICK) != 0 ||
            (source and InputDevice.SOURCE_CLASS_JOYSTICK) != 0
        return has(InputDevice.SOURCE_GAMEPAD) || has(InputDevice.SOURCE_JOYSTICK) || joystickClass
    }
}

interface PadSink {
    fun pointer(type: Int, x: Int, y: Int, state: Int)
    fun key(down: Boolean, keysym: Int)
}

@Composable
fun GuestPadOverlay(pad: GuestPadState) {
    if (!pad.overlay) return
    val c = VexaTheme.colors
    Box(modifier = Modifier.fillMaxSize()) {
        if (pad.aimX >= 0 && pad.aimY >= 0) {
            Box(
                modifier = Modifier
                    .offset {
                        IntOffset(
                            pad.aimX - 7.dp.roundToPx(),
                            pad.aimY - 7.dp.roundToPx(),
                        )
                    }
                    .size(14.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.9f))
                    .border(2.dp, c.plasma, CircleShape),
            )
        }
        Row(
            Modifier.align(Alignment.TopStart).padding(start = 20.dp, top = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            HoldButton("LT", { pad.virtLt = it }, emphasized = pad.virtLt)
            HoldButton("LB", { pad.onFaceKey(it, XKeys.SHIFT_L) })
        }
        Row(
            Modifier.align(Alignment.TopCenter).padding(top = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            HoldButton("BK", { pad.onFaceKey(it, XKeys.TAB) })
            HoldButton("ST", { pad.onFaceKey(it, XKeys.RETURN) })
        }
        Row(
            Modifier.align(Alignment.TopEnd).padding(end = 20.dp, top = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            HoldButton("RB", { pad.virtRightClick = it }, emphasized = pad.virtRightClick)
            HoldButton("RT", { pad.virtRt = it }, emphasized = pad.virtRt)
        }
        Dpad(
            pad,
            Modifier.align(Alignment.BottomStart).padding(start = 188.dp, bottom = 96.dp),
        )
        Stick(
            x = pad.virtLeftX,
            y = pad.virtLeftY,
            label = "L",
            modifier = Modifier.align(Alignment.BottomStart).padding(start = 28.dp, bottom = 72.dp),
        ) { x, y ->
            pad.virtLeftX = x
            pad.virtLeftY = y
        }
        HoldButton(
            "L3",
            { pad.onFaceKey(it, XKeys.C) },
            modifier = Modifier.align(Alignment.BottomStart).padding(start = 78.dp, bottom = 228.dp),
        )
        Diamond(
            pad,
            Modifier.align(Alignment.BottomEnd).padding(end = 188.dp, bottom = 64.dp),
        )
        Stick(
            x = pad.virtRightX,
            y = pad.virtRightY,
            label = "R",
            modifier = Modifier.align(Alignment.BottomEnd).padding(end = 28.dp, bottom = 72.dp),
        ) { x, y ->
            pad.virtRightX = x
            pad.virtRightY = y
        }
        HoldButton(
            "R3",
            { pad.onFaceKey(it, XKeys.R) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(end = 78.dp, bottom = 228.dp),
        )
    }
}

@Composable
private fun Dpad(pad: GuestPadState, modifier: Modifier) {
    Box(modifier.size(132.dp)) {
        HoldButton("↑", { pad.setDpad(0, it) }, modifier = Modifier.align(Alignment.TopCenter))
        HoldButton("↓", { pad.setDpad(1, it) }, modifier = Modifier.align(Alignment.BottomCenter))
        HoldButton("←", { pad.setDpad(2, it) }, modifier = Modifier.align(Alignment.CenterStart))
        HoldButton("→", { pad.setDpad(3, it) }, modifier = Modifier.align(Alignment.CenterEnd))
    }
}

@Composable
private fun Diamond(pad: GuestPadState, modifier: Modifier) {
    Box(modifier.size(164.dp)) {
        HoldButton("Y", { pad.onFaceKey(it, XKeys.E) }, modifier = Modifier.align(Alignment.TopCenter))
        HoldButton("X", { pad.onFaceKey(it, XKeys.SPACE) }, modifier = Modifier.align(Alignment.CenterStart))
        HoldButton("B", { pad.onFaceKey(it, XKeys.ESCAPE) }, modifier = Modifier.align(Alignment.CenterEnd))
        HoldButton(
            "A",
            { pad.virtLeftClick = it },
            emphasized = pad.virtLeftClick,
            modifier = Modifier.align(Alignment.BottomCenter),
        )
    }
}

@Composable
private fun HoldButton(
    label: String,
    onDown: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    emphasized: Boolean = false,
) {
    val c = VexaTheme.colors
    Box(
        modifier = modifier
            .size(52.dp)
            .clip(CircleShape)
            .background(if (emphasized) c.plasma.copy(alpha = 0.85f) else c.surface2.copy(alpha = 0.72f))
            .border(1.dp, c.plasma.copy(alpha = 0.7f), CircleShape)
            .pointerInput(label) {
                awaitEachGesture {
                    val down = awaitFirstDown()
                    onDown(true)
                    try {
                        drag(down.id) { change -> change.consume() }
                    } finally {
                        onDown(false)
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = c.textPrimary, style = MonospaceTypography.labelLarge)
    }
}

@Composable
private fun Stick(
    x: Float,
    y: Float,
    label: String,
    modifier: Modifier,
    onChange: (Float, Float) -> Unit,
) {
    val c = VexaTheme.colors
    Box(
        modifier = modifier
            .size(148.dp)
            .clip(CircleShape)
            .background(c.surface2.copy(alpha = 0.4f))
            .border(2.dp, c.plasma.copy(alpha = 0.55f), CircleShape)
            .pointerInput(label) {
                awaitEachGesture {
                    val down = awaitFirstDown()
                    fun report(px: Float, py: Float) {
                        val dx = ((px - size.width / 2f) / (size.width / 2f)).coerceIn(-1f, 1f)
                        val dy = ((py - size.height / 2f) / (size.height / 2f)).coerceIn(-1f, 1f)
                        onChange(dx, dy)
                    }
                    try {
                        report(down.position.x, down.position.y)
                        drag(down.id) { change ->
                            report(change.position.x, change.position.y)
                            change.consume()
                        }
                    } finally {
                        onChange(0f, 0f)
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Box(
            modifier = Modifier
                .offset { IntOffset((x * 40.dp.toPx()).roundToInt(), (y * 40.dp.toPx()).roundToInt()) }
                .size(54.dp)
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.55f))
                .border(1.dp, c.textPrimary.copy(alpha = 0.8f), CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            Text(label, color = c.textPrimary, style = MonospaceTypography.labelLarge)
        }
    }
}
