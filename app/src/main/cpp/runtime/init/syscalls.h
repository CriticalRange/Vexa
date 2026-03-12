//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_SYSCALLS_H
#define VEXA_EMULATOR_SYSCALLS_H

#pragma once

#include <jni.h>

#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupSyscalls(JNIEnv *env, Resources &state);

    Vexa::Common::Result SetupParentThread(Resources &state);

    void TeardownParentThread(Resources &state);
}

#endif //VEXA_EMULATOR_SYSCALLS_H
