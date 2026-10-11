package com.critical.vexaemulator.ui.theme

import android.app.Activity
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.core.view.WindowCompat

enum class ThemeMode { Light, Dark, System }

@Composable
fun ThemeMode.resolveDark(): Boolean = when (this) {
    ThemeMode.Light -> false
    ThemeMode.Dark -> true
    ThemeMode.System -> isSystemInDarkTheme()
}

val LocalVexaColors = staticCompositionLocalOf { VoidColors }

object VexaTheme {
    val colors: VexaColors
        @Composable @ReadOnlyComposable get() = LocalVexaColors.current
}

object VexaMotion {
    val Standard = CubicBezierEasing(0.2f, 0f, 0f, 1f)
    val Emphasized = CubicBezierEasing(0.05f, 0.7f, 0.1f, 1f)
    const val MICRO = 120
    const val SMALL = 200
    const val MEDIUM = 320
    const val LARGE = 480
}

/**
 * [animate] crossfades every token when the theme flips. Screens that run a circular
 * reveal snapshot instead pass false so the new palette lands in a single frame.
 */
@Composable
fun VexaEmulatorTheme(
    darkTheme: Boolean = true,
    animate: Boolean = false,
    content: @Composable () -> Unit,
) {
    val target = if (darkTheme) VoidColors else PaperColors
    val colors = if (animate) animatedColors(target) else target

    val window = (LocalContext.current as? Activity)?.window
    SideEffect {
        window?.let {
            val controller = WindowCompat.getInsetsController(it, it.decorView)
            controller.isAppearanceLightStatusBars = !darkTheme
            controller.isAppearanceLightNavigationBars = !darkTheme
        }
    }

    CompositionLocalProvider(LocalVexaColors provides colors) {
        MaterialTheme(
            colorScheme = colors.toMaterialScheme(),
            typography = Typography,
            content = content,
        )
    }
}

@Composable
private fun animatedColors(t: VexaColors): VexaColors {
    @Composable
    fun a(c: Color): Color =
        animateColorAsState(c, tween(VexaMotion.LARGE, easing = VexaMotion.Standard), label = "token").value
    return VexaColors(
        isDark = t.isDark,
        bgBase = a(t.bgBase),
        bgElevated = a(t.bgElevated),
        surface1 = a(t.surface1),
        surface2 = a(t.surface2),
        surface3 = a(t.surface3),
        well = a(t.well),
        hairline = a(t.hairline),
        borderStrong = a(t.borderStrong),
        textPrimary = a(t.textPrimary),
        textSecondary = a(t.textSecondary),
        textTertiary = a(t.textTertiary),
        plasma = a(t.plasma),
        ultraviolet = a(t.ultraviolet),
        ion = a(t.ion),
        onAccent = a(t.onAccent),
        success = a(t.success),
        warning = a(t.warning),
        error = a(t.error),
        fatal = a(t.fatal),
        info = a(t.info),
        pink = a(t.pink),
        teal = a(t.teal),
        magenta = a(t.magenta),
        scrim = a(t.scrim),
        glass = a(t.glass),
        glassBorder = a(t.glassBorder),
        shadow = a(t.shadow),
    )
}
