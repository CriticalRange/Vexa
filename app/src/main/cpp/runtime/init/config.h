//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_CONFIG_H
#define VEXA_EMULATOR_CONFIG_H

#include "jni.h"

#include "../../common/paths.h"
#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result
    SetupConfig(JNIEnv *env, const Vexa::Common::Paths &paths,
                const std::vector<std::string> &launchEnv,
                Resources &state);

    void ShutdownConfig();
}

#endif //VEXA_EMULATOR_CONFIG_H
