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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.Dp
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
    const val F = 0x66
    const val R = 0x72
    const val S = 0x73
    const val W = 0x77
    const val X = 0x78
    const val Z = 0x7a
}

/**
 * Control ids for the type-7 records in /tmp/vexa-pointer. Buttons match
 * SDL_GameControllerButton; axes are 100 + SDL_GameControllerAxis so the
 * guest thunks and the evdev shim can fan the same stream out to the
 * virtual controller.
 */
object PadCtl {
    /**
     * Bit 30 marks key/pointer records synthesized by GuestPad as fallback for
     * X11-only games. SDL2/SDL3 thunks skip records with this bit set so games
     * only see native controller events and stay in gamepad UI mode.
     */
    const val SYNTH_FLAG = 0x40000000

    const val A = 0
    const val B = 1
    const val X = 2
    const val Y = 3
    const val BACK = 4
    const val GUIDE = 5
    const val START = 6
    const val LSTICK = 7
    const val RSTICK = 8
    const val LB = 9
    const val RB = 10
    const val DPAD_UP = 11
    const val DPAD_DOWN = 12
    const val DPAD_LEFT = 13
    const val DPAD_RIGHT = 14

    const val AXIS_BASE = 100
    const val LEFTX = AXIS_BASE + 0
    const val LEFTY = AXIS_BASE + 1
    const val RIGHTX = AXIS_BASE + 2
    const val RIGHTY = AXIS_BASE + 3
    const val LTRIGGER = AXIS_BASE + 4
    const val RTRIGGER = AXIS_BASE + 5
}

/**
 * Per-game keyboard/mouse fallback profile. The type-7 pad records always
 * carry the real controller state so games with gamepad support apply their
 * own default bindings automatically; [keys], [stickKeys] and [clicks] only
 * matter for X11-only guests that never look at a gamepad.
 *
 * Adding a game should not require touching this table: DEFAULT mirrors the
 * most common WASD+Space conventions. Override in [PadProfiles.forProgram]
 * only when a game ships unusual defaults.
 */
class PadProfile(
    /** Fallback X11 keysyms emitted alongside each pad control. */
    val keys: Map<Int, IntArray>,
    /** Fallback keysyms for left-stick directions: up, down, left, right. */
    val stickKeys: IntArray = intArrayOf(XKeys.W, XKeys.S, XKeys.A, XKeys.D),
    /** Fallback keysyms for d-pad directions: up, down, left, right. */
    val dpadKeys: IntArray = intArrayOf(XKeys.UP, XKeys.DOWN, XKeys.LEFT, XKeys.RIGHT),
    /** Mouse button each control also drives: 1 = left, 3 = right. */
    val clicks: Map<Int, Int> = emptyMap(),
    /** Emit Escape (direct, not SYNTH) alongside B — Megabonk's cancel path. */
    val bAsEscape: Boolean = false,
    /** Let the left stick drive arrow keys as well — ETS2/FNA menu navigation. */
    val keyboardNavigationFallback: Boolean = false,
)

object PadProfiles {
    val DEFAULT = PadProfile(
        keys = mapOf(
            PadCtl.A to intArrayOf(XKeys.SPACE, XKeys.Z, XKeys.RETURN),
            PadCtl.B to intArrayOf(XKeys.CONTROL_L, XKeys.SHIFT_L, XKeys.C, XKeys.X),
            PadCtl.X to intArrayOf(XKeys.E, XKeys.F),
            PadCtl.Y to intArrayOf(XKeys.R, XKeys.TAB),
            PadCtl.LB to intArrayOf(XKeys.SHIFT_L, XKeys.CONTROL_L),
            PadCtl.RB to intArrayOf(XKeys.E),
            PadCtl.START to intArrayOf(XKeys.ESCAPE, XKeys.RETURN),
            PadCtl.BACK to intArrayOf(XKeys.TAB),
            PadCtl.LSTICK to intArrayOf(XKeys.SHIFT_L, XKeys.C),
            PadCtl.RSTICK to intArrayOf(XKeys.R),
        ),
        clicks = mapOf(
            PadCtl.RTRIGGER to 1,
            PadCtl.LTRIGGER to 3,
            PadCtl.RB to 3,
        ),
    )

    private val MEGABONK = PadProfile(
        keys = DEFAULT.keys,
        clicks = DEFAULT.clicks,
        bAsEscape = true,
    )

    private val ETS2 = PadProfile(
        keys = DEFAULT.keys,
        clicks = DEFAULT.clicks,
        keyboardNavigationFallback = true,
    )

    fun forProgram(programId: String?): PadProfile = when (programId) {
        "megabonk" -> MEGABONK
        "ets2" -> ETS2
        else -> DEFAULT
    }
}

/**
 * Physical sticks and the on-screen pad share one stream. Every control emits
 * a type-7 pad record so guests see a real SDL_Joystick/SDL_GameController (or
 * the evdev Xbox 360 pad through libvexa_evdev.so); the legacy keyboard/mouse
 * records still fire alongside it (tagged with [PadCtl.SYNTH_FLAG]) so games
 * that only read X11 input keep working without switching SDL games into
 * keyboard/mouse mode.
 * Raw screen taps stay clicks only while the overlay is off.
 */
class GuestPadState {
    var profile: PadProfile = PadProfiles.DEFAULT
    var overlay by mutableStateOf(false)
    var aimX by mutableIntStateOf(-1)
    var aimY by mutableIntStateOf(-1)
    var virtLeftX by mutableFloatStateOf(0f)
    var virtLeftY by mutableFloatStateOf(0f)
    var virtRightX by mutableFloatStateOf(0f)
    var virtRightY by mutableFloatStateOf(0f)
    var virtLt by mutableStateOf(false)
    var virtRt by mutableStateOf(false)
    var onFaceKey: (Boolean, Int) -> Unit = { _, _ -> }
    var onDirectKey: (Boolean, Int) -> Unit = { _, _ -> }
    var onPadButton: (Boolean, Int) -> Unit = { _, _ -> }

    private var physLeftX = 0f
    private var physLeftY = 0f
    private var physRightX = 0f
    private var physRightY = 0f
    private var physLtKey = false
    private var physRtKey = false
    private var physLtAxis = false
    private var physRtAxis = false
    private var physLtVal = 0f
    private var physRtVal = 0f
    private var hatX = 0f
    private var hatY = 0f
    private val dpadKey = BooleanArray(4)
    private val dpadVirt = BooleanArray(4)
    private val clickHolds = HashSet<Int>()
    private val held = HashSet<Int>()
    private var leftDown = false
    private var rightDown = false
    private val sentPadAxes = IntArray(6) { Int.MIN_VALUE }
    private val sentPadButtons = IntArray(16) { -1 }

    /**
     * Pad record plus the profile's fallback keysyms/clicks, so both the real
     * controller channel and the X11 keyboard fallback stay in sync.
     */
    fun press(down: Boolean, control: Int) {
        when (control) {
            PadCtl.LTRIGGER -> virtLt = down
            PadCtl.RTRIGGER -> virtRt = down
            else -> onPadButton(down, control)
        }
        profile.keys[control]?.let { for (k in it) onFaceKey(down, k) }
        if (control == PadCtl.B && profile.bAsEscape) onDirectKey(down, XKeys.ESCAPE)
        if (profile.clicks.containsKey(control)) {
            if (down) clickHolds.add(control) else clickHolds.remove(control)
        }
    }

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
        physLtVal = maxOf(
            triggerAxis(event, MotionEvent.AXIS_LTRIGGER),
            triggerAxis(event, MotionEvent.AXIS_BRAKE),
        )
        physRtVal = maxOf(
            triggerAxis(event, MotionEvent.AXIS_RTRIGGER),
            triggerAxis(event, MotionEvent.AXIS_GAS),
        )
        physLtAxis = physLtVal > 0.40f
        physRtAxis = physRtVal > 0.40f
        return true
    }

    fun onKey(event: KeyEvent, sink: PadSink): Boolean {
        if (!isController(event)) return false
        if (event.repeatCount > 0) return true
        val down = event.action == KeyEvent.ACTION_DOWN
        val padDown = if (down) 1 else 0
        when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> press(down, PadCtl.A)
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK -> press(down, PadCtl.B)
            KeyEvent.KEYCODE_BUTTON_X -> press(down, PadCtl.X)
            KeyEvent.KEYCODE_BUTTON_Y -> press(down, PadCtl.Y)
            KeyEvent.KEYCODE_BUTTON_L1 -> press(down, PadCtl.LB)
            KeyEvent.KEYCODE_BUTTON_R1 -> press(down, PadCtl.RB)
            KeyEvent.KEYCODE_BUTTON_L2 -> physLtKey = down
            KeyEvent.KEYCODE_BUTTON_R2 -> physRtKey = down
            KeyEvent.KEYCODE_BUTTON_START -> press(down, PadCtl.START)
            KeyEvent.KEYCODE_BUTTON_SELECT -> press(down, PadCtl.BACK)
            KeyEvent.KEYCODE_BUTTON_THUMBL -> press(down, PadCtl.LSTICK)
            KeyEvent.KEYCODE_BUTTON_THUMBR -> press(down, PadCtl.RSTICK)
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
        emitPadAxis(0, (lx * 32767f).roundToInt(), sink)
        emitPadAxis(1, (ly * 32767f).roundToInt(), sink)
        val stickUp = ly < -MOVE_DEADZONE
        val stickDown = ly > MOVE_DEADZONE
        val stickLeft = lx < -MOVE_DEADZONE
        val stickRight = lx > MOVE_DEADZONE

        val dpadUp = dpadKey[0] || dpadVirt[0] || hatY < -0.5f
        val dpadDown = dpadKey[1] || dpadVirt[1] || hatY > 0.5f
        val dpadLeft = dpadKey[2] || dpadVirt[2] || hatX < -0.5f
        val dpadRight = dpadKey[3] || dpadVirt[3] || hatX > 0.5f
        emitPadButton(PadCtl.DPAD_UP, dpadUp, sink)
        emitPadButton(PadCtl.DPAD_DOWN, dpadDown, sink)
        emitPadButton(PadCtl.DPAD_LEFT, dpadLeft, sink)
        emitPadButton(PadCtl.DPAD_RIGHT, dpadRight, sink)
        val up = dpadUp || stickUp
        val down = dpadDown || stickDown
        val left = dpadLeft || stickLeft
        val right = dpadRight || stickRight
        hold(profile.stickKeys[0], up && !down, sink)
        hold(profile.stickKeys[1], down && !up, sink)
        hold(profile.stickKeys[2], left && !right, sink)
        hold(profile.stickKeys[3], right && !left, sink)
        val navUp = dpadUp || (profile.keyboardNavigationFallback && stickUp)
        val navDown = dpadDown || (profile.keyboardNavigationFallback && stickDown)
        val navLeft = dpadLeft || (profile.keyboardNavigationFallback && stickLeft)
        val navRight = dpadRight || (profile.keyboardNavigationFallback && stickRight)
        hold(profile.dpadKeys[0], navUp && !navDown, sink, profile.keyboardNavigationFallback)
        hold(profile.dpadKeys[1], navDown && !navUp, sink, profile.keyboardNavigationFallback)
        hold(profile.dpadKeys[2], navLeft && !navRight, sink, profile.keyboardNavigationFallback)
        hold(profile.dpadKeys[3], navRight && !navLeft, sink, profile.keyboardNavigationFallback)

        val ltValue = maxOf(if (virtLt) 1f else 0f, if (physLtKey) 1f else 0f, physLtVal)
        val rtValue = maxOf(if (virtRt) 1f else 0f, if (physRtKey) 1f else 0f, physRtVal)
        emitPadAxis(4, (ltValue * 32767f).roundToInt(), sink)
        emitPadAxis(5, (rtValue * 32767f).roundToInt(), sink)

        if (width > 1 && height > 1 && aimX < 0) {
            aimX = width / 2
            aimY = height / 2
        }
        val rx = pick(virtRightX, physRightX)
        val ry = pick(virtRightY, physRightY)
        emitPadAxis(2, (rx * 32767f).roundToInt(), sink)
        emitPadAxis(3, (ry * 32767f).roundToInt(), sink)
        val mag = hypot(rx, ry)
        if (width > 1 && height > 1 && mag > AIM_DEADZONE) {
            val scale = ((mag - AIM_DEADZONE) / (1f - AIM_DEADZONE)).coerceIn(0f, 1f)
            val step = AIM_STEP_PX * scale
            val dx = (rx / mag * step).roundToInt().coerceIn(-512, 512)
            val dy = (ry / mag * step).roundToInt().coerceIn(-512, 512)
            if (dx != 0 || dy != 0) {
                aimX = (aimX + dx).coerceIn(0, width - 1)
                aimY = (aimY + dy).coerceIn(0, height - 1)
                val packedX = (aimX and 0xffff) or ((dx and 0xffff) shl 16)
                val packedY = (aimY and 0xffff) or ((dy and 0xffff) shl 16)
                sink.pointer(6, packedX, packedY, clickMask(0))
            }
        }
        val leftClick = virtRt || physRtKey || physRtAxis || clickHolds.any { profile.clicks[it] == 1 }
        val rightClick = virtLt || physLtKey || physLtAxis || clickHolds.any { profile.clicks[it] == 3 }
        leftDown = syncButton(1, leftClick, leftDown, sink)
        rightDown = syncButton(3, rightClick, rightDown, sink)
    }

    private fun emitPadAxis(axis: Int, value: Int, sink: PadSink) {
        if (axis in sentPadAxes.indices && sentPadAxes[axis] != value) {
            sentPadAxes[axis] = value
            sink.pad(PadCtl.AXIS_BASE + axis, value)
        }
    }

    private fun emitPadButton(button: Int, down: Boolean, sink: PadSink) {
        val value = if (down) 1 else 0
        if (button in sentPadButtons.indices && sentPadButtons[button] != value) {
            sentPadButtons[button] = value
            sink.pad(button, value)
        }
    }

    private fun hold(key: Int, want: Boolean, sink: PadSink, direct: Boolean = false) {
        val has = held.contains(key)
        if (want && !has) {
            held.add(key)
            if (direct) sink.directKey(true, key) else sink.key(true, key)
        } else if (!want && has) {
            held.remove(key)
            if (direct) sink.directKey(false, key) else sink.key(false, key)
        }
    }

    private fun syncButton(button: Int, want: Boolean, wasDown: Boolean, sink: PadSink): Boolean {
        if (want == wasDown || aimX < 0) return wasDown
        val mask = if (button == 3) 1024 else 256
        val encoded = (button shl 24) or mask
        if (want) {
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

    private fun isController(event: android.view.InputEvent): Boolean = isControllerSourced(event)
}

/** True when the event comes from a gamepad/joystick-class device or carries a gamepad source. */
internal fun isControllerSourced(event: android.view.InputEvent): Boolean {
    val sources = event.device?.sources ?: 0
    val source = event.source
    fun has(mask: Int) = (sources and mask) == mask || (source and mask) == mask
    val joystickClass = (sources and InputDevice.SOURCE_CLASS_JOYSTICK) != 0 ||
        (source and InputDevice.SOURCE_CLASS_JOYSTICK) != 0
    return has(InputDevice.SOURCE_GAMEPAD) || has(InputDevice.SOURCE_JOYSTICK) || joystickClass
}

interface PadSink {
    fun pointer(type: Int, x: Int, y: Int, state: Int)
    fun key(down: Boolean, keysym: Int)
    /** Delivered to SDL guests too; use only when a keyboard action is explicitly required. */
    fun directKey(down: Boolean, keysym: Int)
    fun pad(control: Int, value: Int)
}

private val BTN_FACE = 44.dp
private val BTN_SMALL = 34.dp
private val STICK_BASE = 124.dp
private val STICK_KNOB = 48.dp
private val SHOULDER_W = 80.dp
private val SHOULDER_H = 42.dp
private val TRIGGER_W = 60.dp
private val TRIGGER_H = 66.dp
private val DPAD_SIZE = 120.dp
private val DPAD_ARM = 42.dp

@Composable
fun GuestPadOverlay(pad: GuestPadState) {
    if (!pad.overlay) return
    val c = VexaTheme.colors
    Box(modifier = Modifier.fillMaxSize()) {
        // Shoulders: triggers on the outer top corners, bumpers inside them.
        Row(
            Modifier.align(Alignment.TopStart).padding(start = 16.dp, top = 10.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            HoldButton("LT", { pad.press(it, PadCtl.LTRIGGER) }, emphasized = pad.virtLt,
                width = TRIGGER_W, height = TRIGGER_H)
            HoldButton("LB", { pad.press(it, PadCtl.LB) },
                width = SHOULDER_W, height = SHOULDER_H)
        }
        Row(
            Modifier.align(Alignment.TopCenter).padding(top = 12.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            HoldButton("<", { pad.press(it, PadCtl.BACK) }, size = BTN_SMALL)
            HoldButton(">", { pad.press(it, PadCtl.START) }, size = BTN_SMALL)
        }
        Row(
            Modifier.align(Alignment.TopEnd).padding(end = 16.dp, top = 10.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            HoldButton("RB", { pad.press(it, PadCtl.RB) },
                width = SHOULDER_W, height = SHOULDER_H)
            HoldButton("RT", { pad.press(it, PadCtl.RTRIGGER) }, emphasized = pad.virtRt,
                width = TRIGGER_W, height = TRIGGER_H)
        }

        // Left cluster, Series-style diagonal: left stick low in the corner,
        // d-pad above it and shifted inward toward the screen centre.
        Dpad(
            pad,
            Modifier.align(Alignment.BottomStart).padding(start = 144.dp, bottom = 120.dp),
        )
        HoldButton(
            "L3",
            { pad.press(it, PadCtl.LSTICK) },
            modifier = Modifier.align(Alignment.BottomStart).padding(start = 168.dp, bottom = 60.dp),
            size = BTN_SMALL,
        )
        Stick(
            x = pad.virtLeftX,
            y = pad.virtLeftY,
            label = "L",
            modifier = Modifier.align(Alignment.BottomStart).padding(start = 36.dp, bottom = 14.dp),
        ) { x, y ->
            pad.virtLeftX = x
            pad.virtLeftY = y
        }

        // Right cluster mirrors the left: ABXY high in the corner, right
        // stick below and shifted inward.
        Diamond(
            pad,
            Modifier.align(Alignment.BottomEnd).padding(end = 44.dp, bottom = 124.dp),
        )
        HoldButton(
            "R3",
            { pad.press(it, PadCtl.RSTICK) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(end = 276.dp, bottom = 64.dp),
            size = BTN_SMALL,
        )
        Stick(
            x = pad.virtRightX,
            y = pad.virtRightY,
            label = "R",
            modifier = Modifier.align(Alignment.BottomEnd).padding(end = 140.dp, bottom = 14.dp),
        ) { x, y ->
            pad.virtRightX = x
            pad.virtRightY = y
        }
    }
}

/**
 * Xbox-style cross d-pad: one contiguous plus shape, corners pivot to
 * diagonals. Bitmask: 1=up, 2=down, 4=left, 8=right.
 */
@Composable
private fun Dpad(pad: GuestPadState, modifier: Modifier) {
    val c = VexaTheme.colors
    var active by remember { mutableIntStateOf(0) }
    fun applyMask(mask: Int) {
        pad.setDpad(0, mask and 1 != 0)
        pad.setDpad(1, mask and 2 != 0)
        pad.setDpad(2, mask and 4 != 0)
        pad.setDpad(3, mask and 8 != 0)
        active = mask
    }
    Box(
        modifier = modifier
            .size(DPAD_SIZE)
            .pointerInput(Unit) {
                awaitEachGesture {
                    val down = awaitFirstDown()
                    fun maskAt(px: Float, py: Float): Int {
                        val dx = px / size.width - 0.5f
                        val dy = py / size.height - 0.5f
                        var m = 0
                        if (dy < -0.16f) m = m or 1
                        if (dy > 0.16f) m = m or 2
                        if (dx < -0.16f) m = m or 4
                        if (dx > 0.16f) m = m or 8
                        return m
                    }
                    try {
                        applyMask(maskAt(down.position.x, down.position.y))
                        drag(down.id) { change ->
                            applyMask(maskAt(change.position.x, change.position.y))
                            change.consume()
                        }
                    } finally {
                        applyMask(0)
                    }
                }
            },
    ) {
        // Contiguous plus: two overlapping bars sharing one border layer.
        Box(
            Modifier
                .align(Alignment.Center)
                .size(DPAD_ARM, DPAD_SIZE)
                .clip(RoundedCornerShape(10.dp))
                .background(c.surface2.copy(alpha = 0.55f)),
        )
        Box(
            Modifier
                .align(Alignment.Center)
                .size(DPAD_SIZE, DPAD_ARM)
                .clip(RoundedCornerShape(10.dp))
                .background(c.surface2.copy(alpha = 0.55f)),
        )
        // Per-arm highlight + label.
        data class Arm(val bit: Int, val label: String, val align: Alignment)
        val arms = listOf(
            Arm(1, "▲", Alignment.TopCenter),
            Arm(2, "▼", Alignment.BottomCenter),
            Arm(4, "◀", Alignment.CenterStart),
            Arm(8, "▶", Alignment.CenterEnd),
        )
        for (arm in arms) {
            val lit = active and arm.bit != 0
            Box(
                modifier = Modifier
                    .align(arm.align)
                    .size(DPAD_ARM)
                    .clip(RoundedCornerShape(10.dp))
                    .background(if (lit) c.plasma.copy(alpha = 0.85f) else Color.Transparent)
                    .border(1.dp, c.borderStrong.copy(alpha = if (lit) 1f else 0.4f), RoundedCornerShape(10.dp)),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    arm.label,
                    color = if (lit) c.onAccent else c.textPrimary.copy(alpha = 0.7f),
                    style = MonospaceTypography.labelSmall,
                )
            }
        }
    }
}

@Composable
private fun Diamond(pad: GuestPadState, modifier: Modifier) {
    Box(modifier.size(120.dp)) {
        HoldButton("Y", { pad.press(it, PadCtl.Y) }, modifier = Modifier.align(Alignment.TopCenter))
        HoldButton("X", { pad.press(it, PadCtl.X) }, modifier = Modifier.align(Alignment.CenterStart))
        HoldButton("B", { pad.press(it, PadCtl.B) }, modifier = Modifier.align(Alignment.CenterEnd))
        HoldButton("A", { pad.press(it, PadCtl.A) }, modifier = Modifier.align(Alignment.BottomCenter))
    }
}

@Composable
private fun HoldButton(
    label: String,
    onDown: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    emphasized: Boolean = false,
    size: Dp = BTN_FACE,
    width: Dp = size,
    height: Dp = size,
) {
    val c = VexaTheme.colors
    var pressed by remember { mutableStateOf(false) }
    val lit = emphasized || pressed
    val shape = if (width == height) CircleShape else RoundedCornerShape(8.dp)
    Box(
        modifier = modifier
            .size(width, height)
            .clip(shape)
            .background(if (lit) c.plasma.copy(alpha = 0.85f) else c.surface2.copy(alpha = 0.55f))
            .border(if (lit) 1.5.dp else 1.dp, c.borderStrong, shape)
            .pointerInput(label) {
                awaitEachGesture {
                    val down = awaitFirstDown()
                    pressed = true
                    onDown(true)
                    try {
                        drag(down.id) { change -> change.consume() }
                    } finally {
                        pressed = false
                        onDown(false)
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = if (lit) c.onAccent else c.textPrimary, style = MonospaceTypography.labelLarge)
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
            .size(STICK_BASE)
            .clip(CircleShape)
            .background(c.surface2.copy(alpha = 0.35f))
            .border(1.dp, c.borderStrong, CircleShape)
            .pointerInput(label) {
                awaitEachGesture {
                    val down = awaitFirstDown()
                    fun report(px: Float, py: Float) {
                        var dx = (px - size.width / 2f) / (size.width / 2f)
                        var dy = (py - size.height / 2f) / (size.height / 2f)
                        val mag = hypot(dx, dy)
                        if (mag > 1f) {
                            dx /= mag
                            dy /= mag
                        }
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
                .offset {
                    val travel = (STICK_BASE - STICK_KNOB).toPx() / 2f
                    val mag = hypot(x, y)
                    val scale = if (mag > 1f) 1f / mag else 1f
                    IntOffset((x * scale * travel).roundToInt(), (y * scale * travel).roundToInt())
                }
                .size(STICK_KNOB)
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.55f))
                .border(1.dp, c.textPrimary.copy(alpha = 0.8f), CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            Text(label, color = c.textPrimary, style = MonospaceTypography.labelLarge)
        }
    }
}
