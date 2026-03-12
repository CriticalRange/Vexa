//
// Created by critical on 11.03.2026.
//

#include <FEXCore/Config/Config.h>
#include <Common/Config.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../logging/native_log.h"
#include "config.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupConfig(JNIEnv *env, const Vexa::Common::Paths &paths) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();
        const std::string stderrPath = paths.artifactDir + "/fex_stderr.log";
        int fd = ::open(stderrPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (fd >= 0) {
            (void) ::dup2(fd, STDERR_FILENO);
            ::close(fd);
        }
        FEXCore::Config::Shutdown(); // safe defensive

        // Initializing FEX runtime here.
        FEX::Config::LoadConfig(/*ProgramName=*/fextl::string{programName}, /*envp=*/nullptr,
                                                FEX::Config::PortableInformation{});
        FEXCore::Config::Set(FEXCore::Config::CONFIG_SILENTLOG,
                             "0"); // Set silent logs to 0 for dev
        FEXCore::Config::Set(FEXCore::Config::CONFIG_OUTPUTLOG, "stderr");
        const bool silentLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_SILENTLOG);
        const bool outputLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_OUTPUTLOG);
        if (!silentLogSet) {
            VEXA_LOGW(env, "FEX", "SilentLog config doesn't exist. Expect less logs.", "{}");
        }
        if (!outputLogSet) {
            VEXA_LOGW(env, "FEX", "OutputLog config doesn't exist. Logs will not be filed.",
                      "{}");
        }
        // TODO: Attach this to Vexa Logger
        FEXCore::Config::ReloadMetaLayer(); // Apply Config Changes

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Config initialized"};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
