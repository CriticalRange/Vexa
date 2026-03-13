//
// Created by critical on 11.03.2026.
//

#ifndef VEXA_EMULATOR_RESOURCES_H
#define VEXA_EMULATOR_RESOURCES_H

#include <FEXCore/Core/Context.h>
#include <FEXCore/fextl/memory.h>
#include <Tools/LinuxEmulation/Thunks.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/SignalDelegator.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/Utils/Threads.h>
#include <FEXCore/HLE/SyscallHandler.h>

namespace FEX::HLE {
    // Forward-declare concrete LinuxEmulation syscall handler type.
    struct ThreadStateObject;

    class SyscallHandler;
}

namespace Vexa::Runtime {
    struct Resources {
        bool fexStarted{false};
        fextl::unique_ptr<FEXCore::Context::Context> ctx{};
        fextl::unique_ptr<FEX::HLE::SignalDelegator> signalDelegator{};
        fextl::unique_ptr<FEX::HLE::ThunkHandler> thunkHandler{};
        FEX::HLE::ThreadStateObject *parentThread{nullptr};
        fextl::unique_ptr<FEX::LinuxEmulation::Threads::StackTracker> stackTracker{};
        fextl::unique_ptr<FEXCore::HLE::SyscallHandler> syscallHandler{}; // owning base Syscall pointer
        FEX::HLE::SyscallHandler *linuxSyscallHandler{
                nullptr}; //non-owning concrete Syscall pointer
    };
} // namespace Vexa::Runtime

#endif //VEXA_EMULATOR_RESOURCES_H
