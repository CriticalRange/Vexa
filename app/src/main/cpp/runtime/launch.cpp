//
// Created by critical on 10.03.2026.
//
#include <jni.h>

#include "../logging/native_log.h"
#include "launch.h"
#include "init/config_init.h"
#include "init/core_init.h"
#include "init/logging_init.h"
#include "init/preflight.h"
#include "init/thread_init.h"

namespace Vexa::Runtime {

    static RuntimeState g_state{};

    static void CleanupAll() {
        if (g_state.ctx && g_state.parentThread) {

            g_state.ctx->DestroyThread(g_state.parentThread);
            g_state.parentThread = nullptr;
        }
        g_state.ctx.reset();
        g_state.signalDelegator.reset();
        Init::UninstallLogHandlers();
        Init::ShutdownConfig();
        g_state.fexStarted = false;
    }

    LaunchResult StartRuntime(JNIEnv *env, const Vexa::Common::PreflightPaths &paths) {
        if (g_state.fexStarted) {
            return {0, "Already started", ""};
        }
        VEXA_LOGI(env, "FEX", "FEXCore begins", "{}");

        Init::InstallLogHandlers();

        auto r = Init::SetupConfig(paths);
        if (r.code) {
            CleanupAll();
            return r;
        }
        r = Init::SetupCore(env, paths, g_state);
        if (r.code) {
            CleanupAll();
            return r;
        }
        r = Init::SetupThreads(env, g_state);
        if (r.code) {
            CleanupAll();
            return r;
        }

        g_state.fexStarted = true;
        return {0, "OK", ""};
    }

    void StopRuntime() {
        CleanupAll();
    }
}