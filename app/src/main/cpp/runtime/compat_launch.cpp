#include "compat_launch.h"
#include "executable_format.h"
#include "init/preflight.h"
#include "launch.h"
#include "../common/path_checks.h"
#include "../logging/native_log.h"

#include <atomic>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Vexa::Common::Code;
using Vexa::Common::Phase;
using Vexa::Common::Result;
using Vexa::Runtime::ExecutableFormat;
std::atomic<bool> standalone{false};
std::mutex sessionMutex;
pid_t sessionPid = -1;
std::string sessionToken;
std::atomic<bool> sessionCancelled{false};

std::string Value(const std::vector<std::string> &env, const std::string &key) {
    const auto prefix = key + "=";
    for (auto it = env.rbegin(); it != env.rend(); ++it)
        if (it->compare(0, prefix.size(), prefix) == 0) return it->substr(prefix.size());
    return {};
}
void Set(std::vector<std::string> &env, const std::string &key, const std::string &value) {
    const auto prefix = key + "=";
    for (auto it = env.begin(); it != env.end();)
        if (it->compare(0, prefix.size(), prefix) == 0) it = env.erase(it);
        else ++it;
    env.push_back(prefix + value);
}
std::string GuestPath(const std::string &rootfs, const std::string &path) {
    // Rootfs-visible /usr etc. and app-private absolute paths are both accepted.
    if (path.empty()) return {};
    if (path.front() != '/') return path;
    const auto candidate = rootfs + path;
    if (Vexa::Common::IsReadableFile(candidate) || Vexa::Common::IsReadableDir(candidate))
        return candidate;
    return path;
}
Result Failure(Code code, std::string reason, std::string detail = {}) {
    return Result::Failure(code, Phase::Preflight, std::move(reason), std::move(detail));
}
bool Digits(const std::string &s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
}
std::string NewToken() {
    unsigned char bytes[16];
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return {};
    const auto count = read(fd, bytes, sizeof(bytes));
    close(fd);
    if (count != sizeof(bytes)) return {};
    std::string token;
    for (const auto byte : bytes) {
        token += "0123456789abcdef"[byte >> 4];
        token += "0123456789abcdef"[byte & 15];
    }
    return token;
}
void SignalSession(pid_t leader, const std::string &token, int signal) {
    // Wine's daemonised wineserver can leave the process group. An inherited,
    // random per-session tag identifies those processes without killing other games.
    if (leader > 1) kill(-leader, signal);
    if (token.empty()) return;
    DIR *proc = opendir("/proc");
    if (!proc) return;
    while (auto *entry = readdir(proc)) {
        const std::string name = entry->d_name;
        if (!Digits(name)) continue;
        const auto pid = static_cast<pid_t>(strtol(name.c_str(), nullptr, 10));
        if (pid <= 1 || pid == getpid()) continue;
        const auto dir = "/proc/" + name;
        struct stat st{};
        if (stat(dir.c_str(), &st) != 0 || st.st_uid != getuid()) continue;
        std::ifstream input(dir + "/environ", std::ios::binary);
        std::string item;
        while (std::getline(input, item, '\0')) {
            if (item == "VEXA_COMPAT_SESSION=" + token) { kill(pid, signal); break; }
        }
    }
    closedir(proc);
}

Result RunProton(JNIEnv *jni, const Vexa::Common::Paths &paths,
                 std::vector<std::string> env, const std::vector<std::string> &args) {
    auto proton = Value(env, "VEXA_PROTON_PATH");
    auto client = Value(env, "VEXA_STEAM_CLIENT_PATH");
    auto appId = Value(env, "SteamAppId");
    if (!Digits(appId) || appId.find_first_not_of('0') == std::string::npos)
        return Failure(Code::BadCompatibilityConfig, "PE launch needs a nonzero numeric SteamAppId (use a stable ID for non-Steam games)");
    if (proton.empty() || proton.front() != '/' || client.empty() || client.front() != '/')
        return Failure(Code::BadCompatibilityConfig, "Set absolute VEXA_PROTON_PATH and VEXA_STEAM_CLIENT_PATH");
    auto compatRootfs = Value(env, "VEXA_PROTON_ROOTFS");
    if (compatRootfs.empty()) compatRootfs = paths.rootfs;
    if (compatRootfs.front() != '/' || !Vexa::Common::IsReadableDir(compatRootfs))
        return Failure(Code::BadCompatibilityConfig, "Proton rootfs must be an installed absolute directory", compatRootfs);
    proton = GuestPath(compatRootfs, proton);
    client = GuestPath(compatRootfs, client);
    if (!Vexa::Common::IsReadableFile(proton + "/proton") ||
        !Vexa::Common::IsReadableFile(proton + "/filelock.py") ||
        !Vexa::Common::IsReadableFile(proton + "/version") ||
        Vexa::Runtime::DetectExecutableFormat(proton + "/files/bin/wineserver") != ExecutableFormat::Elf64 ||
        (Vexa::Runtime::DetectExecutableFormat(proton + "/files/bin/wine64") != ExecutableFormat::Elf64 &&
         Vexa::Runtime::DetectExecutableFormat(proton + "/files/bin/wine") != ExecutableFormat::Elf64))
        return Failure(Code::BadCompatibilityConfig, "Full x86_64 Proton files/ distribution missing or invalid", proton);
    // Android Steamworks binaries and JavaSteam account data cannot replace a
    // genuine Linux Steam client installation used by Proton's prefix setup.
    if (!Vexa::Common::IsReadableFile(client + "/linux64/steamclient.so"))
        return Failure(Code::BadCompatibilityConfig, "Linux Steam client linux64/steamclient.so missing", client);
    auto python = Value(env, "VEXA_PROTON_PYTHON");
    if (python.empty()) python = "/usr/bin/python3";
    python = GuestPath(compatRootfs, python);
    if (Vexa::Runtime::DetectExecutableFormat(python) != ExecutableFormat::Elf64)
        return Failure(Code::BadCompatibilityConfig, "Guest x86_64 Python3 missing or wrong architecture", python);
    auto script = Value(env, "VEXA_PROTON_SESSION_SCRIPT");
    if (script.empty()) script = "/opt/vexa/proton_session.py";
    script = GuestPath(paths.rootfs, script);
    if (!Vexa::Common::IsReadableFile(script))
        return Failure(Code::BadCompatibilityConfig, "Proton session script missing", script);
    const auto backend = Value(env, "VEXA_PROTON_DISPLAY_BACKEND");
    if (backend != "headless" && (backend != "external-x11" || Value(env, "DISPLAY").empty()))
        return Failure(Code::CompatibilityDisplayUnavailable,
                       "Separate Wine processes require a display transport",
                       "Configure external-x11 with a real X server/WSI, or headless for non-GUI probes. The in-process fake X11/Android Surface is not transferable.");
    const auto loader = Value(env, "VEXA_REEXEC");
    if (!Vexa::Common::IsExecutable(loader))
        return Failure(Code::BadCompatibilityConfig, "VEXA_REEXEC trampoline unavailable", loader);
    auto dataRoot = Value(env, "VEXA_COMPAT_DATA_ROOT");
    if (dataRoot.empty()) dataRoot = std::filesystem::path(paths.rootfs).parent_path().string() + "/compatdata";
    if (dataRoot.front() != '/')
        return Failure(Code::BadCompatibilityConfig, "VEXA_COMPAT_DATA_ROOT must be absolute");
    std::error_code ec;
    const auto data = dataRoot + "/" + appId;
    std::filesystem::create_directories(data, ec);
    if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot create persistent per-game compatdata", ec.message());
    // Linux Wine hardcodes /tmp/.wine-UID and ignores TMPDIR. The scoped
    // FEX Proton hooks map its mkdir/chdir into this private rootfs directory;
    // Wine binds/connects the socket by a relative name after chdir.
    const auto wineTmp = compatRootfs + "/tmp";
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(wineTmp, ec)))
        return Failure(Code::BadCompatibilityConfig, "Wine rootfs tmp must be a private directory, not a symlink", wineTmp);
    ec.clear();
    std::filesystem::create_directories(wineTmp, ec);
    if (ec || chmod(wineTmp.c_str(), 0700) != 0)
        return Failure(Code::BadCompatibilityConfig, "Cannot create private rootfs tmp for Wine", wineTmp);
    // Python and other helpers honor TMPDIR directly.
    if (Value(env, "TMPDIR").empty()) {
        const auto temporary = std::filesystem::path(paths.rootfs).parent_path().string() + "/wtmp";
        std::filesystem::create_directories(temporary, ec);
        if (ec || chmod(temporary.c_str(), 0700) != 0)
            return Failure(Code::BadCompatibilityConfig, "Cannot create private Wine temporary directory", temporary);
        Set(env, "TMPDIR", temporary);
    }
    const auto token = NewToken();
    if (token.empty()) return Failure(Code::InternalError, "Cannot create compatibility session token");
    Set(env, "STEAM_COMPAT_DATA_PATH", data);
    Set(env, "STEAM_COMPAT_CLIENT_INSTALL_PATH", client);
    Set(env, "STEAM_COMPAT_INSTALL_PATH", paths.workingDir);
    if (Value(env, "SteamGameId").empty()) Set(env, "SteamGameId", appId);
    if (Value(env, "PATH").empty()) Set(env, "PATH", "/usr/local/bin:/usr/bin:/bin");
    if (Value(env, "HOME").empty()) {
        std::filesystem::create_directories(dataRoot + "/home", ec);
        if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot create Proton HOME", ec.message());
        Set(env, "HOME", dataRoot + "/home");
    }
    {
        const auto home = Value(env, "HOME");
        if (home.empty() || home.front() != '/')
            return Failure(Code::BadCompatibilityConfig, "Proton HOME must be an absolute directory", home);
        const auto homeSteam = home + "/.steam";
        ec.clear();
        std::filesystem::create_directories(homeSteam, ec);
        if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot create Proton Steam home", ec.message());
        auto ensureSdkLink = [&](const char *name, const std::string &target) -> Result {
            const auto link = homeSteam + "/" + name;
            ec.clear();
            const auto status = std::filesystem::symlink_status(link, ec);
            if (ec && ec != std::errc::no_such_file_or_directory)
                return Failure(Code::BadCompatibilityConfig, "Cannot inspect Steam SDK link", link + ": " + ec.message());
            ec.clear();
            if (std::filesystem::exists(status)) {
                if (!std::filesystem::is_symlink(status))
                    return Failure(Code::BadCompatibilityConfig, "Steam SDK link path is occupied", link);
                const auto previous = std::filesystem::read_symlink(link, ec);
                if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot read Steam SDK link", link + ": " + ec.message());
                if (previous == std::filesystem::path(target)) return Result::Success(Phase::Preflight);
                std::filesystem::remove(link, ec);
                if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot replace Steam SDK link", link + ": " + ec.message());
            }
            std::filesystem::create_directory_symlink(target, link, ec);
            if (ec) return Failure(Code::BadCompatibilityConfig, "Cannot create Steam SDK link", link + ": " + ec.message());
            return Result::Success(Phase::Preflight);
        };
        auto sdkResult = ensureSdkLink("sdk64", client + "/linux64");
        if (!sdkResult.Ok()) return sdkResult;
        if (Vexa::Common::IsReadableDir(client + "/linux32")) {
            sdkResult = ensureSdkLink("sdk32", client + "/linux32");
            if (!sdkResult.Ok()) return sdkResult;
        }
        const auto machineId = compatRootfs + "/etc/machine-id";
        if (!Vexa::Common::IsReadableFile(machineId)) {
            std::filesystem::create_directories(compatRootfs + "/etc", ec);
            std::ofstream out(machineId, std::ios::binary | std::ios::trunc);
            if (out) out << "0123456789abcdef0123456789abcdef\n";
        }
    }
    Set(env, "VEXA_COMPAT_SESSION", token);
    if (backend == "headless") Set(env, "VEXA_HEADLESS", "1");
    Set(env, "VEXA_ROOTFS", compatRootfs);
    Set(env, "VEXA_THUNK_HOST", paths.thunkHost);
    Set(env, "VEXA_THUNK_GUEST", paths.thunkGuest);
    Set(env, "VEXA_WORKDIR", paths.workingDir);
    Set(env, "VEXA_ARTIFACTS", paths.artifactDir);
    Set(env, "PROTON_LOG_DIR", paths.artifactDir);
    Set(env, "VEXA_PROTON_PATH", proton);
    // These variables belong to the x86 guest, not Bionic's native linker.
    // The trampoline restores them only in the emulated environment.
    for (const auto &key : {std::string("LD_PRELOAD"), std::string("LD_LIBRARY_PATH")}) {
        const auto prefix = key + "=";
        bool present = false;
        for (const auto &entry : env) if (entry.compare(0, prefix.size(), prefix) == 0) present = true;
        const auto value = Value(env, key);
        const auto transfer = "VEXA_GUEST_" + key;
        for (auto it = env.begin(); it != env.end();)
            if (it->compare(0, prefix.size(), prefix) == 0 || it->compare(0, transfer.size() + 1, transfer + "=") == 0) it = env.erase(it);
            else ++it;
        if (present) Set(env, transfer, value);
    }
    std::vector<std::string> command{loader, "--vexa-start", python, script, proton, paths.executable};
    command.insert(command.end(), args.begin(), args.end());
    std::vector<char *> argv, envp;
    for (auto &entry : command) argv.push_back(entry.data());
    for (auto &entry : env) envp.push_back(entry.data());
    argv.push_back(nullptr); envp.push_back(nullptr);
    posix_spawnattr_t attr;
    int attrError = posix_spawnattr_init(&attr);
    if (attrError) return Failure(Code::CompatibilitySpawnFailed, "Cannot initialise spawn attributes", strerror(attrError));
    attrError = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    if (!attrError) attrError = posix_spawnattr_setpgroup(&attr, 0);
    if (attrError) { posix_spawnattr_destroy(&attr); return Failure(Code::CompatibilitySpawnFailed, "Cannot isolate Proton process group", strerror(attrError)); }
    pid_t pid;
    int error;
    {
        // Publish PID atomically with spawn so Stop cannot miss a just-created child.
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (sessionCancelled.load()) { posix_spawnattr_destroy(&attr); return Failure(Code::CompatibilitySpawnFailed, "Compatibility launch cancelled"); }
        if (sessionPid > 0) { posix_spawnattr_destroy(&attr); return Failure(Code::AlreadyStarted, "Compatibility session already active"); }
        error = posix_spawn(&pid, loader.c_str(), nullptr, &attr, argv.data(), envp.data());
        if (!error) { sessionPid = pid; sessionToken = token; }
    }
    posix_spawnattr_destroy(&attr);
    if (error) return Failure(Code::CompatibilitySpawnFailed, "Cannot spawn Proton FEX process", strerror(error));
    VEXA_LOGI(jni, "PROTON", "Separate Proton session started", Vexa::Log::AddFields({
        Vexa::Log::F("pid", pid), Vexa::Log::F("appId", appId), Vexa::Log::F("compatdata", data)}).c_str());
    int status = 0;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    const int waitError = errno;
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        // Reap first, then kill orphaned wineserver/helper processes by session tag.
        SignalSession(-1, token, SIGKILL);
        sessionPid = -1; sessionToken.clear();
    }
    if (waited < 0) return Failure(Code::CompatibilitySpawnFailed, "Cannot wait for Proton process", strerror(waitError));
    const int exit = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (exit != 0) return Failure(Code::CompatibilityGuestFailed, "Proton session failed", "exit=" + std::to_string(exit));
    return Result::Success(Phase::Execute);
}
}

namespace Vexa::Runtime {
void SetStandaloneLoader() { standalone.store(true); }
void StopCompatibilitySession() {
    // Worker processes are single-use; a stop permanently cancels a pending spawn.
    sessionCancelled.store(true);
    std::lock_guard<std::mutex> lock(sessionMutex);
    SignalSession(sessionPid, sessionToken, SIGKILL);
}
Result LaunchGuest(JNIEnv *env, Vexa::Common::Paths paths,
                   std::vector<std::string> environment, std::vector<std::string> arguments) {
    paths.executable = GuestPath(paths.rootfs, paths.executable);
    if (!paths.executable.empty() && paths.executable.front() != '/')
        paths.executable = (std::filesystem::path(paths.workingDir) / paths.executable).lexically_normal().string();
    const auto format = DetectExecutableFormat(paths.executable);
    VEXA_LOGI(env, "BOOT", "Executable format selected", Vexa::Log::AddFields({
        Vexa::Log::F("path", paths.executable), Vexa::Log::F("format", ExecutableFormatName(format))}).c_str());
    auto preflight = RunPreflight(paths);
    if (!preflight.Ok()) return preflight;
    if (format == ExecutableFormat::Pe32 || format == ExecutableFormat::Pe64)
        return RunProton(env, paths, std::move(environment), arguments);
    if (format == ExecutableFormat::Script) {
        // FEX's ELF loader does not parse shebangs. Resolve Linux interpreters
        // explicitly so shell/Python helpers never reach the Android kernel.
        std::ifstream script(paths.executable);
        std::array<char, 258> shebang{};
        script.getline(shebang.data(), shebang.size());
        const auto read = script.gcount();
        const auto length = read > 0 && !script.eof() ? read - 1 : read;
        const std::string line(shebang.data(), static_cast<size_t>(length));
        if (script.fail() || line.size() > 256 || line.find('\0') != std::string::npos)
            return Failure(Code::BadExecutable, "Invalid or overlong shebang");
        const auto start = line.find_first_not_of(" \t", 2);
        if (start == std::string::npos) return Failure(Code::BadExecutable, "Missing shebang interpreter");
        const auto end = line.find_first_of(" \t\r", start);
        auto interpreter = line.substr(start, end - start);
        std::string option;
        if (end != std::string::npos) {
            const auto optionStart = line.find_first_not_of(" \t\r", end);
            if (optionStart != std::string::npos) option = line.substr(optionStart);
        }
        if (!option.empty() && option.back() == '\r') option.pop_back();
        if (interpreter == "/usr/bin/env") {
            if (option.empty() || option.find_first_of(" \t=") != std::string::npos)
                return Failure(Code::BadExecutable, "Unsupported env shebang; use an explicit Linux interpreter");
            auto search = Value(environment, "PATH");
            if (search.empty()) search = "/usr/bin:/bin";
            interpreter.clear();
            size_t offset = 0;
            do {
                const auto next = search.find(':', offset);
                const auto dir = search.substr(offset, next - offset);
                const auto candidate = GuestPath(paths.rootfs, (dir.empty() ? paths.workingDir : dir) + "/" + option);
                if (Vexa::Common::IsExecutable(candidate)) { interpreter = candidate; break; }
                if (next == std::string::npos) break;
                offset = next + 1;
            } while (true);
            option.clear();
        } else interpreter = GuestPath(paths.rootfs, interpreter);
        const auto interpreterType = DetectExecutableFormat(interpreter);
        if (interpreterType != ExecutableFormat::Elf32 && interpreterType != ExecutableFormat::Elf64)
            return Failure(Code::BadExecutable, "Shebang needs an installed x86 Linux ELF interpreter", interpreter);
        arguments.insert(arguments.begin(), paths.executable);
        if (!option.empty()) arguments.insert(arguments.begin(), option);
        paths.executable = interpreter;
    } else if (format != ExecutableFormat::Elf32 && format != ExecutableFormat::Elf64)
        return Failure(Code::BadExecutable, "Unsupported executable format (expected x86 ELF or PE)");
    return StartRuntime(env, paths, environment, arguments);
}
}

// Optional bridge imported by the checked-in FEX LinuxEmulation patch.
extern "C" __attribute__((visibility("default")))
int Vexa_IsPortableExecutable(const char *path) {
    if (!path) return 0;
    const auto format = Vexa::Runtime::DetectExecutableFormat(path);
    return format == ExecutableFormat::Pe32 || format == ExecutableFormat::Pe64;
}
extern "C" __attribute__((visibility("default")))
bool Vexa_IsStandaloneLoader() { return standalone.load(); }
