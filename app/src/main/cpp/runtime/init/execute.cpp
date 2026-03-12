//
// Created by critical on 12.03.2026.
//
#include <jni.h>

#include <FEXCore/Core/X86Enums.h>
#include <Tools/FEXInterpreter/ELFCodeLoader.h>
#include <Tools/LinuxEmulation/VDSO_Emulation.h>

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
