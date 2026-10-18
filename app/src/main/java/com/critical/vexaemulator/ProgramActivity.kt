package com.critical.vexaemulator

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.guest.CustomGuestStore
import com.critical.vexaemulator.ui.SteamGameHero
import com.critical.vexaemulator.ui.rememberSteamDetails
import androidx.compose.foundation.clickable
import androidx.compose.material3.Text
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.steam.library.SteamLibrary
import com.critical.vexaemulator.ui.DownloadStatusBar
import com.critical.vexaemulator.ui.SectionCard
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.SwitchRow
import com.critical.vexaemulator.ui.VexaButton
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import kotlinx.coroutines.launch

class ProgramActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaPrefs.init(this)
        CustomGuestStore.load(this)
        val program = resolveProgram(intent) ?: run {
            finish()
            return
        }
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark()) {
                ProgramScreen(
                    program = program,
                    onBack = { finish() },
                    onStart = { launch ->
                        com.critical.vexaemulator.logging.LogStore.clear()
                        VexaLogger.log(
                            LogLevel.INFO,
                            LogCategory.ACTIVITY,
                            "Launch selected: ${launch.title}",
                            mapOf("source" to "program"),
                        )
                        startActivity(
                            Intent(this, GameActivity::class.java)
                                .putExtra(GameActivity.EXTRA_PROGRAM_ID, launch.id),
                        )
                    },
                )
            }
        }
    }

    companion object {
        const val EXTRA_PROGRAM_ID = "programId"
        const val EXTRA_APP_ID = "appId"

        fun resolveProgram(intent: Intent): com.critical.vexaemulator.guest.GuestProgram? {
            val id = intent.getStringExtra(EXTRA_PROGRAM_ID)
            if (id != null) return GuestCatalog.find(id)
            val appId = intent.getIntExtra(EXTRA_APP_ID, 0)
            if (appId <= 0) return null
            GuestCatalog.find("steam-$appId")?.let { return it }
            val known = SteamLibrary.state.value.games.find { it.appId == appId }
            return com.critical.vexaemulator.guest.GuestProgram(
                id = "steam-$appId",
                title = known?.name ?: "App $appId",
                summary = "",
                assetPath = "",
                fileName = "",
                steamAppId = appId.toString(),
            )
        }
    }
}

@Composable
private fun ProgramScreen(
    program: com.critical.vexaemulator.guest.GuestProgram,
    onBack: () -> Unit,
    onStart: (com.critical.vexaemulator.guest.GuestProgram) -> Unit,
) {
    val programId = program.id
    val c = VexaTheme.colors
    val context = androidx.compose.ui.platform.LocalContext.current
    val scope = rememberCoroutineScope()
    val library by SteamLibrary.state.collectAsState()
    val appId = program.steamAppId?.toIntOrNull()
    val progress = appId?.let { library.downloads[it] }
    // The screen is opened before the depot exists, so keep reading the catalog after install.
    val launch = GuestCatalog.find(program.id)?.takeIf { it.fileName.isNotBlank() }
        ?: appId?.let { GuestCatalog.find("steam-$it") }?.takeIf { it.fileName.isNotBlank() }
        ?: program.takeIf { it.fileName.isNotBlank() }
    val canStart = launch != null
    val listed = appId?.let { id -> library.games.find { it.appId == id } }
    val canDownload = appId != null && !canStart && progress == null && listed?.installed != true &&
        listed?.installable != false && listed?.owned != false
    val details = rememberSteamDetails(program)
    var showTechnical by remember(programId) { mutableStateOf(false) }
    var vulkan by remember { mutableStateOf(VexaPrefs.vulkanFor(programId)) }
    var pad by remember { mutableStateOf(VexaPrefs.padFor(programId)) }
    var uncapped by remember { mutableStateOf(VexaPrefs.uncappedFor(programId)) }
    var stretch by remember { mutableStateOf(VexaPrefs.stretchFor(programId)) }
    Box(Modifier.fillMaxSize().background(c.bgBase)) {
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .statusBarsPadding()
                .padding(horizontal = 20.dp)
                .padding(bottom = 180.dp),
        ) {
            Spacer(Modifier.height(4.dp))
            SubscreenHeader(title = "Game details", onBack = onBack)
            Spacer(Modifier.height(16.dp))
            SteamGameHero(program, details)
            Text(
                text = if (showTechnical) "Hide launch details" else "Show launch details",
                color = c.textSecondary,
                modifier = Modifier.clickable { showTechnical = !showTechnical }.padding(vertical = 16.dp),
            )
            if (showTechnical) ProgramDetailCard(program)
            Spacer(Modifier.height(8.dp))
            SectionLabel(text = "This game")
            Spacer(Modifier.height(12.dp))
            SectionCard(modifier = Modifier.fillMaxWidth()) {
                Column {
                    SwitchRow(
                        label = "Vulkan renderer",
                        description = "Use Vulkan for this game when the title supports it",
                        checked = vulkan,
                        icon = VexaGlyph.Cube,
                        onCheckedChange = {
                            vulkan = it
                            VexaPrefs.updateVulkanFor(programId, it)
                        },
                    )
                    SwitchRow(
                        label = "On-screen gamepad",
                        description = "Show the touch pad for this game",
                        checked = pad,
                        icon = VexaGlyph.Gamepad,
                        onCheckedChange = {
                            pad = it
                            VexaPrefs.updatePadFor(programId, it)
                        },
                    )
                    SwitchRow(
                        label = "Uncapped FPS",
                        description = "Disable vsync pacing for this game",
                        checked = uncapped,
                        icon = VexaGlyph.Activity,
                        onCheckedChange = {
                            uncapped = it
                            VexaPrefs.updateUncappedFor(programId, it)
                        },
                    )
                    SwitchRow(
                        label = "Stretch to screen",
                        description = "Fill the display instead of keeping the game's aspect",
                        checked = stretch,
                        icon = VexaGlyph.Expand,
                        onCheckedChange = {
                            stretch = it
                            VexaPrefs.updateStretchFor(programId, it)
                        },
                    )
                }
            }
        }
        Column(
            Modifier
                .align(androidx.compose.ui.Alignment.BottomCenter)
                .fillMaxWidth()
                .background(c.bgBase)
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 12.dp),
        ) {
            if (appId != null && progress != null) {
                DownloadStatusBar(
                    appId = appId,
                    title = details?.name ?: program.title,
                    progress = progress,
                    onCancel = { SteamLibrary.cancelInstall(appId) },
                )
                Spacer(Modifier.height(10.dp))
            }
            when {
                canStart -> VexaButton(
                    text = "Start ${details?.name ?: program.title}",
                    icon = VexaGlyph.Play,
                    onClick = { launch?.let(onStart) },
                    modifier = Modifier.fillMaxWidth(),
                )
                canDownload -> VexaButton(
                    text = "Download",
                    icon = VexaGlyph.Play,
                    onClick = { appId?.let { SteamLibrary.install(context, it) } },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }
    }
}
