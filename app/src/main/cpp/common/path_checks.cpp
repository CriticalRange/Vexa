//
// Created by critical on 13.03.2026.
//

#include <sys/stat.h>
#include <unistd.h>

#include "path_checks.h"

namespace Vexa::Common {

    bool IsReadableDir(const std::string &path) {
        if (path.empty()) return false;
        struct stat st{};
        if (::stat(path.c_str(), &st) != 0) return false;
        if (!S_ISDIR(st.st_mode)) return false;
        return ::access(path.c_str(), R_OK | X_OK) == 0;
    }

    bool IsExecutable(const std::string &path) {
        if (path.empty()) return false;
        struct stat st{};
        if (::stat(path.c_str(), &st) != 0) return false;
        if (!S_ISREG(st.st_mode)) return false;
        return ::access(path.c_str(), R_OK | X_OK) == 0;
    }

} //namespace Vexa::Common