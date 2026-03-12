//
// Created by critical on 10.03.2026.
//
#include <jni.h>

#include "../logging/native_log.h"
#include "launch.h"
#include "init/config.h"
#include "init/core.h"
#include "init/logging.h"
#include "init/preflight.h"
#include "init/thread.h"
#include "init/thunk.h"
#include "init/syscalls.h"

namespace Vexa::Runtime {

    static RuntimeState g_state{};

    static void CleanupAll() {
        if (g_state.parentThread) {
            Init::TeardownParentThread(g_state);
        }
        if (g_state.ctx) {
            g_state.ctx->SetSyscallHandler(nullptr);
            g_state.ctx->SetThunkHandler(nullptr);
        }
        g_state.syscallHandler.reset();
        g_state.linuxSyscallHandler = nullptr;
        g_state.thunkHandler.reset();
        g_state.signalDelegator.reset();
        g_state.ctx.reset();
        g_state.parentThread = nullptr;
        g_state.fexStarted = false;
        // Removing logs last to show all the other logs
        Init::UninstallLogHandlers();
        Init::ShutdownConfig();
    }

    LaunchResult StartRuntime(JNIEnv *env, const Vexa::Common::PreflightPaths &paths) {
        if (g_state.fexStarted) {
            return {0, "Already started", ""};
        }
        VEXA_LOGI(env, "FEX", "FEXCore initialized", "{}");

        Init::InstallLogHandlers();

        auto r = Init::SetupConfig(paths);
        if (r.code) {
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

        r = Init::SetupCore(env, paths, g_state);
        if (r.code) {
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
        if (r.code) {
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
        if (r.code) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up Syscall Handler", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupSyscalls OK", "{}");
        r = Init::SetupThreads(g_state);
        if (r.code) {
            const std::string fields = Vexa::Log::AddFields({
                                                                    Vexa::Log::F("code", r.code),
                                                                    Vexa::Log::F("reason", r.reason)
                                                            });
            VEXA_LOGE(env, "FEX", "Failed setting up Thread Handler", fields.c_str());
            CleanupAll();
            return r;
        }
        VEXA_LOGI(env, "FEX", "SetupThreads OK", "{}");

        g_state.fexStarted = true;
        VEXA_LOGI(env, "FEX", "FEX Started, sending OK", "{}");
        return {0, "OK", ""};
    }

    void StopRuntime() {
        CleanupAll();
    }
}