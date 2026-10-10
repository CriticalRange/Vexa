package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.theme.Edge
import com.critical.vexaemulator.ui.theme.Error
import com.critical.vexaemulator.ui.theme.Ink
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.SurfaceCard
import com.critical.vexaemulator.ui.theme.VexaBlack
import com.critical.vexaemulator.ui.theme.VexaBrandSubtle

// Primary CTA. Accent variant uses the VexaBrandSubtle gradient, which a
// Material3 Button cannot express — so all variants are custom Boxes with a
// consistent 52.dp minimum height and 14.dp corner radius.
@Composable
fun VexaButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    accent: Boolean = true,
    enabled: Boolean = true,
) {
    Box(
        modifier = modifier
            .heightIn(min = 52.dp)
            .clip(RoundedCornerShape(14.dp))
            .then(
                if (accent) {
                    Modifier.background(VexaBrandSubtle)
                } else {
                    Modifier
                        .background(SurfaceCard)
                        .border(1.dp, Edge, RoundedCornerShape(14.dp))
                },
            )
            .clickable(enabled = enabled) { onClick() }
            .alpha(if (enabled) 1f else 0.45f),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = text.uppercase(),
            modifier = Modifier.padding(horizontal = 22.dp),
            style = MaterialTheme.typography.titleMedium.copy(
                fontFamily = FontFamily.SansSerif,
                fontWeight = FontWeight.Black,
                letterSpacing = 1.3.sp,
                color = if (accent) VexaBlack else Ink,
            ),
        )
    }
}

// Destructive action — soft error wash, error hairline, sentence-case label.
@Composable
fun DangerButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .heightIn(min = 52.dp)
            .clip(RoundedCornerShape(14.dp))
            .background(Error.copy(alpha = 0.12f))
            .border(1.dp, Error.copy(alpha = 0.4f), RoundedCornerShape(14.dp))
            .clickable { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(horizontal = 22.dp),
            style = MaterialTheme.typography.titleMedium.copy(
                fontFamily = FontFamily.SansSerif,
                fontWeight = FontWeight.SemiBold,
                color = Error,
            ),
        )
    }
}

// Low-emphasis action — transparent fill, hairline border, muted label.
@Composable
fun GhostButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    Box(
        modifier = modifier
            .heightIn(min = 44.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(Color.Transparent)
            .border(1.dp, Edge, RoundedCornerShape(12.dp))
            .clickable(enabled = enabled) { onClick() }
            .alpha(if (enabled) 1f else 0.45f),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(horizontal = 22.dp),
            style = MaterialTheme.typography.titleMedium.copy(
                fontFamily = FontFamily.SansSerif,
                fontWeight = FontWeight.Medium,
                color = InkMuted,
            ),
        )
    }
}
