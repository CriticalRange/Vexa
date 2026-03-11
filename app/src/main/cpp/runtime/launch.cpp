//
// Created by critical on 10.03.2026.
//
#include <jni.h>
#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/fextl/memory.h>
#include <Common/Config.h>
#include <Common/HostFeatures.h>

#include "launch.h"
#include "../logging/native_log.h"

static fextl::unique_ptr<FEXCore::Context::Context> g_ctx{};

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
    static bool fex_started = false;

    LaunchResult StartRuntime(JNIEnv *env, const Vexa::Common::PreflightPaths &paths) {
        if (fex_started) {
            return {0, "Already started", ""};
        }
        VEXA_LOGI(env, "FEX", "FEXCore begins", "{}");
        FEXCore::Config::Shutdown(); // safe defensive

        LogMan::Throw::InstallHandler(FexAssertHandler);
        LogMan::Msg::InstallHandler(FexMsgHandler);

        // Testing FEX Logman
        FexMsgHandler(LogMan::INFO, "FEX Logman test");
        // Initializing FEX runtime here.
        FEX::Config::LoadConfig(/*ProgramName=*/{}, /*envp=*/nullptr,
                                                FEX::Config::PortableInformation{});
        VEXA_LOGI(env, "FEX", "FEXCore config loaded", "{}");

        FEXCore::Config::Set(FEXCore::Config::CONFIG_SILENTLOG,
                             "0"); // Set silent logs to 0 for dev

        FEXCore::Config::ReloadMetaLayer(); // Apply Config Changes

        // Context creation
        auto hostFeatures = FEX::FetchHostFeatures();
        auto hostFields = Vexa::Log::AddFields({
                                                       Vexa::Log::F("supportsAVX",
                                                                    hostFeatures.SupportsAVX)
                                               });
        VEXA_LOGI(env, "FEX", "Host features fetched", hostFields.c_str());
        g_ctx = FEXCore::Context::Context::CreateNewContext(hostFeatures);
        if (!g_ctx) {
            VEXA_LOGI(env, "FEX", "Context couldn't be created.", "{}");
            return {10, "CreateNewContext failed", ""};
        }
        VEXA_LOGI(env, "FEX", "CreateNewContext OK", "{}");
        fex_started = true;

        return {0, "OK", ""};
    }

    void StopRuntime() {
        if (Vexa::Runtime::fex_started) {
            g_ctx.reset();
            LogMan::Msg::UnInstallHandler();
            LogMan::Throw::UnInstallHandler();
            FEXCore::Config::Shutdown();
            Vexa::Runtime::fex_started = false;
        }
    }
}