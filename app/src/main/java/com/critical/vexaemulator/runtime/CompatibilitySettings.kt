package com.critical.vexaemulator.runtime

import android.content.Context
import org.json.JSONObject
import java.io.File

/** Pinned installation selection. A launch request's explicit env takes precedence. */
object CompatibilitySettings {
    fun environment(context: Context): List<String> {
        val file = File(context.filesDir, "compatibility/proton.json")
        if (!file.isFile) return emptyList()
        val json = JSONObject(file.readText())
        val keys = mapOf(
            "protonPath" to "VEXA_PROTON_PATH",
            "steamClientPath" to "VEXA_STEAM_CLIENT_PATH",
            "dataRoot" to "VEXA_COMPAT_DATA_ROOT",
            "python" to "VEXA_PROTON_PYTHON",
            "rootfs" to "VEXA_PROTON_ROOTFS",
            "displayBackend" to "VEXA_PROTON_DISPLAY_BACKEND",
            "display" to "DISPLAY",
        )
        return keys.mapNotNull { (setting, environment) ->
            if (!json.has(setting)) return@mapNotNull null
            val value = json.getString(setting)
            require(!value.contains('\u0000')) { "Invalid Proton setting: $setting" }
            "$environment=$value"
        }
    }

    fun apply(context: Context, request: LaunchRequest): LaunchRequest {
        com.critical.vexaemulator.guest.GuestInstaller.ensureCompatibilityHelper(context, File(request.rootfsPath))
        val explicit = request.launchEnv.map { it.substringBefore('=') }.toSet()
        val controls = listOf(
            "VEXA_REEXEC=${context.applicationInfo.nativeLibraryDir}/libvexaloader.so",
            "VEXA_ROOTFS=${request.rootfsPath}",
            "VEXA_THUNK_HOST=${request.thunkHostPath}",
            "VEXA_THUNK_GUEST=${request.thunkGuestPath}",
            "VEXA_WORKDIR=${request.workingDirectory}",
            "VEXA_ARTIFACTS=${request.artifactDirectory}",
        )
        val controlKeys = controls.map { it.substringBefore('=') }.toSet()
        val merged = environment(context).filterNot {
            it.substringBefore('=') in explicit
        } + request.launchEnv
        return request.copy(launchEnv = merged.filterNot {
            it.substringBefore('=') in controlKeys
        } + controls)
    }
}
