package com.critical.vexaemulator.input

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaIcon
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlin.math.roundToInt

/**
 * A draggable keyboard card. Android will not open the system IME in floating
 * mode, so this panel is the floating keyboard.
 */
@Composable
fun FloatingKeyboard(
    onChar: (Char) -> Unit,
    onKeysym: (Int) -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    var dx by remember { mutableIntStateOf(0) }
    var dy by remember { mutableIntStateOf(0) }
    var shift by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(18.dp)
    Column(
        modifier
            .offset { IntOffset(dx, dy) }
            .widthIn(max = 520.dp)
            .fillMaxWidth(0.92f)
            .shadow(18.dp, shape)
            .clip(shape)
            .background(c.bgElevated.copy(alpha = 0.96f))
            .border(1.dp, c.hairline, shape)
            .pointerInput(Unit) { detectTapGestures { } }
            .padding(8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .height(22.dp)
                .pointerInput(Unit) {
                    detectDragGestures { change, drag ->
                        change.consume()
                        dx += drag.x.roundToInt()
                        dy += drag.y.roundToInt()
                    }
                },
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            VexaIcon(VexaGlyph.Grip, c.textTertiary, size = 16.dp)
            Box(
                Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .clickable { onClose() }
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.Center,
            ) {
                VexaIcon(VexaGlyph.Close, c.textSecondary, size = 14.dp)
            }
        }
        KeyRow("1234567890", shift, onChar)
        KeyRow("qwertyuiop", shift, onChar)
        KeyRow("asdfghjkl", shift, onChar)
        KeyRow("zxcvbnm", shift, onChar)
        KeyRow("ğüşıöç", shift, onChar)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            KeyFace("Shift", shift, Modifier.width(64.dp)) { shift = !shift }
            KeyFace("space", false, Modifier.weight(1f)) { onChar(' ') }
            KeyFace("⌫", false, Modifier.width(48.dp)) { onKeysym(0xff08) }
            KeyFace("Enter", false, Modifier.width(64.dp)) { onKeysym(XKeys.RETURN) }
        }
    }
}

@Composable
private fun KeyRow(chars: String, shift: Boolean, onChar: (Char) -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        chars.forEach { ch ->
            val shown = if (shift) turkishUpper(ch) else ch
            KeyFace(shown.toString(), false, Modifier.weight(1f)) {
                onChar(shown)
            }
        }
    }
}

private fun turkishUpper(ch: Char): Char = when (ch) {
    'i' -> 'İ'
    'ı' -> 'I'
    'ğ' -> 'Ğ'
    'ü' -> 'Ü'
    'ş' -> 'Ş'
    'ö' -> 'Ö'
    'ç' -> 'Ç'
    else -> ch.uppercaseChar()
}

@Composable
private fun KeyFace(
    label: String,
    active: Boolean,
    modifier: Modifier,
    onClick: () -> Unit,
) {
    val c = VexaTheme.colors
    val source = remember { MutableInteractionSource() }
    Box(
        modifier
            .height(32.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (active) c.soft(c.plasma) else c.surface1)
            .border(1.dp, if (active) c.plasma else c.hairline, RoundedCornerShape(8.dp))
            .clickable(source, ripple(color = c.plasma), onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            label,
            color = if (active) c.plasma else c.textPrimary,
            fontSize = 13.sp,
            fontWeight = FontWeight.Medium,
            textAlign = TextAlign.Center,
            maxLines = 1,
        )
    }
}
