package com.critical.vexaemulator.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

// General UI scale — SansSerif, tight modern tracking.
val Typography = Typography(
    displayLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Black,
        fontSize = 45.sp,
        lineHeight = 52.sp,
        letterSpacing = (-1.0).sp,
    ),
    displayMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Black,
        fontSize = 34.sp,
        lineHeight = 40.sp,
        letterSpacing = (-1.2).sp,
    ),
    displaySmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Bold,
        fontSize = 28.sp,
        lineHeight = 34.sp,
        letterSpacing = (-0.6).sp,
    ),
    headlineLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 26.sp,
        lineHeight = 32.sp,
        letterSpacing = (-0.3).sp,
    ),
    headlineMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        lineHeight = 28.sp,
        letterSpacing = (0.2).sp,
    ),
    headlineSmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 18.sp,
        lineHeight = 24.sp,
        letterSpacing = (0.2).sp,
    ),
    titleLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 16.sp,
        lineHeight = 22.sp,
        letterSpacing = (0.2).sp,
    ),
    titleMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 15.sp,
        lineHeight = 20.sp,
        letterSpacing = (0.2).sp,
    ),
    titleSmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = (0.2).sp,
    ),
    bodyLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        lineHeight = 24.sp,
        letterSpacing = (0.2).sp,
    ),
    bodyMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = (0.2).sp,
    ),
    bodySmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 12.sp,
        lineHeight = 16.sp,
        letterSpacing = (0.2).sp,
    ),
    labelLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
        lineHeight = 18.sp,
        letterSpacing = (0.2).sp,
    ),
    labelMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 11.sp,
        lineHeight = 14.sp,
        letterSpacing = (0.6).sp,
    ),
    labelSmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 10.sp,
        lineHeight = 14.sp,
        letterSpacing = (0.4).sp,
    ),
)

// Terminal-style face used for logs, command surfaces and monospace readouts.
val Monospace = FontFamily.Monospace

val MonospaceTypography = Typography(
    displayLarge = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Black,
        fontSize = 45.sp,
        lineHeight = 52.sp,
        letterSpacing = 0.sp,
    ),
    displayMedium = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Black,
        fontSize = 34.sp,
        lineHeight = 40.sp,
        letterSpacing = 0.sp,
    ),
    displaySmall = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Bold,
        fontSize = 28.sp,
        lineHeight = 34.sp,
        letterSpacing = 0.sp,
    ),
    headlineLarge = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.SemiBold,
        fontSize = 26.sp,
        lineHeight = 32.sp,
        letterSpacing = 0.sp,
    ),
    headlineMedium = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        lineHeight = 28.sp,
        letterSpacing = 0.sp,
    ),
    headlineSmall = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.SemiBold,
        fontSize = 18.sp,
        lineHeight = 24.sp,
        letterSpacing = 0.sp,
    ),
    titleLarge = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.SemiBold,
        fontSize = 16.sp,
        lineHeight = 22.sp,
        letterSpacing = 0.sp,
    ),
    titleMedium = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Medium,
        fontSize = 15.sp,
        lineHeight = 20.sp,
        letterSpacing = 0.sp,
    ),
    titleSmall = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Medium,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = 0.sp,
    ),
    bodyLarge = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        lineHeight = 24.sp,
        letterSpacing = 0.sp,
    ),
    bodyMedium = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = 0.sp,
    ),
    bodySmall = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Normal,
        fontSize = 12.sp,
        lineHeight = 16.sp,
        letterSpacing = 0.sp,
    ),
    labelLarge = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
        lineHeight = 18.sp,
        letterSpacing = 0.sp,
    ),
    labelMedium = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Medium,
        fontSize = 11.sp,
        lineHeight = 14.sp,
        letterSpacing = 0.sp,
    ),
    labelSmall = TextStyle(
        fontFamily = Monospace,
        fontWeight = FontWeight.Medium,
        fontSize = 10.sp,
        lineHeight = 14.sp,
        letterSpacing = 0.sp,
    ),
)
