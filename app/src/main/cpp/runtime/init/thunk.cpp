//
// Created by critical on 11.03.2026.
//

#include <Tools/LinuxEmulation/Thunks.h>

#include "thunk.h"

namespace Vexa::Runtime::Init {
    LaunchResult SetupThunks(JNIEnv *env, RuntimeState &state) {
        state.thunkHandler = FEX::HLE::CreateThunkHandler();
        if (!state.thunkHandler.get()) {
            return {
                    13,
                    "ThunkHandler failed",
                    ""
            };
        }

        state.ctx->SetThunkHandler(state.thunkHandler.get());

        return {
                0,
                "OK",
                ""
        };
    }
}