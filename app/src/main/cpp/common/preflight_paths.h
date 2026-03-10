//
// Created by critical on 10.03.2026.
//

#ifndef VEXA_EMULATOR_PREFLIGHT_PATHS_H
#define VEXA_EMULATOR_PREFLIGHT_PATHS_H

#pragma once

#include <string>

namespace Vexa::Common {
    struct PreflightPaths {
        std::string executable;
        std::string rootfs;
        std::string thunkHost;
        std::string thunkGuest;
        std::string workingDir;
        std::string artifactDir;
    };
}

#endif //VEXA_EMULATOR_PREFLIGHT_PATHS_H
