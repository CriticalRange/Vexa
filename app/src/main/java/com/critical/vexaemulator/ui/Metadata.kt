package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.VexaTheme

data class MetaItem(val text: String, val color: Color? = null, val icon: VexaGlyph? = null)

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun MetadataRow(items: List<MetaItem>, modifier: Modifier = Modifier) {
    if (items.isEmpty()) return
    FlowRow(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        items.forEach { MetaChip(it) }
    }
}

@Composable
fun MetaChip(item: MetaItem, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    val tint = item.color ?: c.textSecondary
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(if (item.color != null) c.soft(tint) else c.surface2)
            .padding(horizontal = 8.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        item.icon?.let {
            VexaIcon(it, tint, size = 12.dp, strokeWidth = 2.2f)
            Spacer(Modifier.width(5.dp))
        }
        Text(
            text = item.text,
            color = tint,
            style = MonospaceTypography.labelSmall,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}

@Composable
fun KeyValueRow(
    key: String,
    value: String,
    modifier: Modifier = Modifier,
    valueColor: Color = VexaTheme.colors.textPrimary,
) {
    val c = VexaTheme.colors
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(text = key, style = MonospaceTypography.labelSmall.copy(color = c.textTertiary))
        Text(
            text = value,
            style = MonospaceTypography.bodySmall.copy(color = valueColor),
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
