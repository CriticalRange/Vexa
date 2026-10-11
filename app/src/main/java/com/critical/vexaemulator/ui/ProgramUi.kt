package com.critical.vexaemulator.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.graphics.Color
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.ui.theme.VexaTheme

enum class ProgramCategory(val label: String) {
    All("All"),
    Basics("Basics"),
    BusyBox("BusyBox"),
    GL("Graphics GL"),
    Vulkan("Vulkan"),
    Games("Games"),
}

fun GuestProgram.category(): ProgramCategory = when {
    preinstalled -> ProgramCategory.Games
    fileName.contains("busybox") -> ProgramCategory.BusyBox
    id.startsWith("vk") || id.startsWith("vulkan") -> ProgramCategory.Vulkan
    env.any { it.startsWith("DISPLAY=") } -> ProgramCategory.GL
    else -> ProgramCategory.Basics
}

fun ProgramCategory.matches(program: GuestProgram): Boolean =
    this == ProgramCategory.All || program.category() == this

fun GuestProgram.glyph(): VexaGlyph = when (category()) {
    ProgramCategory.Games -> VexaGlyph.Gamepad
    ProgramCategory.GL, ProgramCategory.Vulkan -> VexaGlyph.Cube
    else -> VexaGlyph.Terminal
}

fun GuestProgram.tagline(): String = when (category()) {
    ProgramCategory.Games -> "godot · x86_64"
    ProgramCategory.Vulkan -> "vulkan · xcb"
    ProgramCategory.GL -> "mesa · x11"
    ProgramCategory.BusyBox -> "multi-call util"
    else -> "x86_64 elf"
}

/** Short type label and its accent for badges. */
@Composable
@ReadOnlyComposable
fun GuestProgram.typeBadge(): Pair<String, Color> {
    val c = VexaTheme.colors
    return when (category()) {
        ProgramCategory.Games -> "Game" to c.pink
        ProgramCategory.Vulkan -> "Vulkan" to c.ion
        ProgramCategory.GL -> "GL" to c.ultraviolet
        ProgramCategory.BusyBox -> "BusyBox" to c.warning
        else -> if (fileName.endsWith(".so")) "Shared lib" to c.ultraviolet else "ELF" to c.plasma
    }
}
