//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_CONFIG_H
#define VEXA_EMULATOR_CONFIG_H

#include "jni.h"

#include "../../common/paths.h"
#include "../../common/status.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result
    SetupConfig(JNIEnv *env, const Vexa::Common::Paths &paths, char **envp);

    void ShutdownConfig();
}

#endif //VEXA_EMULATOR_CONFIG_H
