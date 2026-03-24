//
// Created by critical on 12.03.2026.
//

#include "crash_signals.h"

#include <android/log.h>

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
            WriteU32(static_cast<unsigned>(-v));
        } else {
            WriteU32(static_cast<unsigned>(v));
        }
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

    struct UnwindState {
        void **current;
        void **end;
    };

    static _Unwind_Reason_Code CollectBacktraceFrame(_Unwind_Context *context, void *arg) {
        auto *state = reinterpret_cast<UnwindState *>(arg);
        const uintptr_t pc = _Unwind_GetIP(context);
        if (pc == 0) {
            return _URC_NO_REASON;
        }
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        }
        *state->current++ = reinterpret_cast<void *>(pc);
        return _URC_NO_REASON;
    }

    static int CaptureBacktrace(void **buffer, int max_frames) {
#if VEXA_HAVE_EXECINFO && !(defined(__ANDROID__) && __ANDROID_API__ < 33)
        return backtrace(buffer, max_frames);
#else
        UnwindState state{buffer, buffer + max_frames};
        _Unwind_Backtrace(CollectBacktraceFrame, &state);
        return static_cast<int>(state.current - buffer);
#endif
    }

    static void DumpBacktraceToFd(void *const *buffer, int frame_count, int fd) {
#if VEXA_HAVE_EXECINFO && !(defined(__ANDROID__) && __ANDROID_API__ < 33)
        backtrace_symbols_fd(buffer, frame_count, fd);
#else
        if (fd != STDERR_FILENO) {
            return;
        }
        for (int i = 0; i < frame_count; ++i) {
            WriteLit("[VEXA][CRASH] bt#");
            WriteU32(static_cast<unsigned>(i));
            WriteLit(" pc=");
            WriteHex64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buffer[i])));
            WriteLit("\n");
        }
#endif
    }

    static inline void WriteKeyHex(const char *key, uint64_t v) {
        WriteLit("[VEXA][CRASH] ");
        WriteCStr(key);
        WriteLit("=");
        WriteHex64(v);
        WriteLit("\n");
    }

#if defined(__aarch64__)

    static void DumpRegsAArch64(const ucontext_t *uc) {
        for (int i = 0; i < 31; ++i) {
            char name[8] = {'x', 0, 0, 0, 0, 0, 0, 0};
            if (i >= 10) {
                name[1] = '0' + (i / 10);
                name[2] = '0' + (i % 10);
                name[3] = '\0';
            } else {
                name[1] = '0' + i;
                name[2] = '\0';
            }
            WriteKeyHex(name, uc->uc_mcontext.regs[i]);
        }
        WriteKeyHex("sp", uc->uc_mcontext.sp);
        WriteKeyHex("pc", uc->uc_mcontext.pc);
        WriteKeyHex("pstate", uc->uc_mcontext.pstate);
    }

#endif

    inline void WriteAddrInfo(const char *label, uint64_t addr) {
        WriteLit("[VEXA][CRASH] ");
        WriteCStr(label);
        WriteLit("=");
        WriteHex64(addr);

        Dl_info info{};
        if (dladdr(reinterpret_cast<void *>(addr), &info)) {
            if (info.dli_fname) {
                WriteLit(" module=");
                WriteCStr(info.dli_fname);
            }
            if (info.dli_fbase) {
                WriteLit(" offset=");
                WriteHex64(addr - reinterpret_cast<uint64_t>(info.dli_fbase));
            }
            if (info.dli_sname) {
                WriteLit(" sym=");
                WriteCStr(info.dli_sname);
                if (info.dli_saddr) {
                    WriteLit("+");
                    WriteHex64(addr - reinterpret_cast<uint64_t>(info.dli_saddr));
                }
            }
        }
        WriteLit("\n");
    }

    void InstallAltStackIfNeeded() {
        if (g_altstack_installed) {
            return;
        }

        void *mem = mmap(nullptr, kAltStackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

    void Handler(int sig, siginfo_t *info, void *uctx) {
        (void) uctx;
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            _exit(128 + sig);
        }
        __android_log_print(
                ANDROID_LOG_FATAL,
                kCrashTag,
                "VEXA CRASH: sig=%d addr=%p pid=%d tid=%ld",
                sig,
                info ? info->si_addr : nullptr,
                getpid(),
                static_cast<long>(gettid()));
        void *bt[64]{};
        const int bt_size = CaptureBacktrace(bt, 64);
        if (bt_size > 0) {
            __android_log_print(ANDROID_LOG_FATAL, kCrashTag, "native backtrace (%d frames):",
                                bt_size);
            // NDK-compatible backtrace dump.
            DumpBacktraceToFd(bt, bt_size, STDERR_FILENO);
        }
        WriteLit("[VEXA][CRASH] An error occured! pid=");
        WriteU32(static_cast<unsigned>(getpid()));
        WriteLit("\n[VEXA][CRASH] tid=");
        WriteU32(static_cast<unsigned>(gettid()));
        WriteLit("\n[VEXA][CRASH] signal caught: ");
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
            WriteS32(info->si_code);
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
            WriteAddrInfo("pc_info", uc->uc_mcontext.pc);
            WriteAddrInfo("lr_info", uc->uc_mcontext.regs[30]);
            if (uc->uc_mcontext.pc == 0) {
                WriteLit("[VEXA][CRASH] hint=pc_is_null_probable_indirect_call\n");
                const uint64_t lr = uc->uc_mcontext.regs[30];
                WriteAddrInfo("lr_callsite", lr >= 4 ? (lr - 4) : lr);
                WriteAddrInfo("x16_target", uc->uc_mcontext.regs[16]);
                WriteAddrInfo("x17_target", uc->uc_mcontext.regs[17]);
            }
            WriteLit("[VEXA][CRASH] dumping regs here:\n");
            DumpRegsAArch64(uc);
        }
#endif
        _exit(1);
    }
} // namespace

namespace Vexa::Log {
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
