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
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include <Tools/LinuxEmulation/LinuxSyscalls/SignalDelegator.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/ThreadManager.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Utils/TypeDefines.h>
#include <stdio.h>

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
    static uint32_t g_early_count = 0;
    static volatile sig_atomic_t g_early_busy = 0;
    static int g_crash_fd = -1;
    static int g_last_fd = -1;
    static int g_maps_fd = -1;
    static int g_maps_out_fd = -1;
    static char g_capture[4096];
    static size_t g_capture_n = 0;
    static bool g_capture_on = false;
    static bool g_capture_to_log = false;
    constexpr size_t kAltStackSize = 64 * 1024;
    constexpr uint32_t kMaxCrashRecords = 32;

    static inline void WriteFd(int fd, const char *s, size_t n) {
        if (fd < 0 || !s || n == 0) return;
        while (n) {
            const ssize_t w = write(fd, s, n);
            if (w < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (w == 0) break;
            s += w;
            n -= static_cast<size_t>(w);
        }
    }

    static inline void WriteLit(const char *s, size_t n) {
        if (!s || n == 0) return;
        if (g_capture_on) {
            if (g_capture_to_log) {
                WriteFd(g_crash_fd >= 0 ? g_crash_fd : STDERR_FILENO, s, n);
            }
            if (g_capture_n < sizeof(g_capture)) {
                size_t c = n;
                if (g_capture_n + c > sizeof(g_capture)) {
                    c = sizeof(g_capture) - g_capture_n;
                }
                memcpy(g_capture + g_capture_n, s, c);
                g_capture_n += c;
            }
            return;
        }
        WriteFd(g_crash_fd >= 0 ? g_crash_fd : STDERR_FILENO, s, n);
    }

    static inline void BeginCapture(bool to_log) {
        g_capture_n = 0;
        g_capture_on = true;
        g_capture_to_log = to_log;
    }

    static inline void EndCapture() {
        g_capture_on = false;
        g_capture_to_log = false;
        if (g_last_fd < 0 || g_capture_n == 0) return;
        if (ftruncate(g_last_fd, static_cast<off_t>(g_capture_n)) != 0) {
            // The following write still replaces the prefix.
        }
        if (lseek(g_last_fd, 0, SEEK_SET) < 0) return;
        WriteFd(g_last_fd, g_capture, g_capture_n);
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
            WriteLit(&buf[i], 1);
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
        WriteLit(static_cast<const char *>(s), N - 1);
    }

    static bool ReadU32(uintptr_t addr, uint32_t *out) {
#if defined(SYS_process_vm_readv)
        if (!out || addr < 0x10000u) return false;
        struct iovec local {};
        struct iovec remote {};
        local.iov_base = out;
        local.iov_len = sizeof(*out);
        remote.iov_base = reinterpret_cast<void *>(addr);
        remote.iov_len = sizeof(*out);
        const long n = syscall(SYS_process_vm_readv, static_cast<long>(getpid()),
                               &local, 1L, &remote, 1L, 0L);
        return n == static_cast<long>(sizeof(*out));
#else
        (void) addr;
        (void) out;
        return false;
#endif
    }

    static void CopyMaps() {
        if (g_maps_fd < 0 || g_maps_out_fd < 0) return;
        (void) ftruncate(g_maps_out_fd, 0);
        if (lseek(g_maps_fd, 0, SEEK_SET) < 0) return;
        if (lseek(g_maps_out_fd, 0, SEEK_SET) < 0) return;
        char buf[4096];
        size_t total = 0;
        constexpr size_t kCap = 2u * 1024u * 1024u;
        for (;;) {
            const ssize_t n = read(g_maps_fd, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;
            WriteFd(g_maps_out_fd, buf, static_cast<size_t>(n));
            total += static_cast<size_t>(n);
            if (total >= kCap) break;
        }
    }

    static inline void WriteKeyHex(const char *key, uint64_t v) {
        WriteLit("[VEXA][CRASH] ");
        WriteCStr(key);
        WriteLit("=");
        WriteHex64(v);
        WriteLit("\n");
    }

    static bool AltStackUsable(const ucontext_t *uc) {
        if (!uc) return false;
        const uintptr_t sp = reinterpret_cast<uintptr_t>(uc->uc_stack.ss_sp);
        if (sp < 0x10000u || (sp & 7u) != 0) return false;
        if (uc->uc_stack.ss_size < 16) return false;
        if (uc->uc_stack.ss_flags & SS_DISABLE) return false;
        return true;
    }

    // FEX raises SEGV_ACCERR on its own guard page and interrupt page while JIT is healthy.
    static bool ShouldCaptureEarly(int sig, const siginfo_t *si, const ucontext_t *uc) {
        if (!AltStackUsable(uc)) return true;
        if (sig == SIGABRT || sig == SIGILL || sig == SIGFPE || sig == SIGSYS || sig == SIGBUS) {
            return true;
        }
        if (sig != SIGSEGV) return false;
        if (!si || si->si_code != SEGV_ACCERR) return true;
        const uintptr_t fault = reinterpret_cast<uintptr_t>(si->si_addr);
        uintptr_t pc = 0;
#if defined(__aarch64__)
        if (uc) pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
#endif
        return fault < 0x10000u || pc < 0x10000u;
    }

    static bool IsInternalFexFault(FEXCore::Core::InternalThreadState *Thread,
                                   int sig,
                                   const siginfo_t *si) {
        if (sig != SIGSEGV || !si || !Thread || si->si_code != SEGV_ACCERR) return false;
        const auto addr = reinterpret_cast<uintptr_t>(si->si_addr);
        const auto fault = reinterpret_cast<uintptr_t>(&Thread->InterruptFaultPage);
        if (addr >= fault && addr < fault + FEXCore::Utils::FEX_PAGE_SIZE) return true;
        if (Thread->JITGuardPage != 0 &&
            addr >= Thread->JITGuardPage &&
            addr < Thread->JITGuardPage + FEXCore::Utils::FEX_PAGE_SIZE) {
            return true;
        }
        return false;
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
        WriteKeyHex("host_x4", uc->uc_mcontext.regs[4]);
        WriteKeyHex("host_x5", uc->uc_mcontext.regs[5]);
        WriteKeyHex("host_x6", uc->uc_mcontext.regs[6]);
        WriteKeyHex("host_x7", uc->uc_mcontext.regs[7]);
        WriteKeyHex("host_x8", uc->uc_mcontext.regs[8]);
        WriteKeyHex("alt_sp", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(uc->uc_stack.ss_sp)));
        WriteKeyHex("alt_size", static_cast<uint64_t>(uc->uc_stack.ss_size));
        uint32_t insn = 0;
        if (ReadU32(static_cast<uintptr_t>(uc->uc_mcontext.pc), &insn)) {
            WriteKeyHex("host_pc_insn", insn);
        }
        const uint64_t lr = uc->uc_mcontext.regs[30];
        if (lr >= 4 && ReadU32(static_cast<uintptr_t>(lr - 4), &insn)) {
            WriteKeyHex("host_lr_insn", insn);
        }
#endif
    }

    static bool ReadU64(uintptr_t addr, uint64_t *out) {
#if defined(SYS_process_vm_readv)
        if (!out || addr < 0x10000u) return false;
        struct iovec local {};
        struct iovec remote {};
        local.iov_base = out;
        local.iov_len = sizeof(*out);
        remote.iov_base = reinterpret_cast<void *>(addr);
        remote.iov_len = sizeof(*out);
        const long n = syscall(SYS_process_vm_readv, static_cast<long>(getpid()),
                               &local, 1L, &remote, 1L, 0L);
        return n == static_cast<long>(sizeof(*out));
#else
        (void) addr;
        (void) out;
        return false;
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

        // Frame-pointer chain: [rbp] = saved rbp, [rbp+8] = return address.
        uint64_t fp = g[5];
        for (int i = 0; i < 24 && fp >= 0x10000u && (fp & 7u) == 0; ++i) {
            uint64_t next = 0, ret = 0;
            if (!ReadU64(static_cast<uintptr_t>(fp), &next)) break;
            if (!ReadU64(static_cast<uintptr_t>(fp + 8), &ret)) break;
            WriteKeyHex("guest_bt_ret", ret);
            if (next <= fp) break;
            fp = next;
        }
        // Raw stack words (covers frames built without frame pointers).
        const uint64_t sp = g[4];
        for (int i = 0; i < 160; ++i) {
            uint64_t w = 0;
            if (!ReadU64(static_cast<uintptr_t>(sp + static_cast<uint64_t>(i) * 8u), &w)) break;
            if (w >= 0x10000u) WriteKeyHex("guest_stk", w);
        }
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

    static void DumpSignalContext(const char *origin, int sig, const siginfo_t *si, void *uctx) {
        WriteLit("[VEXA][CRASH] === crash dump begin ===\n");
        WriteLit("[VEXA][CRASH] origin=");
        WriteCStr(origin ? origin : "unknown");
        WriteLit("\n");

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
        DumpHostState(uc);
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

    static void EarlyThunkHook(int sig, void *info, void *uctx) {
        const auto *si = static_cast<const siginfo_t *>(info);
        const auto *uc = static_cast<const ucontext_t *>(uctx);
        if (!ShouldCaptureEarly(sig, si, uc)) return;
        if (__atomic_exchange_n(&g_early_busy, 1, __ATOMIC_ACQ_REL) != 0) return;

        const uint32_t seq = __atomic_add_fetch(&g_early_count, 1u, __ATOMIC_RELAXED);
        BeginCapture(seq <= kMaxCrashRecords);
        WriteLit("[VEXA][CRASH] crash_seq=");
        WriteU32(seq);
        WriteLit("\n");
        DumpSignalContext("early-thunk", sig, si, uctx);
        WriteLit("[VEXA][CRASH] === crash dump end ===\n");
        EndCapture();
        CopyMaps();
        __atomic_store_n(&g_early_busy, 0, __ATOMIC_RELEASE);
    }

    bool FexCrashHandler(FEXCore::Core::InternalThreadState *Thread,
                         int Signal,
                         void *info,
                         void *uctx) {
        const auto *si = reinterpret_cast<const siginfo_t *>(info);
        if (IsInternalFexFault(Thread, Signal, si)) {
            return false;
        }
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            // Re-entry. Let FEX continue.
            return false;
        }

        const uint32_t seq = __atomic_add_fetch(&g_fex_crash_seq, 1u, __ATOMIC_RELAXED);
        const auto *uc = reinterpret_cast<const ucontext_t *>(uctx);

        BeginCapture(seq <= kMaxCrashRecords);
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
        EndCapture();
        CopyMaps();

        __atomic_store_n(&g_in_handler, 0, __ATOMIC_RELEASE);
        return false; // let FEX Continue handling
    }

    void Handler(int sig, siginfo_t *info, void *uctx) {
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            _exit(128 + sig);
        }
        BeginCapture(true);
        DumpSignalContext("runtime-sigaction", sig, info, uctx);
        WriteLit("[VEXA][CRASH] === crash dump end ===\n");
        EndCapture();
        _exit(128 + sig);
    }
} // namespace

namespace Vexa::Log {
    void OpenCrashLog(const char *artifactDir) {
        if (!artifactDir || !artifactDir[0] || g_crash_fd >= 0) return;

        char path[768];
        snprintf(path, sizeof(path), "%s/crash.log", artifactDir);
        const int crashFd = ::open(path, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
        if (crashFd >= 0) g_crash_fd = crashFd;

        snprintf(path, sizeof(path), "%s/crash_last.log", artifactDir);
        const int lastFd = ::open(path, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0644);
        if (lastFd >= 0) g_last_fd = lastFd;

        g_maps_fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        snprintf(path, sizeof(path), "%s/runtime_worker.maps", artifactDir);
        const int mapsFd = ::open(path, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0644);
        if (mapsFd >= 0) g_maps_out_fd = mapsFd;
        CopyMaps();

        WriteLit("[VEXA][CRASH] crash.log format=1 pid=");
        WriteU32(static_cast<unsigned>(getpid()));
        WriteLit("\n[VEXA][CRASH] note=awaiting-fault\n");
        if (g_last_fd >= 0) {
            const char note[] = "[VEXA][CRASH] note=awaiting-fault\n";
            WriteFd(g_last_fd, note, sizeof(note) - 1);
        }
    }

    void InstallFexCrashHandler(FEX::HLE::SignalDelegator *delegator) {
        FEX::HLE::SetEarlyHostSignalHook(&EarlyThunkHook);
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
