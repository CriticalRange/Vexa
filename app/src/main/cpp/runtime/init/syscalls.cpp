//
// Created by critical on 12.03.2026.
//

#include <FEXCore/HLE/SyscallHandler.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/Syscalls.h>

#include "syscalls.h"
#include "runtime_state.h"

namespace FEX::HLE::x64 {
    fextl::unique_ptr<FEXCore::HLE::SyscallHandler> CreateHandler(
            FEXCore::Context::Context *ctx,
            FEX::HLE::SignalDelegator *signalDelegator,
            FEX::HLE::ThunkHandler *thunkHandler
    );
}

namespace Vexa::Runtime::Init {
    LaunchResult SetupSyscalls(JNIEnv *env, RuntimeState &state) {
        auto sysHandler = FEX::HLE::x64::CreateHandler(
                state.ctx.get(),
                state.signalDelegator.get(),
                state.thunkHandler.get()
        );
        if (!sysHandler) {
            return {
                    14,
                    "Failed to initialize Syscall Handler",
                    ""
            };
        }
        auto *linuxSyscallHandler = dynamic_cast<FEX::HLE::SyscallHandler *>(sysHandler.get());
        if (!linuxSyscallHandler) {
            return {
                    14,
                    "Failed to initialize Syscall Handler",
                    ""
            };
        }
        state.linuxSyscallHandler = linuxSyscallHandler;
        state.syscallHandler = std::move(sysHandler);
        state.ctx->SetSyscallHandler(state.syscallHandler.get());
        return {
                0,
                "OK",
                ""
        };
    }

    LaunchResult SetupParentThread(RuntimeState &state) {
        if (!state.linuxSyscallHandler) {
            return {
                    15,
                    "Missing linux syscall handler",
                    ""
            };
        }
        state.parentThread = state.linuxSyscallHandler->TM.CreateThread(0, 0);
        if (!state.parentThread) {
            return {
                    16,
                    "CreateThread failed",
                    ""
            };
        }
        return {
                0,
                "OK",
                ""
        };
    }

    void TeardownParentThread(RuntimeState &state) {
        if (!state.linuxSyscallHandler || !state.parentThread) return;
        if (state.signalDelegator) {
            state.signalDelegator->UninstallTLSState(state.parentThread);
        };
        state.linuxSyscallHandler->TM.DestroyThread(state.parentThread);
        state.parentThread = nullptr;
    }
}
