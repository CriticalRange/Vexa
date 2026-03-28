//
// Created by critical on 10.03.2026.
//

#include "../../common/path_checks.h"
#include "preflight.h"

namespace Vexa::Runtime {
    namespace {
        bool RootfsHasCompatLib(const std::string &rootfs, const char *soname) {
            static constexpr const char *kLibDirs[] = {
                    "/usr/lib/x86_64-linux-gnu",
                    "/lib/x86_64-linux-gnu",
                    "/lib64",
                    "/lib",
            };

            for (const char *dir: kLibDirs) {
                if (Vexa::Common::IsReadableFile(rootfs + dir + "/" + soname)) {
                    return true;
                }
            }
            return false;
        }
    }

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
        static constexpr const char *kRequiredX11CompatLibs[] = {
                "libX11.so.6",
                "libxcb.so.1",
                "libXau.so.6",
                "libXdmcp.so.6",
        };
        for (const char *soname: kRequiredX11CompatLibs) {
            if (!RootfsHasCompatLib(p.rootfs, soname))
                return Vexa::Common::Result::Failure(
                        Vexa::Common::Code::BadRootfs,
                        Vexa::Common::Phase::Preflight,
                        "RootFS missing X11 compatibility library",
                        soname
                );
        }
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