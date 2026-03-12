//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_EXECUTE_H
#define VEXA_EMULATOR_EXECUTE_H

#pragma once

#include "../launch.h"
#include "runtime_state.h"
#include "../../common/preflight_paths.h"

namespace Vexa::Runtime::Init {
    LaunchResult
    ExecuteRuntime(JNIEnv *env, const Vexa::Common::PreflightPaths &paths, RuntimeState &state);
}

#endif //VEXA_EMULATOR_EXECUTE_H
