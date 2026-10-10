package com.critical.vexaemulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.ui.theme.Edge
import com.critical.vexaemulator.ui.theme.Ink
import com.critical.vexaemulator.ui.theme.InkDim
import com.critical.vexaemulator.ui.theme.InkMuted
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.SmallShape
import com.critical.vexaemulator.ui.theme.SurfaceCard

data class PickerItem(val value: String, val label: String)

@Composable
fun PickerCard(
    label: String,
    selected: String,
    subtitle: String?,
    expanded: Boolean,
    onExpandedChange: (Boolean) -> Unit,
    items: List<PickerItem>,
    onSelect: (PickerItem) -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier.fillMaxWidth()) {
        PanelSurface(
            modifier = Modifier
                .fillMaxWidth()
                .clip(SmallShape)
                .clickable { onExpandedChange(true) },
            shape = SmallShape,
            color = SurfaceCard,
            borderColor = Edge,
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = label,
                        style = MaterialTheme.typography.labelMedium.copy(color = InkDim),
                    )
                    Spacer(modifier = Modifier.height(3.dp))
                    Text(
                        text = selected,
                        style = MaterialTheme.typography.titleMedium.copy(color = Ink),
                    )
                    subtitle?.let {
                        Text(
                            text = it,
                            style = MaterialTheme.typography.bodySmall.copy(color = InkMuted),
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
                Text(
                    text = if (expanded) "▴" else "▾",
                    color = InkMuted,
                    style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                )
            }
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { onExpandedChange(false) },
            modifier = Modifier
                .background(SurfaceCard)
                .border(1.dp, Edge, RoundedCornerShape(12.dp)),
        ) {
            items.forEach { item ->
                DropdownMenuItem(
                    text = {
                        Text(
                            text = item.label,
                            color = Ink,
                            style = MonospaceTypography.bodySmall,
                        )
                    },
                    onClick = {
                        onSelect(item)
                        onExpandedChange(false)
                    },
                )
            }
        }
    }
}
