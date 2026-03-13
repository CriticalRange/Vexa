//
// Created by critical on 13.03.2026.
//

#ifndef VEXA_EMULATOR_ALLOCATOR_H
#define VEXA_EMULATOR_ALLOCATOR_H

#include "jni.h"

#include "../../common/status.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupAllocator(JNIEnv *env);

    void ShutdownAllocator();
}

#endif //VEXA_EMULATOR_ALLOCATOR_H
