package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.steam.CloudChoice
import com.critical.vexaemulator.steam.CloudConflict
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.OverlineStyle
import com.critical.vexaemulator.ui.theme.VexaTheme
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

// Shown while the launch waits on the Steam Cloud step. The local save and the
// cloud save diverged; the user picks which side survives this launch.
@Composable
fun CloudConflictDialog(conflicts: List<CloudConflict>, onChoice: (CloudChoice, Boolean) -> Unit) {
    val c = VexaTheme.colors
    var rememberChoice by remember { mutableStateOf(false) }
    val newestRemote = conflicts.maxOf { it.remoteTime }
    val newestLocal = conflicts.maxOf { it.localMtime }
    Box(
        Modifier.fillMaxSize().background(c.bgBase.copy(alpha = 0.85f)),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier
                .width(380.dp)
                .clip(RoundedCornerShape(18.dp))
                .background(c.surface1)
                .border(1.dp, c.hairline, RoundedCornerShape(18.dp))
                .padding(22.dp),
        ) {
            Text("STEAM CLOUD", style = OverlineStyle.copy(color = c.textTertiary))
            Spacer(Modifier.height(6.dp))
            Text(
                "Save conflict",
                style = MaterialTheme.typography.headlineSmall.copy(color = c.textPrimary, fontWeight = FontWeight.SemiBold),
            )
            Spacer(Modifier.height(10.dp))
            Text(
                "${conflicts.size} file(s) differ. Choose the copy to keep:",
                style = MonospaceTypography.labelMedium.copy(color = c.textSecondary),
            )
            Spacer(Modifier.height(14.dp))
            ConflictOption(
                label = "Steam Cloud",
                time = newestRemote,
                accent = c.plasma,
            )
            Spacer(Modifier.height(8.dp))
            ConflictOption(
                label = "This device",
                time = newestLocal,
                accent = c.success,
            )
            Spacer(Modifier.height(16.dp))
            Text(
                "The losing copy is kept under steam/cloud/backup.",
                style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth(),
            )
            Row(
                Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(10.dp))
                    .clickable { rememberChoice = !rememberChoice }
                    .padding(vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box(
                    Modifier
                        .size(16.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (rememberChoice) c.plasma else c.surface2)
                        .border(1.dp, c.borderStrong, RoundedCornerShape(4.dp)),
                    contentAlignment = Alignment.Center,
                ) {
                    if (rememberChoice) {
                        Text("✓", color = c.onAccent, style = MonospaceTypography.labelSmall)
                    }
                }
                Spacer(Modifier.width(10.dp))
                Text(
                    "Remember this choice for next time",
                    style = MonospaceTypography.labelMedium.copy(color = c.textSecondary),
                )
            }
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth()) {
                GhostButton(
                    text = "Keep device save",
                    onClick = { onChoice(CloudChoice.Local, rememberChoice) },
                    modifier = Modifier.weight(1f),
                )
                Spacer(Modifier.width(10.dp))
                VexaButton(
                    text = "Use cloud save",
                    onClick = { onChoice(CloudChoice.Remote, rememberChoice) },
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

@Composable
private fun ConflictOption(label: String, time: Long, accent: androidx.compose.ui.graphics.Color) {
    val c = VexaTheme.colors
    val date = if (time > 0) {
        SimpleDateFormat("d MMM yyyy · HH:mm", Locale.getDefault()).format(Date(time))
    } else {
        "unknown"
    }
    Row(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(c.surface2)
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier
                .width(10.dp)
                .height(10.dp)
                .clip(androidx.compose.foundation.shape.CircleShape)
                .background(accent),
        )
        Spacer(Modifier.width(10.dp))
        Column {
            Text(label, style = MaterialTheme.typography.titleSmall.copy(color = c.textPrimary))
            Text(date, style = MonospaceTypography.labelSmall.copy(color = c.textTertiary))
        }
    }
}
