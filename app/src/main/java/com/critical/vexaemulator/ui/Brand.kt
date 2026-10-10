package com.critical.vexaemulator.ui

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.theme.SpaceGrotesk
import com.critical.vexaemulator.ui.theme.VexaTheme

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
            ),
        ),
    )
}

enum class LogoSize(val sp: Int) { Hero(84), Large(38), Medium(30), Small(18) }

@Composable
fun BrandLogo(
    modifier: Modifier = Modifier,
    size: LogoSize = LogoSize.Large,
) {
    GradientText(
        text = "VEXA",
        brush = VexaTheme.colors.brand(),
        modifier = modifier,
        fontSize = size.sp.sp,
        fontWeight = FontWeight.Bold,
        letterSpacing = (-0.04).em,
        style = TextStyle(fontFamily = SpaceGrotesk, lineHeight = (size.sp * 1.05f).sp),
    )
}
