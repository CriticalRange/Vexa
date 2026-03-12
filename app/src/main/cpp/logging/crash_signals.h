//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_CRASH_SIGNALS_H
#define VEXA_EMULATOR_CRASH_SIGNALS_H

#pragma once
namespace Vexa::Log {
    void InstallSignalHandlers();

    void UninstallSignalHandlers();
}

#endif //VEXA_EMULATOR_CRASH_SIGNALS_H
