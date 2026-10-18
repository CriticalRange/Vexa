package com.critical.vexaemulator

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.RootLogCollector
import com.critical.vexaemulator.ui.SectionCard
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.SettingRow
import com.critical.vexaemulator.ui.SwitchRow
import com.critical.vexaemulator.ui.ThemeRevealHost
import com.critical.vexaemulator.ui.ThemeSegmented
import com.critical.vexaemulator.ui.TinyPill
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class SettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaPrefs.init(this)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark()) {
                ThemeRevealHost { switchTheme ->
                    SettingsScreen(
                        onBack = { finish() },
                        onThemeChange = switchTheme,
                    )
                }
            }
        }
    }
}

private enum class RootState { Probing, NoRoot, Ready }

private val SettingsIcon = Color(0xFFB00000)

@Composable
private fun SettingsScreen(
    onBack: () -> Unit,
    onThemeChange: (com.critical.vexaemulator.ui.theme.ThemeMode, androidx.compose.ui.geometry.Offset) -> Unit,
) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    var rootLoggingEnabled by remember { mutableStateOf(RootLogCollector.isEnabled(context.filesDir)) }
    var rootSuPath by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) {
        rootSuPath = withContext(Dispatchers.IO) { RootLogCollector.probeSu() } ?: ""
    }
    val rootState = when {
        rootSuPath == null -> RootState.Probing
        rootSuPath!!.isEmpty() -> RootState.NoRoot
        else -> RootState.Ready
    }
    val rootDot: Color = when (rootState) {
        RootState.Probing -> c.textTertiary
        RootState.NoRoot -> c.fatal
        RootState.Ready -> c.success
    }
    Column(
        Modifier
            .fillMaxSize()
            .background(c.bgBase)
            .verticalScroll(rememberScrollState())
            .statusBarsPadding()
            .navigationBarsPadding()
            .padding(horizontal = 20.dp),
    ) {
        Spacer(Modifier.height(14.dp))
        SubscreenHeader(title = "Settings", onBack = onBack)
        Spacer(Modifier.height(18.dp))
        SectionLabel(text = "Appearance")
        Spacer(Modifier.height(12.dp))
        SectionCard(modifier = Modifier.fillMaxWidth()) {
            SettingRow(
                label = "Theme",
                description = "Light, dark, or follow the system",
                icon = VexaGlyph.Contrast,
                iconTint = SettingsIcon,
            ) {
                ThemeSegmented(mode = VexaPrefs.themeMode, onSelect = onThemeChange)
            }
        }
        Spacer(Modifier.height(22.dp))
        SectionLabel(text = "Preferences")
        Spacer(Modifier.height(12.dp))
        SectionCard(modifier = Modifier.fillMaxWidth()) {
            Column {
                SwitchRow(
                    label = "Root logging",
                    description = when (rootState) {
                        RootState.Probing -> "Checking for root…"
                        RootState.NoRoot -> "No root for this app — grant in KernelSU Manager (safe no-op)"
                        RootState.Ready -> "su: $rootSuPath — dmesg/proc snapshots go to artifacts"
                    },
                    checked = rootLoggingEnabled,
                    icon = VexaGlyph.Shield,
                    iconTint = SettingsIcon,
                    dot = rootDot,
                    onCheckedChange = { checked ->
                        rootLoggingEnabled = checked
                        val wrote = RootLogCollector.setEnabled(context.filesDir, checked)
                        VexaLogger.log(
                            LogLevel.INFO,
                            LogCategory.CONFIG,
                            "Root logging ${if (checked) "enabled" else "disabled"}",
                            mapOf("source" to "settings", "flagWritten" to wrote.toString()),
                        )
                    },
                )
                Hairline()
                SwitchRow(
                    label = "Haptic feedback",
                    description = "Vibrate on buttons, toggles and the edge handle",
                    checked = VexaPrefs.haptics,
                    icon = VexaGlyph.Vibrate,
                    iconTint = SettingsIcon,
                    onCheckedChange = VexaPrefs::updateHaptics,
                )
                Hairline()
                SwitchRow(
                    label = "Vulkan renderer",
                    description = "Launch Megabonk with Vulkan instead of OpenGL",
                    checked = VexaPrefs.vulkan,
                    icon = VexaGlyph.Cube,
                    iconTint = SettingsIcon,
                    onCheckedChange = VexaPrefs::updateVulkan,
                )
                Hairline()
                SwitchRow(
                    label = "On-screen gamepad",
                    description = "Show the touch pad overlay when a game starts",
                    checked = VexaPrefs.padOverlay,
                    icon = VexaGlyph.Gamepad,
                    iconTint = SettingsIcon,
                    onCheckedChange = VexaPrefs::updatePadOverlay,
                )
                Hairline()
                SwitchRow(
                    label = "Uncapped FPS",
                    description = "Disable vsync pacing in OpenGL guests",
                    checked = VexaPrefs.uncappedFps,
                    icon = VexaGlyph.Activity,
                    iconTint = SettingsIcon,
                    onCheckedChange = VexaPrefs::updateUncappedFps,
                )
            }
        }
        Spacer(Modifier.height(22.dp))
        SectionLabel(
            text = "Experimental",
            trailing = { TinyPill(text = "unstable", color = c.warning, dot = true) },
        )
        Spacer(Modifier.height(12.dp))
        SectionCard(modifier = Modifier.fillMaxWidth()) {
            SwitchRow(
                label = "Separate game window",
                description = "Launch the runtime in its own window — not implemented yet",
                checked = VexaPrefs.separateWindow,
                icon = VexaGlyph.Monitor,
                iconTint = SettingsIcon,
                onCheckedChange = VexaPrefs::updateSeparateWindow,
            )
        }
        Spacer(Modifier.height(22.dp))
        Text(
            text = "vexa · build ${versionName(context)} · arm64-v8a",
            style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(20.dp))
    }
}

@Composable
internal fun SubscreenHeader(title: String, onBack: () -> Unit) {
    val c = VexaTheme.colors
    Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
        com.critical.vexaemulator.ui.VexaIconButton(VexaGlyph.ChevronLeft, onClick = onBack)
        Spacer(Modifier.height(0.dp))
        Text(
            title,
            style = androidx.compose.material3.MaterialTheme.typography.headlineMedium,
            color = c.textPrimary,
            modifier = Modifier.padding(start = 12.dp),
        )
    }
}

@Composable
private fun Hairline() {
    Box(Modifier.fillMaxWidth().padding(horizontal = 16.dp).height(1.dp).background(VexaTheme.colors.hairline))
}

private fun versionName(context: android.content.Context): String =
    runCatching { context.packageManager.getPackageInfo(context.packageName, 0).versionName }.getOrNull() ?: "dev"
