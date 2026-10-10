// VEXA design tokens: surfaces, ink, semantic colors, accent spectrum, brand gradients, M3 schemes.
package com.critical.vexaemulator.ui.theme

import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

val VexaBlack = Color(0xFF07080B)
val SurfaceBase = Color(0xFF0E1016)
val SurfaceHi = Color(0xFF151922)
val SurfaceCard = Color(0xFF1A1F2B)
val SurfaceCardHover = Color(0xFF202634)
val Edge = Color(0xFF262C3B)
val EdgeBright = Color(0xFF383F52)

val Ink = Color(0xFFF5F7FA)
val InkMuted = Color(0xFFAEB4C0)
val InkDim = Color(0xFF6E7583)

val Success = Color(0xFF34D399)
val Warn = Color(0xFFFBBF24)
val Error = Color(0xFFFB7185)
val Fatal = Color(0xFFF43F5E)
val Info = Color(0xFF38BDF8)
val SuccessSoft = Success.copy(alpha = 0.15f)
val WarnSoft = Warn.copy(alpha = 0.15f)
val ErrorSoft = Error.copy(alpha = 0.15f)
val FatalSoft = Fatal.copy(alpha = 0.16f)
val InfoSoft = Info.copy(alpha = 0.15f)

val Cyan = Color(0xFF22D3EE)
val CyanDeep = Color(0xFF0EA7A7)
val Teal = Color(0xFF2DD4BF)
val Violet = Color(0xFF8B5CF6)
val Indigo = Color(0xFF6366F1)
val Pink = Color(0xFFF472B6)
val Blue = Color(0xFF3B82F6)

val VexaAccent: Color = Cyan

val VexaBrand: Brush = Brush.linearGradient(
    colors = listOf(Cyan, Violet),
    start = Offset.Zero,
    end = Offset.Infinite,
)
val VexaBrandSubtle: Brush = Brush.linearGradient(
    colors = listOf(Cyan.copy(alpha = 0.9f), Violet.copy(alpha = 0.9f)),
    start = Offset.Zero,
    end = Offset.Infinite,
)

val DarkColorScheme = darkColorScheme(
    primary = Cyan,
    onPrimary = VexaBlack,
    primaryContainer = Color(0xFF0C3A45),
    onPrimaryContainer = Color(0xFF66F7FF),
    secondary = Violet,
    onSecondary = VexaBlack,
    secondaryContainer = Color(0xFF2A2150),
    onSecondaryContainer = Color(0xFFD9C8FF),
    tertiary = Teal,
    onTertiary = VexaBlack,
    tertiaryContainer = Color(0xFF054139),
    onTertiaryContainer = Color(0xFF4DFFEE),
    error = Error,
    onError = VexaBlack,
    errorContainer = Color(0xFF4A1221),
    onErrorContainer = Color(0xFFFFB4C4),
    background = VexaBlack,
    onBackground = Ink,
    surface = SurfaceBase,
    onSurface = Ink,
    surfaceVariant = SurfaceCard,
    onSurfaceVariant = InkMuted,
    outline = EdgeBright,
    outlineVariant = Edge,
)

val LightColorScheme = lightColorScheme(
    primary = CyanDeep,
    onPrimary = Ink,
    primaryContainer = Color(0xFFC7F6FF),
    onPrimaryContainer = Color(0xFF003A40),
    secondary = Indigo,
    onSecondary = Ink,
    secondaryContainer = Color(0xFFE0E0FF),
    onSecondaryContainer = Color(0xFF10124A),
    tertiary = Teal,
    onTertiary = VexaBlack,
    tertiaryContainer = Color(0xFF9BFFF1),
    onTertiaryContainer = Color(0xFF003D36),
    error = Fatal,
    onError = Ink,
    errorContainer = Color(0xFFFFDAD6),
    onErrorContainer = Color(0xFF4A1221),
    background = Color(0xFFF7F9FC),
    onBackground = Color(0xFF0E1116),
    surface = Color(0xFFFAFBFD),
    onSurface = Color(0xFF0E1116),
    surfaceVariant = Color(0xFFE2E6EF),
    onSurfaceVariant = Color(0xFF565E6C),
    outline = Color(0xFF8A92A0),
    outlineVariant = Color(0xFFCBD0DB),
)
