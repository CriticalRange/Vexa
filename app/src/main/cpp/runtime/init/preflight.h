#ifndef VEXA_EMULATOR_PREFLIGHT_H
#define VEXA_EMULATOR_PREFLIGHT_H

#include "../../common/paths.h"
#include "../../common/status.h"

namespace Vexa::Runtime {
    Vexa::Common::Result RunPreflight(const Vexa::Common::Paths &p);
} //namespace Vexa::Runtime

#endif //VEXA_EMULATOR_PREFLIGHT_H
