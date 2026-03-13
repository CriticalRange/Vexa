//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_CORE_H
#define VEXA_EMULATOR_CORE_H

#include <jni.h>

#include "../../common/paths.h"
#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result
    SetupCore(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state);
}

#endif //VEXA_EMULATOR_CORE_H
