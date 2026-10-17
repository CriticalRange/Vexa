package com.critical.vexaemulator.guest

data class GuestProgram(
    val id: String,
    val title: String,
    val summary: String,
    val assetPath: String,
    val fileName: String,
    val args: List<String> = emptyList(),
    val env: List<String> = emptyList(),
    val preinstalled: Boolean = false,
)

object GuestCatalog {
    val programs: List<GuestProgram> = listOf(
        GuestProgram(
            id = "hello",
            title = "VEXA Hello",
            summary = "Static x86_64 Linux program. Writes one stdout line and one stderr line, then exits 0.",
            assetPath = "guests/vexa-hello",
            fileName = "vexa-hello",
        ),
        GuestProgram(
            id = "args",
            title = "VEXA Args",
            summary = "Prints the first argument it was given. Checks that argv reaches the guest.",
            assetPath = "guests/vexa-args",
            fileName = "vexa-args",
            args = listOf("from-dropdown"),
        ),
        GuestProgram(
            id = "pid",
            title = "VEXA Pid",
            summary = "Calls the Linux getpid syscall and prints the value the guest sees.",
            assetPath = "guests/vexa-pid",
            fileName = "vexa-pid",
        ),
        GuestProgram(
            id = "busybox-echo",
            title = "BusyBox echo",
            summary = "BusyBox 1.35.0, static musl, x86_64. Runs the echo applet.",
            assetPath = "guests/busybox",
            fileName = "busybox",
            args = listOf("echo", "busybox-echo: ok"),
        ),
        GuestProgram(
            id = "busybox-uname",
            title = "BusyBox uname",
            summary = "Same BusyBox binary. Prints the identity string the guest receives from uname.",
            assetPath = "guests/busybox",
            fileName = "busybox",
            args = listOf("uname", "-a"),
        ),
        graphics("es2gears", "es2gears", "Mesa GLES2 gears on X11. The picture is the Android surface.", "es2gears_x11.x86_64-linux-gnu", "es2gears_x11"),
        graphics("glxgears", "glxgears", "Mesa desktop GL gears. Fixed-function calls are emulated on GLES.", "glxgears.x86_64-linux-gnu", "glxgears"),
        graphics("eglgears", "eglgears", "Mesa EGL gears that also call desktop GL.", "eglgears_x11.x86_64-linux-gnu", "eglgears_x11"),
        graphics("glxinfo", "glxinfo", "Prints the GLX and GL strings, then exits.", "glxinfo.x86_64-linux-gnu", "glxinfo"),
        graphics(
            "glmark2-es2",
            "glmark2-es2",
            "glmark2 GLES2 build scene for five seconds.",
            "glmark2-es2",
            "glmark2-es2",
            listOf("-b", "build:duration=5", "--data-path", "/usr/share/glmark2"),
        ),
        graphics(
            "glmark2",
            "glmark2",
            "glmark2 desktop GL build scene for five seconds.",
            "glmark2",
            "glmark2",
            listOf("-b", "build:duration=5", "--data-path", "/usr/share/glmark2"),
        ),
        graphics("vkcube", "vkcube", "Vulkan cube on the Android surface.", "vkcube", "vkcube", listOf("--wsi", "xcb")),
        graphics("vulkaninfo", "vulkaninfo", "Prints Vulkan devices, then exits.", "vulkaninfo", "vulkaninfo"),
        graphics("vkcubepp", "vkcubepp", "Vulkan cube, C++ build.", "vkcubepp", "vkcubepp", listOf("--wsi", "xcb")),
        graphics("vkgears", "vkgears", "Vulkan gears. Needs Vulkan and its window-system libraries.", "vkgears.x86_64-linux-gnu", "vkgears"),
        GuestProgram(
            id = "brotato",
            title = "Brotato",
            summary = "Godot game. OpenGL on the Android surface. The Linux build is already in the app files directory.",
            assetPath = "",
            fileName = "Brotato.x86_64",
            env = listOf("DISPLAY=:0", "SteamAppId=1942280"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "deadcells",
            title = "Dead Cells",
            summary = "HashLink game. Native Linux build, OpenGL through its own SDL2. The Linux files are already in the app files directory.",
            assetPath = "",
            fileName = "deadcells",
            env = listOf(
                "DISPLAY=:0",
                "LD_LIBRARY_PATH=.",
                "SDL_JOYSTICK_DISABLE_UDEV=1",
                "SDL_HIDAPI_JOYSTICK_DISABLE_UDEV=1",
                "SDL_JOYSTICK_HIDAPI=0",
                "SteamAppId=588650",
            ),
            preinstalled = true,
        ),
        GuestProgram(
            id = "terraria",
            title = "Terraria",
            summary = "Native Linux FNA game. Uses the Linux Steam build with bundled SDL3, FNA3D, and FAudio libraries.",
            assetPath = "",
            fileName = "Terraria.bin.x86_64",
            env = listOf(
                "DISPLAY=:0",
                "LD_LIBRARY_PATH=lib64",
                "MONO_IOMAP=all",
                "FNA3D_MOJOSHADER_PROFILE=glsles3",
                "VEXA_AUDIO_TRACE=1",
                "VEXA_FAUDIO_TRACE=1",
                "FAUDIO_LOG_API_CALLS=1",
                "SteamAppId=105600",
            ),
            preinstalled = true,
        ),
        GuestProgram(
            id = "celeste",
            title = "Celeste",
            summary = "Native Linux FNA game (MonoKickstart). Uses bundled SDL2, FNA3D, and FMOD libraries.",
            assetPath = "",
            fileName = "Celeste.bin.x86_64",
            env = listOf(
                "DISPLAY=:0",
                "LD_LIBRARY_PATH=lib64",
                "MONO_IOMAP=all",
                "FNA3D_MOJOSHADER_PROFILE=glsles",
                "FNA3D_FORCE_DRIVER=OpenGL",
                "FNA3D_OPENGL_FORCE_ES3=1",
                "VEXA_SDL2_TRACE=1",
                "VEXA_AUDIO_TRACE=1",
            ),
            preinstalled = true,
        ),
        GuestProgram(
            id = "megabonk",
            title = "Megabonk",
            summary = "Unity game. Native Linux build. The Linux files are already in the app files directory.",
            assetPath = "",
            fileName = "Megabonk.x86_64",
            // Native landscape size of the test device. Headless launches use
            // the display size by default too; saved in-game settings can override this.
            args = listOf(
                "-screen-fullscreen", "0",
                "-screen-width", "2712",
                "-screen-height", "1220",
                "-force-glcore",
                // -force-gfx-direct deliberately omitted: it serialises draw
                // submission onto the single emulated render thread. Re-test it before
                // adding it back - it was needed when the display list was broken.
            ),
            // Its final sRGB framebuffer must blit into an sRGB EGL surface.
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=3405340", "VEXA_GL_SRGB_SURFACE=1", "VEXA_UNITY_NATIVE_MODE=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "vampiresurvivors",
            title = "Vampire Survivors",
            summary = "Unity game. Native Linux build (Mono runtime). The Linux depot files are already in the app files directory.",
            assetPath = "",
            fileName = "VampireSurvivors.exe",
            // Preserve the tested windowed launch preset, as for Megabonk.
            args = listOf(
                "-screen-fullscreen", "0",
                "-screen-width", "2712",
                "-screen-height", "1220",
                "-force-glcore",
                // -force-gfx-direct deliberately omitted: it serialises draw
                // submission onto the single emulated render thread. Re-test it before
                // adding it back - it was needed when the display list was broken.
            ),
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=1794680", "VEXA_STEAM_TRACE=1", "VEXA_MONO_COMPAT=1", "VEXA_ALSA_BLOCKING=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "hollowknight",
            title = "Hollow Knight",
            summary = "Unity game (Team Cherry). Native Linux Mono build from the GOG installer. " +
                "Exercises the Unity Mono path like Vampire Survivors, on a newer engine generation.",
            assetPath = "",
            fileName = "Hollow Knight",
            args = listOf("-screen-fullscreen", "0", "-screen-width", "2712", "-screen-height", "1220", "-force-glcore"),
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=367520", "VEXA_STEAM_TRACE=1", "VEXA_MONO_COMPAT=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "ets2",
            title = "Euro Truck Simulator 2",
            summary = "SCS Prism3D engine. Native 64-bit Linux build (steamcmd -os linux). " +
                "Heavy GL streaming title; base game files only (no dlc_*.scs).",
            assetPath = "",
            fileName = "bin/linux_x64/eurotrucks2",
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "LD_LIBRARY_PATH=bin/linux_x64", "SteamAppId=227300", "VEXA_STEAM_TRACE=1", "LD_BIND_NOW=1", "VEXA_FAKE_IM=1", "LD_PRELOAD=libvexa_glmap.so"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "undertale",
            title = "Undertale",
            summary = "GameMaker Studio game. Native Linux build (depot 391544). Control test: a second Linux-native " +
                "engine on the same GL stack. Not a Unity title, so it does not exercise the Unity offscreen-backbuffer path.",
            assetPath = "",
            fileName = "runner",
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=391540", "VEXA_STEAM_TRACE=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "unturned",
            title = "Unturned",
            summary = "Unity game (Smartly Dressed Games). Native 64-bit Linux build, depot from app_update " +
                "304930 -os linux. Third Unity data point, older engine generation than Megabonk.",
            assetPath = "",
            fileName = "Unturned.x86_64",
            args = listOf("-screen-fullscreen", "0", "-screen-width", "2712", "-screen-height", "1220"),
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=304930", "VEXA_STEAM_TRACE=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "summer-demo",
            title = "Summer at the Edge of the Universe (Demo)",
            summary = "Free Unity URP demo (app 2757760, depot from app_update -os linux). Reference title for the " +
                "Unity 2022+ URP offscreen-backbuffer path that Megabonk stalls on.",
            assetPath = "",
            fileName = "Summer at the Edge of the Universe.x86_64",
            args = listOf("-screen-fullscreen", "0", "-screen-width", "2712", "-screen-height", "1220"),
            env = listOf("DISPLAY=:0", "SDL_VIDEODRIVER=x11", "SteamAppId=2757760", "VEXA_STEAM_TRACE=1"),
            preinstalled = true,
        ),
        GuestProgram(
            id = "prismlauncher",
            title = "Prism Launcher",
            summary = "Qt6 Minecraft launcher (sharun portable build). Offscreen smoke test — no X server, GUI is not expected to render.",
            assetPath = "",
            fileName = "bin/prismlauncher",
            env = listOf(
                "QT_QPA_PLATFORM=offscreen",
                "QT_DEBUG_PLUGINS=1",
                "PRISMLAUNCHER_DISABLE_GLVULKAN=1",
            ),
            preinstalled = true,
        ),
    )

    private fun graphics(
        id: String,
        title: String,
        summary: String,
        assetFile: String,
        fileName: String,
        args: List<String> = emptyList(),
        env: List<String> = listOf("DISPLAY=:0"),
    ): GuestProgram = GuestProgram(
        id = id,
        title = title,
        summary = summary,
        assetPath = "guests/$assetFile",
        fileName = fileName,
        args = args,
        env = env,
    )

    fun find(id: String): GuestProgram? = programs.find { it.id == id }
}
