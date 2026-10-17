#pragma once

#include <string>

namespace Vexa::Runtime {
enum class ExecutableFormat { Elf32, Elf64, Pe32, Pe64, Script, Unsupported };
ExecutableFormat DetectExecutableFormat(const std::string &path);
const char *ExecutableFormatName(ExecutableFormat format);
}
