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
#include "../../logging/crash_signals.h"
#include "../surface_bridge.h"
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

        void AddDirProbeOverlays(std::vector<std::string> &dst, const std::string &dir,
                                 const std::string &stem,
                                 bool addSo1 = false,
                                 bool addSo0 = false) {
            if (dir.empty()) return;
            PushUnique(dst, dir + "/" + stem + ".so");
            PushUnique(dst, dir + "/lib" + stem + ".so");
            PushUnique(dst, dir + "/" + stem);
            PushUnique(dst, dir + "/lib" + stem);
            if (addSo1) {
                PushUnique(dst, dir + "/" + stem + ".so.1");
                PushUnique(dst, dir + "/lib" + stem + ".so.1");
            }
            if (addSo0) {
                PushUnique(dst, dir + "/" + stem + ".so.0");
                PushUnique(dst, dir + "/lib" + stem + ".so.0");
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
            const std::string gameDir = std::filesystem::path(
                    paths.executable).parent_path().string();
            std::string gameDirAlias;
            constexpr const char *userPrefix = "/data/user/0/";
            if (gameDir.rfind(userPrefix, 0) == 0) {
                gameDirAlias = std::string("/data/data/") +
                               gameDir.substr(std::char_traits<char>::length(userPrefix));
            }

            std::vector<std::string> sdl3Overlays = {
                    "@PREFIX_LIB@/libSDL3.so",
                    "/proc/SDL3.so",
                    "/proc/libSDL3.so",
                    "/proc/SDL3",
                    "/proc/libSDL3",
            };
            AddDirProbeOverlays(sdl3Overlays, gameDir, "SDL3", false, true);
            AddDirProbeOverlays(sdl3Overlays, gameDirAlias, "SDL3", false, true);
            AddDirProbeOverlays(sdl3Overlays, gameDir + "/lib64", "SDL3", false, true);
            AddDirProbeOverlays(sdl3Overlays, gameDirAlias + "/lib64", "SDL3", false, true);

            std::vector<std::string> sdl3ImageOverlays = {
                    "@PREFIX_LIB@/libSDL3_image.so",
                    "/proc/SDL3_image.so",
                    "/proc/libSDL3_image.so",
            };
            AddDirProbeOverlays(sdl3ImageOverlays, gameDir, "SDL3_image");
            AddDirProbeOverlays(sdl3ImageOverlays, gameDirAlias, "SDL3_image");

            // SDL2's real SONAME is libSDL2-2.0.so.0, so probe both the "SDL2"
            // and "SDL2-2.0" stems (stem "SDL2" alone only yields libSDL2.so.0).
            std::vector<std::string> sdl2Overlays = {
                    "@PREFIX_LIB@/libSDL2-2.0.so.0",
                    "@PREFIX_LIB@/libSDL2.so",
                    "/proc/libSDL2-2.0.so.0",
                    "/proc/libSDL2.so",
                    "/proc/SDL2",
                    "/proc/libSDL2",
            };
            AddDirProbeOverlays(sdl2Overlays, gameDir, "SDL2", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDirAlias, "SDL2", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDir + "/lib64", "SDL2", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDirAlias + "/lib64", "SDL2", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDir, "SDL2-2.0", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDirAlias, "SDL2-2.0", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDir + "/lib64", "SDL2-2.0", false, true);
            AddDirProbeOverlays(sdl2Overlays, gameDirAlias + "/lib64", "SDL2-2.0", false, true);

            std::vector<std::string> openalOverlays = {
                    "@PREFIX_LIB@/libopenal.so",
                    "@PREFIX_LIB@/libopenal.so.1",
                    "/usr/lib/i386-linux-gnu/libopenal.so",
                    "/usr/lib/i386-linux-gnu/libopenal.so.1",
                    "/proc/openal.so",
                    "/proc/libopenal.so",
                    "/proc/libopenal.so.1",
            };
            AddDirProbeOverlays(openalOverlays, gameDir, "openal", true);
            AddDirProbeOverlays(openalOverlays, gameDirAlias, "openal", true);

            std::vector<std::string> glOverlays = {
                    "@PREFIX_LIB@/libGL.so",
                    "@PREFIX_LIB@/libGL.so.1",
                    "@PREFIX_LIB@/libGL.so.1.2.0",
                    "@PREFIX_LIB@/libGL.so.1.7.0",
                    "/usr/lib/i386-linux-gnu/libGL.so",
                    "/usr/lib/i386-linux-gnu/libGL.so.1",
                    "/usr/lib/i386-linux-gnu/libGL.so.1.2.0",
                    "/usr/lib/i386-linux-gnu/libGL.so.1.7.0",
                    "/proc/libGL.so",
                    "/proc/libGL.so.1",
            };
            AddDirProbeOverlays(glOverlays, gameDir, "GL");
            AddDirProbeOverlays(glOverlays, gameDirAlias, "GL");

            std::vector<std::string> eglOverlays = {
                    "@PREFIX_LIB@/libEGL.so",
                    "@PREFIX_LIB@/libEGL.so.1",
                    "/usr/lib/i386-linux-gnu/libEGL.so",
                    "/usr/lib/i386-linux-gnu/libEGL.so.1",
                    "/proc/libEGL.so",
                    "/proc/libEGL.so.1",
            };
            AddDirProbeOverlays(eglOverlays, gameDir, "EGL");
            AddDirProbeOverlays(eglOverlays, gameDirAlias, "EGL");

            std::vector<std::string> glesOverlays = {
                    "@PREFIX_LIB@/libGLESv2.so",
                    "@PREFIX_LIB@/libGLESv2.so.2",
                    "/usr/lib/i386-linux-gnu/libGLESv2.so",
                    "/usr/lib/i386-linux-gnu/libGLESv2.so.2",
                    "/proc/libGLESv2.so",
                    "/proc/libGLESv2.so.2",
            };
            AddDirProbeOverlays(glesOverlays, gameDir, "GLESv2");
            AddDirProbeOverlays(glesOverlays, gameDirAlias, "GLESv2");

            std::vector<std::string> vulkanOverlays = {
                    "@PREFIX_LIB@/libvulkan.so",
                    "@PREFIX_LIB@/libvulkan.so.1",
                    "/usr/lib/i386-linux-gnu/libvulkan.so",
                    "/usr/lib/i386-linux-gnu/libvulkan.so.1",
                    "/proc/libvulkan.so",
                    "/proc/libvulkan.so.1",
            };
            AddDirProbeOverlays(vulkanOverlays, gameDir, "vulkan", true);
            AddDirProbeOverlays(vulkanOverlays, gameDirAlias, "vulkan", true);

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
                 << "    \"SDL2\": {\n"
                 << "      \"Library\": \"libSDL2-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(sdl2Overlays) << "\n"
                 << "    },\n"
                 << "    \"openal\": {\n"
                 << "      \"Library\": \"libopenal-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(openalOverlays) << "\n"
                 << "    },\n"
                 << "    \"GL\": {\n"
                 << "      \"Library\": \"libGL-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(glOverlays) << "\n"
                 << "    },\n"
                 << "    \"EGL\": {\n"
                 << "      \"Library\": \"libEGL-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(eglOverlays) << "\n"
                 << "    },\n"
                 << "    \"GLESv2\": {\n"
                 << "      \"Library\": \"libGLESv2-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(glesOverlays) << "\n"
                 << "    },\n"
                 << "    \"Vulkan\": {\n"
                 << "      \"Library\": \"libvulkan-guest.so\",\n"
                 << "      \"Overlay\": " << BuildJsonArray(vulkanOverlays) << "\n"
                 << "    }\n"
                 << "  }\n"
                 << "}\n";
            return json.str();
        }

        int ThunkGuestPresent(const Vexa::Common::Paths &paths, const char *fileName, bool is64) {
            std::error_code ec;
            const auto directory = paths.thunkGuest + (is64 ? "" : "_32");
            const auto full = std::filesystem::path(directory) / fileName;
            const auto expected = is64 ? ELFLoader::ELFContainer::TYPE_X86_64
                                       : ELFLoader::ELFContainer::TYPE_X86_32;
            if (ELFLoader::ELFContainer::GetELFType(full.string().c_str()) != expected) return 0;
            std::string hostName = fileName;
            const auto pos = hostName.find("-guest.so");
            if (pos == std::string::npos) return 0;
            hostName.replace(pos, 9, "-host.so");
            const auto host = std::filesystem::path(paths.thunkHost + (is64 ? "" : "_32")) / hostName;
            return std::filesystem::is_regular_file(host, ec) ? 1 : 0;
        }

        std::string BuildAppTHunkConfigJson(const Vexa::Common::Paths &paths, bool is64) {
            std::ostringstream json;
            json << "{\n"
                 << "  \"ThunksDB\": {\n"
                 << "    \"SDL3\": " << ThunkGuestPresent(paths, "libSDL3-guest.so", is64) << ",\n"
                 << "    \"SDL3_image\": " << ThunkGuestPresent(paths, "libSDL3_image-guest.so", is64) << ",\n"
                 << "    \"SDL2\": " << ThunkGuestPresent(paths, "libSDL2-guest.so", is64) << ",\n"
                 << "    \"openal\": " << ThunkGuestPresent(paths, "libopenal-guest.so", is64) << ",\n"
                 << "    \"GL\": " << ThunkGuestPresent(paths, "libGL-guest.so", is64) << ",\n"
                 << "    \"EGL\": " << ThunkGuestPresent(paths, "libEGL-guest.so", is64) << ",\n"
                 << "    \"GLESv2\": " << ThunkGuestPresent(paths, "libGLESv2-guest.so", is64) << ",\n"
                 << "    \"Vulkan\": " << ThunkGuestPresent(paths, "libvulkan-guest.so", is64) << "\n"
                 << "  }\n"
                 << "}\n";
            return json.str();
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
    SetupConfig(JNIEnv *env, const Vexa::Common::Paths &paths,
                const std::vector<std::string> &launchEnv,
                Resources &state) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();
        const std::string stderrPath = paths.artifactDir + "/fex_stderr.log";
        const auto portableInfo = FEX::ReadPortabilityInformation();

        const bool fdBacked = paths.execFd != -1;

        const auto elfType = fdBacked ? ELFLoader::ELFContainer::GetELFType(paths.execFd)
                                      : ELFLoader::ELFContainer::GetELFType(paths.executable.c_str());
        if (elfType == ELFLoader::ELFContainer::TYPE_NONE) {
            return {
                    Vexa::Common::Code::ElfLoaderFailed,
                    Vexa::Common::Phase::Init,
                    "Executable missing, unreadable, or not an ELF"
            };
        } else if (elfType == ELFLoader::ELFContainer::TYPE_OTHER_ELF) {
            return {
                    Vexa::Common::Code::ElfLoaderFailed,
                    Vexa::Common::Phase::Init,
                    "Unsupported ELF architecture for executable"
            };
        }
        const bool is64 = elfType == ELFLoader::ELFContainer::TYPE_X86_64;
        state.is64BitMode = is64;

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
        if (!WriteTextFile(appCfgPath, BuildAppTHunkConfigJson(paths, is64))) {
            VEXA_LOGW(
                    env,
                    "FEX",
                    "Failed writing app thunk config",
                    Vexa::Log::AddFields({
                                                 Vexa::Log::F("path", appCfgPath)
                                         }).c_str()
            );
        }




        int fd = ::open(stderrPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            (void) ::dup2(fd, STDERR_FILENO);
            ::close(fd);
        }
        const std::string stdoutPath = paths.artifactDir + "/guest_stdout.log";
        int outFd = ::open(stdoutPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (outFd >= 0) {
            (void) ::dup2(outFd, STDOUT_FILENO);
            ::close(outFd);
        }
        Vexa::Log::OpenCrashLog(paths.artifactDir.c_str());
        FEXCore::Config::Shutdown(); // safe defensive

        ::setenv("VEXA_DISABLE_FEXSERVER", "1", 1);
        VEXA_LOGW(env, "FEX", "VEXA_DISABLE_FEXSERVER=1 (runtime only for now", "");

        // Initializing FEX runtime here.
        FEX::Config::LoadConfig(
                /*ProgramName=*/fextl::string{programNameForConfig},
                /*envp=*/nullptr, // don't scan raw envp on Android runtime_worker
                                portableInfo);
        FEXCore::Config::ReloadMetaLayer(); // Apply Meta Config Changes
        Vexa_SetGuestRootfs(paths.rootfs.c_str());
        // libxkbcommon logs an error when this directory is missing. vkgears
        // compiles its keymap from a string, so the directory can be empty.
        std::error_code xkb_dir_error;
        std::filesystem::create_directories(paths.rootfs + "/usr/share/X11/xkb", xkb_dir_error);
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
        // Bitness follows the guest ELF header, not a hardcoded mode: an i386
        // guest takes the x32 syscall handler and the low-4GB allocator path.
        FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, is64 ? "1" : "0");
        FEXCore::Config::Set(FEXCore::Config::CONFIG_ROOTFS, paths.rootfs);
        // Unaligned (split) guest atomics are emulated in the SIGBUS handler.
        // Without the strict process-wide split lock the emulation is a plain
        // read-modify-write, which loses concurrent updates and corrupts
        // atomic counters (observed: Unity job counters go permanently
        // negative and PlayerMain waits forever). Serializing split-lock
        // handling makes the emulation atomic.
        // Bisect: files/fex_splitlock_off.flag disables the strict lock to test
        // whether the serialized fixup path itself wedges the guest.
        const bool splitLockOff =
                ::access("/data/data/com.critical.vexaemulator/files/fex_splitlock_off.flag",
                         F_OK) == 0;
        FEXCore::Config::Set(FEXCore::Config::CONFIG_STRICTINPROCESSSPLITLOCKS,
                             splitLockOff ? "0" : "1");
        if (splitLockOff) {
            VEXA_LOGW(env, "FEX", "StrictInProcessSplitLocks disabled via flag", "");
        }

        // Profiling: per-block JIT symbols ("UnityPlayer.so+0x...") written to
        // /data/local/tmp/perf-<pid>.map so simpleperf can attribute JIT code
        // to guest functions. Enable by creating <files>/fex_jitnames.flag.
        if (::access("/data/data/com.critical.vexaemulator/files/fex_jitnames.flag", F_OK) == 0) {
            FEXCore::Config::Set(FEXCore::Config::CONFIG_BLOCKJITNAMING, "1");
            FEXCore::Config::Set(FEXCore::Config::CONFIG_GLOBALJITNAMING, "1");
            VEXA_LOGW(env, "FEX", "JIT naming enabled (perf map in /data/local/tmp)", "");
        }

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Config initialized"};
    }

    void ShutdownConfig() {
        FEXCore::Config::Shutdown();
    }
}
