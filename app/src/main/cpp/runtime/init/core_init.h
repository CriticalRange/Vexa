//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_CORE_INIT_H
#define VEXA_EMULATOR_CORE_INIT_H

#pragma once

#include <jni.h>

#include "../launch.h"
#include "../../common/preflight_paths.h"
#include "runtime_state.h"

namespace Vexa::Runtime::Init {
    LaunchResult
    SetupCore(JNIEnv *env, const Vexa::Common::PreflightPaths &paths, RuntimeState &state);
}

#endif //VEXA_EMULATOR_CORE_INIT_H
