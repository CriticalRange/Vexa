//
// Created by critical on 10.03.2026.
//

#include "../../common/path_checks.h"
#include "preflight.h"

namespace Vexa::Runtime {
    Vexa::Common::Result RunPreflight(const Vexa::Common::Paths &p) {
        if (!Vexa::Common::IsReadableDir(p.workingDir))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadWorkingDir,
                    Vexa::Common::Phase::Preflight,
                    "Working directory unreadable"
            );
        if (!Vexa::Common::IsReadableDir(p.rootfs))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadRootfs,
                    Vexa::Common::Phase::Preflight,
                    "RootFS unreadable"
            );
        if (!Vexa::Common::IsReadableDir(p.thunkHost))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadThunkHost,
                    Vexa::Common::Phase::Preflight,
                    "Host Thunk Directory unavailable"
            );
        if (!Vexa::Common::IsReadableDir(p.thunkGuest))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadThunkGuest,
                    Vexa::Common::Phase::Preflight,
                    "Guest Thunk Directory unavailable"
            );
        if (!Vexa::Common::IsExecutable(p.executable))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadExecutable,
                    Vexa::Common::Phase::Preflight,
                    "Executable invalid"
            );
        if (!Vexa::Common::IsReadableDir(p.artifactDir))
            return Vexa::Common::Result::Failure(
                    Vexa::Common::Code::BadArtifactDir,
                    Vexa::Common::Phase::Preflight,
                    "ArtifactDir unreadable"
            );
        return Vexa::Common::Result::Success(Vexa::Common::Phase::Preflight);
    }
} // namespace Vexa::Runtime