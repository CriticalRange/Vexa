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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "../../logging/native_log.h"
#include "config.h"

namespace Vexa::Runtime::Init {
    namespace {
        bool WriteTextFile(const std::string &path, const std::string &data) {
            std::error_code ec;

            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
            std::ofstream out(path, std::ios::out | std::ios::trunc);
            if (!out.is_open()) return false;
            out << data;
            return out.good();
        }

        void PushUnique(std::vector<std::string> &dst, const std::string &value) {
            for (const auto &existing: dst) {
                if (existing == value) return;
            }
            dst.push_back(value);
        }

        void AddDirProbeOverlays(std::vector<std::string> &dst, const std::string &dir, const std::string &stem,
                                 bool addSo1 = false) {
            if (dir.empty()) return;
            PushUnique(dst, dir + "/" + stem + ".so");
            PushUnique(dst, dir + "/lib" + stem + ".so");
            PushUnique(dst, dir + "/" + stem);
            PushUnique(dst, dir + "/lib" + stem);
            if (addSo1) {
                PushUnique(dst, dir + "/" + stem + ".so.1");
                PushUnique(dst, dir + "/lib" + stem + ".so.1");
            }
        }

        std::string BuildJsonArray(const std::vector<std::string> &items) {
            std::ostringstream out;
            out << "[\n";
            for (size_t i = 0; i < items.size(); ++i) {
                out << "          \"" << items[i] << "\"";
                if (i + 1 != items.size()) out << ",";
                out << "\n";
            }
            out << "        ]";
            return out.str();
        }

        std::string BuildThunkDBJson(const Vexa::Common::Paths &paths) {
            const std::string gameDir = std::filesystem::path(paths.executable).parent_path().string();
            std::string gameDirAlias;
            constexpr const char *userPrefix = "/data/user/0/";
            if (gameDir.rfind(userPrefix, 0) == 0) {
                gameDirAlias = std::string("/data/data/") + gameDir.substr(std::char_traits<char>::length(userPrefix));
            }

            std::vector<std::string> sdl3Overlays = {
                    "@PREFIX_LIB@/libSDL3.so",
                    "/proc/SDL3.so",
                    "/proc/libSDL3.so",
                    "/proc/SDL3",
                    "/proc/libSDL3",
            };
            AddDirProbeOverlays(sdl3Overlays, gameDir, "SDL3");
            AddDirProbeOverlays(sdl3Overlays, gameDirAlias, "SDL3");

            std::vector<std::string> sdl3ImageOverlays = {
                    "@PREFIX_LIB@/libSDL3_image.so",
                    "/proc/SDL3_image.so",
                    "/proc/libSDL3_image.so",
            };
            AddDirProbeOverlays(sdl3ImageOverlays, gameDir, "SDL3_image");
            AddDirProbeOverlays(sdl3ImageOverlays, gameDirAlias, "SDL3_image");

            std::vector<std::string> openalOverlays = {
                    "@PREFIX_LIB@/libopenal.so",
                    "@PREFIX_LIB@/libopenal.so.1",
                    "/proc/openal.so",
                    "/proc/libopenal.so",
                    "/proc/libopenal.so.1",
            };
            AddDirProbeOverlays(openalOverlays, gameDir, "openal", true);
            AddDirProbeOverlays(openalOverlays, gameDirAlias, "openal", true);

            std::ostringstream json;
            json << "{\n"
                 << "  \"DB\": {\n"
                 << "    \"SDL3\": {\n"
                 << "      \"Library\": \"libSDL3-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(sdl3Overlays) << "\n"
                 << "    },\n"
                 << "    \"SDL3_image\": {\n"
                 << "      \"Library\": \"libSDL3_image-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(sdl3ImageOverlays) << "\n"
                 << "    },\n"
                 << "    \"openal\": {\n"
                 << "      \"Library\": \"libopenal-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(openalOverlays) << "\n"
                 << "    }\n"
                 << "  }\n"
                 << "}\n";
            return json.str();
        }

        std::string BuildAppTHunkConfigJson() {
            return R"json(
  {
    "ThunksDB": {
      "SDL3": 1,
      "SDL3_image": 1,
      "openal": 1
    }
  }
  )json";
        }

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

        const std::string cfgRoot = paths.artifactDir + "/fexcfg";
        const std::string thunksDbPath = cfgRoot + "/ThunksDB.json";
        const std::string appCfgPath =
                cfgRoot + "/AppConfig/" + (appConfigName.empty() ? "Unknown" : appConfigName) +
                ".json";

        if (!WriteTextFile(thunksDbPath, BuildThunkDBJson(paths))) {
            VEXA_LOGW(
                    env,
                    "FEX",
                    "Failed writing ThunksDB.json",
                    Vexa::Log::AddFields({
                                                 Vexa::Log::F("path", thunksDbPath)
                                         }).c_str()
            );
        }
        if (!WriteTextFile(appCfgPath, BuildAppTHunkConfigJson())) {
            VEXA_LOGW(
                    env,
                    "FEX",
                    "Failed writing app thunk config",
                    Vexa::Log::AddFields({
                                                 Vexa::Log::F("path", appCfgPath)
                                         }).c_str()
            );
        }

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
        FEXCore::Config::ReloadMetaLayer(); // Apply Meta Config Changes
        FEXCore::Config::Set(FEXCore::Config::CONFIG_ROOTFS, paths.rootfs);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_THUNKHOSTLIBS, paths.thunkHost);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_THUNKGUESTLIBS, paths.thunkGuest);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_APP_FILENAME, appFilename);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_APP_CONFIG_NAME,
                             appConfigName.empty() ? "Unknown" : appConfigName);

        FEXCore::Config::SetConfigDirectory(cfgRoot + "/", false);
        FEXCore::Config::SetConfigDirectory(cfgRoot + "/", true);
        FEXCore::Config::SetConfigFileLocation(cfgRoot + "/Config.json", false);
        FEXCore::Config::SetConfigFileLocation(cfgRoot + "/Config.json", true);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_THUNKCONFIG, appCfgPath);
        FEXCore::Config::Set(FEXCore::Config::CONFIG_SILENTLOG,
                             "0"); // Set silent logs to 0 for dev
        FEXCore::Config::Set(FEXCore::Config::CONFIG_OUTPUTLOG, "stderr");
        const bool silentLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_SILENTLOG);
        const bool outputLogSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_OUTPUTLOG);
        const bool is64bitSet = FEXCore::Config::Exists(FEXCore::Config::CONFIG_IS64BIT_MODE);

        // ReloadMetaLayer can overwrite these, so applying them later.
        FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
        FEXCore::Config::Set(FEXCore::Config::CONFIG_ROOTFS, paths.rootfs);

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Config initialized"};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
