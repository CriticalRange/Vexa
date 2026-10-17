package com.critical.vexaemulator.guest

import com.critical.vexaemulator.runtime.LaunchRequest

/** Unity launch switch consumed before starting FEX, not passed to Unity as a flag. */
object MegabonkGraphics {
    const val ENV = "VEXA_GRAPHICS_BACKEND"

    fun apply(request: LaunchRequest): LaunchRequest {
        val value = request.launchEnv.lastOrNull { it.startsWith("$ENV=") }
            ?.substringAfter('=')?.lowercase() ?: "gl"
        require(value == "gl" || value == "vulkan") {
            "$ENV must be gl or vulkan (got $value)"
        }
        val args = request.launchArgs.filterNot {
            it == "-force-glcore" || it == "-force-vulkan"
        } + if (value == "vulkan") "-force-vulkan" else "-force-glcore"
        return request.copy(launchArgs = args)
    }
}
