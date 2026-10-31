package com.critical.vexaemulator.ui

import com.critical.vexaemulator.guest.GuestProgram
import org.junit.Assert.assertEquals
import org.junit.Test

class ProgramTaglineTest {
    @Test fun `each preinstalled game keeps its own engine`() {
        assertEquals("godot · x86_64", game("brotato").tagline())
        assertEquals("hashlink · x86_64", game("deadcells").tagline())
        assertEquals("fna · x86_64", game("terraria").tagline())
        assertEquals("unity · x86_64", game("megabonk").tagline())
        assertEquals("prism3d · x86_64", game("ets2").tagline())
        assertEquals("gamemaker · x86_64", game("undertale").tagline())
        assertEquals("qt · x86_64", game("prismlauncher").tagline())
        assertEquals("game · x86_64", game("steam-1").tagline())
    }

    @Test fun `graphics tools do not use the game line`() {
        val vulkan = GuestProgram(
            id = "vkcube",
            title = "vkcube",
            summary = "",
            assetPath = "",
            fileName = "vkcube",
        )
        val gl = GuestProgram(
            id = "glxgears",
            title = "glxgears",
            summary = "",
            assetPath = "",
            fileName = "glxgears",
            env = listOf("DISPLAY=:0"),
        )
        assertEquals("vulkan · xcb", vulkan.tagline())
        assertEquals("mesa · x11", gl.tagline())
    }

    private fun game(id: String) = GuestProgram(
        id = id,
        title = id,
        summary = "",
        assetPath = "",
        fileName = id,
        preinstalled = true,
    )
}
