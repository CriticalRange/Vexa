//
// Created by critical on 11.03.2026.
//

#include <FEXCore/Utils/LogManager.h>

#include "../../logging/native_log.h"
#include "logging.h"

namespace {
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
}

namespace Vexa::Runtime::Init {
    void InstallLogHandlers() {
        LogMan::Msg::InstallHandler(FexMsgHandler);
        LogMan::Throw::InstallHandler(FexAssertHandler);
    }

    void UninstallLogHandlers() {
        LogMan::Msg::UnInstallHandler();
        LogMan::Throw::UnInstallHandler();
    }
}