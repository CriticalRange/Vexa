//
// Created by critical on 13.03.2026.
//

#ifndef VEXA_EMULATOR_CLIENT_H
#define VEXA_EMULATOR_CLIENT_H

#include "jni.h"

#include "../../common/paths.h"
#include "../../common/status.h"

#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupClient(JNIEnv *env,
                                     const Vexa::Common::Paths &paths);

    void TrackClientFDs(JNIEnv *env, Resources
    &state);
}

#endif //VEXA_EMULATOR_CLIENT_H
