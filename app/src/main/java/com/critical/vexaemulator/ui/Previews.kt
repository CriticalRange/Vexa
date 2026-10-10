package com.critical.vexaemulator.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.ui.theme.*

@Preview(name = "Components", showBackground = true, backgroundColor = 0xFF07080B, widthDp = 380)
@Composable
private fun VexaComponentsPreview() {
    VexaEmulatorTheme {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            BrandLogo(size = LogoSize.Medium)
            SectionLabel("Section", trailing = { TinyPill("live", Success) })
            PanelSurface {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    KeyValueRow("key", "value")
                    MetadataRow(listOf("ELF" to Cyan, "arg" to null))
                }
            }
            VexaButton("Run VEXA Program", {})
            GhostButton("Ghost", {})
            DangerButton("Quit", {})
            SwitchRow("Root logging", "su: /system/bin/su", true, {})
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                LogFilterChip("WARN", Warn, true, {})
                LogFilterChip("INFO", Info, false, {})
            }
            StatRow(listOf(Triple("logs", "128", Info), Triple("err", "0", Success)))
            LogRow(
                VexaLogEntry(
                    1, 65_432, LogLevel.INFO, LogCategory.RUNTIME,
                    "vexa-hello: ok", mapOf("source" to "launcher")
                )
            )
        }
    }
}

@Preview(name = "Loading", showBackground = true, backgroundColor = 0xFF07080B, widthDp = 380, heightDp = 300)
@Composable
private fun LoadingPreview() {
    VexaEmulatorTheme {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                BrandLogo(size = LogoSize.Medium)
                BootSpinner()
                PulsingDots()
            }
        }
    }
}
