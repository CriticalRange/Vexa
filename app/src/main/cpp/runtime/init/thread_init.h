//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_THREAD_INIT_H
#define VEXA_EMULATOR_THREAD_INIT_H

#pragma once

#include <jni.h>

#include "../launch.h"
#include "runtime_state.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupThreads(JNIEnv *env, RuntimeState &state);
}

#endif //VEXA_EMULATOR_THREAD_INIT_H
