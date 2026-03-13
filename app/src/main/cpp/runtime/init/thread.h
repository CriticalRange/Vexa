//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_THREAD_H
#define VEXA_EMULATOR_THREAD_H

#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupThreads(Resources &state);

    Vexa::Common::Result SetupThreadHandlers(Resources &state);

    void TeardownThreadHandlers(Resources &state);
}

#endif //VEXA_EMULATOR_THREAD_H
