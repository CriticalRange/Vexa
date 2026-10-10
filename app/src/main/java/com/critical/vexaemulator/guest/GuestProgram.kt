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
