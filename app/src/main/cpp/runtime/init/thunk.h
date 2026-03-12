//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_THUNK_H
#define VEXA_EMULATOR_THUNK_H

#pragma once

#include <jni.h>

#include "../launch.h"
#include "runtime_state.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupThunks(JNIEnv *env, RuntimeState &state);
}

#endif //VEXA_EMULATOR_THUNK_H
