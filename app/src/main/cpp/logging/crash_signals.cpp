//
// Created by critical on 12.03.2026.
//

#include "crash_signals.h"

#include <signal.h>
#include <android/log.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <Tools/LinuxEmulation/LinuxSyscalls/SignalDelegator.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/ThreadManager.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/Core/Context.h>

#if __has_include(<android/set_abort_message.h>)

#include <android/set_abort_message.h>

#define VEXA_HAVE_ANDROID_SET_ABORT_MESSAGE 1
#else
#define VEXA_HAVE_ANDROID_SET_ABORT_MESSAGE 0
#endif

#if __has_include(<execinfo.h>)

#include <execinfo.h>

#define VEXA_HAVE_EXECINFO 1
#else
#define VEXA_HAVE_EXECINFO 0
#endif

#include <signal.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <ucontext.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unwind.h>

namespace {
    constexpr const char *kCrashTag = "VEXA_RUNTIME";
    constexpr int kSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGSYS};
    struct sigaction g_old[sizeof(kSignals) / sizeof(kSignals[0])]{};
    static volatile sig_atomic_t g_in_handler = 0;
    static bool g_handlers_installed = false;
    static bool g_altstack_installed = false;
    static void *g_altstack_mem = nullptr;
    static size_t g_altstack_size = 0;
    static stack_t g_prev_altstack{};
    static uint32_t g_fex_crash_seq = 0;
    constexpr size_t kAltStackSize = 64 * 1024;

    static inline void WriteLit(const char *s, size_t n) {
        (void) !write(STDERR_FILENO, s, n);
    }

    static inline void WriteHex64(uint64_t v) {
        char b[19] = "0x0000000000000000";
        for (int i = 17; i >= 2; --i) {
            b[i] = "0123456789abcdef"[v & 0xF];
            v >>= 4;
        }
        WriteLit(b, sizeof(b) - 1);
    }

    static inline void WriteU32(unsigned v) {
        char buf[16];
        int i = 0;
        if (v == 0) {
            WriteLit("0", 1);
            return;
        }

        while (v && i < static_cast<int>(sizeof(buf))) {
            buf[i++] = "0123456789"[v % 10U];
            v /= 10U;
        }

        while (i--) {
            (void) !write(STDERR_FILENO, &buf[i], 1);
        }
    }

    static inline void WriteS32(int v) {
        if (v < 0) {
            WriteLit("-", 1);
            WriteU32(static_cast<unsigned>(-(int64_t) v));
            return;
        }
        WriteU32(static_cast<unsigned>(v));
    }

    static inline void WriteCStr(const char *s) {
        if (!s) {
            return;
        }
        size_t n = 0;
        while (s[n] && n < 4096) {
            ++n;
        }
        WriteLit(s, n);
    }

    template<size_t N>
    static inline void WriteLit(const char (&s)[N]) {
        (void) !write(STDERR_FILENO, s, N - 1); // exclude "\0"
    }

    static inline void WriteKeyHex(const char *key, uint64_t v) {
        WriteLit("[VEXA][CRASH] ");
        WriteCStr(key);
        WriteLit("=");
        WriteHex64(v);
        WriteLit("\n");
    }

    static inline bool ShouldDumpFexCrash(uint32_t seq) {
        return seq <= 16 || ((seq & 0xFFu) == 0);
    }

    static inline void DumpHostState(const ucontext_t *uc) {
#if defined(__aarch64__)
        if (!uc) return;
        WriteKeyHex("host_pc", uc->uc_mcontext.pc);
        WriteKeyHex("host_lr", uc->uc_mcontext.regs[30]);
        WriteKeyHex("host_sp", uc->uc_mcontext.sp);
        WriteKeyHex("host_x0", uc->uc_mcontext.regs[0]);
        WriteKeyHex("host_x1", uc->uc_mcontext.regs[1]);
        WriteKeyHex("host_x2", uc->uc_mcontext.regs[2]);
        WriteKeyHex("host_x3", uc->uc_mcontext.regs[3]);
#endif
    }

    static inline void DumpGuestStateRaw(FEXCore::Core::InternalThreadState *Thread) {
        if (!Thread || !Thread->CurrentFrame) return;
        WriteKeyHex("guest_rip", Thread->CurrentFrame->State.rip);
        const auto &g = Thread->CurrentFrame->State.gregs;
        WriteKeyHex("guest_rsp", g[4]);
        WriteKeyHex("guest_rbp", g[5]);
        WriteKeyHex("guest_rax", g[0]);
        WriteKeyHex("guest_rcx", g[1]);
        WriteKeyHex("guest_rdx", g[2]);
        WriteKeyHex("guest_rsi", g[6]);
        WriteKeyHex("guest_rdi", g[7]);
        WriteKeyHex("in_syscall_info", Thread->CurrentFrame->InSyscallInfo);
    }

    static inline void WriteSigName(int sig) {
        WriteLit("[VEXA][CRASH] signal_name=");
        switch (sig) {
            case SIGSEGV:
                WriteLit("SIGSEGV\n");
                break;
            case SIGABRT:
                WriteLit("SIGABRT\n");
                break;
            case SIGBUS:
                WriteLit("SIGBUS\n");
                break;
            case SIGILL:
                WriteLit("SIGILL\n");
                break;
            case SIGFPE:
                WriteLit("SIGFPE\n");
                break;
            case SIGSYS:
                WriteLit("SIGSYS\n");
                break;
            default:
                WriteLit("UNKNOWN\n");
                break;
        }
    }

    static inline void WriteProbableReason(int sig, const siginfo_t *si, const ucontext_t *uc) {
        WriteLit("[VEXA][CRASH] probable reason=");
        if (!si) {
            WriteLit("Unknown\n");
            return;
        }

        if (sig == SIGSEGV) {
            const bool is_maperr = (si->si_code == SEGV_MAPERR);
            const bool is_accerr = (si->si_code == SEGV_ACCERR);
            const bool null_addr = (si->si_addr == nullptr);

            if (is_maperr && null_addr) {
#if defined(__aarch64__)
                if (uc && uc->uc_mcontext.pc == 0) {
                    WriteLit("null_indirect_branch_pc0\n");
                } else {
                    WriteLit("null_deref_or_null_fnptr\n");
                }
#else
                WriteLit("null_deref_or_null_fnptr\n");
#endif
                return;
            }

            if (is_maperr) {
                WriteLit("segv_maperr\n");
                return;
            }
            if (is_accerr) {
                WriteLit("segv_accerr\n");
                return;
            }
            WriteLit("segv_other\n");
            return;
        }

        if (sig == SIGBUS) {
            if (si->si_code == BUS_ADRALN) {
                WriteLit("bus_adraln\n");
            }
            if (si->si_code == BUS_ADRERR) {
                WriteLit("bus_adrerr\n");
            }
#ifdef BUS_OBJERR
            if (si->si_code == BUS_OBJERR) {
                WriteLit("bus_objerr\n");
            }
#endif
            WriteLit("bus_other\n");
            return;
        }
        if (sig == SIGABRT) {
            WriteLit("abort\n");
            return;
        }
        if (sig == SIGFPE) {
            WriteLit("arithmetic_fault\n");
            return;
        }
        if (sig == SIGSYS) {
            WriteLit("bad_syscall\n");
            return;
        }
        WriteLit("unknown\n");
    }

    static void DumpSignalContext(bool from_fex, int sig, const siginfo_t *si, void *uctx) {
        WriteLit("[VEXA][CRASH] === crash dump begin ===\n");
        WriteLit("[VEXA][CRASH] origin=");
        if (from_fex) WriteLit("fex-host-handler\n");
        else WriteLit("runtime-sigaction\n");

        WriteLit("[VEXA][CRASH] pid=");
        WriteU32(static_cast<unsigned>(getpid()));
        WriteLit(" tid=");
        WriteU32(static_cast<unsigned>(syscall(SYS_gettid)));
        WriteLit("\n");

        WriteLit("[VEXA][CRASH] signal=");
        WriteS32(sig);
        if (si) {
            WriteLit(" si_code=");
            WriteS32(si->si_code);
            if (sig == SIGSEGV || sig == SIGBUS) {
                WriteLit(" fault_addr=");
                WriteHex64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(si->si_addr)));
            }
            if (sig == SIGSYS) {
                WriteLit(" si_syscall=");
                WriteU32(static_cast<unsigned>(si->si_syscall));
                WriteLit(" si_arch=");
                WriteHex64(static_cast<uint64_t>(static_cast<uint32_t>(si->si_arch)));
            }
        }
        WriteLit("\n");
        WriteSigName(sig);
        const auto *uc = reinterpret_cast<const ucontext_t *>(uctx);
        if (si) WriteProbableReason(sig, si, uc);
    }

    void InstallAltStackIfNeeded() {
        if (g_altstack_installed) {
            return;
        }

        void *mem = mmap(nullptr, kAltStackSize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            __android_log_print(ANDROID_LOG_WARN, kCrashTag,
                                "Failed to allocate alt signal stack errno=%d", errno);
            return;
        }

        stack_t ss{};
        ss.ss_sp = mem;
        ss.ss_size = kAltStackSize;
        ss.ss_flags = 0;

        if (sigaltstack(&ss, &g_prev_altstack) != 0) {
            __android_log_print(ANDROID_LOG_WARN, kCrashTag,
                                "Failed to install alt signal stack errno=%d", errno);
            munmap(mem, kAltStackSize);
            return;
        }

        g_altstack_mem = mem;
        g_altstack_size = kAltStackSize;
        g_altstack_installed = true;
        __android_log_print(ANDROID_LOG_INFO, kCrashTag,
                            "Installed alt signal stack size=%zu", kAltStackSize);
    }

    void UninstallAltStackIfNeeded() {
        if (!g_altstack_installed) {
            return;
        }

        // Best-effort restore of previous alt stack configuration.
        if (sigaltstack(&g_prev_altstack, nullptr) != 0) {
            __android_log_print(ANDROID_LOG_WARN, kCrashTag,
                                "Failed to restore previous alt signal stack errno=%d", errno);
        }

        if (g_altstack_mem && g_altstack_size > 0) {
            munmap(g_altstack_mem, g_altstack_size);
        }
        g_altstack_mem = nullptr;
        g_altstack_size = 0;
        g_altstack_installed = false;
        memset(&g_prev_altstack, 0, sizeof(g_prev_altstack));
    }

    bool FexCrashHandler(FEXCore::Core::InternalThreadState *Thread,
                         int Signal,
                         void *info,
                         void *uctx) {
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            // This is re-entry, won't try to log again
            // we'll let FEX handle it instead.
            return false;
        }

        const uint32_t seq = __atomic_add_fetch(&g_fex_crash_seq, 1u, __ATOMIC_RELAXED);
        if (!ShouldDumpFexCrash(seq)) {
            __atomic_store_n(&g_in_handler, 0, __ATOMIC_RELEASE);
            return false;
        }

        const auto *si = reinterpret_cast<const siginfo_t *>(info);
        const auto *uc = reinterpret_cast<const ucontext_t *>(uctx);

        WriteLit("[VEXA][CRASH] === crash dump begin ===\n");
        WriteLit("[VEXA][CRASH] origin=fex-host-handler\n");
        WriteLit("[VEXA][CRASH] crash_seq=");
        WriteU32(seq);
        WriteLit("\n");
        WriteLit("[VEXA][CRASH] pid=");
        WriteU32(static_cast<unsigned>(getpid()));
        WriteLit(" tid=");
        WriteU32(static_cast<unsigned>(syscall(SYS_gettid)));
        WriteLit("\n");

        WriteLit("[VEXA][CRASH] signal=");
        WriteS32(Signal);
        if (si) {
            WriteLit(" si_code=");
            WriteS32(si->si_code);
            if (Signal == SIGSEGV || Signal == SIGBUS) {
                WriteLit(" fault_addr=");
                WriteHex64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(si->si_addr)));
            }
        }

        WriteLit("\n");
        WriteSigName(Signal);
        WriteProbableReason(Signal, si, uc);
        DumpHostState(uc);
        DumpGuestStateRaw(Thread);
        WriteLit("[VEXA][CRASH] === crash dump end ===\n");

        __atomic_store_n(&g_in_handler, 0, __ATOMIC_RELEASE);
        return false; // let FEX Continue handling
    }

    void Handler(int sig, siginfo_t *info, void *uctx) {
        (void) uctx;
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            _exit(128 + sig);
        }
        WriteLit("[VEXA][CRASH] === end of crash dump ===\n");
        DumpSignalContext(/*from_fex=*/false, sig, info, uctx);
        _exit(128 + sig);
    }
} // namespace

namespace Vexa::Log {
    void InstallFexCrashHandler(FEX::HLE::SignalDelegator *delegator) {
        if (!delegator) return;

        constexpr int signals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGSYS};
        for (int sig: signals) {
            delegator->RegisterHostSignalHandler(
                    sig,
                    FexCrashHandler,
                    /*Required=*/true // This will ensure fex installs the crash handler
            );
        }
        __android_log_print(ANDROID_LOG_INFO, kCrashTag,
                            "FEX host crash handler registered for %zu signals",
                            sizeof(signals) / sizeof(signals[0]));
    }

    void InstallSignalHandlers() {
        if (g_handlers_installed) {
            return;
        }

        InstallAltStackIfNeeded();

        struct sigaction sa{};
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
        sa.sa_sigaction = Handler;

#if VEXA_HAVE_ANDROID_SET_ABORT_MESSAGE
        android_set_abort_message("VEXA: Host crash in runtime_worker (FEX thunk or JIT)");
#endif

        for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
            if (sigaction(kSignals[i], &sa, &g_old[i]) != 0) {
                __android_log_print(ANDROID_LOG_WARN, kCrashTag,
                                    "sigaction install failed sig=%d errno=%d", kSignals[i], errno);
            }
        }
        g_handlers_installed = true;
    }

    void UninstallSignalHandlers() {
        if (!g_handlers_installed) {
            return;
        }

        for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
            sigaction(kSignals[i], &g_old[i], nullptr);
        }
        g_handlers_installed = false;
        UninstallAltStackIfNeeded();
    }
}
