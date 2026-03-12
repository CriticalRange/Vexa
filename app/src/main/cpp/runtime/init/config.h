//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_CONFIG_H
#define VEXA_EMULATOR_CONFIG_H

#pragma once

#include "../launch.h"
#include "../../common/preflight_paths.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupConfig(const Vexa::Common::PreflightPaths &paths);

    void ShutdownConfig();
}

#endif //VEXA_EMULATOR_CONFIG_H
