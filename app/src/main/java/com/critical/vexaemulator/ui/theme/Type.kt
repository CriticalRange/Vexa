package com.critical.vexaemulator.ui.theme

import androidx.annotation.FontRes
import androidx.compose.material3.Typography
import androidx.compose.ui.text.ExperimentalTextApi
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontVariation
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.R

// Bundled variable fonts (SIL OFL): each weight is a variation instance of one file.
@OptIn(ExperimentalTextApi::class)
private fun variableFamily(@FontRes res: Int, vararg weights: Int) = FontFamily(
    weights.map { w ->
        Font(
            resId = res,
            weight = FontWeight(w),
            variationSettings = FontVariation.Settings(FontVariation.weight(w)),
        )
    },
)

val SpaceGrotesk = variableFamily(R.font.space_grotesk, 400, 500, 600, 700)
val Inter = variableFamily(R.font.inter, 400, 500, 600, 700)
val JetBrainsMono = variableFamily(R.font.jetbrains_mono, 400, 500, 600, 700, 800)

private fun display(size: Int, line: Int, weight: FontWeight, tracking: Double) = TextStyle(
    fontFamily = Inter,
    fontWeight = weight,
    fontSize = size.sp,
    lineHeight = line.sp,
    letterSpacing = tracking.em,
)

private fun body(size: Int, line: Int, weight: FontWeight = FontWeight.Normal) = TextStyle(
    fontFamily = Inter,
    fontWeight = weight,
    fontSize = size.sp,
    lineHeight = line.sp,
    letterSpacing = 0.sp,
)

// Inter keeps interface headings and controls cohesive; Space Grotesk stays in the wordmark.
val Typography = Typography(
    displayLarge = display(44, 50, FontWeight.Bold, -0.03),
    displayMedium = display(34, 40, FontWeight.Bold, -0.03),
    displaySmall = display(28, 34, FontWeight.Bold, -0.02),
    headlineLarge = display(26, 32, FontWeight.SemiBold, -0.02),
    headlineMedium = display(22, 28, FontWeight.SemiBold, -0.01),
    headlineSmall = display(18, 24, FontWeight.SemiBold, 0.0),
    titleLarge = display(17, 22, FontWeight.SemiBold, 0.0),
    titleMedium = display(15, 20, FontWeight.Medium, 0.0),
    titleSmall = display(14, 20, FontWeight.Medium, 0.0),
    bodyLarge = body(16, 24),
    bodyMedium = body(14, 20),
    bodySmall = body(12, 16),
    labelLarge = body(13, 18, FontWeight.Medium),
    labelMedium = body(11, 14, FontWeight.Medium),
    labelSmall = body(10, 14, FontWeight.Medium),
)

private fun mono(size: Int, line: Int, weight: FontWeight = FontWeight.Normal) = TextStyle(
    fontFamily = JetBrainsMono,
    fontWeight = weight,
    fontSize = size.sp,
    lineHeight = line.sp,
    letterSpacing = 0.sp,
    fontFeatureSettings = "tnum",
)

// Terminal face for logs, ids, argv/env, timestamps and telemetry readouts.
val MonospaceTypography = Typography(
    displayLarge = mono(44, 52, FontWeight.Bold),
    displayMedium = mono(34, 40, FontWeight.Bold),
    displaySmall = mono(28, 34, FontWeight.Bold),
    headlineLarge = mono(26, 32, FontWeight.SemiBold),
    headlineMedium = mono(22, 28, FontWeight.SemiBold),
    headlineSmall = mono(18, 24, FontWeight.SemiBold),
    titleLarge = mono(16, 22, FontWeight.SemiBold),
    titleMedium = mono(15, 20, FontWeight.Medium),
    titleSmall = mono(14, 20, FontWeight.Medium),
    bodyLarge = mono(16, 24),
    bodyMedium = mono(14, 20),
    bodySmall = mono(12, 17),
    labelLarge = mono(13, 18, FontWeight.Medium),
    labelMedium = mono(11, 15, FontWeight.Medium),
    labelSmall = mono(10, 14, FontWeight.Medium),
)

// Instrument-style overline: mono caps with wide tracking.
val OverlineStyle = TextStyle(
    fontFamily = JetBrainsMono,
    fontWeight = FontWeight.SemiBold,
    fontSize = 11.sp,
    lineHeight = 14.sp,
    letterSpacing = 0.14.em,
)
