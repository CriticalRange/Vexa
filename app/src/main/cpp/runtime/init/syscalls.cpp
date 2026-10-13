//
// Created by critical on 12.03.2026.
//

#include <FEXCore/HLE/SyscallHandler.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/GdbServer.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/LinuxAllocator.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/Syscalls.h>

#include <cstdlib>
#include <unistd.h>

#include "syscalls.h"
#include "resources.h"
#include "allocator.h"
#include "../../logging/native_log.h"

namespace FEX::HLE::x64 {
    fextl::unique_ptr<FEX::HLE::SyscallHandler> CreateHandler(
            FEXCore::Context::Context *ctx,
            FEX::HLE::SignalDelegator *signalDelegator,
            FEX::HLE::ThunkHandler *thunkHandler
    );
}

namespace FEX::HLE::x32 {
    fextl::unique_ptr<FEX::HLE::SyscallHandler> CreateHandler(
            FEXCore::Context::Context *ctx,
            FEX::HLE::SignalDelegator *signalDelegator,
            FEX::HLE::ThunkHandler *thunkHandler,
            fextl::unique_ptr<FEX::HLE::MemAllocator> allocator
    );
}

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupSyscalls(JNIEnv *env, Resources &state) {
        auto sysHandler = state.is64BitMode
                          ? FEX::HLE::x64::CreateHandler(
                                state.ctx.get(),
                                state.signalDelegator.get(),
                                state.thunkHandler.get()
                        )
                          : FEX::HLE::x32::CreateHandler(
                                state.ctx.get(),
                                state.signalDelegator.get(),
                                state.thunkHandler.get(),
                                TakeGuestAllocator()
                        );
        if (!sysHandler) {
            return {Vexa::Common::Code::CreateSyscallHandlerFailed, Vexa::Common::Phase::Init,
                    "Failed to create Syscall Handler"};
        }
        auto *linuxSyscallHandler = dynamic_cast<FEX::HLE::SyscallHandler *>(sysHandler.get());
        if (!linuxSyscallHandler) {
            return {Vexa::Common::Code::UnexpectedSyscallHandlerType, Vexa::Common::Phase::Init,
                    "Unexpected Syscall Handler Type; expected: Linux"};
        }
        state.linuxSyscallHandler = linuxSyscallHandler;
        state.syscallHandler = std::move(sysHandler);
        state.ctx->SetSyscallHandler(state.syscallHandler.get());

        // Optional in-process gdb server for guest-level backtraces. The FEX
        // gdb stub serves guest threads/registers over a unix socket — the only
        // way to see where an x86 guest thread is actually parked. Enable by
        // creating <files>/fex_gdb.flag in the app data dir.
        {
            const char *flagEnv = ::getenv("VEXA_FEX_GDB");
            const bool enabled =
                    ::access("/data/data/com.critical.vexaemulator/files/fex_gdb.flag", F_OK) == 0 ||
                    (flagEnv && flagEnv[0] && flagEnv[0] != '0');
            if (enabled) {
                state.gdbServer = new FEX::GdbServer(
                        state.ctx.get(), state.signalDelegator.get(), state.linuxSyscallHandler);
                VEXA_LOGI(env, "FEX", "GdbServer started", "{}");
            }
        }

        VEXA_LOGI(env, "FEX",
                  "Syscall ABI",
                  Vexa::Log::AddFields({
                                               Vexa::Log::F("OS_ABI",
                                                            static_cast<int>(state.syscallHandler->GetOSABI()))
                                       }).c_str()
        );
        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Syscall Handler initialized OK"};
    }

    Vexa::Common::Result SetupParentThread(Resources &state) {
        if (!state.linuxSyscallHandler) {
            return {Vexa::Common::Code::MissingSyscallHandler, Vexa::Common::Phase::Init,
                    "Missing Syscall Handler!"};
        }
        if (!state.signalDelegator) {
            return {Vexa::Common::Code::MissingSignalDelegator, Vexa::Common::Phase::Init,
                    "Missing Signal Delegator!"};
        }
        if (!state.thunkHandler) {
            return {Vexa::Common::Code::MissingThunkHandler, Vexa::Common::Phase::Init,
                    "Missing Thunk Handler!"};
        }
        state.parentThread = state.linuxSyscallHandler->TM.CreateThread(0, 0);
        if (!state.parentThread) {
            return {Vexa::Common::Code::CreateParentThreadFailed, Vexa::Common::Phase::Init,
                    "Error creating Parent Thread."};
        }

        state.linuxSyscallHandler->TM.TrackThread(state.parentThread);
        state.linuxSyscallHandler->RegisterTLSState(state.parentThread);
        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Syscall Handler initialized"};
    }

    void TeardownParentThread(Resources &state) {
        if (!state.linuxSyscallHandler || !state.parentThread) return;
        state.linuxSyscallHandler->UninstallTLSState(state.parentThread);
        state.linuxSyscallHandler->TM.DestroyThread(state.parentThread);
        state.parentThread = nullptr;
    }
}
