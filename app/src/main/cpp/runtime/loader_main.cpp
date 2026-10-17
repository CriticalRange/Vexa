//
// Entry point for the re-exec trampoline process. A guest execve() of an
// x86_64 ELF cannot run /proc/self/exe (app_process is not a FEX loader), so
// FEX instead execs the VEXA_REEXEC trampoline binary — a tiny aarch64
// executable packaged in the app's native library dir — which links this
// library and lands here. Environment carries the launch paths (VEXA_ROOTFS
// etc.); argv is [vexa-loader, <target>, <target args...>].
//

#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <unistd.h>

#include "../common/paths.h"
#include "../logging/native_log.h"
#include "../logging/crash_signals.h"
#include "init/preflight.h"
#include "launch.h"
#include "compat_launch.h"

extern "C" __attribute__((visibility("default")))
int VexaLoaderMain(int argc, char **argv, char **envp) {
    Vexa::Runtime::SetStandaloneLoader();
    // No JVM exists in this process; force the liblog-only path before any
    // logging, exactly as the FEX-managed runtime thread does.
    Vexa::Log::MarkFexManagedThread();
    Vexa::Log::InstallSignalHandlers();

    const char *rootfs = getenv("VEXA_ROOTFS");
    const char *thunkHost = getenv("VEXA_THUNK_HOST");
    const char *thunkGuest = getenv("VEXA_THUNK_GUEST");
    const char *workDir = getenv("VEXA_WORKDIR");
    const char *artifacts = getenv("VEXA_ARTIFACTS");

    const bool bootstrap = argc > 2 && std::strcmp(argv[1], "--vexa-start") == 0;
    const int targetIndex = bootstrap ? 2 : 1;
    if (argc <= targetIndex || !rootfs || !thunkHost || !thunkGuest || !workDir || !artifacts) {
        Vexa::Log::VexaNativeLogRaw("ERROR", "BOOT",
                                    "vexaloader: missing target or VEXA_* path env",
                                    "{}");
        return 126;
    }

    // The first spawned orchestrator uses the requested working directory.
    // Every guest exec thereafter preserves its actual inherited cwd.
    char cwd[4096];
    if (bootstrap && chdir(workDir) != 0) return 126;
    if (!getcwd(cwd, sizeof(cwd))) return 126;
    // Separate logs/configs for each Wine helper; never truncate its parent's logs.
    const std::string childArtifacts = std::string(artifacts) + "/processes/" + std::to_string(getpid());
    std::error_code ec;
    std::filesystem::create_directories(childArtifacts, ec);
    if (ec) return 126;
    Vexa::Common::Paths paths{
            argv[targetIndex],
            rootfs,
            thunkHost,
            thunkGuest,
            cwd,
            childArtifacts,
    };

    std::vector<std::string> launchEnv;
    for (char **e = envp; *e; ++e) {
        const std::string entry = *e;
        if (!std::string_view(entry).starts_with("VEXA_GUEST_LD_PRELOAD=") &&
            !std::string_view(entry).starts_with("VEXA_GUEST_LD_LIBRARY_PATH="))
            launchEnv.emplace_back(entry);
    }
    for (const char *key : {"LD_PRELOAD", "LD_LIBRARY_PATH"}) {
        const auto transfer = std::string("VEXA_GUEST_") + key;
        if (const auto *value = getenv(transfer.c_str())) {
            launchEnv.emplace_back(std::string(key) + "=" + value);
            unsetenv(transfer.c_str());
        }
    }
    std::vector<std::string> launchArgs;
    for (int i = targetIndex + 1; i < argc; ++i) {
        launchArgs.emplace_back(argv[i]);
    }

    Vexa::Log::VexaNativeLogRaw("INFO", "BOOT", "vexaloader re-exec",
                                Vexa::Log::AddFields({Vexa::Log::F("target", paths.executable)}).c_str());

    auto launch = Vexa::Runtime::LaunchGuest(nullptr, paths, launchEnv, launchArgs);
    if (!launch.Ok()) {
        Vexa::Log::VexaNativeLogRaw("ERROR", "FEX", "vexaloader launch failed",
                                    Vexa::Log::AddFields({Vexa::Log::F("reason", launch.reason),
                                                          Vexa::Log::F("detail", launch.detail)}).c_str());
        return Vexa::Common::ToInt(launch.code);
    }
    return 0;
}
