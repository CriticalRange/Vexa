//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_RUNTIME_STATE_H
#define VEXA_EMULATOR_RUNTIME_STATE_H

#pragma once

#include <FEXCore/Core/Context.h>
#include <FEXCore/fextl/memory.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/SignalDelegator.h>

namespace Vexa::Runtime {
    struct RuntimeState {
        bool fexStarted{false};
        fextl::unique_ptr<FEXCore::Context::Context> ctx{};
        fextl::unique_ptr<FEX::HLE::SignalDelegator> signalDelegator{};
        FEXCore::Core::InternalThreadState *parentThread{nullptr};
    };
} // namespace Vexa::Runtime

#endif //VEXA_EMULATOR_RUNTIME_STATE_H
