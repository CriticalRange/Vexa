#include "../../app/src/main/cpp/runtime/executable_format.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using Vexa::Runtime::DetectExecutableFormat;
using F = Vexa::Runtime::ExecutableFormat;
using Bytes = std::vector<unsigned char>;
static void Put16(Bytes &b, size_t at, unsigned value) {
    b.at(at) = value & 255; b.at(at + 1) = (value >> 8) & 255;
}
static void Put32(Bytes &b, size_t at, unsigned value) {
    Put16(b, at, value & 65535); Put16(b, at + 2, value >> 16);
}
static Bytes Elf(bool wide) {
    Bytes b(wide ? 64 : 52);
    b[0] = 127; b[1] = 'E'; b[2] = 'L'; b[3] = 'F';
    b[4] = wide ? 2 : 1; b[5] = 1; b[6] = 1;
    Put16(b, 16, 2); Put16(b, 18, wide ? 62 : 3); Put32(b, 20, 1);
    Put16(b, wide ? 52 : 40, b.size()); return b;
}
static Bytes Pe(bool wide) {
    Bytes b(64 + 24 + (wide ? 240 : 224));
    b[0] = 'M'; b[1] = 'Z'; Put32(b, 60, 64); b[64] = 'P'; b[65] = 'E';
    Put16(b, 68, wide ? 0x8664 : 0x14c); Put16(b, 84, wide ? 240 : 224);
    Put16(b, 86, 2); Put16(b, 88, wide ? 0x20b : 0x10b);
    Put32(b, 88 + (wide ? 108 : 92), 16); return b;
}
int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("vexa-format-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::error_code e; std::filesystem::remove(p, e); } } cleanup{path};
    int count = 0;
    auto check = [&](const Bytes &bytes, F expected, const char *name) {
        { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()); }
        if (DetectExecutableFormat(path.string()) != expected) throw std::runtime_error(name);
        ++count;
    };
    check({}, F::Unsupported, "empty");
    check({'M', 'Z'}, F::Unsupported, "short DOS");
    check({'#', '!', '/', 'b', 'i', 'n', '/', 's', 'h'}, F::Script, "shebang");
    for (bool wide : {false, true}) {
        auto b = Elf(wide); check(b, wide ? F::Elf64 : F::Elf32, "valid ELF");
        b.resize(b.size() - 1); check(b, F::Unsupported, "truncated ELF");
        b = Elf(wide); b[5] = 2; check(b, F::Unsupported, "big endian ELF");
        b = Elf(wide); b[6] = 0; check(b, F::Unsupported, "invalid ELF ident version");
        b = Elf(wide); Put32(b, 20, 0); check(b, F::Unsupported, "invalid ELF version");
        b = Elf(wide); Put16(b, 18, 183); check(b, F::Unsupported, "ARM ELF");
        b = Elf(wide); Put16(b, 16, 1); check(b, F::Unsupported, "ELF object");
        b = Elf(wide); Put16(b, wide ? 52 : 40, 0); check(b, F::Unsupported, "bad ELF header size");
        b = Elf(wide); Put16(b, 16, 3); check(b, wide ? F::Elf64 : F::Elf32, "ELF PIE");
        b = Pe(wide); check(b, wide ? F::Pe64 : F::Pe32, "valid PE");
        b.pop_back(); check(b, F::Unsupported, "truncated optional header");
        b = Pe(wide); Put32(b, 60, 0xffffffff); check(b, F::Unsupported, "overflow PE offset");
        b = Pe(wide); Put32(b, 60, 2); check(b, F::Unsupported, "PE overlaps DOS header");
        b = Pe(wide); b[66] = 1; check(b, F::Unsupported, "invalid PE signature");
        b = Pe(wide); Put16(b, 86, 0x2002); check(b, F::Unsupported, "DLL");
        b = Pe(wide); Put16(b, 86, 0); check(b, F::Unsupported, "not executable PE");
        b = Pe(wide); Put16(b, 68, wide ? 0x14c : 0x8664); check(b, F::Unsupported, "PE machine mismatch");
        b = Pe(wide); Put16(b, 84, 2); check(b, F::Unsupported, "short declared optional header");
        b = Pe(wide); Put32(b, 88 + (wide ? 108 : 92), 0xffffffff); check(b, F::Unsupported, "directory bounds");
    }
    if (DetectExecutableFormat(path.string() + ".missing") != F::Unsupported) return 1;
    std::cout << "EXECUTABLE_FORMAT_PASS cases=" << count + 1 << '\n';
}
