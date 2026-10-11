package com.critical.vexaemulator.steam

import android.content.Context
import android.net.Uri
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

data class SteamLibraries(
    val apiPresent: Boolean = false,
    val clientPresent: Boolean = false,
    val status: String = "Not checked",
)

// Official Steamworks SDK 1.63 androidarm64 redistributable is libsteam_api.so.
// libsteamclient.so is the Android client library a title connects to. Neither
// binary is packaged in the APK or the git repo; the user imports them.
object SteamworksHost {
    private val _libraries = MutableStateFlow(SteamLibraries())
    val libraries: StateFlow<SteamLibraries> = _libraries.asStateFlow()

    private val nativeLoaded = lazy { System.loadLibrary("vexasteam") }

    fun refresh(context: Context) {
        val api = apiFile(context)
        val client = clientFile(context)
        _libraries.value = _libraries.value.copy(
            apiPresent = api.isFile,
            clientPresent = client.isFile,
        )
    }

    fun importApi(context: Context, uri: Uri): String =
        import(context, uri, apiFile(context), aarch64 = true).also(::publish)

    fun importClient(context: Context, uri: Uri): String =
        import(context, uri, clientFile(context), aarch64 = true).also(::publish)

    fun prepare(context: Context, appId: String): String {
        refresh(context)
        val api = apiFile(context)
        val client = clientFile(context)
        if (!api.isFile && !client.isFile) {
            val missing = "androidarm64 libraries are not installed"
            _libraries.value = SteamLibraries(status = missing)
            return missing
        }
        return try {
            nativeLoaded.value
            val status = nativeProbe(
                if (api.isFile) api.absolutePath else "",
                if (client.isFile) client.absolutePath else "",
                appId,
            )
            _libraries.value = SteamLibraries(api.isFile, client.isFile, status)
            VexaLogger.log(LogLevel.INFO, LogCategory.RUNTIME, "Steamworks androidarm64: $status")
            status
        } catch (error: Throwable) {
            val status = "androidarm64 probe failed: ${error.javaClass.simpleName}"
            _libraries.value = SteamLibraries(api.isFile, client.isFile, status)
            VexaLogger.log(LogLevel.ERROR, LogCategory.FAILURE, status)
            status
        }
    }

    fun launchEnvironment(context: Context, programEnv: List<String>): List<String> {
        val appId = programEnv.firstOrNull { it.startsWith("SteamAppId=") }
            ?.substringAfter("=")
            .orEmpty()
        if (appId.isEmpty()) return emptyList()
        prepare(context, appId)
        val session = SteamSessionStore.load(context) ?: return emptyList()
        if (session.accountName.isEmpty()) return emptyList()
        VexaLogger.log(
            LogLevel.INFO,
            LogCategory.RUNTIME,
            "Steam session attached to launch",
            mapOf("steamId" to session.steamId),
        )
        val extra = mutableListOf<String>()
        if (programEnv.none { it.startsWith("SteamUser=") }) {
            extra += "SteamUser=${session.accountName}"
        }
        if (programEnv.none { it.startsWith("SteamAppUser=") }) {
            extra += "SteamAppUser=${session.accountName}"
        }
        if (programEnv.none { it.startsWith("SteamGameId=") }) {
            extra += "SteamGameId=$appId"
        }
        return extra
    }

    private fun import(context: Context, uri: Uri, dest: File, aarch64: Boolean): String {
        val header = ByteArray(20)
        val read = context.contentResolver.openInputStream(uri)?.use { input ->
            input.read(header)
        } ?: return "Could not read the selected file"
        if (read < header.size) return "The selected file is too small to be a library"
        val machine = elfMachine(header)
        if (aarch64 && machine != EM_AARCH64) {
            val kind = when (machine) {
                EM_X86_64 -> "x86_64"
                null -> "not an ELF"
                else -> "machine $machine"
            }
            return "Need the androidarm64 build ($kind)"
        }
        dest.parentFile?.mkdirs()
        val tmp = File(dest.parentFile, dest.name + ".tmp")
        context.contentResolver.openInputStream(uri)?.use { input ->
            tmp.outputStream().use { output -> input.copyTo(output) }
        } ?: return "Could not copy the selected file"
        if (!tmp.renameTo(dest)) {
            tmp.copyTo(dest, overwrite = true)
            tmp.delete()
        }
        dest.setReadable(true, false)
        refresh(context)
        return "Saved ${dest.name}"
    }

    private fun publish(message: String) {
        _libraries.value = _libraries.value.copy(status = message)
    }

    private fun apiFile(context: Context): File =
        File(context.filesDir, "steam/androidarm64/libsteam_api.so")

    private fun clientFile(context: Context): File =
        File(context.filesDir, "steam/androidarm64/libsteamclient.so")

    private fun elfMachine(header: ByteArray): Int? {
        if (header[0] != 0x7f.toByte() || header[1] != 'E'.code.toByte() ||
            header[2] != 'L'.code.toByte() || header[3] != 'F'.code.toByte()
        ) {
            return null
        }
        return (header[18].toInt() and 0xff) or ((header[19].toInt() and 0xff) shl 8)
    }

    private external fun nativeProbe(apiPath: String, clientPath: String, appId: String): String

    private const val EM_AARCH64 = 183
    private const val EM_X86_64 = 62
}
