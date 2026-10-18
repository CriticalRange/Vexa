package com.critical.vexaemulator.guest

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.File
import java.io.RandomAccessFile
data class ElfProbe(
    val accepted: Boolean,
    val message: String?,
    val title: String,
    val summary: String,
    val arch: String?,
    val steamAppId: String?,
    val icon: Bitmap?,
)

object ElfArtwork {
    private const val EM_X86_64 = 62
    private val cache = android.util.LruCache<String, Bitmap>(12)

    fun cached(key: String): Bitmap? = cache.get(key)

    fun remember(key: String, bitmap: Bitmap) {
        cache.put(key, bitmap)
    }

    fun bitmapFor(program: GuestProgram, guestDir: File): Bitmap? {
        val key = program.id
        cache.get(key)?.let { return it }
        val icon = iconFile(program, guestDir)
        val decoded = icon?.let { decodeFile(it) }?.takeIf { looksLikeIcon(it) }
        if (decoded != null) cache.put(key, decoded)
        return decoded
    }

    /**
     * Named art first (assets/icon.png, Title.png), then a sibling image,
     * then a near-square image inside the x86_64 ELF. A root icon.png is
     * only a cache: font atlases and cursors fail [looksLikeIcon] and are replaced.
     */
    fun load(program: GuestProgram, guestDir: File): Bitmap? {
        bitmapFor(program, guestDir)?.let { return it }
        val named = namedArtwork(program, guestDir)
        if (named != null) return store(program, guestDir, named)
        val sibling = siblingImage(guestDir)
        if (sibling != null) return store(program, guestDir, sibling)
        val exe = File(guestDir, program.fileName)
        if (!exe.isFile || exe.length() < 64) return null
        val scanned = runCatching {
            RandomAccessFile(exe, "r").use { raf -> scanImages(raf) }
        }.getOrNull() ?: return null
        return store(program, guestDir, scanned)
    }

    private fun store(program: GuestProgram, guestDir: File, bitmap: Bitmap): Bitmap {
        cache.put(program.id, bitmap)
        runCatching {
            File(guestDir, "icon.png").outputStream().use { out ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
            }
        }
        return bitmap
    }

    private fun iconFile(program: GuestProgram, guestDir: File): File? =
        program.iconFile?.let { File(guestDir, it) }?.takeIf { it.isFile }
            ?: File(guestDir, "icon.png").takeIf { it.isFile }
            ?: File(guestDir, "icon.jpg").takeIf { it.isFile }
            ?: File(guestDir, "icon.jpeg").takeIf { it.isFile }

    private fun namedArtwork(program: GuestProgram, guestDir: File): Bitmap? {
        val stem = File(program.fileName).nameWithoutExtension.lowercase()
        val title = program.title.lowercase()
        val names = setOf(
            "icon.png", "icon.jpg", "icon.jpeg", "icon.webp",
            "$stem.png", "$title.png",
        )
        val found = ArrayList<File>()
        fun walk(dir: File, depth: Int) {
            if (depth > 2) return
            val children = dir.listFiles() ?: return
            for (child in children) {
                if (child.isDirectory) {
                    if (child.name != ".local" && child.name != ".config" && child.name != ".mono") {
                        walk(child, depth + 1)
                    }
                } else if (child.name.lowercase() in names) {
                    found += child
                }
            }
        }
        walk(guestDir, 0)
        val besidesCache = found.filter { file ->
            !(file.parentFile == guestDir && file.name.equals("icon.png", ignoreCase = true))
        }
        val pool = if (besidesCache.isNotEmpty()) besidesCache else found
        return pool.mapNotNull { decodeFile(it) }.maxByOrNull { it.width * it.height }
    }

    private fun siblingImage(guestDir: File): Bitmap? {
        val files = guestDir.listFiles() ?: return null
        return files
            .filter {
                it.isFile &&
                    it.extension.lowercase() in setOf("png", "jpg", "jpeg", "webp") &&
                    !it.name.equals("icon.png", ignoreCase = true)
            }
            .sortedByDescending { it.length() }
            .firstNotNullOfOrNull { file -> decodeFile(file)?.takeIf { looksLikeIcon(it) } }
    }

    /** Reject font strips, cursors, and mostly empty or one-color sheets. */
    private fun looksLikeIcon(bitmap: Bitmap): Boolean {
        val w = bitmap.width
        val h = bitmap.height
        val shortSide = minOf(w, h)
        val longSide = maxOf(w, h)
        if (shortSide < 48 || longSide > 1024) return false
        if (longSide.toFloat() / shortSide > 1.35f) return false
        var filled = 0
        var colored = 0
        val grid = 16
        for (y in 0 until grid) {
            for (x in 0 until grid) {
                val color = bitmap.getPixel(x * (w - 1) / (grid - 1), y * (h - 1) / (grid - 1))
                val alpha = color ushr 24
                if (alpha < 40) continue
                filled++
                val r = (color shr 16) and 0xff
                val g = (color shr 8) and 0xff
                val b = color and 0xff
                val spread = maxOf(r, g, b) - minOf(r, g, b)
                val luma = (r + g + b) / 3
                if (spread > 18 && luma in 20..245) colored++
            }
        }
        return filled >= 36 && colored >= 12
    }

    fun probe(file: File): ElfProbe {
        val fallbackTitle = file.nameWithoutExtension.ifBlank { file.name }
        RandomAccessFile(file, "r").use { raf ->
            if (raf.length() < 64) {
                return reject(fallbackTitle, "File is too small to be an ELF.")
            }
            val ident = ByteArray(64)
            raf.readFully(ident)
            if (ident[0] != 0x7f.toByte() || ident[1] != 'E'.code.toByte() ||
                ident[2] != 'L'.code.toByte() || ident[3] != 'F'.code.toByte()
            ) {
                return reject(fallbackTitle, "Not an ELF. Pick a Linux executable.")
            }
            if (ident[4].toInt() != 2) {
                return reject(fallbackTitle, "Not a 64-bit ELF. VEXA only runs ELF class 64.")
            }
            val machine = u16(ident, 18)
            if (machine != EM_X86_64) {
                return reject(
                    fallbackTitle,
                    "Not an x86_64 ELF (e_machine=$machine, expected $EM_X86_64). Proton/Windows builds are out of scope.",
                )
            }
            val notes = readNotes(raf, ident)
            val side = readSiblings(file)
            val icon = side.icon ?: scanImages(raf)
            val title = side.name ?: fallbackTitle
            val summary = buildString {
                append(side.comment ?: notes.comment ?: "Linux x86_64 ELF.")
                if (notes.needed.isNotEmpty()) {
                    append(" Needs ")
                    append(notes.needed.take(6).joinToString(", "))
                    append('.')
                }
            }
            val appId = side.steamAppId ?: notes.steamAppId
            return ElfProbe(
                accepted = true,
                message = null,
                title = title,
                summary = summary.trim(),
                arch = "x86_64",
                steamAppId = appId,
                icon = icon,
            )
        }
    }

    private fun reject(title: String, message: String) = ElfProbe(
        accepted = false,
        message = message,
        title = title,
        summary = "",
        arch = null,
        steamAppId = null,
        icon = null,
    )

    private data class Notes(val comment: String?, val needed: List<String>, val steamAppId: String?)
    private data class Side(val name: String?, val comment: String?, val steamAppId: String?, val icon: Bitmap?)

    private fun readSiblings(file: File): Side {
        val dir = file.parentFile ?: return Side(null, null, null, null)
        if (!dir.isDirectory) return Side(null, null, null, null)
        val icon = listOf("icon.png", "icon.jpg", "icon.jpeg")
            .map { File(dir, it) }
            .firstOrNull { it.isFile }
            ?.let { decodeFile(it) }
        val appId = File(dir, "steam_appid.txt").takeIf { it.isFile }
            ?.readText()
            ?.lineSequence()
            ?.map { it.trim() }
            ?.firstOrNull { it.all(Char::isDigit) && it.isNotEmpty() }
        var name: String? = null
        var comment: String? = null
        dir.listFiles { child -> child.isFile && child.name.endsWith(".desktop") }?.forEach { desktop ->
            val text = runCatching { desktop.readText() }.getOrNull() ?: return@forEach
            if (!text.contains("[Desktop Entry]")) return@forEach
            fun field(key: String) = text.lineSequence()
                .map { it.trim() }
                .firstOrNull { it.startsWith("$key=") && !it.startsWith("$key" + "[") }
                ?.substringAfter("=")
                ?.trim()
                ?.takeIf { it.isNotEmpty() }
            name = name ?: field("Name")
            comment = comment ?: field("Comment")
            if (icon == null) {
                val iconName = field("Icon")
                if (iconName != null && (iconName.endsWith(".png") || iconName.endsWith(".jpg"))) {
                    val candidate = File(if (iconName.startsWith("/")) iconName else File(dir, iconName).path)
                    if (candidate.isFile) return Side(name, comment, appId, decodeFile(candidate))
                }
            }
        }
        return Side(name, comment, appId, icon)
    }

    private fun readNotes(raf: RandomAccessFile, ident: ByteArray): Notes {
        val shoff = u64(ident, 40)
        val shentsize = u16(ident, 58)
        val shnum = u16(ident, 60)
        val shstrndx = u16(ident, 62)
        if (shoff <= 0 || shentsize < 64 || shnum <= 0 || shnum > 512 || shstrndx >= shnum) {
            return Notes(null, emptyList(), null)
        }
        if (shoff + shnum.toLong() * shentsize > raf.length()) return Notes(null, emptyList(), null)
        val headers = Array(shnum) { ByteArray(shentsize) }
        for (i in 0 until shnum) {
            raf.seek(shoff + i.toLong() * shentsize)
            raf.readFully(headers[i], 0, shentsize)
        }
        val strHeader = headers[shstrndx]
        val strOff = u64(strHeader, 24)
        val strSize = u64(strHeader, 32).coerceAtMost(1_048_576)
        val strings = ByteArray(strSize.toInt())
        if (strSize > 0 && strOff + strSize <= raf.length()) {
            raf.seek(strOff)
            raf.readFully(strings)
        }
        fun nameOf(header: ByteArray): String {
            val start = u32(header, 0)
            if (start < 0 || start >= strings.size) return ""
            val end = (start until strings.size).firstOrNull { strings[it] == 0.toByte() } ?: strings.size
            return String(strings, start, end - start, Charsets.UTF_8)
        }
        var comment: String? = null
        val needed = mutableListOf<String>()
        var steamAppId: String? = null
        for (header in headers) {
            val name = nameOf(header)
            val off = u64(header, 24)
            val size = u64(header, 32)
            if (size <= 0 || off < 0 || off + size > raf.length()) continue
            when (name) {
                ".comment" -> {
                    val n = size.coerceAtMost(400).toInt()
                    val buf = ByteArray(n)
                    raf.seek(off)
                    raf.readFully(buf)
                    comment = String(buf, Charsets.UTF_8).replace('\u0000', ' ').trim().take(180).ifBlank { null }
                }
                ".dynstr" -> {
                    val n = size.coerceAtMost(262_144).toInt()
                    val buf = ByteArray(n)
                    raf.seek(off)
                    raf.readFully(buf)
                    val text = String(buf, Charsets.ISO_8859_1)
                    text.split('\u0000').forEach { token ->
                        if (token.startsWith("lib") && ".so" in token && token !in needed && needed.size < 8) {
                            needed += token
                        }
                        if (steamAppId == null) {
                            val match = Regex("SteamAppId=(\\d+)").find(token)
                            if (match != null) steamAppId = match.groupValues[1]
                        }
                    }
                }
            }
        }
        return Notes(comment, needed, steamAppId)
    }

    private fun scanImages(raf: RandomAccessFile): Bitmap? {
        val png = byteArrayOf(0x89.toByte(), 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
        val window = 1024 * 1024
        val overlap = 32
        var pos = 0L
        var best: Bitmap? = null
        var bestScore = Int.MAX_VALUE
        val length = raf.length()
        while (pos < length) {
            val n = minOf(window.toLong(), length - pos).toInt()
            val buf = ByteArray(n)
            raf.seek(pos)
            raf.readFully(buf)
            var i = 0
            while (i < buf.size - 8) {
                val pngAt = indexOf(buf, png, i)
                val jpgAt = indexOfJpeg(buf, i)
                val at = listOfNotNull(pngAt, jpgAt).minOrNull() ?: break
                val chunk = if (at == pngAt) slicePng(buf, at, raf, pos) else sliceJpeg(buf, at, raf, pos)
                if (chunk != null) {
                    val bmp = decodeBytes(chunk)
                    if (bmp != null && looksLikeIcon(bmp)) {
                        val score = -minOf(bmp.width, bmp.height)
                        if (score < bestScore) {
                            best = bmp
                            bestScore = score
                        }
                    }
                }
                i = at + 8
            }
            if (pos + n >= length) break
            pos += (n - overlap).coerceAtLeast(1)
        }
        return best
    }

    private fun slicePng(buf: ByteArray, at: Int, raf: RandomAccessFile, base: Long): ByteArray? {
        val iend = byteArrayOf(0x49, 0x45, 0x4E, 0x44, 0xAE.toByte(), 0x42, 0x60, 0x82.toByte())
        val localEnd = indexOf(buf, iend, at + 8)
        if (localEnd != null && localEnd + 8 - at <= 1_500_000) {
            return buf.copyOfRange(at, localEnd + 8)
        }
        val abs = base + at
        val max = minOf(1_500_000L, raf.length() - abs).toInt()
        if (max < 32) return null
        val bytes = ByteArray(max)
        raf.seek(abs)
        raf.readFully(bytes)
        val end = indexOf(bytes, iend, 8) ?: return null
        if (end + 8 > bytes.size) return null
        return bytes.copyOfRange(0, end + 8)
    }

    private fun sliceJpeg(buf: ByteArray, at: Int, raf: RandomAccessFile, base: Long): ByteArray? {
        val local = jpegEnd(buf, at)
        if (local != null && local - at <= 1_500_000) return buf.copyOfRange(at, local)
        val abs = base + at
        val max = minOf(1_500_000L, raf.length() - abs).toInt()
        if (max < 4) return null
        val bytes = ByteArray(max)
        raf.seek(abs)
        raf.readFully(bytes)
        val end = jpegEnd(bytes, 0) ?: return null
        return bytes.copyOfRange(0, end)
    }

    private fun jpegEnd(buf: ByteArray, start: Int): Int? {
        var i = start + 2
        while (i < buf.size - 1) {
            if (buf[i] == 0xFF.toByte() && buf[i + 1] == 0xD9.toByte()) return i + 2
            i++
        }
        return null
    }

    private fun indexOfJpeg(buf: ByteArray, from: Int): Int? {
        var i = from
        while (i < buf.size - 3) {
            if (buf[i] == 0xFF.toByte() && buf[i + 1] == 0xD8.toByte() && buf[i + 2] == 0xFF.toByte()) return i
            i++
        }
        return null
    }

    private fun indexOf(buf: ByteArray, needle: ByteArray, from: Int): Int? {
        var i = from
        while (i <= buf.size - needle.size) {
            var ok = true
            for (j in needle.indices) {
                if (buf[i + j] != needle[j]) {
                    ok = false
                    break
                }
            }
            if (ok) return i
            i++
        }
        return null
    }

    private fun decodeBytes(bytes: ByteArray): Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
        if (bounds.outWidth < 32 || bounds.outHeight < 32) return null
        if (bounds.outWidth > 2048 || bounds.outHeight > 2048) return null
        return BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
    }

    private fun decodeFile(file: File): Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(file.absolutePath, bounds)
        if (bounds.outWidth < 32 || bounds.outHeight < 32) return null
        return BitmapFactory.decodeFile(file.absolutePath)?.also { cache.put(file.absolutePath, it) }
    }

    private fun u16(b: ByteArray, at: Int) =
        (b[at].toInt() and 0xff) or ((b[at + 1].toInt() and 0xff) shl 8)

    private fun u32(b: ByteArray, at: Int): Int {
        var v = 0L
        for (i in 0 until 4) v = v or ((b[at + i].toLong() and 0xff) shl (8 * i))
        return v.toInt()
    }

    private fun u64(b: ByteArray, at: Int): Long {
        var v = 0L
        for (i in 0 until 8) v = v or ((b[at + i].toLong() and 0xff) shl (8 * i))
        return v
    }

}
