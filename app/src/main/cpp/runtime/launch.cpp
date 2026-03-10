//
// Created by critical on 10.03.2026.
//
#include <jni.h>
#include <dlfcn.h>
#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/LogManager.h>

#include "launch.h"
#include "../logging/native_log.h"

static void FexMsgHandler(LogMan::DebugLevels level, const char *message) {
    const char *vexaLevel = "INFO";
    switch (level) {
        case LogMan::ASSERT:
            vexaLevel = "FATAL";
            break;
        case LogMan::ERROR:
            vexaLevel = "ERROR";
            break;
        case LogMan::DEBUG:
            vexaLevel = "DEBUG";
            break;
        case LogMan::INFO:
            vexaLevel = "INFO";
            break;
        default:
            break;
    }
    std::string prefixed = std::string("[LOGMAN] ") + (message ? message : "");
    Vexa::Log::VexaNativeLogRaw(vexaLevel, "FEX", prefixed.c_str(), "{}");
}

static void FexAssertHandler(const char *message) {
    std::string prefixed =
            std::string("[LOGMAN ASSERT] ") + (message ? message : "FEX assert handled");
    Vexa::Log::VexaNativeLogRaw("FATAL", "FEX", prefixed.c_str(), "{}");
}

namespace Vexa::Runtime {
    static void *g_fex_handle = nullptr;

    LaunchResult StartRuntime(JNIEnv *env, const Vexa::Common::PreflightPaths &paths) {

        if (g_fex_handle) {
            return {0, "Already started", ""};
        }

        VEXA_LOGI(env, "FEX", "dlopen libFEXCore begins", "{}");
        g_fex_handle = dlopen("libFEXCore.so", RTLD_NOW);
        if (!g_fex_handle) {
            const char *err = dlerror();
            return {7, "Failed to load libFEXCore.so", err ? err : "unknown"};
        }
        LogMan::Throw::InstallHandler(FexAssertHandler);
        LogMan::Msg::InstallHandler(FexMsgHandler);

        // Testing FEX Logman
        FexMsgHandler(LogMan::INFO, "FEX Logman test");
        // Initializing FEX runtime here.
        FEXCore::Config::Initialize();
        VEXA_LOGI(env, "FEX", "libFEXCore initialized", "{}");
        FEXCore::Config::Load();
        VEXA_LOGI(env, "FEX", "FEXCore config loaded", "{}");

        return {0, "OK", ""};
    }

    void StopRuntime() {
        if (g_fex_handle) {
            LogMan::Msg::UnInstallHandler();
            LogMan::Throw::UnInstallHandler();
            dlclose(g_fex_handle);
            g_fex_handle = nullptr;
        }
    }
}