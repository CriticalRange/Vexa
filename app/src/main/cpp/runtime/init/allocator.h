//
// Created by critical on 13.03.2026.
//

#ifndef VEXA_EMULATOR_ALLOCATOR_H
#define VEXA_EMULATOR_ALLOCATOR_H

#include "jni.h"

#include "../../common/status.h"

#include <FEXCore/fextl/memory.h>

namespace FEX::HLE {
    class MemAllocator;
}

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupAllocator(JNIEnv *env);

    // Transfers the low-address allocator to the x32 syscall handler.
    // Returns nullptr when the runtime is in 64-bit mode.
    fextl::unique_ptr<FEX::HLE::MemAllocator> TakeGuestAllocator();

    void ShutdownAllocator();
}

#endif //VEXA_EMULATOR_ALLOCATOR_H
