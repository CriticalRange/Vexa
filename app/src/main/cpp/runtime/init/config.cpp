//
// Created by critical on 11.03.2026.
//

#include <FEXCore/Config/Config.h>
#include <Common/Config.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"


namespace Vexa::Runtime::Init {
    LaunchResult SetupConfig(const Vexa::Common::PreflightPaths &paths) {
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
        // TODO: Attach this to Vexa Logger
        FEXCore::Config::ReloadMetaLayer(); // Apply Config Changes

        return {0, "OK", ""};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
