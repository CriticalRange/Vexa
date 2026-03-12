//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_EXECUTE_H
#define VEXA_EMULATOR_EXECUTE_H

#pragma once

#include "resources.h"
#include "../../common/paths.h"
#include "../../common/status.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result
    ExecuteRuntime(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state);
}

#endif //VEXA_EMULATOR_EXECUTE_H
