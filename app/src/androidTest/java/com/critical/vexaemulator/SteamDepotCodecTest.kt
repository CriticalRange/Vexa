package com.critical.vexaemulator

import java.io.ByteArrayOutputStream
import org.tukaani.xz.LZMA2Options
import org.tukaani.xz.LZMAOutputStream
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.github.luben.zstd.Zstd
import `in`.dragonbra.javasteam.util.VZipUtil
import `in`.dragonbra.javasteam.util.VZstdUtil
import `in`.dragonbra.javasteam.util.stream.MemoryStream
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

/** Runs the actual JavaSteam codecs inside the installed Android APK. */
@RunWith(AndroidJUnit4::class)
class SteamDepotCodecTest {
    private val expected = "VEXA Steam download codec regression\n".repeat(32).toByteArray()

    @Test fun steamLzmaChunkDecodesOnDevice() {
        // Steam uses a known-size raw LZMA stream, without the .lzma end marker.
        val stream = ByteArrayOutputStream()
        LZMAOutputStream(stream, LZMA2Options(), expected.size.toLong()).use { it.write(expected) }
        val alone = stream.toByteArray()
        val crc = CRC32().apply { update(expected) }.value.toInt()
        val fixture = ByteBuffer.allocate(7 + 5 + alone.size - 13 + 10).order(ByteOrder.LITTLE_ENDIAN)
            .put("VZa".toByteArray()).putInt(crc).put(alone, 0, 5).put(alone, 13, alone.size - 13)
            .putInt(crc).putInt(expected.size).put("zv".toByteArray()).array()
        val actual = ByteArray(expected.size)
        assertEquals(expected.size, VZipUtil.decompress(MemoryStream(fixture), actual, true))
        assertArrayEquals(expected, actual)
    }

    @Test fun steamZstdChunkDecodesWithAndroidNativeLibrary() {
        val compressed = Zstd.compress(expected)
        val crc = CRC32().apply { update(expected) }.value.toInt()
        val fixture = ByteBuffer.allocate(8 + compressed.size + 15).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(0x615A5356).putInt(crc).put(compressed)
            .putInt(crc).putInt(expected.size).putInt(0).put("zsv".toByteArray()).array()
        val actual = ByteArray(expected.size)
        assertEquals(expected.size, VZstdUtil.decompress(fixture, actual, true))
        assertArrayEquals(expected, actual)
    }
}
