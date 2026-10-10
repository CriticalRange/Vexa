// Shape tokens shared across components so every surface rounds consistently.
package com.critical.vexaemulator.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

data class VexaShapeTokens(
    val extraSmall: Dp,
    val small: Dp,
    val medium: Dp,
    val large: Dp,
    val extraLarge: Dp,
)

val ShapeTokens = VexaShapeTokens(
    extraSmall = 10.dp,
    small = 12.dp,
    medium = 16.dp,
    large = 22.dp,
    extraLarge = 28.dp,
)

val SmallShape: Shape = RoundedCornerShape(ShapeTokens.extraSmall)
val CardShape: Shape = RoundedCornerShape(ShapeTokens.medium)
val PanelShape: Shape = RoundedCornerShape(ShapeTokens.large)
