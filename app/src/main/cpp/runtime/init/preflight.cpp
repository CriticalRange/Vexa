//
// Created by critical on 10.03.2026.
//
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>

#include "preflight.h"

static bool IsReadableDir(const char *path) {
    if (!path) return false;
    struct stat st{};
    if (stat(path, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return false;
    return access(path, R_OK | X_OK) == 0;
}

static bool IsExecutable(const char *path) {
    if (!path) return false;
    struct stat st{};
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return access(path, R_OK | X_OK) == 0;
}

namespace Vexa::Runtime {
    PreflightResult RunPreflight(const Vexa::Common::PreflightPaths &p) {
        if (!IsReadableDir(p.workingDir.c_str()))
            return {
                    PreflightCode::BadWorkingDir, "Working dir unreadable", ""
            };
        if (!IsReadableDir(p.rootfs.c_str()))
            return {
                    PreflightCode::BadRootfs, "Rootfs unreadable", ""
            };
        if (!IsReadableDir(p.thunkHost.c_str()))
            return {
                    PreflightCode::BadThunkHost, "Host thunk dir unreadable", ""
            };
        if (!IsReadableDir(p.thunkGuest.c_str()))
            return {
                    PreflightCode::BadThunkGuest, "Guest thunk dir unreadable", ""
            };
        if (!IsExecutable(p.executable.c_str()))
            return {
                    PreflightCode::BadExecutable, "Executable invalid", ""
            };
        if (!IsReadableDir(p.artifactDir.c_str()))
            return {
                    PreflightCode::BadArtifactDir, "Artifact dir unreadable", ""
            };
        return {PreflightCode::Ok, "OK", ""};
    }
} // namespace Vexa::Runtime