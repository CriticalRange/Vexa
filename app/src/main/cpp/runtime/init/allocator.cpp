//
// Created by critical on 13.03.2026.
//

#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/Allocator.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/LinuxAllocator.h>
#include <FEXCore/fextl/vector.h>
#include <FEXCore/fextl/memory.h>

#include "../../logging/native_log.h"
#include "allocator.h"

namespace {
    fextl::unique_ptr<FEX::HLE::MemAllocator> g_alloc32{};
    fextl::vector<FEXCore::Allocator::MemoryRegion> g_base48{};
    fextl::vector<FEXCore::Allocator::MemoryRegion> g_low4gb{};
    bool g_allocatorReady{false};
}

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupAllocator(JNIEnv *env) {
        if (g_allocatorReady) {
            return {
                    Vexa::Common::Code::Ok,
                    Vexa::Common::Phase::Init,
                    "Allocator already initialized!"
            };
        }

        const bool is64 = FEXCore::Config::Get_IS64BIT_MODE();

        if (is64) {
            g_base48 =
                    FEXCore::Allocator::Setup48BitAllocatorIfExists();
        } else {
            constexpr uint64_t First64BitAddr = 0x1'0000'0000ULL;
            g_low4gb = FEXCore::Allocator::StealMemoryRegion(First64BitAddr,
                                                             First64BitAddr + First64BitAddr);

            FEXCore::Allocator::SetupHooks();
            g_alloc32 = FEX::HLE::CreatePassthroughAllocator();
        }

        g_allocatorReady = true;
        VEXA_LOGI(
                env,
                "FEX",
                "Allocator initialized.",
                Vexa::Log::AddFields({
                                             Vexa::Log::F("is64", is64)
                                     }).c_str());

        return {
                Vexa::Common::Code::Ok,
                Vexa::Common::Phase::Init,
                "Allocator initialized"
        };
    }

    void ShutdownAllocator() {
        if (!g_allocatorReady) return;

        g_alloc32.reset();
        FEXCore::Allocator::ClearHooks();

        FEXCore::Allocator::ReclaimMemoryRegion(g_base48);
        FEXCore::Allocator::ReclaimMemoryRegion(g_low4gb);
        g_base48.clear();
        g_low4gb.clear();
        g_allocatorReady = false;
    }
}