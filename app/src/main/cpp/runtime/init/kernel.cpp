//
// Created by critical on 13.03.2026.
//

#include <cerrno>
#include <cstring>
#include <cstdint>
#include <sys/prctl.h>

#include <Common/Config.h>
#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/PrctlUtils.h>

#include "../../logging/native_log.h"
#include "kernel.h"


#ifndef PR_ARM64_SET_UNALIGN_ATOMIC
#define PR_ARM64_SET_UNALIGN_ATOMIC 0x46455849
#define PR_ARM64_UNALIGN_ATOMIC_EMULATE (1UL << 0)
#define PR_ARM64_UNALIGN_ATOMIC_BACKPATCH (1UL << 1)
#define PR_ARM64_UNALIGN_ATOMIC_STRICT_SPLIT_LOCKS (1UL << 2)
#endif

namespace {
    struct KernelCompatReport {
        static const char *ErrnoName(int e) {
            if (e == 0) return "OK";
            return std::strerror(e);
        }

        bool memModelQueryOk{false};
        bool hardwareTSOEnabled{false};
        bool compatInputSet{false};
        bool unalignedAtomicSet{false};
        int memModelQueryErrno{0};
        int memModelSetErrno{0};
        int compatInputErrno{0};
        int unalignedAtomicErrno{0};
    };

    void SetupTSOEmulation(FEXCore::Context::Context *ctx, KernelCompatReport &r) {
        auto result = prctl(PR_GET_MEM_MODEL, 0, 0, 0, 0);
        if (result == -1) {
            r.memModelQueryErrno = errno;
            return;
        }

        r.memModelQueryOk = true;

        FEX_CONFIG_OPT(TSOEnabled, TSOENABLED);
        if (!TSOEnabled()) {
            return;
        }

        if (result == PR_SET_MEM_MODEL_DEFAULT) {
            result = prctl(PR_SET_MEM_MODEL, PR_SET_MEM_MODEL_TSO, 0, 0, 0);
            if (result == 0) {
                ctx->SetHardwareTSOSupport(true);
                r.hardwareTSOEnabled = true;
            } else {
                r.memModelSetErrno = errno;
            }
            return;
        }

        if (result == PR_SET_MEM_MODEL_TSO) {
            ctx->SetHardwareTSOSupport(true);
            r.hardwareTSOEnabled = true;
        }
    }

    void SetupCompatInput(bool is64Bit, KernelCompatReport &r) {
        auto result = prctl(PR_GET_COMPAT_INPUT, 0, 0, 0, 0);
        if (result == -1) {
            r.compatInputErrno = errno;
            return;
        }

        const unsigned long mode = is64Bit ? PR_SET_COMPAT_INPUT_DISABLE
                                           : PR_SET_COMPAT_INPUT_ENABLE;
        result = prctl(PR_SET_COMPAT_INPUT, mode, 0, 0, 0);
        if (result == 0) {
            r.compatInputSet = true;
        } else {
            r.compatInputErrno = errno;
        }
    }

    void SetupKernelUnalignedAtomics(KernelCompatReport &r) {
        FEX_CONFIG_OPT(StrictInProcessSplitLocks, STRICTINPROCESSSPLITLOCKS);
        FEX_CONFIG_OPT(KernelUnalignedAtomicBackpatching, KERNELUNALIGNEDATOMICBACKPATCHING);

        uint64_t flags = PR_ARM64_UNALIGN_ATOMIC_EMULATE;
        if (StrictInProcessSplitLocks()) {
            flags |=
                    PR_ARM64_UNALIGN_ATOMIC_STRICT_SPLIT_LOCKS;
        }
        if (KernelUnalignedAtomicBackpatching()) {
            flags |=
                    PR_ARM64_UNALIGN_ATOMIC_BACKPATCH;
        }

        auto result = prctl(PR_ARM64_SET_UNALIGN_ATOMIC, flags, 0, 0, 0);
        if (result == 0) {
            r.unalignedAtomicSet = true;
        } else {
            r.unalignedAtomicErrno = errno;
        }
    }
}

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupKernelModes(JNIEnv *env, Resources &state) {
        if (!state.ctx) {
            return {
                    Vexa::Common::Code::InternalError,
                    Vexa::Common::Phase::Init,
                    "Missing context for kernel compatibility init"
            };
        }

        const bool is64Bit = FEXCore::Config::Get_IS64BIT_MODE();
        KernelCompatReport report{};

        SetupTSOEmulation(state.ctx.get(), report);
        SetupKernelUnalignedAtomics(report);
        SetupCompatInput(is64Bit, report);

        VEXA_LOGI(
                env,
                "FEX",
                "Kernel compatibility modes initialized",
                Vexa::Log::AddFields({
                                             Vexa::Log::F("is64", is64Bit),
                                             Vexa::Log::F("memModelQueryOk",
                                                          report.memModelQueryOk),
                                             Vexa::Log::F("hardwareTSOEnabled",
                                                          report.hardwareTSOEnabled),
                                             Vexa::Log::F("compatInputSet", report.compatInputSet),
                                             Vexa::Log::F("unalignedAtomicSet",
                                                          report.unalignedAtomicSet),
                                             Vexa::Log::F("memModelQueryErrno",
                                                          report.memModelQueryErrno),
                                             Vexa::Log::F("memModelSetErrno",
                                                          report.memModelSetErrno),
                                             Vexa::Log::F("compatInputErrno",
                                                          report.compatInputErrno),
                                             Vexa::Log::F("unalignedAtomicErrno",
                                                          report.unalignedAtomicErrno),
                                     }).c_str()
        );

        return {
                Vexa::Common::Code::Ok,
                Vexa::Common::Phase::Init,
                "Kernel checks OK"
        };
    }
}