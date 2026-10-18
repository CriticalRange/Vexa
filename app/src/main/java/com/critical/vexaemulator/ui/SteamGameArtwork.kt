package com.critical.vexaemulator.ui

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.steam.SteamArtwork
import com.critical.vexaemulator.steam.SteamGameDetails
import com.critical.vexaemulator.ui.theme.VexaTheme

@Composable
fun rememberSteamDetails(program: GuestProgram): SteamGameDetails? {
    val context = LocalContext.current.applicationContext
    var details by remember(program) { mutableStateOf<SteamGameDetails?>(null) }
    LaunchedEffect(program) {
        details = SteamArtwork.cached(context, program)
        details = SteamArtwork.details(context, program)
    }
    return details
}

@Composable
fun rememberSteamImage(urls: List<String>): Bitmap? {
    val context = LocalContext.current.applicationContext
    var bitmap by remember(urls) { mutableStateOf<Bitmap?>(null) }
    LaunchedEffect(urls) { bitmap = SteamArtwork.image(context, urls) }
    return bitmap
}

@Composable
fun SteamGameHero(program: GuestProgram, details: SteamGameDetails?) {
    val c = VexaTheme.colors
    val hero = rememberSteamImage(details?.let { listOf(it.hero, it.background, it.header) }.orEmpty())
    val title = details?.name ?: program.title
    Box(Modifier.fillMaxWidth().heightIn(min = 260.dp).clip(RoundedCornerShape(18.dp)).background(c.surface2)) {
        if (hero != null) Image(hero.asImageBitmap(), null, Modifier.matchParentSize(), contentScale = ContentScale.Crop)
        Box(Modifier.matchParentSize().background(Brush.verticalGradient(listOf(Color.Black.copy(alpha = .12f), Color.Black.copy(alpha = .9f)))))
        Column(Modifier.align(Alignment.BottomStart).padding(start = 22.dp, end = 22.dp, top = 22.dp, bottom = 12.dp)) {
            Text(title, style = MaterialTheme.typography.headlineLarge, color = Color.White)
            details?.developer?.takeIf { it.isNotBlank() }?.let {
                Spacer(Modifier.height(4.dp))
                Text(it, style = MaterialTheme.typography.labelLarge, color = Color.White.copy(alpha = .9f))
            }
            details?.genres?.joinToString(" · ")?.takeIf { it.isNotBlank() }?.let {
                Spacer(Modifier.height(8.dp))
                Text(it, style = MaterialTheme.typography.bodyMedium, color = Color.White.copy(alpha = .8f))
            }
        }
    }
    Spacer(Modifier.height(16.dp))
    Text(details?.description?.takeIf { it.isNotBlank() } ?: program.summary,
        style = MaterialTheme.typography.bodyLarge, color = c.textSecondary)
    if (details != null) {
        val uriHandler = androidx.compose.ui.platform.LocalUriHandler.current
        TextButton(onClick = { uriHandler.openUri("https://store.steampowered.com/app/${details.appId}/") }) {
            Text("View on Steam", color = c.plasma)
        }
        LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            items(details.screenshots) { url ->
                val shot = rememberSteamImage(listOf(url))
                Box(Modifier.width(260.dp).aspectRatio(16f / 9f).clip(RoundedCornerShape(12.dp)).background(c.surface2)) {
                    if (shot != null) Image(shot.asImageBitmap(), "$title screenshot", Modifier.fillMaxSize(), contentScale = ContentScale.Crop)
                }
            }
        }
        Spacer(Modifier.height(20.dp))
    }
}
