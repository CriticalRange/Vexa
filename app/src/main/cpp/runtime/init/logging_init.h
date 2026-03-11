//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_LOGGING_INIT_H
#define VEXA_EMULATOR_LOGGING_INIT_H

#pragma once

namespace Vexa::Runtime::Init {
    void InstallLogHandlers();

    void UninstallLogHandlers();
}

#endif //VEXA_EMULATOR_LOGGING_INIT_H
