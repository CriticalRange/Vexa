//
// Created by critical on 11.03.2026.
//

#include <Common/HostFeatures.h>
#include <FEXCore/Core/DiskCacheFileMapper.h>
#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/AllocatorHooks.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "../../logging/native_log.h"
#include "kernel.h"
#include "core.h"

namespace Vexa::Runtime::Init {
    namespace {
        void *MapDiskCache(FEXCore::File::File::FileHandleType handle, uint64_t size) {
            const int fd = ::dup(handle);
            if (fd < 0) return nullptr;
            void *mapping = FEXCore::Allocator::mmap(nullptr, size, PROT_READ,
                                                     MAP_SHARED | MAP_NORESERVE, fd, 0);
            ::close(fd);
            // FEX transparently falls back to pread when mapping fails.
            return mapping == MAP_FAILED ? nullptr : mapping;
        }
    }
    Vexa::Common::Result
    SetupCore(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();

        // Context construction initializes the disk cache; register first.
        FEXCore::DiskCache::SetFileMapper(MapDiskCache);
        auto hostFeatures = FEX::FetchHostFeatures();
        const char *diagnostics = std::getenv("VEXA_FEX_DIAGNOSTICS");
        if (diagnostics && std::strcmp(diagnostics, "1") == 0) {
            FEX_CONFIG_OPT(diskCache, DISKCACHE);
            FEX_CONFIG_OPT(diskMapping, DISKCACHEFILEMAPPING);
            FEX_CONFIG_OPT(dynamicL1, DYNAMICL1CACHE);
            FEX_CONFIG_OPT(disableL2, DISABLEL2CACHE);
            FEX_CONFIG_OPT(smc, SMCCHECKS);
            FEX_CONFIG_OPT(tso, TSOENABLED);
            FEX_CONFIG_OPT(vectorTso, VECTORTSOENABLED);
            FEX_CONFIG_OPT(memcpyTso, MEMCPYSETTSOENABLED);
            FEX_CONFIG_OPT(strictSplit, STRICTINPROCESSSPLITLOCKS);
            FEX_CONFIG_OPT(kernelBackpatch, KERNELUNALIGNEDATOMICBACKPATCHING);
            FEX_CONFIG_OPT(x87Reduced, X87REDUCEDPRECISION);
            const int sveVl = prctl(PR_SVE_GET_VL);
            VEXA_LOGI(env, "FEX", "Effective FEX options and host capabilities",
                Vexa::Log::AddFields({
                    Vexa::Log::F("diskCache", diskCache()), Vexa::Log::F("diskMapping", diskMapping()),
                    Vexa::Log::F("dynamicL1", dynamicL1()), Vexa::Log::F("disableL2", disableL2()),
                    Vexa::Log::F("smc", static_cast<unsigned>(smc())), Vexa::Log::F("tso", tso()),
                    Vexa::Log::F("vectorTso", vectorTso()), Vexa::Log::F("memcpyTso", memcpyTso()),
                    Vexa::Log::F("strictSplit", strictSplit()), Vexa::Log::F("kernelBackpatch", kernelBackpatch()),
                    Vexa::Log::F("x87Reduced", x87Reduced()), Vexa::Log::F("sveVlBytes", sveVl < 0 ? -1 : (sveVl & PR_SVE_VL_LEN_MASK)),
                    Vexa::Log::F("sve128", bool(hostFeatures.SupportsSVE128)),
                    Vexa::Log::F("sve256", bool(hostFeatures.SupportsSVE256)),
                    Vexa::Log::F("avx", bool(hostFeatures.SupportsAVX)), Vexa::Log::F("rcpc", bool(hostFeatures.SupportsRCPC)),
                    Vexa::Log::F("i8mm", bool(hostFeatures.SupportsI8MM)), Vexa::Log::F("mops", bool(hostFeatures.SupportsMOPS))
                }).c_str());
        }
        state.ctx = FEXCore::Context::Context::CreateNewContext(hostFeatures);
        if (!state.ctx) {
            return {Vexa::Common::Code::CreateContextFailed, Vexa::Common::Phase::Init,
                    "CreateNewContext failed"};
        }
        auto kernelResult = SetupKernelModes(env, state);
        if (!kernelResult.Ok()) {
            return kernelResult;
        }
        state.signalDelegator = FEX::HLE::CreateSignalDelegator(
                state.ctx.get(),
                programName,
                hostFeatures.SupportsAVX,
                hostFeatures.SupportsSVE256
        );
        if (!state.signalDelegator) {
            return {Vexa::Common::Code::CreateSignalDelegatorFailed, Vexa::Common::Phase::Init,
                    "CreateSignalDelegator failed"};
        }

        state.ctx->SetSignalDelegator(state.signalDelegator.get());

        if (!state.ctx->InitCore()) {
            return {Vexa::Common::Code::InitCoreFailed, Vexa::Common::Phase::Init,
                    "InitCore failed"};
        }
        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Core initialized"};
    }
}
