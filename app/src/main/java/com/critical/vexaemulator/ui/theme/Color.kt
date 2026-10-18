// VEXA design tokens: Void (pure black / blood red) and Paper palettes, brand gradient, M3 schemes.
package com.critical.vexaemulator.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Immutable
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

@Immutable
data class VexaColors(
    val isDark: Boolean,
    val bgBase: Color,
    val bgElevated: Color,
    val surface1: Color,
    val surface2: Color,
    val surface3: Color,
    val well: Color,
    val hairline: Color,
    val borderStrong: Color,
    val textPrimary: Color,
    val textSecondary: Color,
    val textTertiary: Color,
    val plasma: Color,
    val ultraviolet: Color,
    val ion: Color,
    val onAccent: Color,
    val success: Color,
    val warning: Color,
    val error: Color,
    val fatal: Color,
    val info: Color,
    val pink: Color,
    val teal: Color,
    val magenta: Color,
    val scrim: Color,
    val glass: Color,
    val glassBorder: Color,
    val shadow: Color,
) {
    // Hot red → blood red (plasma) → deeper red. Not tied to ion/ultraviolet order.
    val brandStops: List<Color> get() = listOf(Color(0xFFFF1A1A), plasma, Color(0xFF5A0000))

    fun brand(start: Offset = Offset.Zero, end: Offset = Offset.Infinite): Brush =
        Brush.linearGradient(brandStops, start, end)

    fun soft(color: Color): Color = color.copy(alpha = if (isDark) 0.14f else 0.10f)
}

val VoidColors = VexaColors(
    isDark = true,
    bgBase = Color(0xFF000000),
    bgElevated = Color(0xFF070707),
    surface1 = Color(0xFF0C0C0C),
    surface2 = Color(0xFF121212),
    surface3 = Color(0xFF161616),
    well = Color(0xFF050505),
    hairline = Color(0xFF1A0A0A),
    borderStrong = Color(0xFF2A1010),
    textPrimary = Color(0xFFF5F2F2),
    textSecondary = Color(0xFFC4B8B8),
    textTertiary = Color(0xFF7A7070),
    plasma = Color(0xFFB00000),
    ultraviolet = Color(0xFF5A0000),
    ion = Color(0xFFFF1A1A),
    onAccent = Color(0xFFFFF5F5),
    success = Color(0xFF34D399),
    warning = Color(0xFFFBBF24),
    error = Color(0xFFFF1A1A),
    fatal = Color(0xFF8B0000),
    info = Color(0xFFFF6A6A),
    pink = Color(0xFFFF8A8A),
    teal = Color(0xFF7A1515),
    magenta = Color(0xFFE10600),
    scrim = Color(0x99000000),
    glass = Color(0xCC070707),
    glassBorder = Color(0x332A1010),
    shadow = Color(0xFFB00000),
)

val PaperColors = VexaColors(
    isDark = false,
    bgBase = Color(0xFFF4F6FA),
    bgElevated = Color(0xFFFFFFFF),
    surface1 = Color(0xFFFFFFFF),
    surface2 = Color(0xFFEEF1F6),
    surface3 = Color(0xFFE5E9F1),
    well = Color(0xFFF7F9FC),
    hairline = Color(0xFFDDE2EB),
    borderStrong = Color(0xFFC3CAD7),
    textPrimary = Color(0xFF0D1117),
    textSecondary = Color(0xFF4E5768),
    textTertiary = Color(0xFF8A93A3),
    plasma = Color(0xFFB00000),
    ultraviolet = Color(0xFF5A0000),
    ion = Color(0xFFD42020),
    onAccent = Color(0xFFFFF5F5),
    success = Color(0xFF059669),
    warning = Color(0xFFD97706),
    error = Color(0xFFB00000),
    fatal = Color(0xFF5A0000),
    info = Color(0xFFC44848),
    pink = Color(0xFFE07070),
    teal = Color(0xFF7A1515),
    magenta = Color(0xFFE10600),
    scrim = Color(0x660D1117),
    glass = Color(0xEBFFFFFF),
    glassBorder = Color(0x0F0D1117),
    shadow = Color(0xFF0D1117),
)

fun VexaColors.toMaterialScheme(): ColorScheme = if (isDark) {
    darkColorScheme(
        primary = plasma, onPrimary = onAccent,
        secondary = ultraviolet, onSecondary = onAccent,
        tertiary = ion, onTertiary = onAccent,
        error = error, onError = onAccent,
        background = bgBase, onBackground = textPrimary,
        surface = bgElevated, onSurface = textPrimary,
        surfaceVariant = surface2, onSurfaceVariant = textSecondary,
        outline = borderStrong, outlineVariant = hairline,
    )
} else {
    lightColorScheme(
        primary = plasma, onPrimary = onAccent,
        secondary = ultraviolet, onSecondary = onAccent,
        tertiary = ion, onTertiary = onAccent,
        error = error, onError = onAccent,
        background = bgBase, onBackground = textPrimary,
        surface = bgElevated, onSurface = textPrimary,
        surfaceVariant = surface2, onSurfaceVariant = textSecondary,
        outline = borderStrong, outlineVariant = hairline,
    )
}
