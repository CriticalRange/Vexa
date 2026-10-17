package com.critical.vexaemulator

import com.critical.vexaemulator.runtime.GuestExecutableFormat
import com.critical.vexaemulator.runtime.GuestExecutableFormats
import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File
import java.nio.file.Files

class GuestExecutableFormatsTest {
    @Test fun `extension does not decide launch backend`() {
        val directory = Files.createTempDirectory("vexa-format-test").toFile()
        try {
            val elfNamedExe = File(directory, "native.exe")
            elfNamedExe.writeBytes(byteArrayOf(0x7f, 0x45, 0x4c, 0x46, 2, 1))
            assertEquals(GuestExecutableFormat.ELF, GuestExecutableFormats.detect(elfNamedExe))
            val mzOnly = File(directory, "game.exe")
            mzOnly.writeBytes(ByteArray(256).also { it[0] = 0x4d; it[1] = 0x5a })
            assertEquals(GuestExecutableFormat.OTHER, GuestExecutableFormats.detect(mzOnly))
        } finally {
            directory.deleteRecursively()
        }
    }

    @Test fun `valid PE routes to Proton while DLL and truncated PE do not`() {
        val directory = Files.createTempDirectory("vexa-pe-test").toFile()
        try {
            val header = ByteArray(256)
            header[0] = 0x4d; header[1] = 0x5a; header[60] = 64
            header[64] = 0x50; header[65] = 0x45
            header[68] = 0x64; header[69] = 0x86.toByte() // AMD64
            header[84] = 112 // optional header size
            header[86] = 0x02 // executable image
            header[88] = 0x0b; header[89] = 0x02 // PE32+
            val executable = File(directory, "game.bin")
            executable.writeBytes(header)
            assertEquals(GuestExecutableFormat.PE, GuestExecutableFormats.detect(executable))
            header[86] = 0x02; header[87] = 0x20 // DLL characteristic
            executable.writeBytes(header)
            assertEquals(GuestExecutableFormat.OTHER, GuestExecutableFormats.detect(executable))
            executable.writeBytes(header.copyOf(100))
            assertEquals(GuestExecutableFormat.OTHER, GuestExecutableFormats.detect(executable))
        } finally {
            directory.deleteRecursively()
        }
    }
}
