//
// Created by critical on 12.03.2026.
//

#include "crash_signals.h"

#include <signal.h>
#include <ucontext.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
    constexpr int kSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGSYS};
    struct sigaction g_old[sizeof(kSignals) / sizeof(kSignals[0])]{};

    static inline void WriteLit(const char *s, size_t n) {
        (void) !write(STDERR_FILENO, s, n);
    }

    static inline void WriteHex64(uint64_t v) {
        char b[18] = "0x000000000000000";
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

    template<size_t N>
    static inline void WriteLit(const char (&s)[N]) {
        (void) !write(STDERR_FILENO, s, N - 1); // exclude "\0"
    }

    void ReRaise(int sig) {
        signal(sig, SIG_DFL);
        syscall(SYS_tgkill, getpid(), gettid(), sig);
    }

    void Handler(int sig, siginfo_t *info, void *uctx) {
        (void) uctx;
        WriteLit("[VEXA][CRASH] signal caught: ");
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
        if (info) {
            WriteLit("[VEXA][CRASH] si_code=");
            WriteU32((unsigned) info->si_code);
            if (sig == SIGSEGV || sig == SIGBUS) {
                WriteLit(" fault_addr=");
                WriteHex64((uint64_t) (uintptr_t) info->si_addr);
            }
            if (sig == SIGSYS) {
                WriteLit(" si_syscall=");
                WriteU32((unsigned) info->si_syscall);
                WriteHex64((uint64_t) (uintptr_t) info->si_addr);
                WriteLit(" si_arch=0x");
                WriteHex64((uint64_t) (uint32_t) info->si_arch);
            }
            WriteLit("\n");
        }
#if defined(__aarch64__)
        auto *uc = reinterpret_cast<ucontext_t *>(uctx);
        if (uc) {
            WriteLit("[VEXA][CRASH] host_pc=");
            WriteHex64(uc->uc_mcontext.pc);
            WriteLit(" host_lr=");
            WriteHex64(uc->uc_mcontext.regs[30]);
            WriteLit(" host_sp=");
            WriteHex64(uc->uc_mcontext.sp);
            WriteLit("\n");
        }
#endif
        ReRaise(sig); // Re-raising so debuggerd/tombstone can log crash and handle it like normal
    }
} // namespace

namespace Vexa::Log {
    void InstallSignalHandlers() {
        struct sigaction sa{};
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sa.sa_sigaction = Handler;

        for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
            sigaction(kSignals[i], &sa, &g_old[i]);
        }
    }

    void UninstallSignalHandlers() {
        for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
            sigaction(kSignals[i], &g_old[i], nullptr);
        }
    }
}