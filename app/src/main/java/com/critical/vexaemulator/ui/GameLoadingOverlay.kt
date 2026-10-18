package com.critical.vexaemulator.ui

import android.graphics.BitmapFactory
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.ui.theme.MonospaceTypography
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaMotion
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

data class BootStep(val title: String, val detail: String)

private const val ICON_BYTE_CAP = 4L * 1024L * 1024L

fun bootSteps(program: GuestProgram?, surfaceSize: String?, cloudDetail: String? = null): List<BootStep> = listOfNotNull(
    BootStep("Preparing rootfs", "guest files staged in app storage"),
    cloudDetail?.let { BootStep("Syncing Steam Cloud", it) },
    BootStep("Connecting runtime service", ":runtime process bound over Messenger"),
    BootStep("Waiting for landscape surface", "SurfaceHolder → sensorLandscape"),
    BootStep("Publishing guest window size", surfaceSize?.let { "/tmp/vexa-window-size ← $it" } ?: "/tmp/vexa-window-size"),
    BootStep("Starting runtime", "exec ./${program?.fileName ?: "guest"}"),
)

/**
 * Boot screen (landscape): scrollable boot checklist on the left, game icon or
 * brand mark on the right. [step] is the number of finished steps.
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
            Modifier.fillMaxSize().padding(start = 40.dp, end = 40.dp, top = 72.dp, bottom = 28.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            BootChecklist(steps, step, Modifier.weight(1f).fillMaxHeight().staggerIn(1))
            Spacer(Modifier.width(24.dp))
            Column(
                Modifier.weight(1.15f),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                BootMark(program, Modifier.staggerIn(0))
                Spacer(Modifier.height(22.dp))
                SegmentRing(sizeDp = 56.dp)
                Spacer(Modifier.height(16.dp))
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

        Row(
            Modifier.fillMaxWidth().padding(horizontal = 24.dp, vertical = 16.dp).staggerIn(2),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            program?.let { p ->
                val (_, typeColor) = p.typeBadge()
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
                    Text(
                        p.title,
                        style = MaterialTheme.typography.titleMedium.copy(color = c.textPrimary, fontWeight = FontWeight.SemiBold),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
            Spacer(Modifier.weight(1f))
            VexaIconButton(VexaGlyph.Bug, onDebug, tint = c.ultraviolet)
            Spacer(Modifier.width(8.dp))
            VexaIconButton(VexaGlyph.Close, onClose)
        }
    }
}

@Composable
private fun BootMark(program: GuestProgram?, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    var logo by remember(program?.id) { mutableStateOf<ImageBitmap?>(null) }
    LaunchedEffect(program?.id, program?.fileName) {
        logo = withContext(Dispatchers.IO) { decodeGuestIcon(context.filesDir, program) }
    }
    val bitmap = logo
    if (bitmap != null) {
        Image(
            bitmap = bitmap,
            contentDescription = program?.title,
            contentScale = ContentScale.Fit,
            modifier = modifier.size(120.dp).clip(RoundedCornerShape(20.dp)),
        )
    } else {
        BrandLogo(size = LogoSize.Hero, modifier = modifier.lightSweep(Unit, VexaTheme.colors.bgBase))
    }
}

/** Sibling icon.png / icon.jpg next to the guest executable. PNG or JPEG magic only. */
private fun decodeGuestIcon(filesDir: File, program: GuestProgram?): ImageBitmap? {
    if (program == null) return null
    val guestDir = File(filesDir, "guests/${program.id}")
    val executable = File(guestDir, program.fileName)
    val dirs = listOfNotNull(executable.parentFile, guestDir).distinctBy { it.path }
    val names = listOf("icon.png", "icon.jpg", "icon.jpeg")
    for (dir in dirs) {
        for (name in names) {
            val bitmap = decodeIconFile(File(dir, name)) ?: continue
            return bitmap.asImageBitmap()
        }
    }
    return null
}

private fun decodeIconFile(file: File): android.graphics.Bitmap? {
    if (!file.isFile) return null
    val length = file.length()
    if (length < 8L || length > ICON_BYTE_CAP) return null
    val header = ByteArray(8)
    val read = runCatching { file.inputStream().use { it.read(header) } }.getOrDefault(-1)
    if (read < 3) return null
    val png = read >= 8 &&
        header[0] == 0x89.toByte() && header[1] == 0x50.toByte() &&
        header[2] == 0x4E.toByte() && header[3] == 0x47.toByte()
    val jpeg = header[0] == 0xFF.toByte() && header[1] == 0xD8.toByte() && header[2] == 0xFF.toByte()
    if (!png && !jpeg) return null
    return BitmapFactory.decodeFile(file.absolutePath)
}

@Composable
private fun BootChecklist(steps: List<BootStep>, done: Int, modifier: Modifier = Modifier) {
    Column(modifier.verticalScroll(rememberScrollState())) {
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
                        1 -> ActiveStepMark()
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

@Composable
private fun ActiveStepMark() {
    val c = VexaTheme.colors
    val spin = rememberInfiniteTransition(label = "activeStep")
    val rotation by spin.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(tween(900, easing = LinearEasing), RepeatMode.Restart),
        label = "activeStepSpin",
    )
    Box(Modifier.size(20.dp), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize().graphicsLayer { rotationZ = rotation }) {
            val stroke = 2.dp.toPx()
            val inset = stroke / 2f
            drawArc(
                color = c.plasma,
                startAngle = 0f,
                sweepAngle = 270f,
                useCenter = false,
                topLeft = Offset(inset, inset),
                size = Size(size.width - stroke, size.height - stroke),
                style = Stroke(width = stroke, cap = StrokeCap.Round),
            )
        }
        Box(Modifier.size(6.dp).clip(CircleShape).background(c.plasma))
    }
}
