//
// Created by critical on 12.03.2026.
//
#include <jni.h>

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
    Vexa::Common::Result
    ExecuteRuntime(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state) {
        if (!state.ctx || !state.parentThread || !state.linuxSyscallHandler ||
            !state.thunkHandler || !state.signalDelegator) {
            return {Vexa::Common::Code::ExecutePrereqMissing, Vexa::Common::Phase::Init,
                    "Execute prerequisites missing. this can be context, syscall, thread, thunk or signal handling."};
        }
        // Argv template: argv[0] = executable
        const fextl::string binary{paths.executable.c_str()};
        const fextl::string rootfs{paths.rootfs.c_str()};
        fextl::vector<fextl::string> args;
        args.emplace_back(binary);
        fextl::vector<fextl::string> parsedArgs;
        parsedArgs.emplace_back(binary);

        auto mkStat = [](const std::string &p) {
            struct stat st{};
            const int rc = ::stat(p.c_str(), &st);
            return Vexa::Log::AddFields({
                                                Vexa::Log::F("path", p),
                                                Vexa::Log::F("stat_ok", rc == 0),
                                                Vexa::Log::F("is_reg",
                                                             rc == 0 ? S_ISREG(st.st_mode) : false),
                                                Vexa::Log::F("r_ok",
                                                             ::access(p.c_str(), R_OK) == 0),
                                                Vexa::Log::F("x_ok",
                                                             ::access(p.c_str(), X_OK) == 0),
                                        });
        };
        VEXA_LOGI(env, "FEX", "ELF input executable", mkStat(paths.executable).c_str());

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
                nullptr, // envp, for now empty
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

        auto vdso = FEX::VDSO::LoadVDSOThunks(
                state.parentThread->Thread,
                loader.Is64BitMode(),
                state.linuxSyscallHandler
        );
        VEXA_LOGI(env, "FEX", "VDSO Thunks loaded", "{}");

        state.thunkHandler->AppendThunkDefinitions(
                FEX::VDSO::GetVDSOThunkDefinitions(loader.Is64BitMode()));
        state.signalDelegator->SetVDSOSymbols();

        loader.SetVDSOBase(vdso.VDSOBase);
        loader.CalculateHWCaps(state.ctx.get());
        VEXA_LOGI(env, "FEX", "VDSO set up", "{}");

        if (!loader.MapMemory(state.linuxSyscallHandler, state.parentThread->Thread)) {
            FEX::VDSO::UnloadVDSOMapping(state.parentThread->Thread, state.linuxSyscallHandler,
                                         vdso);
            return {Vexa::Common::Code::MapMemoryFailed, Vexa::Common::Phase::Init,
                    "Map memory failed."};
        }
        VEXA_LOGI(env, "FEX", "MapMemory loaded", "{}");

        auto brk = loader.GetBRKInfo();

        state.linuxSyscallHandler->DefaultProgramBreak(brk.Base, brk.Size);
        VEXA_LOGI(env, "FEX", "Default Program Break set up", "{}");

        auto *frame = state.parentThread->Thread->CurrentFrame;
        VEXA_LOGI(env, "FEX", "CpuStateFrame set up", "{}");
        frame->State.rip = loader.DefaultRIP();
        VEXA_LOGI(env, "FEX", "Default RIP set up", "{}");
        frame->State.gregs[FEXCore::X86State::REG_RSP] = loader.GetStackPointer();

        // Validation RIP is executable
        const bool abi64 =
                state.syscallHandler->GetOSABI() == FEXCore::HLE::SyscallOSABI::OS_LINUX64;
        if (loader.Is64BitMode() != abi64) {
            return {
                    Vexa::Common::Code::InternalError,
                    Vexa::Common::Phase::Execute,
                    "Loader bitness and syscall ABI mismatch"
            };
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
            return {
                    Vexa::Common::Code::ExecutePrereqMissing,
                    Vexa::Common::Phase::Execute,
                    "Entry RIP is not executable!"
            };
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

        FEX::VDSO::UnloadVDSOMapping(state.parentThread->Thread, state.linuxSyscallHandler,
                                     vdso);
        VEXA_LOGI(env, "FEX", "VDSO Mappings unloaded (not needed anymore)", "{}");

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Execution OK"};
    }
} //namespace Vexa::Runtime::Init
