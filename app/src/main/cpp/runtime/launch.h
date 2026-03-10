//
// Created by critical on 10.03.2026.
//

#ifndef VEXA_EMULATOR_LAUNCH_H
#define VEXA_EMULATOR_LAUNCH_H

#pragma once

#include "../common/preflight_paths.h"

namespace Vexa::Runtime {
    struct LaunchResult {
        int code;

        std::string reason;
        std::string detail;
    };

    LaunchResult StartRuntime(const Vexa::Common::PreflightPaths &paths);

    void StopRuntime();
}

#endif //VEXA_EMULATOR_LAUNCH_H
