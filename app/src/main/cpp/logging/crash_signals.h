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

    // Opens artifactDir/crash.log before the guest runs. Signal handlers only write this fd.
    void OpenCrashLog(const char *artifactDir);
}

#endif //VEXA_EMULATOR_CRASH_SIGNALS_H
