package com.critical.vexaemulator.ui.theme

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.platform.LocalContext
import androidx.core.view.WindowCompat

@Composable
fun VexaEmulatorTheme(
    darkTheme: Boolean = true,
    content: @Composable () -> Unit
) {
    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme

    val window = (LocalContext.current as? Activity)?.window
    SideEffect {
        window?.let {
            it.statusBarColor = android.graphics.Color.TRANSPARENT
            it.navigationBarColor = android.graphics.Color.TRANSPARENT
            WindowCompat.getInsetsController(it, it.decorView)
                .isAppearanceLightStatusBars = !darkTheme
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography,
        content = content
    )
}
