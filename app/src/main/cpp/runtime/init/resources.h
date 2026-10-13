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

namespace FEX {
    class GdbServer;
}

namespace FEX::HLE {
    // Forward-declare concrete LinuxEmulation syscall handler type.
    struct ThreadStateObject;

    class SyscallHandler;
}

namespace Vexa::Runtime {
    struct Resources {
        bool fexStarted{false};
        // Probed from the guest ELF header before allocator/syscall setup.
        // Drives CONFIG_IS64BIT_MODE, the allocator path, and the x64/x32
        // syscall handler selection.
        bool is64BitMode{true};
        fextl::unique_ptr<FEXCore::Context::Context> ctx{};
        fextl::unique_ptr<FEX::HLE::SignalDelegator> signalDelegator{};
        fextl::unique_ptr<FEX::HLE::ThunkHandler> thunkHandler{};
        FEX::HLE::ThreadStateObject *parentThread{nullptr};
        fextl::unique_ptr<FEX::LinuxEmulation::Threads::StackTracker> stackTracker{};
        // Raw pointer — kept intentionally so translation units that only see
        // the forward declaration do not need the full GdbServer type. The
        // object lives for the lifetime of the runtime process anyway.
        FEX::GdbServer *gdbServer{nullptr};
        fextl::unique_ptr<FEXCore::HLE::SyscallHandler> syscallHandler{}; // owning base Syscall pointer
        FEX::HLE::SyscallHandler *linuxSyscallHandler{
                nullptr}; //non-owning concrete Syscall pointer
    };
} // namespace Vexa::Runtime

#endif //VEXA_EMULATOR_RESOURCES_H
