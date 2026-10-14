package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogEntry
import com.critical.vexaemulator.ui.theme.ThemeMode
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaTheme

@Composable
private fun ComponentGallery() {
    val c = VexaTheme.colors
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(c.bgBase)
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        BrandLogo(size = LogoSize.Medium)
        SectionLabel("Section", trailing = { TinyPill("live", c.success, dot = true) })
        ThemeSegmented(ThemeMode.Dark, { _, _ -> })
        VexaButton("Start GL Probe", {}, Modifier.fillMaxWidth())
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SecondaryButton("Configure", {})
            GhostButton("Export", {}, icon = VexaGlyph.Copy)
        }
        DangerButton("Stop & exit", {}, Modifier.fillMaxWidth())
        SectionCard {
            SwitchRow("Root logging", "su: /system/bin/su", true, {}, icon = VexaGlyph.Shield)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LogFilterChip("WARN", c.warning, true, {})
            LogFilterChip("INFO", c.info, false, {})
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            StatTile("logs", 1284, c.info)
            StatTile("err", 0, c.success)
        }
        LogRow(
            VexaLogEntry(1, 65_432, LogLevel.FATAL, LogCategory.ROOT, "SIGSEGV in guest", mapOf("pid" to "22927")),
        )
    }
}

@Preview(name = "Void", widthDp = 380)
@Composable
private fun VoidPreview() {
    VexaEmulatorTheme(darkTheme = true) { ComponentGallery() }
}

@Preview(name = "Paper", widthDp = 380)
@Composable
private fun PaperPreview() {
    VexaEmulatorTheme(darkTheme = false) { ComponentGallery() }
}
