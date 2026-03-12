//
// Created by critical on 11.03.2026.
//

#include <Common/HostFeatures.h>

#include "../../logging/native_log.h"
#include "core.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result
    SetupCore(JNIEnv *env, const Vexa::Common::Paths &paths, Resources &state) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();

        auto hostFeatures = FEX::FetchHostFeatures();
        state.ctx = FEXCore::Context::Context::CreateNewContext(hostFeatures);
        if (!state.ctx) {
            return {Vexa::Common::Code::CreateContextFailed, Vexa::Common::Phase::Init,
                    "CreateNewContext failed"};
        }
        state.signalDelegator = FEX::HLE::CreateSignalDelegator(
                state.ctx.get(),
                programName,
                hostFeatures.SupportsAVX //FEX checks for this
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
