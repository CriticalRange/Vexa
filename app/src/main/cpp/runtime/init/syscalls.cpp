//
// Created by critical on 12.03.2026.
//

#include <FEXCore/HLE/SyscallHandler.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/Syscalls.h>

#include "syscalls.h"
#include "resources.h"
#include "../../logging/native_log.h"

namespace FEX::HLE::x64 {
    fextl::unique_ptr<FEXCore::HLE::SyscallHandler> CreateHandler(
            FEXCore::Context::Context *ctx,
            FEX::HLE::SignalDelegator *signalDelegator,
            FEX::HLE::ThunkHandler *thunkHandler
    );
}

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupSyscalls(JNIEnv *env, Resources &state) {
        auto sysHandler = FEX::HLE::x64::CreateHandler(
                state.ctx.get(),
                state.signalDelegator.get(),
                state.thunkHandler.get()
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
