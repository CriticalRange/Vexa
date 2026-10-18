package com.critical.vexaemulator.steam

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.text.Html
import android.util.LruCache
import com.critical.vexaemulator.guest.GuestProgram
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.security.MessageDigest
import java.util.concurrent.ConcurrentHashMap

data class SteamGameDetails(
    val appId: Int,
    val name: String,
    val description: String,
    val header: String,
    val background: String,
    val screenshots: List<String>,
    val genres: List<String>,
    val developer: String,
) {
    val hero: String get() = "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/library_hero.jpg"
    val cover: String get() = "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/library_600x900.jpg"
    /** Store vertical capsule. The public file is the 374×448 render of the 748×896 upload. */
    val vertical: String get() = "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/$appId/hero_capsule.jpg"
}

/** Public Store data, independent of Steam login. Failures leave local launch data intact. */
object SteamArtwork {
    private const val DAY = 86_400_000L
    private val locks = Array(32) { Mutex() }
    private val attempts = ConcurrentHashMap<String, Long>()
    private val bitmaps = object : LruCache<String, Bitmap>(24 * 1024 * 1024) {
        override fun sizeOf(key: String, value: Bitmap) = value.byteCount
    }

    private fun key(program: GuestProgram) = SteamGameIdentity.appId(program)?.toString()
        ?: "name:${SteamGameIdentity.normalizedTitle(program.title)}"
    private fun hash(value: String) = MessageDigest.getInstance("SHA-256")
        .digest(value.toByteArray()).joinToString("") { "%02x".format(it) }
    private fun file(context: Context, key: String) = File(context.cacheDir, "steam-artwork/${hash(key)}")
    private fun lock(key: String) = locks[(key.hashCode() and Int.MAX_VALUE) % locks.size]

    suspend fun cached(context: Context, program: GuestProgram): SteamGameDetails? = withContext(Dispatchers.IO) {
        runCatching { parse(JSONObject(file(context, key(program)).readText())) }.getOrNull()
    }

    suspend fun details(context: Context, program: GuestProgram): SteamGameDetails? = withContext(Dispatchers.IO) {
        if (!program.preinstalled && SteamGameIdentity.appId(program) == null) return@withContext null
        val key = key(program)
        lock(key).withLock {
            val cache = file(context, key)
            val old = runCatching { parse(JSONObject(cache.readText())) }.getOrNull()
            val now = System.currentTimeMillis()
            if (old != null && now - cache.lastModified() < 7 * DAY) return@withLock old
            if (now - (attempts[key] ?: 0) < 60_000) return@withLock old
            attempts[key] = now
            try {
                val id = SteamGameIdentity.appId(program) ?: search(program.title) ?: return@withLock old
                val response = JSONObject(String(download("https://store.steampowered.com/api/appdetails?appids=$id&l=english", 2_000_000), Charsets.UTF_8))
                    .getJSONObject(id.toString())
                check(response.optBoolean("success"))
                val data = response.getJSONObject("data")
                check(data.optInt("steam_appid") == id)
                val result = parse(data)
                runCatching { save(cache, data.toString().toByteArray()) }
                result
            } catch (_: Exception) { old }
        }
    }

    private fun search(title: String): Int? {
        val term = URLEncoder.encode(title, "UTF-8")
        val items = JSONObject(String(download("https://store.steampowered.com/api/storesearch/?term=$term&l=english&cc=US", 1_000_000), Charsets.UTF_8))
            .optJSONArray("items") ?: return null
        return SteamGameIdentity.exactMatch(title, (0 until items.length()).map {
            items.getJSONObject(it).let { item -> item.optInt("id") to item.optString("name") }
        })
    }

    private fun parse(data: JSONObject): SteamGameDetails {
        val id = data.getInt("steam_appid")
        require(id > 0 && data.getString("name").isNotBlank())
        val shots = data.optJSONArray("screenshots")
        val genres = data.optJSONArray("genres")
        return SteamGameDetails(id, data.getString("name"),
            Html.fromHtml(data.optString("short_description"), Html.FROM_HTML_MODE_LEGACY).toString().trim(),
            data.optString("header_image"), data.optString("background_raw"),
            (0 until (shots?.length() ?: 0)).take(6).map { shots!!.getJSONObject(it).optString("path_thumbnail") }.filter { it.isNotBlank() },
            (0 until (genres?.length() ?: 0)).map { genres!!.getJSONObject(it).optString("description") },
            data.optJSONArray("developers")?.optString(0).orEmpty())
    }

    suspend fun image(context: Context, urls: List<String>): Bitmap? = withContext(Dispatchers.IO) {
        for (url in urls.filter { it.isNotBlank() }.distinct()) {
            bitmaps.get(url)?.let { return@withContext it }
            val image = lock(url).withLock {
                bitmaps.get(url)?.let { return@withLock it }
                val cache = file(context, url)
                var bytes = if (cache.isFile) runCatching { cache.readBytes() }.getOrNull() else null
                if (bytes == null) {
                    val now = System.currentTimeMillis()
                    if (now - (attempts[url] ?: 0) < 60_000) return@withLock null
                    attempts[url] = now
                    bytes = runCatching { download(url, 8_000_000) }.getOrNull()
                }
                val data = bytes ?: return@withLock null
                val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                BitmapFactory.decodeByteArray(data, 0, data.size, bounds)
                if (bounds.outWidth <= 0 || bounds.outHeight <= 0) { cache.delete(); return@withLock null }
                val options = BitmapFactory.Options().apply {
                    inSampleSize = 1
                    while (bounds.outWidth / inSampleSize > 1600 || bounds.outHeight / inSampleSize > 1000) inSampleSize *= 2
                }
                BitmapFactory.decodeByteArray(data, 0, data.size, options)?.also {
                    bitmaps.put(url, it)
                    if (!cache.exists()) runCatching { save(cache, data) }
                }
            }
            if (image != null) return@withContext image
        }
        null
    }

    private fun save(target: File, bytes: ByteArray) {
        target.parentFile!!.mkdirs()
        val temp = File.createTempFile("download-", ".tmp", target.parentFile)
        try {
            temp.writeBytes(bytes)
            check(temp.renameTo(target))
            // Bound disk use; eviction is safe because this is disposable presentation data.
            val files = target.parentFile!!.listFiles().orEmpty().filter { !it.name.endsWith(".tmp") }
            var size = files.sumOf { it.length() }
            for (item in files.sortedBy { it.lastModified() }) {
                if (size <= 96 * 1024 * 1024) break
                val length = item.length()
                if (item.delete()) size -= length
            }
        } finally { temp.delete() }
    }

    private fun download(address: String, limit: Int): ByteArray {
        val url = URL(address)
        require(url.protocol == "https" && (url.host == "store.steampowered.com" || url.host.endsWith(".steamstatic.com")))
        val connection = url.openConnection() as HttpURLConnection
        try {
            connection.connectTimeout = 6000
            connection.readTimeout = 8000
            connection.instanceFollowRedirects = false
            connection.setRequestProperty("User-Agent", "VEXA/1.0")
            check(connection.responseCode == 200)
            return connection.inputStream.use { stream ->
                val output = java.io.ByteArrayOutputStream()
                val buffer = ByteArray(8192)
                while (true) {
                    val count = stream.read(buffer)
                    if (count < 0) break
                    check(output.size() + count <= limit)
                    output.write(buffer, 0, count)
                }
                output.toByteArray()
            }
        } finally { connection.disconnect() }
    }
}
