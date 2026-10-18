package com.critical.vexaemulator.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.R

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

enum class LogoSize(val mark: Int) {
    Hero(96),
    Large(56),
    Header(64),
    Medium(48),
    Small(28),
}

@Composable
fun BrandLogo(
    modifier: Modifier = Modifier,
    size: LogoSize = LogoSize.Large,
) {
    Image(
        painter = painterResource(R.drawable.ic_vexa_mark),
        contentDescription = "VEXA",
        modifier = modifier.size(size.mark.dp),
    )
}
