// VEXA "Quantum Terminal" design tokens: Void (dark) and Paper (light) palettes, brand gradients, M3 schemes.
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
    val brandStops: List<Color> get() = listOf(plasma, ion, ultraviolet)

    fun brand(start: Offset = Offset.Zero, end: Offset = Offset.Infinite): Brush =
        Brush.linearGradient(brandStops, start, end)

    fun soft(color: Color): Color = color.copy(alpha = if (isDark) 0.14f else 0.10f)
}

val VoidColors = VexaColors(
    isDark = true,
    bgBase = Color(0xFF07080C),
    bgElevated = Color(0xFF0D0F15),
    surface1 = Color(0xFF12151D),
    surface2 = Color(0xFF181C26),
    surface3 = Color(0xFF1F2431),
    well = Color(0xFF0A0C12),
    hairline = Color(0xFF242A38),
    borderStrong = Color(0xFF343B4E),
    textPrimary = Color(0xFFF3F5F9),
    textSecondary = Color(0xFFA9B0BE),
    textTertiary = Color(0xFF6B7282),
    plasma = Color(0xFF2EE6F2),
    ultraviolet = Color(0xFF8B6CFF),
    ion = Color(0xFF4F8BFF),
    onAccent = Color(0xFF07080C),
    success = Color(0xFF34D399),
    warning = Color(0xFFFBBF24),
    error = Color(0xFFFB7185),
    fatal = Color(0xFFF43F5E),
    info = Color(0xFF38BDF8),
    pink = Color(0xFFF472B6),
    teal = Color(0xFF2DD4BF),
    magenta = Color(0xFFE879F9),
    scrim = Color(0x99000000),
    glass = Color(0xB812151D),
    glassBorder = Color(0x14FFFFFF),
    shadow = Color(0xFF2EE6F2),
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
    plasma = Color(0xFF0891B2),
    ultraviolet = Color(0xFF6D4AFF),
    ion = Color(0xFF2F6BFF),
    onAccent = Color(0xFFFFFFFF),
    success = Color(0xFF059669),
    warning = Color(0xFFD97706),
    error = Color(0xFFE11D48),
    fatal = Color(0xFFBE123C),
    info = Color(0xFF0284C7),
    pink = Color(0xFFDB2777),
    teal = Color(0xFF0D9488),
    magenta = Color(0xFFC026D3),
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
