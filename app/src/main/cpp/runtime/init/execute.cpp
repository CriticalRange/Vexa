//
// Created by critical on 12.03.2026.
//

#include <jni.h>
#include <vector>
#include <string>
#include <string_view>
#include <exception>

#include <FEXCore/Core/X86Enums.h>
#include <Tools/FEXInterpreter/ELFCodeLoader.h>
#include <Tools/LinuxEmulation/VDSO_Emulation.h>
#include <Linux/Utils/ELFContainer.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../logging/native_log.h"
#include "../../common/status.h"
#include "execute.h"

namespace Vexa::Runtime::Init {
    namespace {
        auto MkStatFields(const std::string &p) {
            struct stat st{};
            const int rc = ::stat(p.c_str(), &st);
            return Vexa::Log::AddFields({
                                                Vexa::Log::F("path", p),
                                                Vexa::Log::F("stat_ok", rc == 0),
                                                Vexa::Log::F("is_reg",
                                                             rc == 0 ? S_ISREG(st.st_mode) : false),
                                                Vexa::Log::F("is_dir",
                                                             rc == 0 ? S_ISDIR(st.st_mode) : false),
                                                Vexa::Log::F("r_ok",
                                                             ::access(p.c_str(), R_OK) == 0),
                                                Vexa::Log::F("x_ok",
                                                             ::access(p.c_str(), X_OK) == 0),
                                        });
        }

        bool IsAccessiblePath(const std::string &path, bool requireExecute) {
            struct stat st{};
            if (::stat(path.c_str(), &st) != 0) return false;
            if (requireExecute) {
                return S_ISREG(st.st_mode) && ::access(path.c_str(), R_OK) == 0 && ::access(path.c_str(),
                                                                                          X_OK) == 0;
            }
            return S_ISDIR(st.st_mode) && ::access(path.c_str(), R_OK) == 0 && ::access(path.c_str(),
                                                                                        X_OK) == 0;
        }
    } // namespace

    Vexa::Common::Result
    ExecuteRuntime(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state,
                   const std::vector<std::string> &launchEnv,
                   const std::vector<std::string> &launchArgs) {
        if (!state.ctx || !state.parentThread || !state.linuxSyscallHandler ||
            !state.thunkHandler || !state.signalDelegator) {
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Init,
                    "Execute prerequisites missing. this can be context, syscall, thread, thunk or signal handling."};
        }
        if (!env) {
            return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                    "Invalid JNI environment"};
        }
        if (paths.executable.empty() || paths.rootfs.empty()) {
            return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                    "Invalid runtime paths"};
        }
        if (!IsAccessiblePath(paths.executable, true)) {
            VEXA_LOGE(env, "FEX", "Executable path is not accessible",
                      MkStatFields(paths.executable).c_str());
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                    "Executable path is not accessible"};
        }
        if (!IsAccessiblePath(paths.rootfs, false)) {
            VEXA_LOGE(env, "FEX", "Rootfs path is not accessible",
                      MkStatFields(paths.rootfs).c_str());
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                    "Rootfs path is not accessible"};
        }
        if (!state.parentThread->Thread) {
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                    "Missing parent thread instance"};
        }
        if (!state.syscallHandler) {
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                    "Missing base syscall handler"};
        }

        FEX::VDSO::VDSOMapping vdso{};
        bool vdsoLoaded{false};
        bool codeLoaderBound{false};
        auto cleanupRuntimeState = [&]() {
            if (state.parentThread && state.parentThread->Thread && state.linuxSyscallHandler) {
                if (vdsoLoaded) {
                    FEX::VDSO::UnloadVDSOMapping(state.parentThread->Thread, state.linuxSyscallHandler,
                                                 vdso);
                    VEXA_LOGI(env, "FEX", "VDSO Mappings unloaded (cleanup path)", "{}");
                    vdsoLoaded = false;
                }
            }
            if (codeLoaderBound && state.linuxSyscallHandler) {
                state.linuxSyscallHandler->SetCodeLoader(nullptr);
                codeLoaderBound = false;
            }
        };

        try {
            auto *frame = state.parentThread->Thread->CurrentFrame;
            if (!frame) {
                cleanupRuntimeState();
                return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                        "Missing guest CPU frame"};
            }

            // executable
            const fextl::string binary{paths.executable.c_str()};

            // rootfs
            const fextl::string rootfs{paths.rootfs.c_str()};

            // Arguments
            fextl::vector<fextl::string> args;
            args.emplace_back(binary);
            for (const auto &arg: launchArgs) {
                if (!arg.empty()) {
                    args.emplace_back(arg.c_str());
                }
            }
            fextl::vector<fextl::string> parsedArgs = args;

            // Environment Variables
            fextl::vector<fextl::string> envVars;
            envVars.reserve(launchEnv.size());
            for (const auto &entry: launchEnv) {
                if (!entry.empty() && entry.find("=") != std::string::npos) {
                    envVars.emplace_back(entry.c_str());
                }
            }

            fextl::vector<char *> envpVec;
            envpVec.reserve(envVars.size() + 1);
            for (auto &e: envVars) {
                envpVec.emplace_back(const_cast<char *>(e.c_str()));
            }
            envpVec.emplace_back(nullptr);

            VEXA_LOGI(env, "FEX", "ELF input executable", MkStatFields(paths.executable).c_str());

            const auto hostType = ELFLoader::ELFContainer::GetELFType(paths.executable.c_str());

            VEXA_LOGI(env, "FEX", "ELF type (host path",
                      Vexa::Log::AddFields({
                                                   Vexa::Log::F("type", static_cast<int>(hostType))
                                           }).c_str());

            ELFCodeLoader loader(
                    binary, //filename (also path)
                    -1, //ProgramFDFromEnv
                    rootfs, // rootfs obviously
                    args,
                    parsedArgs,
                    envpVec.data(),
                    nullptr, // AdditionalEnvp
                    false //SkipInterpreter
            );

            if (!loader.ELFWasLoaded()) {
                VEXA_LOGE(env, "FEX", "ELF loader failed",
                          Vexa::Log::AddFields({
                                                       Vexa::Log::F("exe", paths.executable),
                                                       Vexa::Log::F("rootfs", paths.rootfs),
                                                       Vexa::Log::F("hint",
                                                                    "main/interpreter read or type mismatch"),
                                               }).c_str());
                return {Vexa::Common::Code::ElfLoaderFailed, Vexa::Common::Phase::Init,
                        "Elf Loader failed."};
            }
            VEXA_LOGI(env, "FEX", "ELF Loader successful", "{}");
            state.linuxSyscallHandler->SetCodeLoader(&loader);
            codeLoaderBound = true;

            vdso = FEX::VDSO::LoadVDSOThunks(
                    state.parentThread->Thread,
                    loader.Is64BitMode(),
                    state.linuxSyscallHandler
            );
            vdsoLoaded = true;
            VEXA_LOGI(env, "FEX", "VDSO Thunks loaded", "{}");

            state.thunkHandler->AppendThunkDefinitions(
                    FEX::VDSO::GetVDSOThunkDefinitions(loader.Is64BitMode()));
            state.signalDelegator->SetVDSOSymbols();

            loader.SetVDSOBase(vdso.VDSOBase);
            loader.CalculateHWCaps(state.ctx.get());
            VEXA_LOGI(env, "FEX", "VDSO set up", "{}");

            if (!loader.MapMemory(state.linuxSyscallHandler, state.parentThread->Thread)) {
                cleanupRuntimeState();
                return {Vexa::Common::Code::MapMemoryFailed, Vexa::Common::Phase::Init,
                        "Map memory failed."};
            }
            VEXA_LOGI(env, "FEX", "MapMemory loaded", "{}");

            auto brk = loader.GetBRKInfo();

            state.linuxSyscallHandler->DefaultProgramBreak(brk.Base, brk.Size);
            VEXA_LOGI(env, "FEX", "Default Program Break set up", "{}");

            VEXA_LOGI(env, "FEX", "CpuStateFrame set up", "{}");
            frame->State.rip = loader.DefaultRIP();
            if (frame->State.rip == 0) {
                VEXA_LOGE(env, "FEX", "Computed entry RIP is zero", "{}");
                cleanupRuntimeState();
                return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                        "Entry RIP is invalid (zero)"};
            }
            VEXA_LOGI(env, "FEX", "Default RIP set up", "{}");
            frame->State.gregs[FEXCore::X86State::REG_RSP] = loader.GetStackPointer();
            if (frame->State.gregs[FEXCore::X86State::REG_RSP] == 0) {
                VEXA_LOGE(env, "FEX", "Computed entry stack pointer is zero", "{}");
                cleanupRuntimeState();
                return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                        "Entry stack pointer is invalid (zero)"};
            }

            // Validation RIP is executable
            const bool abi64 =
                    state.syscallHandler->GetOSABI() == FEXCore::HLE::SyscallOSABI::OS_LINUX64;
            if (loader.Is64BitMode() != abi64) {
                const auto fields = Vexa::Log::AddFields({
                                                             Vexa::Log::F("loaderIs64", loader.Is64BitMode()),
                                                             Vexa::Log::F("sysAbiIs64", abi64),
                                                     });
                VEXA_LOGE(env, "FEX", "Loader bitness and syscall ABI mismatch", fields.c_str());
                cleanupRuntimeState();
                return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                        "Loader bitness and syscall ABI mismatch"};
            }

            const uint64_t rip = frame->State.rip;
            const auto range = state.linuxSyscallHandler->QueryGuestExecutableRange(
                    state.parentThread->Thread, rip
            );
            const auto sec = state.linuxSyscallHandler->LookupExecutableFileSection(
                    state.parentThread->Thread, rip);
            VEXA_LOGI(env, "FEX", "Entry RIP exec check", Vexa::Log::AddFields({
                                                                                   Vexa::Log::F(
                                                                                           "rip",
                                                                                           rip),
                                                                                   Vexa::Log::F(
                                                                                           "rangeBase",
                                                                                           range.Base),
                                                                                   Vexa::Log::F(
                                                                                           "rangeSize",
                                                                                           range.Size),
                                                                                   Vexa::Log::F(
                                                                                           "hasSection",
                                                                                           sec.has_value()),
                                                                                   Vexa::Log::F(
                                                                                           "loaderIs64",
                                                                                           loader.Is64BitMode()),
                                                                           }).c_str());
            if (range.Size == 0) {
                cleanupRuntimeState();
                return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Execute,
                        "Entry RIP is not executable!"};
            }
            const uint64_t rip64 = frame->State.rip;
            const uint64_t rip32 = static_cast<uint32_t>(rip64);

            const auto r64 = state.linuxSyscallHandler->QueryGuestExecutableRange(
                    state.parentThread->Thread, rip64
            );
            const auto r32 = state.linuxSyscallHandler->QueryGuestExecutableRange(
                    state.parentThread->Thread, rip32
            );
            VEXA_LOGI(env, "FEX", "RIP mode probe", Vexa::Log::AddFields({
                                                                                 Vexa::Log::F("rip64",
                                                                                              rip64),
                                                                                 Vexa::Log::F("rip32",
                                                                                              rip32),
                                                                                 Vexa::Log::F("range64",
                                                                                              r64.Size),
                                                                                 Vexa::Log::F("range32",
                                                                                              r32.Size),
                                                                                 Vexa::Log::F(
                                                                                         "cfg_is64",
                                                                                         FEXCore::Config::Get_IS64BIT_MODE()),
                                                                                 Vexa::Log::F(
                                                                                         "sys_is64",
                                                                                         state.linuxSyscallHandler->Is64BitMode()),
                                                                         }).c_str());

            loader.CloseFDs();

            VEXA_LOGI(env, "FEX", "ExecuteThread begin", "{}");
            state.ctx->ExecuteThread(state.parentThread->Thread);
            VEXA_LOGI(env, "FEX", "ExecuteThread returned", "{}");

            cleanupRuntimeState();
            VEXA_LOGI(env, "FEX", "VDSO Mappings unloaded (not needed anymore)", "{}");

            return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                    "Execution OK"};
        } catch (const std::exception &e) {
            VEXA_LOGE(env, "FEX", "ExecuteThread threw std::exception",
                      Vexa::Log::AddFields({
                                                   Vexa::Log::F("what", std::string_view(e.what()))
                                           }).c_str());
            cleanupRuntimeState();
            return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                    "Unhandled C++ exception during runtime execution"};
        } catch (...) {
            VEXA_LOGE(env, "FEX", "ExecuteThread threw unknown exception", "{}");
            cleanupRuntimeState();
            return {Vexa::Common::Code::InternalError, Vexa::Common::Phase::Execute,
                    "Unhandled non-standard exception during runtime execution"};
        }
    }
} //namespace Vexa::Runtime::Init
