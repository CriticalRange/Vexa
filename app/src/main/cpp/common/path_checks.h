//
// Created by critical on 13.03.2026.
//

#ifndef VEXA_EMULATOR_PATH_CHECKS_H
#define VEXA_EMULATOR_PATH_CHECKS_H

#include <string>

namespace Vexa::Common {
    bool IsReadableDir(const std::string &path);

    bool IsExecutable(const std::string &path);
}

#endif //VEXA_EMULATOR_PATH_CHECKS_H
