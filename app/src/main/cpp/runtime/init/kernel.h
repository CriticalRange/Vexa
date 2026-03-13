//
// Created by critical on 13.03.2026.
//

//    Right now FEX::Kernel::Init(...) lives in
//    FEXInterpreter.cpp:~326, which is built into the FEX
//    executable target, not a reusable library API.
//    So we're re-implementing it here with also Android compatibility in mind.

#ifndef VEXA_EMULATOR_KERNEL_H
#define VEXA_EMULATOR_KERNEL_H

#include "jni.h"

#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupKernelModes(JNIEnv *env, Resources &state);
}

#endif //VEXA_EMULATOR_KERNEL_H
