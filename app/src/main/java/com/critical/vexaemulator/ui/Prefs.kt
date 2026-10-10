package com.critical.vexaemulator.ui

import android.content.Context
import android.content.SharedPreferences
import android.view.HapticFeedbackConstants
import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalView
import androidx.core.content.edit
import com.critical.vexaemulator.ui.theme.ThemeMode

/** UI preferences shared by the launcher and the game screen (UI process only). */
object VexaPrefs {
    private var sp: SharedPreferences? = null

    var themeMode by mutableStateOf(ThemeMode.Dark)
        private set
    var haptics by mutableStateOf(true)
        private set
    var separateWindow by mutableStateOf(false)
        private set

    fun init(context: Context) {
        if (sp != null) return
        val prefs = context.applicationContext.getSharedPreferences("vexa_ui", Context.MODE_PRIVATE)
        sp = prefs
        themeMode = runCatching { ThemeMode.valueOf(prefs.getString(KEY_THEME, null)!!) }
            .getOrDefault(ThemeMode.Dark)
        haptics = prefs.getBoolean(KEY_HAPTICS, true)
        separateWindow = prefs.getBoolean(KEY_SEPARATE_WINDOW, false)
    }

    fun updateThemeMode(mode: ThemeMode) {
        themeMode = mode
        sp?.edit { putString(KEY_THEME, mode.name) }
    }

    fun updateHaptics(enabled: Boolean) {
        haptics = enabled
        sp?.edit { putBoolean(KEY_HAPTICS, enabled) }
    }

    fun updateSeparateWindow(enabled: Boolean) {
        separateWindow = enabled
        sp?.edit { putBoolean(KEY_SEPARATE_WINDOW, enabled) }
    }

    private const val KEY_THEME = "theme_mode"
    private const val KEY_HAPTICS = "haptics"
    private const val KEY_SEPARATE_WINDOW = "separate_window"
}

class VexaHaptics(private val view: View) {
    fun tap() = perform(HapticFeedbackConstants.KEYBOARD_TAP)
    fun tick() = perform(HapticFeedbackConstants.CLOCK_TICK)
    fun confirm() = perform(HapticFeedbackConstants.CONFIRM)
    fun reject() = perform(HapticFeedbackConstants.REJECT)

    private fun perform(constant: Int) {
        if (VexaPrefs.haptics) view.performHapticFeedback(constant)
    }
}

@Composable
fun rememberHaptics(): VexaHaptics {
    val view = LocalView.current
    return remember(view) { VexaHaptics(view) }
}
