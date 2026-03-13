//
// Created by critical on 11.03.2026.
//

#include <FEXCore/Utils/LogManager.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#include "../../logging/native_log.h"
#include "logging.h"

namespace {
    int g_fex_log_fd = -1;
    std::mutex g_fex_log_mu;

    void CloseFexLogFile() {
        std::scoped_lock lk(g_fex_log_mu);
        if (g_fex_log_fd >= 0) {
            ::close(g_fex_log_fd);
            g_fex_log_fd = -1;
        }
    }

    bool OpenFexLogFile(const std::string &logPath) {
        std::scoped_lock lk(g_fex_log_mu);
        if (g_fex_log_fd >= 0) {
            ::close(g_fex_log_fd);
            g_fex_log_fd = -1;
        }
        g_fex_log_fd = ::open(logPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        return g_fex_log_fd >= 0;
    }

    void AppendFexLogLine(const char *level, const char *msg) {
        std::scoped_lock lk(g_fex_log_mu);
        if (g_fex_log_fd < 0) return;
        dprintf(g_fex_log_fd, "[%s] %s\n", level ? level : "INFO", msg ? msg : "");
    }

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
        AppendFexLogLine(vexaLevel, prefixed.c_str());
    }

    static void FexAssertHandler(const char *message) {
        std::string prefixed =
                std::string("[LOGMAN ASSERT] ") + (message ? message : "FEX assert handled");
        Vexa::Log::VexaNativeLogRaw("FATAL", "FEX", prefixed.c_str(), "{}");
        AppendFexLogLine("FATAL", prefixed.c_str());
    }
}

namespace Vexa::Runtime::Init {
    bool InitFexFileLogSink(const std::string &logPath) {
        return OpenFexLogFile(logPath);
    }

    void CloseFexFileLogSink() {
        CloseFexLogFile();
    }

    void InstallLogHandlers() {
        LogMan::Msg::InstallHandler(FexMsgHandler);
        LogMan::Throw::InstallHandler(FexAssertHandler);
    }

    void UninstallLogHandlers() {
        LogMan::Msg::UnInstallHandler();
        LogMan::Throw::UnInstallHandler();
        CloseFexLogFile();
    }

    int GetFexFileLogSinkFd() {
        std::scoped_lock lk(g_fex_log_mu);
        return g_fex_log_fd;
    }
}