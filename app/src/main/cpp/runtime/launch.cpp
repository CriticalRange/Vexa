//
// Created by critical on 10.03.2026.
//

#include "../logging/native_log.h"
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

    static void CleanupAll() {
        if (g_state.parentThread) {
            Init::TeardownParentThread(g_state);
        }
        Init::TeardownThreadHandlers(g_state);
        Init::ShutdownAllocator();
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

    Vexa::Common::Result
    StartRuntime(JNIEnv *env, const Vexa::Common::Paths &paths, char **envp) {
        if (g_state.fexStarted) {
            return {Vexa::Common::Code::AlreadyStarted, Vexa::Common::Phase::Init,
                    "Runtime already started"};
        }
        VEXA_LOGI(env, "FEX", "FEXCore initialized", "{}");

        Init::InstallLogHandlers();

        auto r = Init::SetupConfig(env, paths, envp);
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
        r = Init::ExecuteRuntime(env, paths, g_state);
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

        g_state.fexStarted = true;
        VEXA_LOGI(env, "FEX", "FEX Started, sending OK", "{}");
        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Application launched successfully!"};
    }

    void StopRuntime() {
        CleanupAll();
    }
}