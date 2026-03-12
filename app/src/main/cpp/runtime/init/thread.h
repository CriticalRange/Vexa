//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_THREAD_H
#define VEXA_EMULATOR_THREAD_H

#pragma once

#include "../../common/status.h"
#include "resources.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupThreads(Resources &state);
}

#endif //VEXA_EMULATOR_THREAD_H
