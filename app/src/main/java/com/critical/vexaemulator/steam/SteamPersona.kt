package com.critical.vexaemulator.steam

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.net.HttpURLConnection
import java.net.URL

data class SteamPersona(val name: String?, val avatar: Bitmap?)

object SteamPersonaCache {
    private val memory = android.util.LruCache<String, SteamPersona>(4)

    fun cached(steamId: String): SteamPersona? = memory.get(steamId)

    fun load(steamId: String): SteamPersona? {
        if (steamId.isBlank() || !steamId.all(Char::isDigit)) return null
        memory.get(steamId)?.let { return it }
        val xml = runCatching { http("https://steamcommunity.com/profiles/$steamId/?xml=1") }.getOrNull()
            ?: return null
        val name = cdata(xml, "steamID")
        val avatarUrl = cdata(xml, "avatarMedium") ?: cdata(xml, "avatarFull")
        val bitmap = avatarUrl?.let { url ->
            runCatching {
                httpBytes(url)?.let { BitmapFactory.decodeByteArray(it, 0, it.size) }
            }.getOrNull()
        }
        if (name.isNullOrBlank() && bitmap == null) return null
        return SteamPersona(name?.takeIf { it.isNotBlank() }, bitmap).also { memory.put(steamId, it) }
    }

    private fun cdata(xml: String, tag: String): String? {
        val open = "<$tag>"
        val close = "</$tag>"
        val start = xml.indexOf(open)
        val end = xml.indexOf(close)
        if (start < 0 || end <= start) return null
        return xml.substring(start + open.length, end)
            .replace("<![CDATA[", "")
            .replace("]]>", "")
            .trim()
            .ifBlank { null }
    }

    private fun http(url: String): String? = httpBytes(url)?.toString(Charsets.UTF_8)

    private fun httpBytes(url: String): ByteArray? {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 5000
            readTimeout = 5000
            instanceFollowRedirects = true
            setRequestProperty("User-Agent", "VEXA")
        }
        return try {
            if (connection.responseCode !in 200..299) null else connection.inputStream.use { it.readBytes() }
        } finally {
            connection.disconnect()
        }
    }
}
