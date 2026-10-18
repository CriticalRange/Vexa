package com.critical.vexaemulator.guest

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

object CustomGuestStore {
    private const val FILE_NAME = "custom-programs.json"
    private var persisted: List<GuestProgram> = emptyList()

    fun load(context: Context) {
        val app = context.applicationContext
        val file = File(app.filesDir, FILE_NAME)
        persisted = if (!file.isFile) emptyList() else runCatching { parse(file.readText()) }.getOrDefault(emptyList())
        val steam = com.critical.vexaemulator.steam.library.InstalledSteamPrograms.read(app).map { item ->
            GuestProgram(
                id = item.id,
                title = item.title,
                summary = "Installed from Steam. Linux x86_64 depot.",
                assetPath = "",
                fileName = item.fileName,
                env = listOf("SteamAppId=${item.appId}"),
                preinstalled = true,
                arch = "x86_64",
                steamAppId = item.appId.toString(),
                userAdded = true,
            )
        }
        val ids = persisted.map { it.id }.toSet()
        GuestCatalog.userPrograms = persisted + steam.filter { it.id !in ids }
    }

    fun programs(): List<GuestProgram> = GuestCatalog.userPrograms

    fun save(context: Context, program: GuestProgram) {
        val app = context.applicationContext
        persisted = persisted.filterNot { it.id == program.id } + program
        // Steam installs live in installed.json; keep them out of custom-programs.json.
        val steamIds = com.critical.vexaemulator.steam.library.InstalledSteamPrograms.read(app).map { it.id }.toSet()
        GuestCatalog.userPrograms = persisted + GuestCatalog.userPrograms.filter { it.id in steamIds }
        val array = JSONArray()
        persisted.forEach { item ->
            array.put(JSONObject().apply {
                put("id", item.id)
                put("title", item.title)
                put("summary", item.summary)
                put("fileName", item.fileName)
                put("arch", item.arch ?: "x86_64")
                put("steamAppId", item.steamAppId ?: "")
                put("iconFile", item.iconFile ?: "")
                put("args", JSONArray(item.args))
                put("env", JSONArray(item.env))
            })
        }
        File(app.filesDir, FILE_NAME).writeText(array.toString())
    }

    fun guestDir(context: Context, id: String): File =
        File(context.applicationContext.filesDir, "guests/$id")

    private fun parse(text: String): List<GuestProgram> {
        val array = JSONArray(text)
        return buildList {
            for (i in 0 until array.length()) {
                val o = array.optJSONObject(i) ?: continue
                val id = o.optString("id")
                val fileName = o.optString("fileName")
                if (id.isBlank() || fileName.isBlank()) continue
                val env = jsonStrings(o.optJSONArray("env"))
                val appId = o.optString("steamAppId").ifBlank {
                    env.firstOrNull { it.startsWith("SteamAppId=") }?.substringAfter("=")
                }
                add(
                    GuestProgram(
                        id = id,
                        title = o.optString("title").ifBlank { id },
                        summary = o.optString("summary"),
                        assetPath = "",
                        fileName = fileName,
                        args = jsonStrings(o.optJSONArray("args")),
                        env = env,
                        preinstalled = true,
                        arch = o.optString("arch").ifBlank { "x86_64" },
                        steamAppId = appId?.ifBlank { null },
                        iconFile = o.optString("iconFile").ifBlank { null },
                        userAdded = true,
                    ),
                )
            }
        }
    }

    private fun jsonStrings(array: JSONArray?): List<String> {
        if (array == null) return emptyList()
        return buildList {
            for (i in 0 until array.length()) {
                val value = array.optString(i)
                if (value.isNotBlank()) add(value)
            }
        }
    }
}
