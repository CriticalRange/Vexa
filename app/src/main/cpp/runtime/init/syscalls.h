//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_SYSCALLS_H
#define VEXA_EMULATOR_SYSCALLS_H

#pragma once

#include <jni.h>

#include "../launch.h"
#include "runtime_state.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupSyscalls(JNIEnv *env, RuntimeState &state);

    LaunchResult SetupParentThread(RuntimeState &state);

    void TeardownParentThread(RuntimeState &state);
}

#endif //VEXA_EMULATOR_SYSCALLS_H
