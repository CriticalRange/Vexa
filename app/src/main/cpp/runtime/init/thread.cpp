//
// Created by critical on 11.03.2026.
//

#include <LinuxSyscalls/Utils/Threads.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/ThreadManager.h>

#include "thread.h"
#include "syscalls.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupThreads(Resources &state) {
        if (!state.linuxSyscallHandler) {
            return {Vexa::Common::Code::MissingSyscallHandler, Vexa::Common::Phase::Init,
                    "Missing Syscall handler before thread setup"};
        }

        return SetupParentThread(state);
    }

    Vexa::Common::Result SetupThreadHandlers(Resources &state) {
        state.stackTracker = FEX::LinuxEmulation::Threads::SetupThreadHandlers();
        if (!state.stackTracker) {
            return {
                    Vexa::Common::Code::InternalError,
                    Vexa::Common::Phase::Init,
                    "SetupThreadHandlers failed"
            };
        }

        return {
                Vexa::Common::Code::Ok,
                Vexa::Common::Phase::Init,
                "Thread handlers initialized"
        };
    }

    void TeardownThreadHandlers(Resources &state) {
        if (!state.stackTracker) return;

        FEX::LinuxEmulation::Threads::Shutdown(std::move(state.stackTracker));
    }
}