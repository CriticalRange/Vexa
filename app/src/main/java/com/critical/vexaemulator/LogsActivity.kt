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
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.ui.LogList
import com.critical.vexaemulator.ui.PanelSurface
import com.critical.vexaemulator.ui.StatusDot
import com.critical.vexaemulator.ui.VexaGlyph
import com.critical.vexaemulator.ui.VexaIconButton
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.copyToClipboard
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import com.critical.vexaemulator.ui.toPlainLine

class LogsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaPrefs.init(this)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark()) {
                LogsScreen(onBack = { finish() })
            }
        }
    }
}

@Composable
private fun LogsScreen(onBack: () -> Unit) {
    val c = VexaTheme.colors
    val entries by LogStore.entries.collectAsState()
    val context = LocalContext.current
    Column(
        Modifier
            .fillMaxSize()
            .background(c.bgBase)
            .statusBarsPadding()
            .navigationBarsPadding()
            .padding(horizontal = 20.dp),
    ) {
        Spacer(Modifier.height(14.dp))
        SubscreenHeader(title = "Logs", onBack = onBack)
        Spacer(Modifier.height(16.dp))
        PanelSurface(modifier = Modifier.fillMaxWidth(), color = c.well) {
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
                Box(Modifier.fillMaxWidth().padding(start = 12.dp, top = 8.dp, bottom = 8.dp, end = 4.dp)) {
                    LogList(
                        entries = entries,
                        compact = true,
                        modifier = Modifier.fillMaxWidth().heightIn(max = 640.dp),
                    )
                }
            }
        }
    }
}
