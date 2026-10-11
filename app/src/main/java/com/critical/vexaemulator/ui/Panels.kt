package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.CardShape
import com.critical.vexaemulator.ui.theme.VexaTheme

enum class AccentEdge { None, Top }

/** Hairline-bordered plate. Light theme gets a soft diffuse shadow in place of glow. */
@Composable
fun PanelSurface(
    modifier: Modifier = Modifier,
    shape: Shape = CardShape,
    color: Color = VexaTheme.colors.surface1,
    borderColor: Color = VexaTheme.colors.hairline,
    accentEdge: AccentEdge = AccentEdge.None,
    elevated: Boolean = true,
    content: @Composable () -> Unit,
) {
    val c = VexaTheme.colors
    Box(
        modifier
            .then(
                if (elevated && !c.isDark) {
                    Modifier.shadow(10.dp, shape, ambientColor = c.shadow.copy(alpha = 0.10f), spotColor = c.shadow.copy(alpha = 0.10f))
                } else {
                    Modifier
                },
            )
            .clip(shape)
            .background(color)
            .border(1.dp, borderColor, shape),
    ) {
        if (accentEdge == AccentEdge.Top) {
            Box(
                Modifier
                    .align(Alignment.TopCenter)
                    .fillMaxWidth()
                    .height(1.5.dp)
                    .background(c.brand()),
            )
        }
        content()
    }
}

@Composable
fun SectionCard(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    PanelSurface(modifier, color = VexaTheme.colors.surface1, content = content)
}

/** Floating layer over the game surface: translucent plate + light hairline. */
@Composable
fun GlassSurface(
    modifier: Modifier = Modifier,
    shape: Shape = CardShape,
    accentEdge: AccentEdge = AccentEdge.None,
    content: @Composable () -> Unit,
) {
    val c = VexaTheme.colors
    PanelSurface(
        modifier = modifier,
        shape = shape,
        color = c.glass,
        borderColor = if (c.isDark) c.glassBorder else c.hairline,
        accentEdge = accentEdge,
        content = content,
    )
}
