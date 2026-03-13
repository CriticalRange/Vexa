//
// Created by critical on 10.03.2026.
//

#ifndef VEXA_EMULATOR_PATHS_H
#define VEXA_EMULATOR_PATHS_H

#include <string>

namespace Vexa::Common {
    struct Paths {
        std::string executable;
        std::string rootfs;
        std::string thunkHost;
        std::string thunkGuest;
        std::string workingDir;
        std::string artifactDir;

        int execFd{-1}; // -1 here means path-based launch (current behavior for now)
    };
}

#endif //VEXA_EMULATOR_PATHS_H
