//
// Created by critical on 11.03.2026.
//

#include <FEXCore/Config/Config.h>
#include <Common/Config.h>

#include "config.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupConfig(const Vexa::Common::PreflightPaths &paths) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();

        FEXCore::Config::Shutdown(); // safe defensive

        // Initializing FEX runtime here.
        FEX::Config::LoadConfig(/*ProgramName=*/fextl::string{programName}, /*envp=*/nullptr,
                                                FEX::Config::PortableInformation{});
        FEXCore::Config::Set(FEXCore::Config::CONFIG_SILENTLOG,
                             "0"); // Set silent logs to 0 for dev
        FEXCore::Config::ReloadMetaLayer(); // Apply Config Changes

        return {0, "OK", ""};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
