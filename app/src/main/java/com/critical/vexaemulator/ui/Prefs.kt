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
    var stretchFullscreen by mutableStateOf(true)
        private set
    var uncappedFps by mutableStateOf(false)
        private set
    var lastProgram by mutableStateOf<String?>(null)
        private set
    var libraryColumns by mutableStateOf(1)
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
        stretchFullscreen = prefs.getBoolean(KEY_STRETCH, true)
        lastProgram = prefs.getString(KEY_LAST_PROGRAM, null)
        libraryColumns = prefs.getInt(KEY_LIBRARY_COLUMNS, 1).coerceIn(1, 3)
        com.critical.vexaemulator.guest.CustomGuestStore.load(context.applicationContext)
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

    fun updateStretchFullscreen(enabled: Boolean) {
        stretchFullscreen = enabled
        sp?.edit { putBoolean(KEY_STRETCH, enabled) }
    }

    fun updateUncappedFps(enabled: Boolean) {
        uncappedFps = enabled
        sp?.edit { putBoolean(KEY_UNCAPPED_FPS, enabled) }
    }

    fun updateLastProgram(id: String) {
        lastProgram = id
        sp?.edit { putString(KEY_LAST_PROGRAM, id) }
    }

    fun updateLibraryColumns(columns: Int) {
        val next = columns.coerceIn(1, 3)
        libraryColumns = next
        sp?.edit { putInt(KEY_LIBRARY_COLUMNS, next) }
    }

    fun vulkanFor(programId: String): Boolean = gameBool(programId, "vulkan", vulkan)
    fun padFor(programId: String): Boolean = gameBool(programId, "pad", padOverlay)
    fun uncappedFor(programId: String): Boolean = gameBool(programId, "fps", uncappedFps)
    fun stretchFor(programId: String): Boolean = gameBool(programId, "stretch", stretchFullscreen)

    fun updateVulkanFor(programId: String, enabled: Boolean) = putGameBool(programId, "vulkan", enabled)
    fun updatePadFor(programId: String, enabled: Boolean) = putGameBool(programId, "pad", enabled)
    fun updateUncappedFor(programId: String, enabled: Boolean) = putGameBool(programId, "fps", enabled)
    fun updateStretchFor(programId: String, enabled: Boolean) = putGameBool(programId, "stretch", enabled)

    private fun gameBool(programId: String, name: String, fallback: Boolean): Boolean {
        val key = "game.$programId.$name"
        val prefs = sp ?: return fallback
        return if (prefs.contains(key)) prefs.getBoolean(key, fallback) else fallback
    }

    private fun putGameBool(programId: String, name: String, enabled: Boolean) {
        sp?.edit { putBoolean("game.$programId.$name", enabled) }
    }

    /** Remembered cloud-conflict answer for one game: "local", "remote", or null. */
    fun cloudChoice(programId: String): String? =
        sp?.getString(KEY_CLOUD_CHOICE_PREFIX + programId, null)

    fun updateCloudChoice(programId: String, choice: String?) {
        sp?.edit {
            val key = KEY_CLOUD_CHOICE_PREFIX + programId
            if (choice == null) remove(key) else putString(key, choice)
        }
    }

    private const val KEY_CLOUD_CHOICE_PREFIX = "cloud_choice_"
    private const val KEY_THEME = "theme_mode"
    private const val KEY_HAPTICS = "haptics"
    private const val KEY_SEPARATE_WINDOW = "separate_window"
    private const val KEY_VULKAN = "vulkan_renderer"
    private const val KEY_PAD_OVERLAY = "pad_overlay"
    private const val KEY_STRETCH = "stretch_fullscreen"
    private const val KEY_UNCAPPED_FPS = "uncapped_fps"
    private const val KEY_LAST_PROGRAM = "last_program"
    private const val KEY_LIBRARY_COLUMNS = "library_columns"
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
