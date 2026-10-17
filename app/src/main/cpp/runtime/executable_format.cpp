#include "executable_format.h"

#include <array>
#include <cstdint>
#include <fstream>

namespace Vexa::Runtime {
namespace {
uint16_t U16(const unsigned char *p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t U32(const unsigned char *p) {
    return U16(p) | (uint32_t(U16(p + 2)) << 16);
}
bool Read(std::ifstream &file, uint64_t offset, unsigned char *out, size_t size) {
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset));
    return file.good() && bool(file.read(reinterpret_cast<char *>(out), size));
}
}

ExecutableFormat DetectExecutableFormat(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return ExecutableFormat::Unsupported;
    const auto end = file.tellg();
    if (end < 2) return ExecutableFormat::Unsupported;
    const uint64_t length = static_cast<uint64_t>(end);
    std::array<unsigned char, 64> header{};
    const auto initialSize = static_cast<size_t>(length < header.size() ? length : header.size());
    if (!Read(file, 0, header.data(), initialSize)) return ExecutableFormat::Unsupported;
    if (header[0] == '#' && header[1] == '!') return ExecutableFormat::Script;
    if (length >= 4 && header[0] == 0x7f && header[1] == 'E' &&
        header[2] == 'L' && header[3] == 'F') {
        const bool elf32 = header[4] == 1;
        const bool elf64 = header[4] == 2;
        const size_t headerSize = elf32 ? 52 : 64;
        if ((!elf32 && !elf64) || length < headerSize || header[5] != 1 ||
            header[6] != 1 || U32(header.data() + 20) != 1 ||
            (U16(header.data() + 16) != 2 && U16(header.data() + 16) != 3) ||
            U16(header.data() + 18) != (elf32 ? 3 : 62) ||
            U16(header.data() + (elf32 ? 40 : 52)) != headerSize)
            return ExecutableFormat::Unsupported;
        return elf32 ? ExecutableFormat::Elf32 : ExecutableFormat::Elf64;
    }
    if (length < 64 || header[0] != 'M' || header[1] != 'Z')
        return ExecutableFormat::Unsupported;
    const uint64_t peOffset = U32(header.data() + 60);
    // A PE signature and complete COFF header must follow the DOS header.
    if (peOffset < 64 || peOffset > length || length - peOffset < 24)
        return ExecutableFormat::Unsupported;
    std::array<unsigned char, 24> coff{};
    if (!Read(file, peOffset, coff.data(), coff.size()) ||
        coff[0] != 'P' || coff[1] != 'E' || coff[2] || coff[3])
        return ExecutableFormat::Unsupported;
    const auto machine = U16(coff.data() + 4);
    const auto optionalSize = U16(coff.data() + 20);
    const auto characteristics = U16(coff.data() + 22);
    if (!(characteristics & 0x0002) || (characteristics & 0x2000) ||
        optionalSize > length - peOffset - 24 || optionalSize < 2)
        return ExecutableFormat::Unsupported;
    std::array<unsigned char, 112> optional{};
    if (!Read(file, peOffset + 24, optional.data(), 2)) return ExecutableFormat::Unsupported;
    const auto magic = U16(optional.data());
    const bool pe32 = magic == 0x10b && machine == 0x14c;
    const bool pe64 = magic == 0x20b && machine == 0x8664;
    const size_t fixedSize = pe32 ? 96 : 112;
    if ((!pe32 && !pe64) || optionalSize < fixedSize ||
        !Read(file, peOffset + 24, optional.data(), fixedSize))
        return ExecutableFormat::Unsupported;
    // The declared data directories must fit inside SizeOfOptionalHeader.
    const auto directories = U32(optional.data() + (pe32 ? 92 : 108));
    if (directories > (optionalSize - fixedSize) / 8)
        return ExecutableFormat::Unsupported;
    return pe32 ? ExecutableFormat::Pe32 : ExecutableFormat::Pe64;
}

const char *ExecutableFormatName(ExecutableFormat format) {
    switch (format) {
        case ExecutableFormat::Elf32: return "ELF32";
        case ExecutableFormat::Elf64: return "ELF64";
        case ExecutableFormat::Pe32: return "PE32";
        case ExecutableFormat::Pe64: return "PE64";
        case ExecutableFormat::Script: return "script";
        case ExecutableFormat::Unsupported: return "unsupported";
    }
    return "unsupported";
}
}
