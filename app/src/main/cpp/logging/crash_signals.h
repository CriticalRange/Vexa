//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_CRASH_SIGNALS_H
#define VEXA_EMULATOR_CRASH_SIGNALS_H

namespace FEX::HLE { class SignalDelegator; }

namespace Vexa::Log {
    void InstallSignalHandlers();

    // Registers crash handler through FEX's signal delegation
    void InstallFexCrashHandler(FEX::HLE::SignalDelegator *delegator);

    void UninstallSignalHandlers();
}

#endif //VEXA_EMULATOR_CRASH_SIGNALS_H
