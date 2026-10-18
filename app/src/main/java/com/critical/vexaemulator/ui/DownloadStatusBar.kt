package com.critical.vexaemulator.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.steam.library.DepotProgress
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlin.math.roundToInt

@Composable
fun DownloadStatusBar(
    appId: Int,
    title: String,
    progress: DepotProgress,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val c = VexaTheme.colors
    val logo = rememberSteamImage(
        listOf("https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/header.jpg"),
    )
    val indeterminate = progress.fraction < 0f
    val speed = if (progress.bytesPerSecond == 0L && indeterminate) {
        "Starting"
    } else {
        "${(progress.bytesPerSecond.toDouble() / (1024.0 * 1024.0)).roundToInt()} MB/s"
    }
    val sizes = formatTransfer(progress.receivedBytes, progress.totalBytes)
    val context = LocalContext.current

    Row(
        modifier
            .fillMaxWidth()
            .height(76.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(c.surface2)
            .clickable { openDownloadingProgram(context, appId) }
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier
                .size(52.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(c.surface3),
        ) {
            if (logo != null) {
                Image(
                    logo.asImageBitmap(),
                    contentDescription = title,
                    modifier = Modifier.fillMaxWidth().fillMaxHeight(),
                    contentScale = ContentScale.Crop,
                )
            }
        }
        Column(
            Modifier
                .weight(1f)
                .padding(start = 12.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                title,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
                color = c.textPrimary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(2.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "$speed · $sizes",
                    style = MaterialTheme.typography.labelMedium,
                    color = c.textSecondary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f, fill = false),
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "Cancel",
                    style = MaterialTheme.typography.labelMedium,
                    color = c.plasma,
                    modifier = Modifier.clickable(onClick = onCancel),
                )
            }
            Spacer(Modifier.height(6.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                val bar = Modifier
                    .weight(1f)
                    .height(6.dp)
                    .clip(RoundedCornerShape(3.dp))
                if (indeterminate) {
                    LinearProgressIndicator(
                        modifier = bar,
                        color = c.plasma,
                        trackColor = c.surface3,
                    )
                } else {
                    val fraction = progress.fraction.coerceIn(0f, 1f)
                    LinearProgressIndicator(
                        progress = { fraction },
                        modifier = bar,
                        color = c.plasma,
                        trackColor = c.surface3,
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(
                        "${(fraction * 100f).roundToInt()}%",
                        style = MaterialTheme.typography.labelMedium,
                        color = c.textSecondary,
                    )
                }
            }
        }
    }
}

private fun openDownloadingProgram(context: android.content.Context, appId: Int) {
    val id = "steam-$appId"
    val intent = android.content.Intent(context, com.critical.vexaemulator.ProgramActivity::class.java)
    if (com.critical.vexaemulator.guest.GuestCatalog.find(id) != null) {
        intent.putExtra(com.critical.vexaemulator.ProgramActivity.EXTRA_PROGRAM_ID, id)
    } else {
        intent.putExtra(com.critical.vexaemulator.ProgramActivity.EXTRA_APP_ID, appId)
    }
    context.startActivity(intent)
}

private fun formatTransfer(receivedBytes: Long, totalBytes: Long): String {
    val gbCut = 1000L * 1024L * 1024L
    return if (totalBytes > gbCut) {
        val unit = 1024.0 * 1024.0 * 1024.0
        "${(receivedBytes / unit).roundToInt()} / ${(totalBytes / unit).roundToInt()} GB"
    } else {
        val unit = 1024.0 * 1024.0
        "${(receivedBytes / unit).roundToInt()} / ${(totalBytes / unit).roundToInt()} MB"
    }
}
