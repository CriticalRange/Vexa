//
// Created by critical on 11.03.2026.
//

#include <Tools/LinuxEmulation/LinuxSyscalls/ThreadManager.h>

#include "thread.h"
#include "syscalls.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupThreads(RuntimeState &state) {
        if (!state.linuxSyscallHandler) {
            return {
                    15,
                    "Syscall handler missing before thread setup",
                    ""
            };
        }

        return SetupParentThread(state);
    }
}