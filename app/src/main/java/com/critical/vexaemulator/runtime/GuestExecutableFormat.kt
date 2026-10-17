package com.critical.vexaemulator.runtime

import java.io.File
import java.io.RandomAccessFile

/** Header-based routing. A filename (including .exe) never selects Proton. */
enum class GuestExecutableFormat { ELF, PE, OTHER }

object GuestExecutableFormats {
    fun detect(file: File): GuestExecutableFormat = runCatching {
        RandomAccessFile(file, "r").use { input ->
            if (input.length() < 4) return@use GuestExecutableFormat.OTHER
            val first = ByteArray(4)
            input.readFully(first)
            if (first.contentEquals(byteArrayOf(0x7f, 0x45, 0x4c, 0x46)))
                return@use GuestExecutableFormat.ELF
            if (input.length() < 64 || first[0] != 0x4d.toByte() || first[1] != 0x5a.toByte())
                return@use GuestExecutableFormat.OTHER
            input.seek(60)
            val offset = readU32(input)
            if (offset < 64 || offset > input.length() - 26) return@use GuestExecutableFormat.OTHER
            input.seek(offset)
            if (input.read() != 0x50 || input.read() != 0x45 || input.read() != 0 || input.read() != 0)
                return@use GuestExecutableFormat.OTHER
            val machine = readU16(input)
            input.skipBytes(14)
            val optionalSize = readU16(input)
            val characteristics = readU16(input)
            if (characteristics and 0x0002 == 0 || characteristics and 0x2000 != 0 ||
                optionalSize > input.length() - offset - 24)
                return@use GuestExecutableFormat.OTHER
            val magic = readU16(input)
            val fixedSize = when {
                machine == 0x14c && magic == 0x10b -> 96
                machine == 0x8664 && magic == 0x20b -> 112
                else -> return@use GuestExecutableFormat.OTHER
            }
            if (optionalSize < fixedSize) return@use GuestExecutableFormat.OTHER
            input.seek(offset + 24 + fixedSize - 4)
            val directories = readU32(input)
            if (directories > (optionalSize - fixedSize) / 8)
                GuestExecutableFormat.OTHER else GuestExecutableFormat.PE
        }
    }.getOrDefault(GuestExecutableFormat.OTHER)

    private fun readU16(input: RandomAccessFile): Int = input.readUnsignedByte() or (input.readUnsignedByte() shl 8)
    private fun readU32(input: RandomAccessFile): Long = readU16(input).toLong() or (readU16(input).toLong() shl 16)
}
