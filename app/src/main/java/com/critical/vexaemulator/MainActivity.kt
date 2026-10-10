package com.critical.vexaemulator

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.RootLogCollector
import com.critical.vexaemulator.ui.AccentEdge
import com.critical.vexaemulator.ui.BrandLogo
import com.critical.vexaemulator.ui.DangerButton
import com.critical.vexaemulator.ui.LogList
import com.critical.vexaemulator.ui.LogoSize
import com.critical.vexaemulator.ui.MetadataRow
import com.critical.vexaemulator.ui.PanelSurface
import com.critical.vexaemulator.ui.PickerCard
import com.critical.vexaemulator.ui.PickerItem
import com.critical.vexaemulator.ui.SectionCard
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.StatusDot
import com.critical.vexaemulator.ui.SwitchRow
import com.critical.vexaemulator.ui.TinyPill
import com.critical.vexaemulator.ui.VexaButton
import com.critical.vexaemulator.ui.theme.Cyan
import com.critical.vexaemulator.ui.theme.Edge
import com.critical.vexaemulator.ui.theme.Fatal
import com.critical.vexaemulator.ui.theme.Ink
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.Success
import com.critical.vexaemulator.ui.theme.SurfaceHi
import com.critical.vexaemulator.ui.theme.Typography
import com.critical.vexaemulator.ui.theme.VexaBlack
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.Violet
import com.critical.vexaemulator.ui.theme.Warn
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = true) {
                LauncherScreen(
                    onRun = { programId ->
                        LogStore.clear()
                        VexaLogger.log(
                            LogLevel.INFO,
                            LogCategory.ACTIVITY,
                            "Launch selected: ${GuestCatalog.find(programId)?.title ?: programId}",
                            mapOf("source" to "launcher"),
                        )
                        startActivity(
                            Intent(this, GameActivity::class.java)
                                .putExtra(GameActivity.EXTRA_PROGRAM_ID, programId)
                        )
                    },
                    onQuit = { finish() },
                )
            }
        }
    }
}

@Composable
private fun LauncherScreen(
    onRun: (String) -> Unit,
    onQuit: () -> Unit,
) {
    val programs = GuestCatalog.programs
    var selectedId by remember { mutableStateOf(programs.first().id) }
    var menuOpen by remember { mutableStateOf(false) }
    val selected = programs.first { it.id == selectedId }
    val entries by LogStore.entries.collectAsState()

    val context = LocalContext.current
    var rootLoggingEnabled by remember {
        mutableStateOf(RootLogCollector.isEnabled(context.filesDir))
    }
    var rootSuPath by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) {
        rootSuPath = withContext(Dispatchers.IO) { RootLogCollector.probeSu() } ?: ""
    }

    val rootState = remember(rootSuPath) {
        when {
            rootSuPath == null -> RootState.Probing
            rootSuPath!!.isEmpty() -> RootState.NoRoot
            else -> RootState.Ready
        }
    }
    val rootDot: Color = when (rootState) {
        RootState.Probing -> InkDim
        RootState.NoRoot -> Fatal
        RootState.Ready -> Success
    }

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        containerColor = VexaBlack,
        contentColor = Ink,
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 20.dp),
        ) {
            Spacer(modifier = Modifier.height(18.dp))
            Header()
            Spacer(modifier = Modifier.height(24.dp))

            SectionLabel(text = "Program")
            Spacer(modifier = Modifier.height(10.dp))
            PickerCard(
                label = "Selected",
                selected = selected.title,
                subtitle = "id: ${selected.id}",
                expanded = menuOpen,
                onExpandedChange = { menuOpen = it },
                items = programs.map { PickerItem(value = it.id, label = it.title) },
                onSelect = { item ->
                    selectedId = item.value
                    menuOpen = false
                },
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(modifier = Modifier.height(12.dp))
            ProgramDetailCard(program = selected)
            Spacer(modifier = Modifier.height(16.dp))
            VexaButton(
                text = "Run VEXA Program",
                onClick = { onRun(selected.id) },
                modifier = Modifier.fillMaxWidth(),
            )

            Spacer(modifier = Modifier.height(26.dp))
            SectionLabel(text = "System")
            Spacer(modifier = Modifier.height(10.dp))
            SectionCard(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    SwitchRow(
                        label = "Root logging",
                        description = rootStateDescription(rootState, rootSuPath),
                        checked = rootLoggingEnabled,
                        onCheckedChange = { checked ->
                            rootLoggingEnabled = checked
                            val wrote = RootLogCollector.setEnabled(context.filesDir, checked)
                            VexaLogger.log(
                                LogLevel.INFO,
                                LogCategory.CONFIG,
                                "Root logging ${if (checked) "enabled" else "disabled"}",
                                mapOf(
                                    "source" to "launcher",
                                    "flagWritten" to wrote.toString(),
                                ),
                            )
                        },
                        dot = rootDot,
                    )
                }
            }

            Spacer(modifier = Modifier.height(26.dp))
            SectionLabel(
                text = "Logs",
                trailing = { TinyPill(text = "live", color = Success) },
            )
            Spacer(modifier = Modifier.height(10.dp))
            LogPanel(entries = entries)

            Spacer(modifier = Modifier.height(20.dp))
            DangerButton(
                text = "Quit",
                onClick = onQuit,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(modifier = Modifier.height(20.dp))
        }
    }
}

private enum class RootState { Probing, NoRoot, Ready }

private fun rootStateDescription(state: RootState, suPath: String?): String = when (state) {
    RootState.Probing -> "Checking for root…"
    RootState.NoRoot -> "No root for this app — grant in KernelSU Manager (safe no-op)"
    RootState.Ready -> "su: $suPath — dmesg/proc snapshots go to artifacts"
}

@Composable
private fun Header() {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        BrandLogo(size = LogoSize.Medium)
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = "Linux x86_64 runtime",
                style = Typography.bodyMedium.copy(
                    fontFamily = FontFamily.Monospace,
                    color = InkMuted,
                ),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Spacer(modifier = Modifier.height(4.dp))
            TinyPill(
                text = "VEXA",
                color = Cyan,
            )
        }
        Spacer(modifier = Modifier.width(12.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            StatusDot(color = Success)
            Spacer(modifier = Modifier.width(6.dp))
            TinyPill(text = "ready", color = Success)
        }
    }
}

@Composable
private fun ProgramDetailCard(program: GuestProgram) {
    PanelSurface(
        modifier = Modifier.fillMaxWidth(),
        borderColor = Edge,
        accentEdge = AccentEdge.Top,
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = program.title,
                style = Typography.headlineSmall.copy(
                    fontFamily = FontFamily.Monospace,
                    color = Ink,
                ),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = program.summary,
                style = MonospaceTypography.bodySmall.copy(color = InkMuted),
                maxLines = 3,
                overflow = TextOverflow.Ellipsis,
            )
            Spacer(modifier = Modifier.height(12.dp))
            MetadataRow(items = programMetadata(program))
        }
    }
}

private fun programMetadata(program: GuestProgram): List<Pair<String, Color?>> = buildList {
    val (typeLabel, typeColor) = typeBadge(program)
    add(typeLabel to typeColor)
    if (program.args.isNotEmpty()) {
        add("argv: ${program.args.joinToString(" ")}" to null)
    }
    if (program.env.isNotEmpty()) {
        add("env: ${program.env.joinToString(" ")}" to Violet)
    }
}

private fun typeBadge(program: GuestProgram): Pair<String, Color?> = when {
    program.fileName.contains("busybox") -> "BusyBox" to Warn
    program.fileName.endsWith(".x86_64-linux-gnu") -> "ELF" to Cyan
    program.fileName.endsWith(".so") -> "Shared lib" to Violet
    else -> "Native" to Success
}

@Composable
private fun LogPanel(entries: List<VexaLogEntry>) {
    PanelSurface(
        modifier = Modifier.fillMaxWidth(),
        borderColor = Edge,
        accentEdge = AccentEdge.Top,
        color = SurfaceHi,
    ) {
        Box(modifier = Modifier.fillMaxWidth().padding(10.dp)) {
            LogList(
                entries = entries,
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 320.dp),
            )
        }
    }
}
