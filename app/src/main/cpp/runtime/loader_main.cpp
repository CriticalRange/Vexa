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

#include "../common/paths.h"
#include "../logging/native_log.h"
#include "../logging/crash_signals.h"
#include "init/preflight.h"
#include "launch.h"

extern "C" __attribute__((visibility("default")))
int VexaLoaderMain(int argc, char **argv, char **envp) {
    // No JVM exists in this process; force the liblog-only path before any
    // logging, exactly as the FEX-managed runtime thread does.
    Vexa::Log::MarkFexManagedThread();
    Vexa::Log::InstallSignalHandlers();

    const char *rootfs = getenv("VEXA_ROOTFS");
    const char *thunkHost = getenv("VEXA_THUNK_HOST");
    const char *thunkGuest = getenv("VEXA_THUNK_GUEST");
    const char *workDir = getenv("VEXA_WORKDIR");
    const char *artifacts = getenv("VEXA_ARTIFACTS");

    if (argc < 2 || !rootfs || !thunkHost || !thunkGuest || !workDir || !artifacts) {
        Vexa::Log::VexaNativeLogRaw("ERROR", "BOOT",
                                    "vexaloader: missing target or VEXA_* path env",
                                    "{}");
        return 126;
    }

    Vexa::Common::Paths paths{
            argv[1],
            rootfs,
            thunkHost,
            thunkGuest,
            workDir,
            artifacts,
    };

    std::vector<std::string> launchEnv;
    for (char **e = envp; *e; ++e) {
        launchEnv.emplace_back(*e);
    }
    std::vector<std::string> launchArgs;
    for (int i = 2; i < argc; ++i) {
        launchArgs.emplace_back(argv[i]);
    }

    Vexa::Log::VexaNativeLogRaw("INFO", "BOOT", "vexaloader re-exec",
                                Vexa::Log::AddFields({Vexa::Log::F("target", paths.executable)}).c_str());

    auto preflight = Vexa::Runtime::RunPreflight(paths);
    if (!preflight.Ok()) {
        Vexa::Log::VexaNativeLogRaw("ERROR", "BOOT", "vexaloader preflight failed",
                                    Vexa::Log::AddFields({Vexa::Log::F("reason", preflight.reason),
                                                          Vexa::Log::F("detail", preflight.detail)}).c_str());
        return Vexa::Common::ToInt(preflight.code);
    }

    auto launch = Vexa::Runtime::StartRuntime(nullptr, paths, launchEnv, launchArgs);
    if (!launch.Ok()) {
        Vexa::Log::VexaNativeLogRaw("ERROR", "FEX", "vexaloader launch failed",
                                    Vexa::Log::AddFields({Vexa::Log::F("reason", launch.reason),
                                                          Vexa::Log::F("detail", launch.detail)}).c_str());
        return Vexa::Common::ToInt(launch.code);
    }
    return 0;
}
