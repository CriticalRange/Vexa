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
#include <stdlib.h>

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
    static pid_t g_altstack_tid = 0;
    static void *g_altstack_mem = nullptr;
    static size_t g_altstack_size = 0;
    static stack_t g_prev_altstack{};
    static uint32_t g_fex_crash_seq = 0;
    static uint32_t g_early_count = 0;
    static volatile sig_atomic_t g_early_busy = 0;
    static int g_crash_fd = -1;
    static int g_last_fd = -1;
    static int g_maps_out_fd = -1;
    static char g_capture[65536];
    static size_t g_capture_n = 0;
    static bool g_capture_on = false;
    static bool g_capture_to_log = false;
    constexpr size_t kAltStackSize = 64 * 1024;
    constexpr uint32_t kMaxCrashRecords = 32;

    // FEX recovers from several signals without crashing: it emulates unaligned
    // atomics (ldapr/stlr) raised from JIT code, and its dispatcher stubs run a
    // deliberate hlt/brk "fault dance" to return from guest signals. All of
    // these reach this hook. Cache anonymous executable ranges (JIT code and
    // dispatcher stubs) to tell these handled fixups apart from real faults.
    struct JitRange {
        uintptr_t lo;
        uintptr_t hi;
    };
    static JitRange g_jit_ranges[32]{};
    static size_t g_jit_range_count = 0;
    static uint32_t g_fixup_count = 0;

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

    // Never dereference a potentially stale/protected mapping in a signal handler.
    // The kernel returns EIO/EFAULT for unreadable memory instead of recursively
    // faulting here. Android may deny /proc/self/mem; in that case omit the data.
    static bool ReadDirect(uintptr_t addr, void *out, size_t len) {
        if (!out || addr < 0x10000u || len > UINTPTR_MAX - addr) return false;
        const int savedErrno = errno;
        const int fd = ::open("/proc/self/mem", O_RDONLY | O_CLOEXEC);
        if (fd < 0) { errno = savedErrno; return false; }
        const ssize_t n = ::pread(fd, out, len, static_cast<off_t>(addr));
        ::close(fd);
        errno = savedErrno;
        return n == static_cast<ssize_t>(len);
    }

    static bool ReadU32(uintptr_t addr, uint32_t *out) {
        return ReadDirect(addr, out, sizeof(*out));
    }

    static bool ReadU64(uintptr_t addr, uint64_t *out) {
        return ReadDirect(addr, out, sizeof(*out));
    }

    // Reads /proc/self/maps with a fresh fd. The previous implementation
    // cached the fd at init and rewound it in the signal handler; on this
    // device the rewound seq_file produced an empty file, so every call
    // opens and closes its own descriptor instead.
    static void CopyMaps() {
        if (g_maps_out_fd < 0) return;
        const int fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (fd < 0) return;
        (void) ftruncate(g_maps_out_fd, 0);
        if (lseek(g_maps_out_fd, 0, SEEK_SET) < 0) {
            close(fd);
            return;
        }
        char buf[4096];
        size_t total = 0;
        constexpr size_t kCap = 2u * 1024u * 1024u;
        for (;;) {
            const ssize_t n = read(fd, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;
            WriteFd(g_maps_out_fd, buf, static_cast<size_t>(n));
            total += static_cast<size_t>(n);
            if (total >= kCap) break;
        }
        close(fd);
    }

    // Resolves an address to its /proc/self/maps region and writes
    // "map:<key>=0xADDR lo=0x.. off=0x.. <path>" into the active capture.
    // Fresh fd per call keeps this usable inside the signal handler.
    static void WriteMapFor(const char *key, uint64_t addr) {
        if (addr < 0x10000u) return;
        const int fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (fd < 0) return;
        char chunk[8192];
        char line[1024];
        size_t llen = 0;
        bool done = false;
        while (!done) {
            const ssize_t n = read(fd, chunk, sizeof(chunk));
            if (n <= 0) break;
            for (ssize_t i = 0; i < n && !done; ++i) {
                if (chunk[i] != '\n') {
                    if (llen < sizeof(line) - 1) line[llen++] = chunk[i];
                    continue;
                }
                line[llen] = '\0';
                llen = 0;
                char *end = nullptr;
                const uint64_t lo = strtoull(line, &end, 16);
                if (end == line || !end || *end != '-') continue;
                const uint64_t hi = strtoull(end + 1, &end, 16);
                if (addr < lo || addr >= hi) continue;
                while (*end == ' ') ++end;            // perms
                while (*end && *end != ' ') ++end;
                while (*end == ' ') ++end;
                const uint64_t off = strtoull(end, &end, 16);
                while (*end == ' ') ++end;            // dev
                while (*end && *end != ' ') ++end;
                while (*end == ' ') ++end;            // inode
                while (*end && *end != ' ') ++end;
                while (*end == ' ') ++end;            // path
                WriteLit("[VEXA][CRASH] map:");
                WriteCStr(key);
                WriteLit("=");
                WriteHex64(addr);
                WriteLit(" lo=");
                WriteHex64(lo);
                WriteLit(" off=");
                WriteHex64(off);
                WriteLit(" ");
                WriteCStr(end);
                WriteLit("\n");
                done = true;
            }
        }
        close(fd);
    }

    static void ScanJitRanges() {
        g_jit_range_count = 0;
        const int fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            {
                static char buf[1024 * 1024];
                size_t used = 0;
                for (;;) {
                    const ssize_t n = read(fd, buf + used, sizeof(buf) - used - 1);
                    if (n < 0) {
                        if (errno == EINTR) continue;
                        break;
                    }
                    if (n == 0) break;
                    used += static_cast<size_t>(n);
                    if (used >= sizeof(buf) - 1) break;
                }
                close(fd);
                buf[used] = '\0';

                char *line = buf;
                while (*line) {
                    char *eol = line;
                    while (*eol && *eol != '\n') ++eol;
                    char *end = nullptr;
                    const uintptr_t lo = strtoull(line, &end, 16);
                    uintptr_t hi = 0;
                    char *perms = nullptr;
                    if (end && *end == '-') {
                        hi = strtoull(end + 1, &end, 16);
                        perms = end;
                        while (*perms == ' ') ++perms;
                    }
                    if (perms && perms[2] == 'x' &&
                        g_jit_range_count < sizeof(g_jit_ranges) / sizeof(g_jit_ranges[0])) {
                        // FEX JIT code buffers are anonymous executable pages,
                        // shown empty or as [anon:*] tags in maps. File-backed
                        // and other named mappings are host libraries.
                        char *tail = eol;
                        while (tail > perms && (tail[-1] == ' ' || tail[-1] == '\t')) --tail;
                        char *name = tail;
                        while (name > perms && name[-1] != ' ' && name[-1] != '\t') --name;
                        const bool anon_tag = (tail - name) >= 6 && memcmp(name, "[anon:", 6) == 0;
                        // Anonymous regions leave the inode field as the last
                        // token (all digits); named files end in a path. Check
                        // the whole token so digit-leading filenames aren't
                        // mistaken for anonymous JIT regions.
                        bool inode_tail = name < tail;
                        for (char *c = name; inode_tail && c < tail; ++c) {
                            inode_tail = (*c >= '0' && *c <= '9');
                        }
                        if (anon_tag || inode_tail) {
                            g_jit_ranges[g_jit_range_count].lo = lo;
                            g_jit_ranges[g_jit_range_count].hi = hi;
                            ++g_jit_range_count;
                        }
                    }
                    line = *eol ? eol + 1 : eol;
                }
            }
        }
    }

    static bool PcInJitRegion(uintptr_t pc) {
        for (size_t i = 0; i < g_jit_range_count; ++i) {
            if (pc >= g_jit_ranges[i].lo && pc < g_jit_ranges[i].hi) {
                return true;
            }
        }
        return false;
    }

    static bool IsHandledJitAtomicFixup(int sig, const siginfo_t *si, const ucontext_t *uc) {
#if defined(__aarch64__)
        if (!si || !uc) {
            return false;
        }
        switch (sig) {
        case SIGBUS:
            if (si->si_code != BUS_ADRALN) {
                return false;
            }
            break;
        case SIGILL:
        case SIGTRAP:
            // FEX's dispatcher stubs end every handled guest signal with a
            // deliberate hlt/brk "fault dance" back into JIT code, and guest
            // SIGILL/SIGTRAP delivery uses the same trampolines — all handled.
            break;
        default:
            return false;
        }
        const uintptr_t pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
        if (PcInJitRegion(pc)) {
            return true;
        }
        // JIT may have allocated a new code buffer since the last scan.
        ScanJitRanges();
        return PcInJitRegion(pc);
#else
        (void) sig;
        (void) si;
        (void) uc;
        return false;
#endif
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

    // True when the faulting instruction is translated guest code. Celeste's
    // Mono heap commits by touching pages that are not mapped yet; each touch
    // is a SIGSEGV inside the JIT. Those are guest faults, not host crashes.
    static bool GuestJitFault(const ucontext_t *uc) {
#if defined(__aarch64__)
        if (!uc) return false;
        const uintptr_t pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
        if (PcInJitRegion(pc)) return true;
        ScanJitRanges();
        return PcInJitRegion(pc);
#else
        (void) uc;
        return false;
#endif
    }

    // FEX raises SEGV_ACCERR on its own guard page and interrupt page while JIT is healthy.
    static bool ShouldCaptureEarly(int sig, const siginfo_t *si, const ucontext_t *uc) {
        if (!AltStackUsable(uc)) return true;
        if (sig == SIGABRT || sig == SIGILL || sig == SIGFPE || sig == SIGSYS || sig == SIGBUS) {
            return true;
        }
        if (sig != SIGSEGV) return false;
        // Copying the process map on every guest page fault made Celeste's
        // content load look frozen. FEX still delivers the signal.
        if (GuestJitFault(uc)) return false;
        if (!si || si->si_code != SEGV_ACCERR) return true;
        const uintptr_t fault = reinterpret_cast<uintptr_t>(si->si_addr);
        uintptr_t pc = 0;
#if defined(__aarch64__)
        if (uc) pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
#endif
        return fault < 0x10000u || pc < 0x10000u;
    }

    // Scudo keeps an MTE/TBI tag in the top byte. Kernel si_addr does not.
    static inline uintptr_t UntagHostPtr(uintptr_t Ptr) {
        return Ptr & 0x00FFFFFFFFFFFFFFULL;
    }

    // g_old[] holds the dispositions that were installed before VEXA's plain
    // sigaction handlers — i.e. ART's signal handlers. A FEX thread that is
    // JNI-attached executes JIT'd Java inside CallVoidMethod; an implicit
    // null-check SEGV there must reach ART's handler so it becomes a
    // NullPointerException instead of a fatal "host-code-fault" kill. For
    // genuine native bugs, ART's chain still produces a debuggerd tombstone,
    // which is strictly better than _exit.
    // Returns true when the previous disposition handled (or ignored) it.
    static bool ChainToPreviousHandler(int sig, siginfo_t *info, void *uctx) {
        for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
            if (kSignals[i] != sig) continue;
            const struct sigaction &old = g_old[i];
            const auto handler = reinterpret_cast<uintptr_t>(old.sa_handler);
            if (handler == reinterpret_cast<uintptr_t>(SIG_IGN)) return true;
            if (handler == reinterpret_cast<uintptr_t>(SIG_DFL) || handler == 0) return false;
            if (old.sa_flags & SA_SIGINFO) {
                old.sa_sigaction(sig, info, uctx);
            } else {
                old.sa_handler(sig);
            }
            return true;
        }
        return false;
    }

    static bool IsInternalFexFault(FEXCore::Core::InternalThreadState *Thread,
                                   int sig,
                                   const siginfo_t *si) {
        if (sig != SIGSEGV || !si || !Thread || si->si_code != SEGV_ACCERR) return false;
        const auto addr = UntagHostPtr(reinterpret_cast<uintptr_t>(si->si_addr));
        const auto fault = UntagHostPtr(reinterpret_cast<uintptr_t>(&Thread->InterruptFaultPage));
        if (addr >= fault && addr < fault + FEXCore::Utils::FEX_PAGE_SIZE) return true;
        const auto guard = UntagHostPtr(Thread->JITGuardPage);
        if (guard != 0 && addr >= guard && addr < guard + FEXCore::Utils::FEX_PAGE_SIZE) {
            return true;
        }
        return false;
    }

    static uint32_t g_bt_map_budget = 0;

    static _Unwind_Reason_Code BacktraceCb(struct _Unwind_Context *ctx, void *) {
        const uintptr_t pc = static_cast<uintptr_t>(_Unwind_GetIP(ctx));
        if (pc == 0) return _URC_END_OF_STACK;
        WriteLit("[VEXA][CRASH] bt_pc=");
        WriteHex64(pc);
        WriteLit("\n");
        if (g_bt_map_budget < 16) {
            ++g_bt_map_budget;
            WriteMapFor("bt", pc);
        }
        return _URC_NO_REASON;
    }

    static inline void DumpHostState(const ucontext_t *uc) {
#if defined(__aarch64__)
        if (!uc) return;
        g_bt_map_budget = 0;
        WriteKeyHex("host_pc", uc->uc_mcontext.pc);
        WriteKeyHex("host_lr", uc->uc_mcontext.regs[30]);
        WriteKeyHex("host_sp", uc->uc_mcontext.sp);
        WriteMapFor("pc", static_cast<uint64_t>(uc->uc_mcontext.pc));
        WriteMapFor("lr", static_cast<uint64_t>(uc->uc_mcontext.regs[30]));
        WriteMapFor("sp", static_cast<uint64_t>(uc->uc_mcontext.sp));
        WriteKeyHex("host_x0", uc->uc_mcontext.regs[0]);
        WriteKeyHex("host_x1", uc->uc_mcontext.regs[1]);
        WriteKeyHex("host_x2", uc->uc_mcontext.regs[2]);
        WriteKeyHex("host_x3", uc->uc_mcontext.regs[3]);
        WriteKeyHex("host_x4", uc->uc_mcontext.regs[4]);
        WriteKeyHex("host_x5", uc->uc_mcontext.regs[5]);
        WriteKeyHex("host_x6", uc->uc_mcontext.regs[6]);
        WriteKeyHex("host_x7", uc->uc_mcontext.regs[7]);
        WriteKeyHex("host_x8", uc->uc_mcontext.regs[8]);
        {
            static const char *const kRegNames[] = {
                    "host_x9", "host_x10", "host_x11", "host_x12", "host_x13",
                    "host_x14", "host_x15", "host_x16", "host_x17", "host_x18",
                    "host_x19", "host_x20", "host_x21", "host_x22", "host_x23",
                    "host_x24", "host_x25", "host_x26", "host_x27", "host_x28",
                    "host_x29",
            };
            for (int i = 9; i <= 29; ++i) {
                WriteKeyHex(kRegNames[i - 9], uc->uc_mcontext.regs[i]);
            }
        }
        // AAPCS64 frame chain from the faulting context: [x29]=prev fp,
        // [x29+8]=return address. Captures the real fault stack, which
        // _Unwind_Backtrace below (handler context) cannot see.
        {
            uint64_t fp = uc->uc_mcontext.regs[29];
            for (int i = 0; i < 48 && fp >= 0x10000u; ++i) {
                uint64_t prev = 0, ret = 0;
                if (!ReadU64(fp, &prev) || !ReadU64(fp + 8, &ret)) break;
                WriteLit("[VEXA][CRASH] frame_fp=");
                WriteHex64(fp);
                WriteLit(" ret=");
                WriteHex64(ret);
                WriteLit("\n");
                if (i < 16) WriteMapFor("frame_ret", ret);
                if (prev <= fp || prev - fp > (1u << 24)) break;
                fp = prev;
            }
        }
        // Raw stack scan: potential return addresses get resolved offline
        // against runtime_worker.maps.
        {
            const uintptr_t sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
            for (uintptr_t a = sp; a < sp + 0x1000; a += 8) {
                uint64_t w = 0;
                if (!ReadU64(a, &w)) break;
                WriteLit("[VEXA][CRASH] stack@");
                WriteHex64(a - sp);
                WriteLit("=");
                WriteHex64(w);
                WriteLit("\n");
            }
        }
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
        // Dump code around the faulting PC so JIT/dispatcher blocks can be
        // disassembled offline. Each word is one little-endian instruction.
        // A second window around lr captures the call site (blr/br) that
        // entered the faulting stub; dispatcher stubs are spaced ~0x300
        // apart so a wide window is needed.
        {
            const uintptr_t pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
            const uintptr_t start = pc >= 0x400 ? pc - 0x400 : pc;
            WriteLit("[VEXA][CRASH] insn_window_base=");
            WriteHex64(start);
            WriteLit("\n");
            for (uintptr_t a = start; a < pc + 0x40; a += 4) {
                uint32_t w = 0;
                if (!ReadU32(a, &w)) break;
                WriteLit("[VEXA][CRASH] insn@");
                WriteHex64(a - pc);
                WriteLit("=");
                WriteHex64(w);
                WriteLit("\n");
            }
            const uintptr_t lrw = static_cast<uintptr_t>(lr);
            const uintptr_t lstart = lrw >= 0x48 ? lrw - 0x48 : lrw;
            WriteLit("[VEXA][CRASH] insn_lr_window_base=");
            WriteHex64(lstart);
            WriteLit("\n");
            for (uintptr_t a = lstart; a < lrw + 0x10 && lrw >= 0x10000u; a += 4) {
                uint32_t w = 0;
                if (!ReadU32(a, &w)) break;
                WriteLit("[VEXA][CRASH] insn_lr@");
                WriteHex64(a - lrw);
                WriteLit("=");
                WriteHex64(w);
                WriteLit("\n");
            }
        }
        _Unwind_Backtrace(BacktraceCb, nullptr);
#endif
    }

    static inline void DumpGuestStateRaw(FEXCore::Core::InternalThreadState *Thread) {
        if (!Thread || !Thread->CurrentFrame) return;
        WriteKeyHex("guest_rip", Thread->CurrentFrame->State.rip);
        WriteMapFor("guest_rip", Thread->CurrentFrame->State.rip);
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
        g_altstack_tid = static_cast<pid_t>(syscall(SYS_gettid));
        g_altstack_installed = true;
        __android_log_print(ANDROID_LOG_INFO, kCrashTag,
                            "Installed alt signal stack size=%zu", kAltStackSize);
    }

    void UninstallAltStackIfNeeded() {
        if (!g_altstack_installed) {
            return;
        }

        // sigaltstack is per-thread: InstallSignalHandlers ran on the JNI
        // caller's thread while UninstallSignalHandlers runs on the
        // runtime_worker pthread. Restoring thread A's saved state onto
        // thread B and then unmapping the buffer would leave thread A's
        // registered alt stack pointing at freed memory — a fault there
        // would be delivered onto unmapped pages. On a foreign thread we
        // simply leave the alt stack in place; the worker process is
        // single-use and exits with the run anyway.
        if (g_altstack_tid != static_cast<pid_t>(syscall(SYS_gettid))) {
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
        g_altstack_tid = 0;
        memset(&g_prev_altstack, 0, sizeof(g_prev_altstack));
    }

    static void EarlyThunkHook(int sig, void *info, void *uctx) {
        const auto *si = static_cast<const siginfo_t *>(info);
        const auto *uc = static_cast<const ucontext_t *>(uctx);
        if (IsHandledJitAtomicFixup(sig, si, uc)) {
            const uint32_t count = __atomic_add_fetch(&g_fixup_count, 1u, __ATOMIC_RELAXED);
            if ((count & 0xFF) == 1) {
                BeginCapture(true);
                WriteLit("[VEXA][CRASH] handled_fixup count=");
                WriteU32(count);
                WriteLit(" signal=");
                WriteU32(static_cast<unsigned>(sig));
                WriteLit("\n");
                WriteSigName(sig);
                EndCapture();
            }
            return;
        }
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
#if defined(__aarch64__)
        // Deliberate FEX faults: the InterruptFaultPage write and the JIT guard
        // page are FEX's own signaling mechanism. They must reach FEX's signal
        // handlers instead of being dumped and killed as host-code faults.
        if (Thread && si && Signal == SIGSEGV) {
            const uintptr_t faultAddr = reinterpret_cast<uintptr_t>(si->si_addr) & 0x00FF'FFFF'FFFF'FFFFULL;
            const uintptr_t interruptPage = reinterpret_cast<uintptr_t>(&Thread->InterruptFaultPage) & 0x00FF'FFFF'FFFF'FFFFULL;
            const uintptr_t jitGuard = static_cast<uintptr_t>(Thread->JITGuardPage) & 0x00FF'FFFF'FFFF'FFFFULL;
            if (faultAddr == interruptPage ||
                (jitGuard != 0 && faultAddr >= jitGuard && faultAddr < jitGuard + 0x1000)) {
                return false;
            }
        }

        // Deliberate dispatcher faults: the GuestSignal_*/SignalReturn/pause
        // stubs live in the FEXMem_Misc code buffer (outside the JIT region)
        // and fault on purpose to deliver guest signals or unwind state.
        // IsAddressInCodeBuffer doesn't cover them, so without this check a
        // plain guest SIGSEGV is misclassified as a fatal host-code fault.
        // Let FEX's delegator (HandleGuestSignal) handle them instead.
        if (Signal == SIGSEGV && GuestJitFault(reinterpret_cast<const ucontext_t *>(uctx))) {
            // Same guest page faults as the early hook. Do not dump or copy
            // maps; the guest handler commits the page and resumes.
            return false;
        }

        if (uctx && Thread && Thread->CurrentFrame) {
            const uintptr_t pc = static_cast<uintptr_t>(
                reinterpret_cast<const ucontext_t *>(uctx)->uc_mcontext.pc);
            const auto &P = Thread->CurrentFrame->Pointers;
            const uint64_t stubs[] = {
                P.DispatcherLoopTopFillSRA, P.DispatcherLoopTop,
                P.DispatcherLoopTopEnterEC, P.DispatcherLoopTopEnterECFillSRA,
                P.ExitFunctionLinker, P.ThreadStopHandlerSpillSRA,
                P.ThreadPauseHandlerSpillSRA, P.GuestSignal_SIGILL,
                P.GuestSignal_SIGTRAP, P.GuestSignal_SIGSEGV,
                P.SignalReturnHandler, P.SignalReturnHandlerRT,
                P.LUDIVHandler, P.LDIVHandler,
            };
            uint64_t lo = UINT64_MAX;
            for (uint64_t s : stubs) {
                if (s && s < lo) lo = s;
            }
            if (lo != UINT64_MAX) {
                const uint64_t base = lo & ~0xFFFULL;
                // MAX_DISPATCHER_CODE_SIZE is 4 pages.
                if (pc >= base && pc < base + 0x4000) {
                    WriteLit("[VEXA][CRASH] dispatcher_stub_fault pc=");
                    WriteHex64(pc);
                    WriteLit(" base=");
                    WriteHex64(base);
                    WriteLit(" signal=");
                    WriteS32(Signal);
                    WriteLit("\n");
                    return false;
                }
            }
        }
#endif
        if (__atomic_exchange_n(&g_in_handler, 1, __ATOMIC_ACQ_REL) != 0) {
            // Re-entry. Let FEX continue.
            return false;
        }

        const uint32_t seq = __atomic_add_fetch(&g_fex_crash_seq, 1u, __ATOMIC_RELAXED);
        const auto *uc = reinterpret_cast<const ucontext_t *>(uctx);

        // Faults raised while executing host code (syscall handlers, thunk
        // code, FEX internals) can never be fixed up by the guest handler.
        // Letting FEX deliver them to the guest just refaults ~30x until the
        // deferred-signal vector overflows. Dump once and die instead.
        bool hostCodeFault = false;
#if defined(__aarch64__)
        if (uc && Thread && Thread->CTX) {
            const uintptr_t pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
            hostCodeFault = !Thread->CTX->IsAddressInCodeBuffer(Thread, pc);
        }
#endif

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
        if (hostCodeFault) {
            WriteLit("[VEXA][CRASH] fatal=host-code-fault\n");
        }
        WriteLit("[VEXA][CRASH] === crash dump end ===\n");
        EndCapture();
        CopyMaps();

        __atomic_store_n(&g_in_handler, 0, __ATOMIC_RELEASE);
        if (hostCodeFault) {
            // Fault while executing host code on a FEX thread. Two cases:
            //  - managed/JNI code (jit-cache, oat, libart): ART's handler turns
            //    implicit null checks into NPEs; the process must survive.
            //  - native thunk/FEX/syscall code: chaining still ends the run,
            //    but through debuggerd so a real tombstone is produced.
            if (ChainToPreviousHandler(Signal, static_cast<siginfo_t *>(info), uctx)) {
                return true;
            }
            _exit(128 + Signal);
        }
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
        __atomic_store_n(&g_in_handler, 0, __ATOMIC_RELEASE);
        // Give ART/debuggerd the last word: managed faults become NPEs and the
        // process survives; real native faults still die with a tombstone.
        if (ChainToPreviousHandler(sig, info, uctx)) {
            return;
        }
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
