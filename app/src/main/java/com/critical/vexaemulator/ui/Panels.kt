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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.CardShape
import com.critical.vexaemulator.ui.theme.Edge
import com.critical.vexaemulator.ui.theme.SurfaceCard
import com.critical.vexaemulator.ui.theme.SurfaceHi
import com.critical.vexaemulator.ui.theme.VexaBrandSubtle

enum class AccentEdge { None, Top }

@Composable
fun PanelSurface(
    modifier: Modifier = Modifier,
    shape: Shape = CardShape,
    color: Color = SurfaceCard,
    borderColor: Color = Edge,
    accentEdge: AccentEdge = AccentEdge.None,
    content: @Composable () -> Unit,
) {
    Box(
        modifier
            .clip(shape)
            .background(color)
            .border(1.dp, borderColor, shape)
    ) {
        if (accentEdge == AccentEdge.Top) {
            Box(
                Modifier
                    .align(Alignment.TopCenter)
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(VexaBrandSubtle)
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
    PanelSurface(modifier, color = SurfaceHi, borderColor = Edge, content = content)
}
