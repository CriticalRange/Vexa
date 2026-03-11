#ifndef VEXA_EMULATOR_PREFLIGHT_H
#define VEXA_EMULATOR_PREFLIGHT_H

#pragma once

#include <string>
#include "../../common/preflight_paths.h"

// Preflight codes (Determines whether FEX will run or not
// 0 = OK
// 1 = Working directory missing/unreadable
// 2 = rootfs missing/unreadable
// 3 = Host thunk directory missing/unreadable
// 4 = Guest thunk directory missing/unreadable
// 5 = Executable path missing/unreadable
// 6 = Artifacts directory missing/unreadable
// 7 = libFEXCore load failed
// 8 = JNI/internal error

namespace Vexa::Runtime {
    enum class PreflightCode : int {
        Ok = 0,
        BadWorkingDir = 1,
        BadRootfs = 2,
        BadThunkHost = 3,
        BadThunkGuest = 4,
        BadExecutable = 5,
        BadArtifactDir = 6,
        FexLoadFailed = 7,
        InternalError = 8
    };
    struct PreflightResult {
        PreflightCode code{PreflightCode::Ok};
        std::string reason;
        std::string detail; // This is optional (e.g. dlerror)
    };

    PreflightResult RunPreflight(const Vexa::Common::PreflightPaths &p);
} //namespace Vexa::Runtime

#endif //VEXA_EMULATOR_PREFLIGHT_H
