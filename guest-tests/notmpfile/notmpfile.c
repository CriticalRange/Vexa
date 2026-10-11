/*
 * libnotmpfile — LD_PRELOAD shim for VEXA guests.
 *
 * Qt's QTemporaryFileEngine prefers open(O_TMPFILE) unnamed tmpfiles and later
 * materializes them with linkat("/proc/self/fd/N", target). Android's SELinux
 * policy denies hardlinks inside app-private storage for the app domain, so the
 * commit fails with EPERM. Returning EOPNOTSUPP makes glibc/Qt fall back to a
 * named temp file + rename(), which works fine.
 *
 * Freestanding on purpose: no libc headers are needed and the only runtime
 * dependency is __errno_location, resolved from the guest libc at load time.
 */

#include <stdarg.h>

#define O_TMPFILE_FLG 0x410000 /* __O_TMPFILE | O_DIRECTORY on x86_64 */
#define O_CREAT_FLG   0x000040
#define AT_FDCWD_     (-100)
#define EOPNOTSUPP_   95

extern int *__errno_location(void);

static long sys_openat(int dirfd, const char *path, int flags, int mode)
{
    long ret;
    register long r10 __asm__("r10") = mode;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(257L), "D"((long)dirfd), "S"(path), "d"((long)flags), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
}

static int finish(long ret)
{
    if (ret < 0) {
        *__errno_location() = (int)-ret;
        return -1;
    }
    return (int)ret;
}

static int check_tmpfile(int flags)
{
    if ((flags & O_TMPFILE_FLG) == O_TMPFILE_FLG) {
        *__errno_location() = EOPNOTSUPP_;
        return 1;
    }
    return 0;
}

int open(const char *path, int flags, ...)
{
    int mode = 0;
    if (check_tmpfile(flags)) return -1;
    if (flags & O_CREAT_FLG) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return finish(sys_openat(AT_FDCWD_, path, flags, mode));
}

int openat(int dirfd, const char *path, int flags, ...)
{
    int mode = 0;
    if (check_tmpfile(flags)) return -1;
    if (flags & O_CREAT_FLG) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return finish(sys_openat(dirfd, path, flags, mode));
}

/* Aliases used by glibc-built callers (_FILE_OFFSET_BITS=64, fortify, no-cancel). */
#define ALIAS64(name) \
    int name(const char *path, int flags, ...) \
    { \
        int mode = 0; \
        if (check_tmpfile(flags)) return -1; \
        if (flags & O_CREAT_FLG) { \
            va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); \
        } \
        return finish(sys_openat(AT_FDCWD_, path, flags, mode)); \
    }

#define ALIASAT64(name) \
    int name(int dirfd, const char *path, int flags, ...) \
    { \
        int mode = 0; \
        if (check_tmpfile(flags)) return -1; \
        if (flags & O_CREAT_FLG) { \
            va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); \
        } \
        return finish(sys_openat(dirfd, path, flags, mode)); \
    }

ALIAS64(open64)
ALIAS64(__open)
ALIAS64(__open64)
ALIAS64(__open64_2)
ALIAS64(__open_nocancel)
ALIAS64(__open64_nocancel)
ALIAS64(__open_2)

ALIASAT64(openat64)
ALIASAT64(__openat_2)
ALIASAT64(__openat64_2)
ALIASAT64(__openat_nocancel)
ALIASAT64(__openat64_nocancel)

/*
 * The VEXA libGL guest thunk does not export the legacy fixed-function entry
 * points GLES does not have (Android host has no glOrtho/glLoadMatrixf).
 * libQt6OpenGL links them unconditionally; satisfy the linker here. They are
 * only reached through Qt's legacy paint engine paths, which eglfs never uses.
 */
void glOrtho(void) {}
void glLoadMatrixf(void) {}

/*
 * Fatal-signal diagnostics: guest abort() reaches the host as a real SIGABRT
 * through FEX's tgkill passthrough, which kills the whole runtime_worker with
 * no guest-side backtrace. Install guest signal handlers that dump a DWARF
 * backtrace to stderr before re-raising, so we can see who aborted.
 */
extern void (*signal(int, void (*)(int)))(int);
extern int backtrace(void **, int);
extern void backtrace_symbols_fd(void *const *, int, int);
extern int raise(int);

static long sys_write2(const char *buf, long len)
{
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(1L), "D"(2L), "S"(buf), "d"(len)
                     : "rcx", "r11", "memory");
    return ret;
}

static void on_fatal(int sig)
{
    static const char hdr[] = "\n[VEXA-SHIM] guest fatal signal caught, backtrace:\n";
    void *bt[48];
    sys_write2(hdr, sizeof(hdr) - 1);
    int n = backtrace(bt, 48);
    backtrace_symbols_fd(bt, n, 2);
    sys_write2(hdr, sizeof(hdr) - 1); /* flush marker; then die for real */
    signal(sig, (void (*)(int))0);
    raise(sig);
    for (;;) { }
}

__attribute__((constructor))
static void vexa_shim_init(void)
{
    /* Warm up backtrace() outside the signal handler: its first call may
       dlopen libgcc, which is not async-signal-safe. */
    void *warm[4];
    (void)backtrace(warm, 4);
    signal(6, on_fatal);  /* SIGABRT */
    signal(11, on_fatal); /* SIGSEGV */
    signal(4, on_fatal);  /* SIGILL  */
}
