//
// Created by critical on 13.03.2026.
//

#include <algorithm>
#include <cstdlib>

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
            g_alloc32 = FEX::HLE::Create32BitAllocator();

            // Upstream hands the x32 handler a passthrough allocator because
            // FEXCore's OSAllocator64Bit reserves all usable high VA at init,
            // so a raw ::mmap() can only land below 4GB. That invariant does
            // not hold on Android: bionic's Scudo has no mmap hook, and the
            // kernel freely returns VAs above 4GB — which trips the
            // "values must fit to 32 bits" assert in GuestMmap.
            // MemAllocator32Bit instead owns the low 4GB explicitly: it
            // tracks the guest VA in a page bitset and probes candidates with
            // MAP_FIXED_NOREPLACE, so ART/bionic collisions are skipped on
            // retry and every returned address is guaranteed to fit.
            //
            // The malloc probe below is diagnostic only. Upstream drains
            // glibc/jemalloc's pre-reserved arenas here so later allocations
            // route through the hooked mmap. Bionic's Scudo has no mmap hook,
            // so nothing can be drained; instead record where a handful of
            // mallocs land. Pointers below 4GB mean the host heap is
            // competing for the guest's address space (survivable — the guest
            // allocator retries around it); pointers above are benign for the
            // guest but dangerous if they ever cross the thunk/callback
            // boundary.
            uintptr_t lowest = ~uintptr_t{0};
            uintptr_t highest = 0;
            int below4gb = 0;
            constexpr int kProbes = 64;
            for (int i = 0; i < kProbes; ++i) {
                void *p = ::malloc(1);
                const auto v = reinterpret_cast<uintptr_t>(p);
                lowest = std::min(lowest, v);
                highest = std::max(highest, v);
                below4gb += (v >> 32) == 0;
                ::free(p);
            }
            VEXA_LOGI(
                    env,
                    "FEX",
                    "Host malloc VA probe (32-bit mode)",
                    Vexa::Log::AddFields({
                                                 Vexa::Log::F("below4gb", below4gb),
                                                 Vexa::Log::F("probes", kProbes),
                                                 Vexa::Log::F("lowest", lowest),
                                                 Vexa::Log::F("highest", highest),
                                         }).c_str());
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

    fextl::unique_ptr<FEX::HLE::MemAllocator> TakeGuestAllocator() {
        return std::move(g_alloc32);
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