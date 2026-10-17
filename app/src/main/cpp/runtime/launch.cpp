//
// Created by critical on 10.03.2026.
//
#include <vector>
#include <string>
#include <algorithm>
#include <atomic>

#include "../logging/native_log.h"
#include "../logging/crash_signals.h"
#include "launch.h"
#include "init/config.h"
#include "init/core.h"
#include "init/logging.h"
#include "init/thread.h"
#include "init/thunk.h"
#include "init/syscalls.h"
#include "init/execute.h"
#include "init/client.h"
#include "init/allocator.h"

namespace Vexa::Runtime {

    static Resources g_state{};
    static std::atomic<int> g_guestExitCode{Vexa::Common::ToInt(Vexa::Common::Code::InternalError)};

    int GetGuestExitCode() {
        return g_guestExitCode.load(std::memory_order_relaxed);
    }

    extern "C" __attribute__((visibility("default"))) void Vexa_ReportGuestExit(int status) {
        g_guestExitCode.store(status & 0xff, std::memory_order_relaxed);
    }

    static void CleanupAll() {
        if (g_state.parentThread) {
            Init::TeardownParentThread(g_state);
        }
        Init::TeardownThreadHandlers(g_state);
        // SetSyscallHandler(nullptr) dereferences the handler; the context is destroyed below anyway.
        if (g_state.ctx) {
            g_state.ctx->SetThunkHandler(nullptr);
        }
        g_state.syscallHandler.reset();
        g_state.linuxSyscallHandler = nullptr;
        g_state.thunkHandler.reset();
        g_state.signalDelegator.reset();
        g_state.ctx.reset();
        // FEX objects (including the x32 handler's allocator) must be destroyed
        // before their allocator hooks and reserved address ranges disappear.
        Init::ShutdownAllocator();
        g_state.parentThread = nullptr;
        g_state.fexStarted = false;
        // Removing logs last to show all the other logs
        Init::UninstallLogHandlers();
        Init::ShutdownConfig();
    }

    Vexa::Common::Result
    StartRuntime(JNIEnv *env, const Vexa::Common::Paths &paths,
                 const std::vector<std::string> &launchEnv,
                 const std::vector<std::string> &launchArgs) {
        if (g_state.fexStarted) {
            return {Vexa::Common::Code::AlreadyStarted, Vexa::Common::Phase::Init,
                    "Runtime already started"};
        }
        g_guestExitCode.store(Vexa::Common::ToInt(Vexa::Common::Code::InternalError), std::memory_order_relaxed);
        g_state.fexStarted = true;
        VEXA_LOGI(env, "FEX", "FEXCore initialized", "{}");

        // launchEnv feeds the guest envp only, but host-side thunk libraries
        // (libGL-host) inspect VEXA_HEADLESS via getenv to pick a pbuffer over
        // a SurfaceTexture-backed window and select its color space. Mirror only
        // these explicit host graphics controls
        // into this process's environment. VEXA_REEXEC is also mirrored: the
        // guest execve fallback in FEX reads it with host getenv() to find the
        // trampoline binary, and it must survive into re-exec'd children for
        // grandchild execs.
        const std::vector<std::string> hostControls{
            "VEXA_HEADLESS", "VEXA_HEADLESS_W", "VEXA_HEADLESS_H", "VEXA_GL_SRGB_SURFACE",
            "VEXA_REEXEC", "VEXA_ROOTFS", "VEXA_THUNK_HOST", "VEXA_THUNK_GUEST",
            "VEXA_WORKDIR", "VEXA_ARTIFACTS", "VEXA_COMPAT_SESSION",
            "VEXA_PROTON_PATH", "VEXA_STEAM_CLIENT_PATH", "VEXA_COMPAT_DATA_ROOT",
            "VEXA_PROTON_PYTHON", "VEXA_PROTON_SESSION_SCRIPT", "VEXA_PROTON_DISPLAY_BACKEND",
            "VEXA_PROTON_ROOTFS",
            "VEXA_REAL_X11", "VEXA_X11_SOCKET",
            "VEXA_GRAPHICS_BACKEND", "VEXA_GLX_SWAP_INTERVAL", "VEXA_GL_SHADOW",
            "VEXA_VK_PROFILE", "VEXA_VK_MAILBOX", "VEXA_VK_OPTIMIZE_LOOPS",
        };
        for (const auto &key : hostControls) {
            ::unsetenv(key.c_str());
        }
        for (const auto &entry: launchEnv) {
            const auto eq = entry.find('=');
            if (eq == std::string::npos) continue;
            if (std::find(hostControls.begin(), hostControls.end(), entry.substr(0, eq)) != hostControls.end()) {
                ::setenv(entry.substr(0, eq).c_str(), entry.c_str() + eq + 1, 1);
            }
        }

        Init::InstallLogHandlers();

        auto r = Init::SetupConfig(env, paths, launchEnv, g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up config", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupConfig OK", "{}");

        if (!Init::InitFexFileLogSink(paths.artifactDir + "/fex_logman.log")) {
            VEXA_LOGE(env, "FEX", "InitFexFileLogSink failed", "{}");
        }
        LogMan::Msg::IFmt("[VEXA_CANARY] file-sink-opened");

        r = Init::SetupClient(env, paths);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up FEX Server client", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupClient OK", "{}");
        r = Init::SetupAllocator(env);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up the allocator", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupAllocator OK", "{}");
        // Hand signal ownership to FEX before its thread/core setup installs
        // the delegator thunks. Restoring the early handlers afterwards would
        // overwrite FEX's SIGBUS recovery (and other required signal handlers)
        // with the pre-runtime actions, which are SIG_DFL in standalone loaders.
        // The early alt-stack teardown already preserves a foreign JNI thread's
        // stack; FEX registers its own guest-thread stack during SetupThreads.
        Vexa::Log::UninstallSignalHandlers();
        r = Init::SetupThreadHandlers(g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up thread handlers", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupThreadHandlers OK", "{}");
        r = Init::SetupCore(env, paths, g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed initializing Core", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupCore OK", "{}");
        r = Init::SetupThunks(env, g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up Thunks", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupThunks OK", "{}");
        r = Init::SetupSyscalls(env, g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up Syscall Handler", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupSyscalls OK", "{}");

        Init::TrackClientFDs(env, g_state);
        VEXA_LOGI(env, "FEX", "Track FEX Server FDs OK", "{}");

        Vexa::Log::InstallFexCrashHandler(g_state.signalDelegator.get());
        VEXA_LOGI(env, "FEX", "FEX crash handler installed", "{}");

        r = Init::SetupThreads(g_state);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up Thread Handler", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupThreads OK", "{}");
        r = Init::ExecuteRuntime(env, paths, g_state, launchEnv, launchArgs);
        if (!r.Ok()) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed Executing runtime!", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "ExecuteRuntime OK", "{}");

        CleanupAll();
        VEXA_LOGI(env, "FEX", "FEX Started, sending OK", "{}");
        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Application launched successfully!"};
    }

    static bool g_needsProcessRestart = false;

    void StopRuntime() {
        if (!g_state.fexStarted || g_needsProcessRestart) return;
        CleanupAll();
    }

    void ReleaseAfterGuestExit() {
        // FEX already destroyed the parent thread in the guest exit syscall and its
        // remaining state cannot be torn down safely. Only hand the host signals back
        // to ART; the worker process must be restarted before the next launch.
        g_state.parentThread = nullptr;
        g_state.signalDelegator.reset();
        (void) g_state.syscallHandler.release();
        (void) g_state.thunkHandler.release();
        (void) g_state.stackTracker.release();
        (void) g_state.ctx.release();
        g_needsProcessRestart = true;
    }
}
