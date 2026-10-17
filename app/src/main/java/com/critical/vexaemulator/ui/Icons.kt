package com.critical.vexaemulator.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Fill
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.scale
import androidx.compose.ui.graphics.vector.PathParser
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

// Stroke glyphs on a 24-unit grid (Lucide geometry, ISC license). Drawn with Canvas
// so the app keeps its no-material-icons rule.
enum class VexaGlyph(val filled: Boolean = false, vararg val d: String) {
    Play(true, "M7 4.5v15l12.5-7.5z"),
    Sun(false, circle(12f, 12f, 4f), "M12 2v2", "M12 20v2", "m4.93 4.93 1.41 1.41", "m17.66 17.66 1.41 1.41",
        "M2 12h2", "M20 12h2", "m6.34 17.66-1.41 1.41", "m19.07 4.93-1.41 1.41"),
    Moon(false, "M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"),
    Monitor(false, rect(2f, 3f, 20f, 14f, 2f), "M8 21h8", "M12 17v4"),
    Terminal(false, "m4 17 6-6-6-6", "M12 19h8"),
    Bug(false, "m8 2 1.88 1.88", "M14.12 3.88 16 2", "M9 7.13v-1a3 3 0 1 1 6 0v1",
        "M12 20c-3.3 0-6-2.7-6-6v-3a4 4 0 0 1 4-4h4a4 4 0 0 1 4 4v3c0 3.3-2.7 6-6 6", "M12 20v-9",
        "M6.53 9C4.6 8.8 3 7.1 3 5", "M6 13H2", "M3 21c0-2.1 1.7-3.9 3.8-4", "M20.97 5c0 2.1-1.6 3.8-3.5 4",
        "M22 13h-4", "M17.2 17c2.1.1 3.8 1.9 3.8 4"),
    Power(false, "M12 2v10", "M18.4 6.6a9 9 0 1 1-12.77.04"),
    Close(false, "M18 6 6 18", "m6 6 12 12"),
    Filter(false, "M22 3H2l8 9.46V19l4 2v-8.54L22 3z"),
    ChevronRight(false, "m9 18 6-6-6-6"),
    ChevronLeft(false, "m15 18-6-6 6-6"),
    ChevronDown(false, "m6 9 6 6 6-6"),
    Check(false, "M20 6 9 17l-5-5"),
    Cube(false, "M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z",
        "m3.3 7 8.7 5 8.7-5", "M12 22V12"),
    Keyboard(false, "M4 6h16a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2z",
        "M6 10h.01", "M10 10h.01", "M14 10h.01", "M18 10h.01", "M8 14h8"),
    Gamepad(false, "M6 12h4", "M8 10v4", "M15 13h.01", "M18 11h.01",
        "M17.32 5H6.68a4 4 0 0 0-3.98 3.59C2.6 9.42 2 14.46 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.41-1.41A2 2 0 0 1 9.83 16h4.34a2 2 0 0 1 1.41.59L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.55-.6-6.58-.69-7.26A4 4 0 0 0 17.32 5z"),
    Copy(false, rect(8f, 8f, 14f, 14f, 2f), "M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2"),
    Search(false, circle(11f, 11f, 8f), "m21 21-4.3-4.3"),
    Shield(false, "M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"),
    EyeOff(false, "M9.88 9.88a3 3 0 1 0 4.24 4.24", "M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68",
        "M6.61 6.61A13.53 13.53 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61", "m2 2 20 20"),
    Vibrate(false, "m2 8 2 2-2 2 2 2-2 2", "m22 8-2 2 2 2-2 2 2 2", rect(8f, 5f, 8f, 14f, 1f)),
    Contrast(false, circle(12f, 12f, 10f), "M12 18a6 6 0 0 0 0-12v12z"),
    ArrowDown(false, "M12 5v14", "m19 12-7 7-7-7"),
    Activity(false, "M22 12h-4l-3 9L9 3l-3 9H2"),
    Grip(true, circle(9f, 6f, 1.3f), circle(9f, 12f, 1.3f), circle(9f, 18f, 1.3f),
        circle(15f, 6f, 1.3f), circle(15f, 12f, 1.3f), circle(15f, 18f, 1.3f)),
    Dot(true, circle(12f, 12f, 4f)),
    ;

    internal val path: Path by lazy {
        Path().apply { d.forEach { addPath(PathParser().parsePathString(it).toPath()) } }
    }
}

private fun circle(cx: Float, cy: Float, r: Float) =
    "M${cx - r} ${cy}a$r $r 0 1 0 ${2 * r} 0a$r $r 0 1 0 ${-2 * r} 0z"

private fun rect(x: Float, y: Float, w: Float, h: Float, r: Float) =
    "M${x + r} ${y}h${w - 2 * r}a$r $r 0 0 1 $r ${r}v${h - 2 * r}a$r $r 0 0 1 ${-r} ${r}h${-(w - 2 * r)}" +
        "a$r $r 0 0 1 ${-r} ${-r}v${-(h - 2 * r)}a$r $r 0 0 1 $r ${-r}z"

@Composable
fun VexaIcon(
    glyph: VexaGlyph,
    tint: Color,
    modifier: Modifier = Modifier,
    size: Dp = 20.dp,
    strokeWidth: Float = 1.9f,
) {
    Canvas(modifier.size(size)) {
        val k = this.size.minDimension / 24f
        scale(k, k, pivot = androidx.compose.ui.geometry.Offset.Zero) {
            drawPath(
                path = glyph.path,
                color = tint,
                style = if (glyph.filled) Fill else Stroke(
                    width = strokeWidth,
                    cap = StrokeCap.Round,
                    join = StrokeJoin.Round,
                ),
            )
        }
    }
}
