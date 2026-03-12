//
// Created by critical on 11.03.2026.
//

#include <Common/HostFeatures.h>

#include "../../logging/native_log.h"
#include "core.h"

namespace Vexa::Runtime::Init {
    LaunchResult
    SetupCore(JNIEnv *env, const Vexa::Common::PreflightPaths &paths, RuntimeState &state) {
        const char *programName =
                paths.executable.empty() ? "unknown" : paths.executable.c_str();

        auto hostFeatures = FEX::FetchHostFeatures();
        state.ctx = FEXCore::Context::Context::CreateNewContext(hostFeatures);
        if (!state.ctx)
            return {
                    10,
                    "CreateNewContext failed",
                    ""
            };
        state.signalDelegator = FEX::HLE::CreateSignalDelegator(
                state.ctx.get(),
                programName,
                hostFeatures.SupportsAVX //FEX checks for this
        );
        if (!state.signalDelegator)
            return {
                    12,
                    "CreateSignalDelegator failed",
                    ""
            };

        state.ctx->SetSignalDelegator(state.signalDelegator.get());

        if (!state.ctx->InitCore())
            return {
                    11,
                    "InitCore failed",
                    ""
            };
        return {
                0,
                "OK",
                ""
        };
    }
}
