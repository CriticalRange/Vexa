//
// Created by critical on 11.03.2026.
//

#include <Linux/Utils/ELFContainer.h>
#include <FEXCore/Config/Config.h>
#include <Tools/CommonTools/PortabilityInfo.h>
#include <Common/Config.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#include "../../logging/native_log.h"
#include "config.h"

namespace Vexa::Runtime::Init {
    namespace {
        std::string GetFilename(const std::string &path) {
            const auto pos = path.find_last_of("/\\");
            return (pos == std::string::npos) ? path : path.substr(pos + 1);
        }

        std::string CanonicalOrOriginal(const std::string &path) {
            if (path.empty()) return {};
            char resolved[PATH_MAX]{};
            if (char *rp = ::realpath(path.c_str(), resolved); rp) {
                return std::string(rp);
            }
            return path;
        }
    }

    Vexa::Common::Result
    SetupConfig(JNIEnv *env, const Vexa::Common::Paths &paths, char **envp) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();
        const std::string stderrPath = paths.artifactDir + "/fex_stderr.log";
        const auto portableInfo = FEX::ReadPortabilityInformation();

        const bool fdBacked = paths.execFd != -1;

        const std::string appConfigName = fdBacked
                                          ? "<Anonymous>"
                                          : GetFilename(paths.executable);

        const std::string appFilename = fdBacked
                                        ? "<Anonymous>"
                                        : CanonicalOrOriginal(paths.executable);

        const std::string programNameForConfig = appConfigName.empty() ? "Unknown" : appConfigName;
        VEXA_LOGI(env, "FEX", "Setting up executable names",
                  Vexa::Log::AddFields({
                                               Vexa::Log::F("appConfigName", appConfigName),
                                               Vexa::Log::F("appFilename", appFilename),
                                               Vexa::Log::F("programNameForConfig",
                                                            programNameForConfig)
                                       }).c_str());

        const auto elfType = ELFLoader::ELFContainer::GetELFType(paths.executable.c_str());
        if (elfType == ELFLoader::ELFContainer::TYPE_NONE) {
            return {
                    Vexa::Common::Code::ElfLoaderFailed,
                    Vexa::Common::Phase::Init,
                    "Unsupported ELF type for executable"
            };
        } else if (elfType == ELFLoader::ELFContainer::TYPE_OTHER_ELF) {
            return {
                    Vexa::Common::Code::ElfLoaderFailed,
                    Vexa::Common::Phase::Init,
                    "Executable missing, unreadable, or not an ELF"
            };
        }
        const bool is64 = elfType == ELFLoader::ELFContainer::TYPE_X86_64;


        int fd = ::open(stderrPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            (void) ::dup2(fd, STDERR_FILENO);
            ::close(fd);
        }
        FEXCore::Config::Shutdown(); // safe defensive

        // Initializing FEX runtime here.
        FEX::Config::LoadConfig(
                /*ProgramName=*/fextl::string{programNameForConfig},
                /*envp=*/envp,
                                portableInfo);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_ROOTFS, paths.rootfs);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_THUNKHOSTLIBS, paths.thunkHost);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_THUNKGUESTLIBS, paths.thunkGuest);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_APP_FILENAME, appFilename);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_APP_CONFIG_NAME,
                             appConfigName.empty() ? "Unknown" : appConfigName);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_SILENTLOG,
                             "0"); // Set silent logs to 0 for dev
        FEXCore::Config::Set(FEXCore::Config::CONFIG_OUTPUTLOG, "stderr");
        const bool silentLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_SILENTLOG);
        const bool outputLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_OUTPUTLOG);
        const bool is64bitSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_IS64BIT_MODE);
        if (!silentLogSet) {
            VEXA_LOGW(env, "FEX", "SilentLog config doesn't exist. Expect less logs.", "{}");
        }
        if (!outputLogSet) {
            VEXA_LOGW(env, "FEX", "OutputLog config doesn't exist. Logs will not be filed.",
                      "{}");
        }
        if (!is64bitSet) {
            VEXA_LOGW(env, "FEX", "Is64bit_mode config doesn't exist. Logs will not be filed.",
                      "{}");
        }

        FEXCore::Config::ReloadMetaLayer(); // Apply Config Changes
        // ReloadMetaLayer can overwrite these, so applying them later.
        FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE,
                             is64 ? "1"
                                  : "0");
        FEXCore::Config::Set(FEXCore::Config::CONFIG_ROOTFS, paths.rootfs);

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Config initialized"};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
