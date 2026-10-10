package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.theme.MonospaceTypography

@Composable
fun GameHudChip(
    text: String,
    color: Color,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Text(
        text = text.uppercase(),
        style = MonospaceTypography.labelSmall.copy(
            color = color,
            fontWeight = FontWeight.SemiBold,
            letterSpacing = 1.2.sp,
        ),
        modifier = modifier
            .clip(RoundedCornerShape(100.dp))
            .background(Color.Black.copy(alpha = 0.55f))
            .border(1.dp, color.copy(alpha = 0.45f), RoundedCornerShape(100.dp))
            .clickable { onClick() }
            .padding(horizontal = 14.dp, vertical = 8.dp),
    )
}
