//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_THUNK_H
#define VEXA_EMULATOR_THUNK_H

#pragma once

#include <jni.h>

#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupThunks(JNIEnv *env, Resources &state);
}

#endif //VEXA_EMULATOR_THUNK_H
