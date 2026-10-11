package com.critical.vexaemulator

import android.app.ActivityOptions
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
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
import com.critical.vexaemulator.ui.AmbientOrb
import com.critical.vexaemulator.ui.BrandLogo
import com.critical.vexaemulator.ui.DangerButton
import com.critical.vexaemulator.ui.DotGrid
import com.critical.vexaemulator.ui.LogList
import com.critical.vexaemulator.ui.LogoSize
import com.critical.vexaemulator.ui.MetaItem
import com.critical.vexaemulator.ui.MetadataRow
import com.critical.vexaemulator.ui.PanelSurface
import com.critical.vexaemulator.ui.SectionCard
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.SteamCard
import com.critical.vexaemulator.ui.StatusDot
import com.critical.vexaemulator.ui.SwitchRow
import com.critical.vexaemulator.ui.ThemeRevealHost
import com.critical.vexaemulator.ui.ThemeSegmented
import com.critical.vexaemulator.ui.TinyPill
import com.critical.vexaemulator.ui.VexaButton
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaIcon
import com.critical.vexaemulator.ui.VexaIconButton
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.copyToClipboard
import com.critical.vexaemulator.ui.glyph
import com.critical.vexaemulator.ui.pressScale
import com.critical.vexaemulator.ui.rememberHaptics
import com.critical.vexaemulator.ui.staggerIn
import com.critical.vexaemulator.ui.tagline
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.OverlineStyle
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.ThemeMode
import com.critical.vexaemulator.ui.theme.TileShape
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import com.critical.vexaemulator.ui.toPlainLine
import com.critical.vexaemulator.ui.typeBadge
import com.critical.vexaemulator.ui.vexaGlow
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.hypot
import kotlin.math.max

class MainActivity : ComponentActivity() {
    private var resumeGeneration by mutableIntStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaPrefs.init(this)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark()) {
                ThemeRevealHost { switchTheme ->
                    LauncherScreen(
                        resumeGeneration = resumeGeneration,
                        onThemeChange = switchTheme,
                        onRun = { programId ->
                            LogStore.clear()
                            VexaLogger.log(
                                LogLevel.INFO,
                                LogCategory.ACTIVITY,
                                "Launch selected: ${GuestCatalog.find(programId)?.title ?: programId}",
                                mapOf("source" to "launcher"),
                            )
                            // TODO(experimental): VexaPrefs.separateWindow — start the game in its
                            // own window/task so it can stay alive in the background and be resumed.
                            startActivity(
                                Intent(this, GameActivity::class.java)
                                    .putExtra(GameActivity.EXTRA_PROGRAM_ID, programId),
                                ActivityOptions.makeCustomAnimation(
                                    this, android.R.anim.fade_in, android.R.anim.fade_out,
                                ).toBundle(),
                            )
                        },
                        onQuit = { finish() },
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        resumeGeneration++
    }
}

@Composable
private fun LauncherScreen(
    resumeGeneration: Int,
    onThemeChange: (ThemeMode, Offset) -> Unit,
    onRun: (String) -> Unit,
    onQuit: () -> Unit,
) {
    val c = VexaTheme.colors
    val programs = GuestCatalog.programs
    var selectedId by remember { mutableStateOf(programs.first().id) }
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
        RootState.Probing -> c.textTertiary
        RootState.NoRoot -> c.fatal
        RootState.Ready -> c.success
    }

    // Run → brand circle floods the screen from the button, then the game activity fades in.
    val scope = rememberCoroutineScope()
    val launchFlood = remember { Animatable(0f) }
    var floodOrigin by remember { mutableStateOf(Offset.Zero) }
    var runCenter by remember { mutableStateOf(Offset.Zero) }
    LaunchedEffect(resumeGeneration) { launchFlood.snapTo(0f) }

    Box(Modifier.fillMaxSize().background(c.bgBase)) {
        DotGrid(Modifier.fillMaxWidth().height(280.dp))
        AmbientOrb(Modifier.fillMaxWidth().height(320.dp), center = null, radiusFraction = 0.75f)

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .statusBarsPadding()
                .navigationBarsPadding()
                .padding(horizontal = 18.dp),
        ) {
            Spacer(Modifier.height(14.dp))
            Header(
                themeMode = VexaPrefs.themeMode,
                onThemeChange = onThemeChange,
                modifier = Modifier.staggerIn(0),
            )
            Spacer(Modifier.height(24.dp))
            SectionLabel(
                text = "Program",
                modifier = Modifier.staggerIn(1),
                trailing = {
                    Text(
                        "${programs.size} BINARIES",
                        style = OverlineStyle.copy(color = c.textTertiary),
                    )
                },
            )
            Spacer(Modifier.height(12.dp))
            ProgramTiles(
                programs = programs,
                selectedId = selectedId,
                onSelect = { selectedId = it },
                modifier = Modifier.staggerIn(2),
            )
            Spacer(Modifier.height(14.dp))
            ProgramDetailCard(program = selected, modifier = Modifier.staggerIn(3))
            Spacer(Modifier.height(22.dp))
            SteamCard(modifier = Modifier.fillMaxWidth().staggerIn(4))
            Spacer(Modifier.height(16.dp))
            VexaButton(
                text = if (selected.preinstalled) "Launch game" else "Run program",
                icon = if (selected.preinstalled) VexaGlyph.Gamepad else VexaGlyph.Play,
                onClick = {
                    if (launchFlood.isRunning) return@VexaButton
                    floodOrigin = runCenter
                    scope.launch {
                        launchFlood.animateTo(1f, tween(VexaMotion.LARGE, easing = VexaMotion.Emphasized))
                        onRun(selected.id)
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .staggerIn(4)
                    .onGloballyPositioned {
                        runCenter = it.positionInRoot() + Offset(it.size.width / 2f, it.size.height / 2f)
                    },
            )

            Spacer(Modifier.height(30.dp))
            SectionCard(modifier = Modifier.fillMaxWidth().staggerIn(5)) {
                Column {
                    SwitchRow(
                        label = "Root logging",
                        description = rootStateDescription(rootState, rootSuPath),
                        checked = rootLoggingEnabled,
                        icon = VexaGlyph.Shield,
                        iconTint = c.plasma,
                        dot = rootDot,
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
                    )
                    RowDivider()
                    SwitchRow(
                        label = "Haptic feedback",
                        description = "Vibrate on buttons, toggles and the edge handle",
                        checked = VexaPrefs.haptics,
                        icon = VexaGlyph.Vibrate,
                        iconTint = c.teal,
                        onCheckedChange = VexaPrefs::updateHaptics,
                    )
                }
            }

            Spacer(Modifier.height(30.dp))
            SectionLabel(
                text = "Experimental",
                modifier = Modifier.staggerIn(6),
                trailing = { TinyPill(text = "unstable", color = c.warning, dot = true) },
            )
            Spacer(Modifier.height(12.dp))
            SectionCard(modifier = Modifier.fillMaxWidth().staggerIn(7)) {
                SwitchRow(
                    label = "Separate game window",
                    description = "Launch the runtime in its own window — not implemented yet",
                    checked = VexaPrefs.separateWindow,
                    icon = VexaGlyph.Monitor,
                    iconTint = c.warning,
                    onCheckedChange = VexaPrefs::updateSeparateWindow,
                )
            }

            Spacer(Modifier.height(30.dp))
            SectionLabel(
                text = "Logs",
                modifier = Modifier.staggerIn(8),
                trailing = { TinyPill(text = "live", color = c.success, dot = true, pulse = true) },
            )
            Spacer(Modifier.height(12.dp))
            LogPanel(entries = entries, modifier = Modifier.staggerIn(9))

            Spacer(Modifier.height(22.dp))
            DangerButton(
                text = "Quit VEXA",
                onClick = onQuit,
                modifier = Modifier.fillMaxWidth().staggerIn(10),
            )
            Spacer(Modifier.height(14.dp))
            Text(
                text = "vexa · build ${versionName(context)} · arm64-v8a",
                style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(20.dp))
        }

        if (launchFlood.value > 0f) {
            val brand = c.brandStops
            val bg = c.bgBase
            Canvas(Modifier.fillMaxSize()) {
                val maxR = hypot(max(floodOrigin.x, size.width - floodOrigin.x), max(floodOrigin.y, size.height - floodOrigin.y))
                val p = launchFlood.value
                drawCircle(
                    brush = androidx.compose.ui.graphics.Brush.radialGradient(brand, center = floodOrigin, radius = maxR * p + 1f),
                    radius = maxR * p,
                    center = floodOrigin,
                    alpha = (1f - p * 0.6f),
                )
                drawCircle(bg, radius = maxR * (p * 1.25f - 0.25f).coerceAtLeast(0f), center = floodOrigin)
            }
        }
    }
}

private fun versionName(context: android.content.Context): String =
    runCatching { context.packageManager.getPackageInfo(context.packageName, 0).versionName }.getOrNull() ?: "dev"

private enum class RootState { Probing, NoRoot, Ready }

private fun rootStateDescription(state: RootState, suPath: String?): String = when (state) {
    RootState.Probing -> "Checking for root…"
    RootState.NoRoot -> "No root for this app — grant in KernelSU Manager (safe no-op)"
    RootState.Ready -> "su: $suPath — dmesg/proc snapshots go to artifacts"
}

@Composable
private fun RowDivider() {
    Box(Modifier.fillMaxWidth().padding(horizontal = 16.dp).height(1.dp).background(VexaTheme.colors.hairline))
}

@Composable
private fun Header(
    themeMode: ThemeMode,
    onThemeChange: (ThemeMode, Offset) -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    Row(modifier = modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(modifier = Modifier.weight(1f)) {
            BrandLogo(size = LogoSize.Large)
            Text(
                text = "LINUX X86_64 RUNTIME",
                style = OverlineStyle.copy(color = c.textSecondary),
            )
        }
        ThemeSegmented(mode = themeMode, onSelect = onThemeChange)
    }
}

@Composable
private fun ProgramTiles(
    programs: List<GuestProgram>,
    selectedId: String,
    onSelect: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val listState = rememberLazyListState()
    LazyRow(
        state = listState,
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        contentPadding = PaddingValues(horizontal = 2.dp, vertical = 10.dp),
    ) {
        items(programs, key = { it.id }) { program ->
            ProgramTile(
                program = program,
                selected = program.id == selectedId,
                onClick = { onSelect(program.id) },
                modifier = Modifier.animateItem(),
            )
        }
    }
}

@Composable
private fun ProgramTile(
    program: GuestProgram,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    val (typeLabel, typeColor) = program.typeBadge()
    val border by animateColorAsState(if (selected) c.plasma else c.hairline, label = "tileBorder")
    val titleColor by animateColorAsState(if (selected) c.plasma else c.textPrimary, label = "tileTitle")
    Box(
        modifier
            .width(148.dp)
            .height(122.dp)
            .pressScale(source, 0.95f)
            .then(if (selected) Modifier.vexaGlow(radius = 16.dp, shape = TileShape) else Modifier)
            .clip(TileShape)
            .background(if (selected) c.surface2 else c.surface1)
            .border(if (selected) 1.5.dp else 1.dp, border, TileShape)
            .clickable(source, ripple(color = c.plasma)) {
                haptics.tap()
                onClick()
            },
    ) {
        AnimatedVisibility(
            visible = selected,
            enter = fadeIn() + scaleIn(initialScale = 0.4f),
            exit = fadeOut(),
            modifier = Modifier.align(Alignment.TopCenter),
        ) {
            Box(Modifier.fillMaxWidth().height(3.dp).background(c.brand()))
        }
        Column(Modifier.fillMaxSize().padding(12.dp)) {
            Row(verticalAlignment = Alignment.Top) {
                Box(
                    Modifier
                        .size(34.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(if (selected) c.soft(c.plasma) else c.surface3),
                    contentAlignment = Alignment.Center,
                ) {
                    VexaIcon(program.glyph(), tint = if (selected) c.plasma else typeColor, size = 18.dp)
                }
                Spacer(Modifier.weight(1f))
                AnimatedContent(
                    targetState = selected,
                    transitionSpec = {
                        (fadeIn() + scaleIn(spring(dampingRatio = 0.45f, stiffness = 500f), initialScale = 0f)) togetherWith
                            fadeOut(tween(90))
                    },
                    label = "tileCorner",
                ) { isSel ->
                    if (isSel) {
                        Box(
                            Modifier.size(20.dp).clip(CircleShape).background(c.plasma),
                            contentAlignment = Alignment.Center,
                        ) {
                            VexaIcon(VexaGlyph.Check, tint = c.onAccent, size = 13.dp, strokeWidth = 3f)
                        }
                    } else {
                        Text(
                            typeLabel.uppercase(),
                            style = MonospaceTypography.labelSmall.copy(color = typeColor, fontWeight = FontWeight.SemiBold),
                        )
                    }
                }
            }
            Spacer(Modifier.weight(1f))
            Text(
                program.title,
                style = MaterialTheme.typography.titleLarge.copy(color = titleColor),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                if (selected) "active target" else program.tagline(),
                style = MonospaceTypography.labelSmall.copy(color = if (selected) c.plasma.copy(alpha = 0.8f) else c.textTertiary),
                maxLines = 1,
            )
        }
    }
}

@Composable
private fun ProgramDetailCard(program: GuestProgram, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    PanelSurface(
        modifier = modifier.fillMaxWidth(),
        accentEdge = AccentEdge.Top,
    ) {
        AnimatedContent(
            targetState = program,
            transitionSpec = {
                (fadeIn(tween(VexaMotion.MEDIUM)) + slideInVertically(tween(VexaMotion.MEDIUM, easing = VexaMotion.Emphasized)) { it / 8 }) togetherWith
                    fadeOut(tween(VexaMotion.MICRO)) using SizeTransform(clip = false)
            },
            label = "detail",
        ) { p ->
            val (typeLabel, typeColor) = p.typeBadge()
            Column(modifier = Modifier.padding(18.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    StatusDot(c.plasma, size = 7.dp, pulse = true)
                    Text(
                        text = p.title,
                        style = MaterialTheme.typography.headlineMedium.copy(color = c.textPrimary),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    TinyPill(typeLabel, typeColor)
                }
                Spacer(Modifier.height(8.dp))
                Text(
                    text = p.summary,
                    style = MaterialTheme.typography.bodyMedium.copy(color = c.textSecondary),
                )
                Spacer(Modifier.height(14.dp))
                MetadataRow(items = programMetadata(p, c.plasma, c.ultraviolet, c.success))
            }
        }
    }
}

private fun programMetadata(program: GuestProgram, argv: Color, env: Color, ok: Color): List<MetaItem> = buildList {
    if (program.preinstalled) add(MetaItem("preinstalled", ok, VexaGlyph.Check))
    add(MetaItem(program.fileName, null, VexaGlyph.Terminal))
    if (program.args.isNotEmpty()) add(MetaItem("argv: ${program.args.joinToString(" ")}", argv, VexaGlyph.ChevronRight))
    program.env.forEach { add(MetaItem("env: $it", env, VexaGlyph.Activity)) }
}

@Composable
private fun LogPanel(entries: List<VexaLogEntry>, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    PanelSurface(
        modifier = modifier.fillMaxWidth(),
        color = c.well,
    ) {
        Column {
            Row(
                Modifier.fillMaxWidth().background(c.surface1).padding(start = 14.dp, end = 8.dp, top = 8.dp, bottom = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                StatusDot(c.plasma, size = 6.dp)
                Text(
                    "vexa-ui [pid ${android.os.Process.myPid()}]",
                    style = MonospaceTypography.labelMedium.copy(color = c.textSecondary),
                    modifier = Modifier.weight(1f),
                )
                Text(
                    "${entries.size} lines",
                    style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
                )
                Spacer(Modifier.width(8.dp))
                VexaIconButton(
                    VexaGlyph.Copy,
                    onClick = { copyToClipboard(context, entries.joinToString("\n") { it.toPlainLine() }) },
                    size = 30.dp,
                )
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(c.hairline))
            Box(modifier = Modifier.fillMaxWidth().padding(start = 12.dp, top = 8.dp, bottom = 8.dp, end = 4.dp)) {
                LogList(
                    entries = entries,
                    compact = true,
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 320.dp),
                )
            }
        }
    }
}
