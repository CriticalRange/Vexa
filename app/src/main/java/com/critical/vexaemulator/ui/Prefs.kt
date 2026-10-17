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
    var vulkan by mutableStateOf(false)
        private set
    var padOverlay by mutableStateOf(false)
        private set
    var uncappedFps by mutableStateOf(false)
        private set
    var lastProgram by mutableStateOf<String?>(null)
        private set

    fun init(context: Context) {
        if (sp != null) return
        val prefs = context.applicationContext.getSharedPreferences("vexa_ui", Context.MODE_PRIVATE)
        sp = prefs
        themeMode = runCatching { ThemeMode.valueOf(prefs.getString(KEY_THEME, null)!!) }
            .getOrDefault(ThemeMode.Dark)
        haptics = prefs.getBoolean(KEY_HAPTICS, true)
        separateWindow = prefs.getBoolean(KEY_SEPARATE_WINDOW, false)
        vulkan = prefs.getBoolean(KEY_VULKAN, false)
        padOverlay = prefs.getBoolean(KEY_PAD_OVERLAY, false)
        uncappedFps = prefs.getBoolean(KEY_UNCAPPED_FPS, false)
        lastProgram = prefs.getString(KEY_LAST_PROGRAM, null)
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

    fun updateVulkan(enabled: Boolean) {
        vulkan = enabled
        sp?.edit { putBoolean(KEY_VULKAN, enabled) }
    }

    fun updatePadOverlay(enabled: Boolean) {
        padOverlay = enabled
        sp?.edit { putBoolean(KEY_PAD_OVERLAY, enabled) }
    }

    fun updateUncappedFps(enabled: Boolean) {
        uncappedFps = enabled
        sp?.edit { putBoolean(KEY_UNCAPPED_FPS, enabled) }
    }

    fun updateLastProgram(id: String) {
        lastProgram = id
        sp?.edit { putString(KEY_LAST_PROGRAM, id) }
    }

    private const val KEY_THEME = "theme_mode"
    private const val KEY_HAPTICS = "haptics"
    private const val KEY_SEPARATE_WINDOW = "separate_window"
    private const val KEY_VULKAN = "vulkan_renderer"
    private const val KEY_PAD_OVERLAY = "pad_overlay"
    private const val KEY_UNCAPPED_FPS = "uncapped_fps"
    private const val KEY_LAST_PROGRAM = "last_program"
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
