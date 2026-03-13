//
// Created by critical on 12.03.2026.
//

#ifndef VEXA_EMULATOR_STATUS_H
#define VEXA_EMULATOR_STATUS_H

#include <string>
#include <utility>

namespace Vexa::Common {
    enum class Phase : int {
        Preflight = 1,
        Init = 2,
        Execute = 3,
    };
    enum class Code : int {
        Ok = 0,

        // Preflight (100+)
        BadWorkingDir = 101,
        BadRootfs = 102,
        BadThunkHost = 103,
        BadThunkGuest = 104,
        BadExecutable = 105,
        BadArtifactDir = 106,
        InternalError = 199,

        // Init (200+)
        AlreadyStarted = 201,
        CreateContextFailed = 210,
        InitCoreFailed = 211,
        CreateSignalDelegatorFailed = 212,
        CreateThunkHandlerFailed = 213,
        CreateSyscallHandlerFailed = 214,
        MissingSyscallHandler = 215,
        UnexpectedSyscallHandlerType = 216,
        CreateParentThreadFailed = 217,
        MissingSignalDelegator = 218,
        MissingThunkHandler = 219,

        // Execute (300+)
        ExecutePrereqMissing = 320,
        ElfLoaderFailed = 321,
        MapMemoryFailed = 322,
    };

    struct Result {
        Code code{Code::Ok};
        Phase phase{Phase::Init};
        std::string reason;
        std::string detail;

        [[nodiscard]] bool Ok() const {
            return code == Code::Ok;
        }

        static Result Success(Phase p = Phase::Init) {
            return {
                    Code::Ok,
                    p,
                    "OK",
                    ""
            };
        }

        static Result Failure(Code c, Phase p, std::string r, std::string d = {}) {
            return {
                    c,
                    p,
                    std::move(r),
                    std::move(d)
            };
        }
    };

    inline constexpr int ToInt(Code c) {
        return static_cast<int>(c);
    }
} // namespace Vexa::Common

#endif //VEXA_EMULATOR_STATUS_H
