//
// Created by critical on 11.03.2026.
//

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
}