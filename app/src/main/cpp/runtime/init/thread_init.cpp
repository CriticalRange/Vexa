//
// Created by critical on 11.03.2026.
//

#include <Tools/LinuxEmulation/LinuxSyscalls/Utils/Threads.h>

#include "thread_init.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupThreads(JNIEnv *env, RuntimeState &state) {
        FEX::LinuxEmulation::Threads::SetupThreadHandlers();
        return {
                0,
                "OK",
                ""
        };
    }
}