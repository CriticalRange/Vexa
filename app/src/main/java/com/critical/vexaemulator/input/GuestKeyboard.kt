package com.critical.vexaemulator.input

import android.view.InputDevice
import android.view.KeyEvent

/**
 * Physical keyboards feed the same /tmp/vexa-pointer stream as GuestPad.
 * Maps Android key codes onto unshifted X11 keysyms; the guest thunk turns
 * those into SDL key events. Shifted symbols stay unshifted on purpose so the
 * derived scancode matches the physical key.
 */
object GuestKeyboard {

    fun isKeyboardSource(event: KeyEvent): Boolean {
        val sources = event.device?.sources ?: event.source
        return sources and InputDevice.SOURCE_KEYBOARD == InputDevice.SOURCE_KEYBOARD ||
            sources and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
    }

    /** Returns the X11 keysym for an Android key code, or 0 when unmapped. */
    fun keysym(keyCode: Int): Int {
        if (keyCode in KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z) {
            return keyCode - KeyEvent.KEYCODE_A + XKeys.A
        }
        if (keyCode in KeyEvent.KEYCODE_0..KeyEvent.KEYCODE_9) {
            return keyCode - KeyEvent.KEYCODE_0 + 0x30
        }
        if (keyCode in KeyEvent.KEYCODE_F1..KeyEvent.KEYCODE_F12) {
            return keyCode - KeyEvent.KEYCODE_F1 + 0xffbe
        }
        if (keyCode in KeyEvent.KEYCODE_NUMPAD_0..KeyEvent.KEYCODE_NUMPAD_9) {
            return keyCode - KeyEvent.KEYCODE_NUMPAD_0 + 0xffb0
        }
        return when (keyCode) {
            KeyEvent.KEYCODE_SPACE -> XKeys.SPACE
            KeyEvent.KEYCODE_ENTER -> XKeys.RETURN
            KeyEvent.KEYCODE_NUMPAD_ENTER -> 0xff8d
            KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BACK -> XKeys.ESCAPE
            KeyEvent.KEYCODE_TAB -> XKeys.TAB
            KeyEvent.KEYCODE_DEL -> 0xff08
            KeyEvent.KEYCODE_FORWARD_DEL -> 0xffff
            KeyEvent.KEYCODE_INSERT -> 0xff63
            KeyEvent.KEYCODE_MOVE_HOME -> 0xff50
            KeyEvent.KEYCODE_MOVE_END -> 0xff57
            KeyEvent.KEYCODE_PAGE_UP -> 0xff55
            KeyEvent.KEYCODE_PAGE_DOWN -> 0xff56
            KeyEvent.KEYCODE_DPAD_UP -> XKeys.UP
            KeyEvent.KEYCODE_DPAD_DOWN -> XKeys.DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> XKeys.LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> XKeys.RIGHT
            KeyEvent.KEYCODE_DPAD_CENTER -> XKeys.RETURN
            KeyEvent.KEYCODE_SHIFT_LEFT -> XKeys.SHIFT_L
            KeyEvent.KEYCODE_SHIFT_RIGHT -> 0xffe2
            KeyEvent.KEYCODE_CTRL_LEFT -> XKeys.CONTROL_L
            KeyEvent.KEYCODE_CTRL_RIGHT -> 0xffe4
            KeyEvent.KEYCODE_ALT_LEFT -> 0xffe9
            KeyEvent.KEYCODE_ALT_RIGHT -> 0xffea
            KeyEvent.KEYCODE_META_LEFT -> 0xffeb
            KeyEvent.KEYCODE_META_RIGHT -> 0xffec
            KeyEvent.KEYCODE_CAPS_LOCK -> 0xffe5
            KeyEvent.KEYCODE_NUM_LOCK -> 0xff7f
            KeyEvent.KEYCODE_SCROLL_LOCK -> 0xff14
            KeyEvent.KEYCODE_BREAK -> 0xff13
            KeyEvent.KEYCODE_SYSRQ -> 0xff61
            KeyEvent.KEYCODE_MENU -> 0xff67
            KeyEvent.KEYCODE_MINUS -> 0x2d
            KeyEvent.KEYCODE_EQUALS -> 0x3d
            KeyEvent.KEYCODE_LEFT_BRACKET -> 0x5b
            KeyEvent.KEYCODE_RIGHT_BRACKET -> 0x5d
            KeyEvent.KEYCODE_BACKSLASH -> 0x5c
            KeyEvent.KEYCODE_SEMICOLON -> 0x3b
            KeyEvent.KEYCODE_APOSTROPHE -> 0x27
            KeyEvent.KEYCODE_GRAVE -> 0x60
            KeyEvent.KEYCODE_COMMA -> 0x2c
            KeyEvent.KEYCODE_PERIOD -> 0x2e
            KeyEvent.KEYCODE_SLASH -> 0x2f
            KeyEvent.KEYCODE_NUMPAD_DIVIDE -> 0xffaf
            KeyEvent.KEYCODE_NUMPAD_MULTIPLY -> 0xffaa
            KeyEvent.KEYCODE_NUMPAD_SUBTRACT -> 0xffad
            KeyEvent.KEYCODE_NUMPAD_ADD -> 0xffab
            KeyEvent.KEYCODE_NUMPAD_DOT -> 0xffae
            KeyEvent.KEYCODE_NUMPAD_COMMA -> 0xffac
            KeyEvent.KEYCODE_NUMPAD_EQUALS -> 0xffbd
            KeyEvent.KEYCODE_STAR -> 0x2a
            KeyEvent.KEYCODE_POUND -> 0x23
            KeyEvent.KEYCODE_PLUS -> 0x2b
            else -> 0
        }
    }
}

/** One X11 key, optionally with Shift held around the press. */
data class GuestKeyPlan(val keysym: Int, val shift: Boolean)

/**
 * Characters from the on-screen keyboard. Letters and digits use the evdev
 * keysyms the guest already maps. Other letters go out as Unicode keysyms
 * (0x01000000 + code point) so Xutf8LookupString can return them.
 */
fun guestKeyPlan(ch: Char): GuestKeyPlan? {
    if (ch in 'a'..'z') return GuestKeyPlan(XKeys.A + (ch - 'a'), false)
    if (ch in 'A'..'Z') return GuestKeyPlan(XKeys.A + (ch - 'A'), true)
    if (ch in '0'..'9') return GuestKeyPlan(0x30 + (ch - '0'), false)
    val shiftedFrom = when (ch) {
        '!' -> '1'; '@' -> '2'; '#' -> '3'; '$' -> '4'; '%' -> '5'
        '^' -> '6'; '&' -> '7'; '*' -> '8'; '(' -> '9'; ')' -> '0'
        '_' -> '-'; '+' -> '='; '{' -> '['; '}' -> ']'; '|' -> '\\'
        ':' -> ';'; '"' -> '\''; '<' -> ','; '>' -> '.'; '?' -> '/'
        '~' -> '`'
        else -> null
    }
    if (shiftedFrom != null) return GuestKeyPlan(shiftedFrom.code, true)
    val plain = when (ch) {
        ' ' -> XKeys.SPACE
        '-' -> 0x2d; '=' -> 0x3d; '[' -> 0x5b; ']' -> 0x5d; '\\' -> 0x5c
        ';' -> 0x3b; '\'' -> 0x27; '`' -> 0x60; ',' -> 0x2c; '.' -> 0x2e; '/' -> 0x2f
        else -> 0
    }
    if (plain != 0) return GuestKeyPlan(plain, false)
    if (ch.code in 0xa0..0x10ffff) return GuestKeyPlan(0x01000000 or ch.code, false)
    return null
}
