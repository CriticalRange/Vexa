package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaAccent
import com.critical.vexaemulator.ui.theme.VexaBlack

@Composable
fun GameLoadingOverlay(
    message: String = "Starting the program",
    isDebugMenuOpen: Boolean,
    onToggleDebugMenu: () -> Unit,
    onHideFilters: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(VexaBlack)
    ) {
        ScanlineOverlay(modifier = Modifier.fillMaxSize())

        Column(
            modifier = Modifier.align(Alignment.Center),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            BrandLogo(size = LogoSize.Medium)
            Spacer(Modifier.height(28.dp))
            BootSpinner(color = VexaAccent, sizeDp = 52.dp)
            Spacer(Modifier.height(20.dp))
            Text(
                text = message,
                style = MonospaceTypography.bodyMedium.copy(color = InkMuted),
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(horizontal = 48.dp),
            )
            Spacer(Modifier.height(10.dp))
            PulsingDots(color = VexaAccent)
        }

        TinyPill(
            text = "initializing runtime",
            color = InkDim,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(bottom = 24.dp),
        )
    }
}
