package com.critical.vexaemulator

import android.app.ActivityOptions
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults.Indicator
import androidx.compose.material3.pulltorefresh.pullToRefresh
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.guest.CustomGuestStore
import com.critical.vexaemulator.guest.ElfArtwork
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.steam.SteamLink
import com.critical.vexaemulator.steam.SteamPersona
import com.critical.vexaemulator.steam.SteamPersonaCache
import com.critical.vexaemulator.steam.SteamUiState
import com.critical.vexaemulator.steam.library.CatalogKind
import com.critical.vexaemulator.steam.library.SteamLibrary
import com.critical.vexaemulator.ui.BrandLogo
import com.critical.vexaemulator.ui.DownloadStatusBar
import com.critical.vexaemulator.ui.GhostButton
import com.critical.vexaemulator.ui.LogoSize
import com.critical.vexaemulator.ui.MetaItem
import com.critical.vexaemulator.ui.MetadataRow
import com.critical.vexaemulator.ui.PanelSurface
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.TinyPill
import com.critical.vexaemulator.ui.VexaButton
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaIcon
import com.critical.vexaemulator.ui.VexaIconButton
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.online.OnlineGamesScreen
import com.critical.vexaemulator.ui.pressScale
import com.critical.vexaemulator.ui.rememberHaptics
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.TileShape
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import com.critical.vexaemulator.ui.typeBadge
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
                LauncherScreen(resumeGeneration = resumeGeneration)
            }
        }
    }

    override fun onResume() {
        super.onResume()
        CustomGuestStore.load(this)
        resumeGeneration++
    }
}

private enum class LibraryFilter { Local, Owned, All }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LauncherScreen(resumeGeneration: Int) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    var libraryTick by remember { mutableIntStateOf(0) }
    LaunchedEffect(resumeGeneration) { libraryTick++ }
    val programs = remember(libraryTick, resumeGeneration) {
        GuestCatalog.merged().sortedByDescending { it.preinstalled && !it.userAdded }
    }
    var filter by rememberSaveable { mutableStateOf(LibraryFilter.Local) }
    val layoutColumns = VexaPrefs.libraryColumns
    val library by SteamLibrary.state.collectAsState()
    val scope = rememberCoroutineScope()
    var refreshing by remember { mutableStateOf(false) }
    val pullState = rememberPullToRefreshState()

    Box(Modifier.fillMaxSize().background(c.bgBase).imePadding()) {
        Column(
            Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .padding(horizontal = 20.dp)
                .padding(top = 2.dp),
        ) {
            HomeHeader()
            Spacer(Modifier.height(12.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                GhostButton("Local", { filter = LibraryFilter.Local }, Modifier.weight(1f), active = filter == LibraryFilter.Local)
                GhostButton("Owned", { filter = LibraryFilter.Owned }, Modifier.weight(1f), active = filter == LibraryFilter.Owned)
                GhostButton("All", { filter = LibraryFilter.All }, Modifier.weight(1f), active = filter == LibraryFilter.All)
            }
            Spacer(Modifier.height(16.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                VexaIconButton(
                    VexaGlyph.Plus,
                    tint = c.onAccent,
                    container = c.plasma,
                    onClick = { context.startActivity(Intent(context, AddGameActivity::class.java)) },
                )
                Spacer(Modifier.weight(1f))
                LibraryLayoutToggle(
                    columns = layoutColumns,
                    onColumns = { VexaPrefs.updateLibraryColumns(it) },
                )
            }
            Spacer(Modifier.height(12.dp))
            // Material's default threshold is 80.dp and the drag is halved, so the
            // spinner is already on screen before a release counts as a refresh.
            val refreshDistance = 40.dp
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .pullToRefresh(
                        isRefreshing = refreshing,
                        state = pullState,
                        threshold = refreshDistance,
                        onRefresh = {
                            refreshing = true
                            scope.launch {
                                when (filter) {
                                    LibraryFilter.Local -> {
                                        CustomGuestStore.load(context)
                                        libraryTick++
                                    }
                                    LibraryFilter.Owned -> SteamLibrary.refresh(context, CatalogKind.Owned)
                                    LibraryFilter.All -> SteamLibrary.refresh(context, CatalogKind.All)
                                }
                                refreshing = false
                            }
                        },
                    ),
            ) {
                Column(
                    Modifier
                        .fillMaxSize()
                        .verticalScroll(rememberScrollState())
                        .navigationBarsPadding()
                        .padding(bottom = 24.dp),
                ) {
            when (filter) {
                LibraryFilter.Owned -> OnlineGamesScreen(CatalogKind.Owned, columns = layoutColumns)
                LibraryFilter.All -> OnlineGamesScreen(CatalogKind.All, columns = layoutColumns)
                LibraryFilter.Local -> {
                    val selectProgram: (GuestProgram) -> Unit = { program ->
                        VexaPrefs.updateLastProgram(program.id)
                        context.startActivity(
                            Intent(context, ProgramActivity::class.java)
                                .putExtra(ProgramActivity.EXTRA_PROGRAM_ID, program.id),
                        )
                    }
                    if (layoutColumns > 1) {
                        programs.chunked(layoutColumns).forEach { rowItems ->
                            Row(
                                Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(10.dp),
                            ) {
                                rowItems.forEach { program ->
                                    ProgramCard(
                                        program = program,
                                        selected = false,
                                        onClick = { selectProgram(program) },
                                        modifier = Modifier.weight(1f),
                                    )
                                }
                                repeat(layoutColumns - rowItems.size) { Spacer(Modifier.weight(1f)) }
                            }
                            Spacer(Modifier.height(10.dp))
                        }
                    } else {
                        programs.forEach { program ->
                            ProgramRow(
                                program = program,
                                selected = false,
                                onClick = { selectProgram(program) },
                            )
                            Spacer(Modifier.height(8.dp))
                        }
                    }
                }
            }
            Spacer(Modifier.height(if (library.downloads.isEmpty()) 20.dp else 120.dp))
                }
                Indicator(
                    modifier = Modifier.align(Alignment.TopCenter),
                    isRefreshing = refreshing,
                    state = pullState,
                    containerColor = c.surface2,
                    color = c.plasma,
                    maxDistance = refreshDistance,
                )
            }
        }
        if (library.downloads.isNotEmpty()) {
            Column(
                Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .background(c.bgBase)
                    .navigationBarsPadding()
                    .padding(horizontal = 20.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                library.downloads.forEach { (appId, progress) ->
                    val title = library.games.find { it.appId == appId }?.name ?: "App $appId"
                    DownloadStatusBar(
                        appId = appId,
                        title = title,
                        progress = progress,
                        onCancel = { SteamLibrary.cancelInstall(appId) },
                    )
                }
            }
        }
    }
}

@Composable
private fun HomeHeader() {
    val context = LocalContext.current
    Box(Modifier.fillMaxWidth().height(64.dp)) {
        BrandLogo(size = LogoSize.Header, modifier = Modifier.align(Alignment.CenterStart))
        AccountChip(
            onClick = { context.startActivity(Intent(context, AccountActivity::class.java)) },
            modifier = Modifier.align(Alignment.Center),
        )
        Row(Modifier.align(Alignment.CenterEnd), verticalAlignment = Alignment.CenterVertically) {
            VexaIconButton(
                VexaGlyph.Logs,
                onClick = { context.startActivity(Intent(context, LogsActivity::class.java)) },
            )
            Spacer(Modifier.width(8.dp))
            VexaIconButton(
                VexaGlyph.Settings,
                onClick = { context.startActivity(Intent(context, SettingsActivity::class.java)) },
            )
        }
    }
}

@Composable
private fun AccountChip(onClick: () -> Unit, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val steam by SteamLink.state.collectAsState()
    LaunchedEffect(Unit) { SteamLink.restore(context) }
    val signed = steam as? SteamUiState.SignedIn
    var persona by remember { mutableStateOf<SteamPersona?>(null) }
    LaunchedEffect(signed?.steamId) {
        val id = signed?.steamId
        persona = if (id.isNullOrBlank()) null else {
            SteamPersonaCache.cached(id) ?: withContext(Dispatchers.IO) { SteamPersonaCache.load(id) }
        }
    }
    val label = when {
        signed == null -> "Sign in"
        !persona?.name.isNullOrBlank() -> persona?.name.orEmpty()
        else -> signed.accountName
    }
    val initials = label.filter { it.isLetterOrDigit() }.take(2).uppercase().ifBlank { "?" }
    val source = remember { MutableInteractionSource() }
    Row(
        modifier
            .clip(PillShape)
            .background(c.surface2)
            .border(1.dp, c.hairline, PillShape)
            .widthIn(max = 180.dp)
            .clickable(source, ripple(color = c.plasma), onClick = onClick)
            .padding(start = 4.dp, end = 12.dp, top = 4.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier.size(28.dp).clip(CircleShape).background(c.surface3),
            contentAlignment = Alignment.Center,
        ) {
            val avatar = persona?.avatar
            if (avatar != null) {
                Image(avatar.asImageBitmap(), contentDescription = label, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
            } else {
                Text(initials, style = MaterialTheme.typography.labelSmall, color = c.textPrimary)
            }
        }
        Spacer(Modifier.width(8.dp))
        Text(
            label,
            style = MaterialTheme.typography.labelLarge,
            color = c.textPrimary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}

@Composable
private fun rememberProgramLogo(program: GuestProgram, guestDir: java.io.File): android.graphics.Bitmap? {
    val details = com.critical.vexaemulator.ui.rememberSteamDetails(program)
    val steamImage = com.critical.vexaemulator.ui.rememberSteamImage(
        details?.let { listOf(it.vertical, it.header) }.orEmpty(),
    )
    var logo by remember(program.id, program.iconFile) {
        mutableStateOf(ElfArtwork.bitmapFor(program, guestDir))
    }
    LaunchedEffect(program.id, program.fileName, program.iconFile) {
        if (logo == null) {
            logo = withContext(Dispatchers.IO) { ElfArtwork.load(program, guestDir) }
        }
    }
    return steamImage ?: logo
}

@Composable
private fun ProgramRow(program: GuestProgram, selected: Boolean, onClick: () -> Unit) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    val guestDir = remember(program.id) { CustomGuestStore.guestDir(context, program.id) }
    val logo = rememberProgramLogo(program, guestDir)
    Row(
        Modifier
            .fillMaxWidth()
            .pressScale(source, 0.98f)
            .clip(TileShape)
            .background(if (selected) c.surface2 else c.surface1)
            .border(if (selected) 1.5.dp else 1.dp, if (selected) c.plasma else c.hairline, TileShape)
            .clickable(source, ripple(color = c.plasma)) {
                haptics.tap()
                onClick()
            }
            .padding(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier.size(48.dp).clip(RoundedCornerShape(10.dp)).background(c.surface3),
            contentAlignment = Alignment.Center,
        ) {
            if (logo != null) {
                Image(
                    bitmap = logo.asImageBitmap(),
                    contentDescription = program.title,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                )
            } else {
                VexaIcon(program.glyphFallback(), tint = c.textSecondary, size = 20.dp)
            }
        }
        Spacer(Modifier.width(12.dp))
        Text(
            program.title,
            style = MaterialTheme.typography.titleMedium,
            color = if (selected) c.plasma else c.textPrimary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
        if (selected) {
            VexaIcon(VexaGlyph.Check, tint = c.plasma, size = 16.dp, strokeWidth = 2.6f)
        }
    }
}

@Composable
private fun LibraryLayoutToggle(columns: Int, onColumns: (Int) -> Unit) {
    val c = VexaTheme.colors
    val haptics = rememberHaptics()
    Row(
        Modifier
            .clip(PillShape)
            .background(c.surface2)
            .border(1.dp, c.hairline, PillShape)
            .padding(3.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        listOf(1, 2, 3).forEach { count ->
            LayoutModeIcon(
                columns = count,
                selected = columns == count,
                onClick = {
                    if (columns != count) {
                        haptics.tick()
                        onColumns(count)
                    }
                },
            )
        }
    }
}

@Composable
private fun LayoutModeIcon(columns: Int, selected: Boolean, onClick: () -> Unit) {
    val c = VexaTheme.colors
    val source = remember { MutableInteractionSource() }
    val tint = if (selected) c.onAccent else c.textTertiary
    Box(
        Modifier
            .size(34.dp)
            .clip(CircleShape)
            .background(if (selected) c.plasma else Color.Transparent)
            .clickable(source, ripple(color = c.plasma), onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Canvas(Modifier.size(16.dp)) {
            if (columns == 1) {
                val barH = size.height * 0.14f
                val gap = (size.height - barH * 3f) / 2f
                repeat(3) { i ->
                    drawRoundRect(
                        color = tint,
                        topLeft = Offset(0f, i * (barH + gap)),
                        size = Size(size.width, barH),
                        cornerRadius = CornerRadius(barH / 2f),
                    )
                }
            } else {
                val gap = size.width * 0.12f
                val cell = (size.width - gap * (columns - 1)) / columns
                val rows = 2
                val rowGap = size.height * 0.12f
                val cellH = (size.height - rowGap) / rows
                repeat(columns) { col ->
                    repeat(rows) { row ->
                        drawRoundRect(
                            color = tint,
                            topLeft = Offset(col * (cell + gap), row * (cellH + rowGap)),
                            size = Size(cell, cellH),
                            cornerRadius = CornerRadius(cell * 0.22f),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ProgramCard(
    program: GuestProgram,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val haptics = rememberHaptics()
    val source = remember { MutableInteractionSource() }
    val guestDir = remember(program.id) { CustomGuestStore.guestDir(context, program.id) }
    val logo = rememberProgramLogo(program, guestDir)
    Column(
        modifier
            .pressScale(source, 0.98f)
            .clip(TileShape)
            .background(if (selected) c.surface2 else c.surface1)
            .border(if (selected) 1.5.dp else 1.dp, if (selected) c.plasma else c.hairline, TileShape)
            .clickable(source, ripple(color = c.plasma)) {
                haptics.tap()
                onClick()
            },
    ) {
        val portrait = logo != null && logo.height > logo.width
        Box(
            Modifier.fillMaxWidth().aspectRatio(if (portrait) 748f / 896f else 1f).background(c.surface3),
            contentAlignment = Alignment.Center,
        ) {
            if (logo != null) {
                Image(
                    bitmap = logo.asImageBitmap(),
                    contentDescription = program.title,
                    contentScale = ContentScale.Fit,
                    modifier = Modifier.fillMaxSize(),
                )
            } else {
                VexaIcon(program.glyphFallback(), tint = c.textSecondary, size = 28.dp)
            }
        }
        Text(
            program.title,
            style = MaterialTheme.typography.titleSmall,
            color = if (selected) c.plasma else c.textPrimary,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.fillMaxWidth().padding(horizontal = 10.dp, vertical = 10.dp),
        )
    }
}

private fun GuestProgram.glyphFallback(): VexaGlyph = when {
    preinstalled -> VexaGlyph.Gamepad
    else -> VexaGlyph.Terminal
}

@Composable
internal fun ProgramDetailCard(program: GuestProgram) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val path = CustomGuestStore.guestDir(context, program.id).resolve(program.fileName).absolutePath
    PanelSurface(
        modifier = Modifier.fillMaxWidth(),
        color = c.bgBase,
        borderColor = Color.Transparent,
        elevated = false,
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
            val appId = p.steamAppId ?: p.env.firstOrNull { it.startsWith("SteamAppId=") }?.substringAfter("=")
            Column(modifier = Modifier.padding(vertical = 14.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        text = p.title,
                        style = MaterialTheme.typography.headlineLarge.copy(color = c.textPrimary),
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f).padding(end = 12.dp),
                    )
                    TinyPill(typeLabel, typeColor)
                }
                Spacer(Modifier.height(8.dp))
                Text(text = p.summary, style = MaterialTheme.typography.bodyMedium.copy(color = c.textSecondary))
                Spacer(Modifier.height(14.dp))
                MetadataRow(items = detailMeta(p, path, appId, c.plasma, c.ultraviolet, c.success))
            }
        }
    }
}

private fun detailMeta(
    program: GuestProgram,
    path: String,
    appId: String?,
    argv: Color,
    env: Color,
    ok: Color,
): List<MetaItem> = buildList {
    if (program.preinstalled && !program.userAdded) add(MetaItem("preinstalled", ok, VexaGlyph.Check))
    if (program.userAdded) add(MetaItem("added", ok, VexaGlyph.Check))
    add(MetaItem(program.fileName, null, VexaGlyph.Terminal))
    add(MetaItem(program.arch ?: "x86_64", null, VexaGlyph.Cube))
    if (!appId.isNullOrBlank()) add(MetaItem("SteamAppId $appId", argv, VexaGlyph.Activity))
    add(MetaItem(path, null, VexaGlyph.ChevronRight))
    if (program.args.isNotEmpty()) add(MetaItem("argv: ${program.args.joinToString(" ")}", argv, VexaGlyph.ChevronRight))
    program.env.forEach { add(MetaItem("env: $it", env, VexaGlyph.Activity)) }
}
