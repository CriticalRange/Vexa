package com.critical.vexaemulator.ui

import android.os.Build
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.OverlineStyle
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme

data class BootStep(val title: String, val detail: String)

fun bootSteps(program: GuestProgram?, surfaceSize: String?, cloudDetail: String? = null): List<BootStep> = listOfNotNull(
    BootStep("Preparing rootfs", "guest files staged in app storage"),
    cloudDetail?.let { BootStep("Syncing Steam Cloud", it) },
    BootStep("Connecting runtime service", ":runtime process bound over Messenger"),
    BootStep("Waiting for landscape surface", "SurfaceHolder → sensorLandscape"),
    BootStep("Publishing guest window size", surfaceSize?.let { "/tmp/vexa-window-size ← $it" } ?: "/tmp/vexa-window-size"),
    BootStep("Starting runtime", "exec ./${program?.fileName ?: "guest"}"),
)

/**
 * Boot screen (landscape): live boot checklist on the left, hero brand + segment ring on
 * the right. [step] is the number of finished steps; the last log line streams underneath.
 */
@Composable
fun GameLoadingOverlay(
    program: GuestProgram?,
    step: Int,
    surfaceSize: String?,
    lastMessage: String?,
    cloudDetail: String? = null,
    onDebug: () -> Unit,
    onClose: () -> Unit,
) {
    val c = VexaTheme.colors
    val steps = bootSteps(program, surfaceSize, cloudDetail)
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(c.bgBase),
    ) {
        ScanlineOverlay(modifier = Modifier.fillMaxSize())
        AmbientOrb(Modifier.fillMaxSize(), center = null, radiusFraction = 0.7f)

        Row(
            Modifier.fillMaxSize().padding(start = 40.dp, end = 40.dp, top = 60.dp, bottom = 44.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            BootChecklist(steps, step, Modifier.weight(1f).staggerIn(1))
            Spacer(Modifier.width(24.dp))
            Column(
                Modifier.weight(1.15f),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                BrandLogo(size = LogoSize.Hero, modifier = Modifier.staggerIn(0).lightSweep(Unit, c.bgBase))
                Text(
                    "LINUX X86_64 RUNTIME",
                    style = OverlineStyle.copy(color = c.textSecondary, letterSpacing = 0.3.em),
                )
                Spacer(Modifier.height(22.dp))
                SegmentRing(sizeDp = 56.dp)
                Spacer(Modifier.height(16.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "Starting the program",
                        style = MaterialTheme.typography.titleLarge.copy(color = c.textPrimary),
                    )
                    Spacer(Modifier.width(8.dp))
                    PulsingDots()
                }
                Spacer(Modifier.height(6.dp))
                AnimatedContent(
                    targetState = lastMessage.orEmpty(),
                    transitionSpec = { fadeIn(tween(VexaMotion.SMALL)) togetherWith fadeOut(tween(VexaMotion.MICRO)) },
                    label = "bootPipe",
                ) { msg ->
                    Text(
                        if (msg.isEmpty()) "" else "› $msg",
                        style = MonospaceTypography.labelMedium.copy(color = c.plasma),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        }

        // Top bar: program chip · device chip · actions.
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 24.dp, vertical = 16.dp).staggerIn(2),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            program?.let { p ->
                val (typeLabel, typeColor) = p.typeBadge()
                Row(
                    Modifier
                        .clip(PillShape)
                        .background(c.glass)
                        .border(1.dp, c.hairline, PillShape)
                        .padding(start = 6.dp, end = 12.dp, top = 5.dp, bottom = 5.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    IconTile(p.glyph(), typeColor, size = 26.dp)
                    Spacer(Modifier.width(8.dp))
                    Text(p.title, style = MaterialTheme.typography.titleMedium.copy(color = c.textPrimary, fontWeight = FontWeight.SemiBold))
                    Spacer(Modifier.width(8.dp))
                    TinyPill("$typeLabel · x86_64", typeColor)
                }
            }
            Spacer(Modifier.weight(1f))
            TinyPill(
                "${Build.MODEL}" + (surfaceSize?.let { " · $it" } ?: ""),
                c.textSecondary,
                dot = true,
            )
            Spacer(Modifier.weight(1f))
            VexaIconButton(VexaGlyph.Bug, onDebug, tint = c.ultraviolet)
            Spacer(Modifier.width(8.dp))
            VexaIconButton(VexaGlyph.Close, onClose)
        }

        // Bottom bar.
        Row(
            Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(horizontal = 24.dp, vertical = 16.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "FEX · arm64-v8a · Android ${Build.VERSION.RELEASE}",
                style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
                modifier = Modifier.weight(1f),
            )
            TinyPill("initializing runtime", c.plasma, dot = true, pulse = true)
            Text(
                "STEP ${(step + 1).coerceAtMost(steps.size)} / ${steps.size}",
                style = MonospaceTypography.labelSmall.copy(color = c.textTertiary),
                textAlign = TextAlign.End,
                modifier = Modifier.weight(1f),
            )
        }
    }
}

@Composable
private fun BootChecklist(steps: List<BootStep>, done: Int, modifier: Modifier = Modifier) {
    val c = VexaTheme.colors
    Column(modifier) {
        Text("BOOT SEQUENCE", style = OverlineStyle.copy(color = c.textTertiary))
        Spacer(Modifier.height(4.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Runtime initializing", style = MaterialTheme.typography.headlineSmall.copy(color = c.textPrimary))
            Spacer(Modifier.width(10.dp))
            Badge("step ${(done + 1).coerceAtMost(steps.size)}/${steps.size}", c.plasma)
        }
        Spacer(Modifier.height(16.dp))
        steps.forEachIndexed { i, s ->
            val state = when {
                i < done -> 2
                i == done -> 1
                else -> 0
            }
            StepRow(s, state, isLast = i == steps.lastIndex, index = i + 1)
        }
    }
}

@Composable
private fun StepRow(step: BootStep, state: Int, isLast: Boolean, index: Int) {
    val c = VexaTheme.colors
    val lineFill by animateFloatAsState(if (state == 2) 1f else 0f, tween(VexaMotion.LARGE, easing = VexaMotion.Standard), label = "stepLine")
    val titleColor by animateColorAsState(
        when (state) {
            0 -> c.textTertiary
            else -> c.textPrimary
        },
        label = "stepTitle",
    )
    val detailColor by animateColorAsState(
        when (state) {
            2 -> c.success
            1 -> c.plasma
            else -> c.textTertiary.copy(alpha = 0.7f)
        },
        label = "stepDetail",
    )
    Row {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Box(Modifier.size(22.dp), contentAlignment = Alignment.Center) {
                AnimatedContent(
                    targetState = state,
                    transitionSpec = {
                        (fadeIn() + scaleIn(spring(dampingRatio = 0.5f, stiffness = 500f), initialScale = 0.3f)) togetherWith fadeOut()
                    },
                    label = "stepIcon",
                ) { st ->
                    when (st) {
                        2 -> Box(
                            Modifier.size(20.dp).clip(CircleShape).background(c.soft(c.success)).border(1.dp, c.success, CircleShape),
                            contentAlignment = Alignment.Center,
                        ) { VexaIcon(VexaGlyph.Check, c.success, size = 12.dp, strokeWidth = 3f) }
                        1 -> Box(contentAlignment = Alignment.Center) {
                            BootSpinner(color = c.plasma, sizeDp = 20.dp)
                            Box(Modifier.size(6.dp).clip(CircleShape).background(c.plasma))
                        }
                        else -> Box(Modifier.size(14.dp).clip(CircleShape).border(1.dp, c.borderStrong, CircleShape))
                    }
                }
            }
            if (!isLast) {
                Canvas(Modifier.width(2.dp).height(18.dp)) {
                    drawLine(c.hairline, Offset(size.width / 2, 0f), Offset(size.width / 2, size.height), size.width)
                    drawLine(c.success, Offset(size.width / 2, 0f), Offset(size.width / 2, size.height * lineFill), size.width)
                }
            }
        }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.padding(top = 1.dp)) {
            Text(
                "$index. ${step.title}",
                style = MonospaceTypography.labelLarge.copy(color = titleColor, fontWeight = FontWeight.SemiBold),
            )
            Text(
                step.detail,
                style = MonospaceTypography.labelSmall.copy(color = detailColor),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}
