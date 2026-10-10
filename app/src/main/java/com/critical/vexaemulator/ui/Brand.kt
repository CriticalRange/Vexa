package com.critical.vexaemulator.ui

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.theme.VexaBrand

@Composable
fun GradientText(
    text: String,
    brush: Brush,
    modifier: Modifier = Modifier,
    fontSize: TextUnit = TextUnit.Unspecified,
    fontWeight: FontWeight? = null,
    letterSpacing: TextUnit = TextUnit.Unspecified,
    style: TextStyle = TextStyle.Default,
) {
    Text(
        text = text,
        modifier = modifier,
        style = style.merge(
            TextStyle(
                brush = brush,
                fontSize = fontSize,
                fontWeight = fontWeight,
                letterSpacing = letterSpacing,
            )
        ),
    )
}

enum class LogoSize { Large, Medium, Small }

@Composable
fun BrandLogo(
    modifier: Modifier = Modifier,
    size: LogoSize = LogoSize.Large,
) {
    GradientText(
        text = "VEXA",
        brush = VexaBrand,
        modifier = modifier,
        fontSize = when (size) {
            LogoSize.Large -> 38.sp
            LogoSize.Medium -> 26.sp
            LogoSize.Small -> 18.sp
        },
        fontWeight = FontWeight.Black,
        letterSpacing = (-1.8).sp,
        style = TextStyle(fontFamily = FontFamily.SansSerif),
    )
}
