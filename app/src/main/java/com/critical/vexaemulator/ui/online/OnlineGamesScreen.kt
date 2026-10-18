package com.critical.vexaemulator.ui.online
import androidx.compose.foundation.Image
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.steam.library.CatalogKind
import com.critical.vexaemulator.steam.library.OnlineGame
import com.critical.vexaemulator.steam.library.SteamLibrary
import com.critical.vexaemulator.ui.GhostButton
import com.critical.vexaemulator.ui.SecondaryButton
import com.critical.vexaemulator.ui.SectionCard
import com.critical.vexaemulator.ui.TinyPill
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlinx.coroutines.launch
@Composable
fun OnlineGamesScreen(kind: CatalogKind, columns: Int, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val library by SteamLibrary.state.collectAsState()
    LaunchedEffect(kind) {
        SteamLibrary.refresh(context, kind)
    }
    val colors = VexaTheme.colors
    Column(modifier.fillMaxWidth()) {
        if (library.loading) {
            Spacer(Modifier.height(10.dp))
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth(), color = colors.plasma, trackColor = colors.surface3)
            Text(library.phase, color = colors.textSecondary)
        }
        if (library.connectionProblem && !library.loading) {
            Column(
                Modifier.fillMaxWidth().height(480.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Text("Steam", style = androidx.compose.material3.MaterialTheme.typography.headlineMedium, color = colors.textPrimary)
                Spacer(Modifier.height(12.dp))
                Text(
                    library.message,
                    color = colors.textSecondary,
                    textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                    modifier = Modifier.padding(horizontal = 24.dp),
                )
                Spacer(Modifier.height(20.dp))
                com.critical.vexaemulator.ui.VexaButton(
                    text = "Try again",
                    icon = null,
                    onClick = { scope.launch { SteamLibrary.refresh(context, kind) } },
                )
            }
        }
        if (library.message.isNotBlank() && !library.connectionProblem) {
            Spacer(Modifier.height(10.dp))
            Text(library.message, color = colors.textSecondary)
        }
        Spacer(Modifier.height(10.dp))
        if (library.games.isEmpty() && !library.loading && !library.connectionProblem) {
            Text(
                when {
                    library.signedOut -> "Signed out."
                    kind == CatalogKind.Owned -> "No owned games to show."
                    else -> "No games on this page."
                },
                color = colors.textTertiary,
            )
        }
        if (columns > 1) {
            library.games.chunked(columns).forEach { rowItems ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    rowItems.forEach { game ->
                        GameRow(
                            game = game,
                            onOpen = { openProgram(context, game) },
                            modifier = Modifier.weight(1f),
                            grid = true,
                        )
                    }
                    repeat(columns - rowItems.size) { Spacer(Modifier.weight(1f)) }
                }
                Spacer(Modifier.height(10.dp))
            }
        } else {
            library.games.forEach { game ->
                GameRow(
                    game = game,
                    onOpen = { openProgram(context, game) },
                )
                Spacer(Modifier.height(8.dp))
            }
        }
        if (library.canLoadMore) {
            SecondaryButton(
                text = if (library.loading) "Loading" else "Load more",
                onClick = { scope.launch { SteamLibrary.loadMore(context) } },
                enabled = !library.loading,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
private fun openProgram(context: android.content.Context, game: OnlineGame) {
    val id = "steam-${game.appId}"
    val intent = android.content.Intent(context, com.critical.vexaemulator.ProgramActivity::class.java)
    if (com.critical.vexaemulator.guest.GuestCatalog.find(id) != null) {
        intent.putExtra(com.critical.vexaemulator.ProgramActivity.EXTRA_PROGRAM_ID, id)
    } else {
        intent.putExtra(com.critical.vexaemulator.ProgramActivity.EXTRA_APP_ID, game.appId)
    }
    context.startActivity(intent)
}

@Composable
private fun GameRow(
    game: OnlineGame,
    onOpen: () -> Unit,
    modifier: Modifier = Modifier,
    grid: Boolean = false,
) {
    val colors = VexaTheme.colors
    SectionCard(modifier.fillMaxWidth().clickable(onClick = onOpen)) {
        Column(Modifier.fillMaxWidth().padding(12.dp)) {
            HeaderImage(game.appId, grid)
            Spacer(Modifier.height(10.dp))
            Text(game.name, color = colors.textPrimary, fontWeight = FontWeight.SemiBold)
            if (game.genre.isNotBlank()) {
                Spacer(Modifier.height(4.dp))
                Text(
                    game.genre,
                    color = colors.textTertiary,
                    modifier = Modifier.fillMaxWidth(),
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            Spacer(Modifier.height(8.dp))
            if (game.installable && !game.installed) {
                TinyPill("Linux version available!", colors.success)
            } else {
                Text(statusText(game), color = colors.textTertiary)
            }
        }
    }
}
private fun statusText(game: OnlineGame): String = when {
    !game.owned -> "Not owned"
    game.installed -> "Linux build installed"
    !game.detailsReady -> "Download availability not checked"
    game.installable -> "Linux version available!"
    // TODO: Proton later. Shown when the app info has no Linux depot.
    else -> "No Linux depot yet"
}
@Composable
private fun HeaderImage(appId: Int, grid: Boolean) {
    val colors = VexaTheme.colors
    val bitmap = com.critical.vexaemulator.ui.rememberSteamImage(listOf(
        "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/hero_capsule.jpg",
        "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/header.jpg",
    ))
    // Every grid cell uses the vertical capsule frame. Odd files sit inside it.
    val frame = if (grid) {
        Modifier.fillMaxWidth().aspectRatio(748f / 896f).clip(RoundedCornerShape(8.dp))
    } else {
        Modifier.fillMaxWidth().height(130.dp).clip(RoundedCornerShape(8.dp))
    }
    if (bitmap != null) {
        Image(
            bitmap = bitmap.asImageBitmap(),
            contentDescription = null,
            modifier = frame,
            contentScale = if (grid) ContentScale.Fit else ContentScale.Crop,
        )
    } else {
        Canvas(frame) {
            drawRect(colors.surface3)
            drawRect(
                color = colors.plasma.copy(alpha = 0.35f),
                topLeft = Offset(size.width * 0.2f, size.height * 0.35f),
                size = Size(size.width * 0.6f, size.height * 0.12f),
            )
        }
    }
}
