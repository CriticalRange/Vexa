package com.critical.vexaemulator.steam.library

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

data class InstalledSteamProgram(
    val id: String,
    val title: String,
    val appId: Int,
    val relativePath: String,
    val fileName: String,
)

// Finished downloads are also merged into GuestCatalog.userPrograms by CustomGuestStore.load.
object InstalledSteamPrograms {
    private const val relativeFile = "steam-library/installed.json"

    fun read(context: Context): List<InstalledSteamProgram> {
        val file = File(context.filesDir, relativeFile)
        if (!file.isFile) return emptyList()
        return runCatching {
            val array = JSONObject(file.readText()).optJSONArray("programs") ?: JSONArray()
            buildList {
                for (index in 0 until array.length()) {
                    val item = array.optJSONObject(index) ?: continue
                    val id = item.optString("id")
                    val title = item.optString("title")
                    val appId = item.optInt("appId")
                    val relativePath = item.optString("relativePath")
                    val fileName = item.optString("fileName")
                    if (id.isBlank() || appId <= 0 || relativePath.isBlank() || fileName.isBlank()) continue
                    add(
                        InstalledSteamProgram(
                            id = id,
                            title = title.ifBlank { "App $appId" },
                            appId = appId,
                            relativePath = relativePath,
                            fileName = fileName,
                        ),
                    )
                }
            }
        }.getOrDefault(emptyList())
    }

    fun upsert(context: Context, program: InstalledSteamProgram) {
        val current = read(context).filterNot { it.id == program.id } + program
        val array = JSONArray()
        current.forEach { item ->
            array.put(
                JSONObject()
                    .put("id", item.id)
                    .put("title", item.title)
                    .put("appId", item.appId)
                    .put("relativePath", item.relativePath)
                    .put("fileName", item.fileName),
            )
        }
        val file = File(context.filesDir, relativeFile)
        file.parentFile?.mkdirs()
        val temp = File(file.parentFile, "${file.name}.tmp")
        temp.writeText(JSONObject().put("programs", array).toString())
        if (file.exists() && !file.delete()) error("Could not replace installed Steam library")
        if (!temp.renameTo(file)) error("Could not write installed Steam library")
        com.critical.vexaemulator.guest.CustomGuestStore.load(context)
    }
}
