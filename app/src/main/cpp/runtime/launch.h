//
// Created by critical on 10.03.2026.
//

#ifndef VEXA_EMULATOR_LAUNCH_H
#define VEXA_EMULATOR_LAUNCH_H

#include <vector>
#include <string>

#include "jni.h"
#include "../common/status.h"
#include "../common/paths.h"

namespace Vexa::Runtime {

    Vexa::Common::Result
    StartRuntime(JNIEnv *env, const Vexa::Common::Paths &preflightPaths,
                 const std::vector<std::string> &launchEnv,
                 const std::vector<std::string> &launchArgs);

    void StopRuntime();
}

#endif //VEXA_EMULATOR_LAUNCH_H
