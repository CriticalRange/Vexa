package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.Ink
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.MonospaceTypography

@Composable
fun MetadataRow(items: List<Pair<String, Color?>>, modifier: Modifier = Modifier) {
    if (items.isEmpty()) return
    Row(
        modifier = modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        items.forEach { (text, color) ->
            MetadataPill(text, color)
        }
    }
}

@Composable
private fun MetadataPill(text: String, color: Color?) {
    Text(
        text = text,
        modifier = Modifier
            .clip(RoundedCornerShape(100.dp))
            .background((color ?: InkDim).copy(alpha = 0.12f))
            .padding(horizontal = 9.dp, vertical = 4.dp),
        color = color ?: InkMuted,
        style = MonospaceTypography.labelSmall,
    )
}

@Composable
fun KeyValueRow(
    key: String,
    value: String,
    modifier: Modifier = Modifier,
    valueColor: Color = Ink,
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
            text = key,
            style = MonospaceTypography.labelSmall.copy(color = InkDim),
        )
        Text(
            text = value,
            style = MonospaceTypography.bodySmall.copy(color = valueColor),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
