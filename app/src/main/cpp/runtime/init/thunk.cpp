//
// Created by critical on 11.03.2026.
//

#include <Tools/LinuxEmulation/Thunks.h>

#include "../../common/status.h"
#include "thunk.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupThunks(JNIEnv *env, Resources &state) {
        state.thunkHandler = FEX::HLE::CreateThunkHandler();
        if (!state.thunkHandler.get()) {
            return {Vexa::Common::Code::CreateThunkHandlerFailed, Vexa::Common::Phase::Init,
                    "Create Thunk Handler failed"};
        }

        state.ctx->SetThunkHandler(state.thunkHandler.get());

        return {Vexa::Common::Code::Ok, Vexa::Common::Phase::Init,
                "Thunk handler initialized"};
    }
}