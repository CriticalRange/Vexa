//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_LOGGING_H
#define VEXA_EMULATOR_LOGGING_H

#include <string>

namespace Vexa::Runtime::Init {
    bool InitFexFileLogSink(const std::string &logPath);

    void CloseFexFileLogSink();

    void InstallLogHandlers();

    void UninstallLogHandlers();

    int GetFexFileLogSinkFd();
}

#endif //VEXA_EMULATOR_LOGGING_H
