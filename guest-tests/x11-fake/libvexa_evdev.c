/* libvexa_evdev.so - virtual Xbox 360 gamepad for guests without a real
 * /dev/input backend.
 *
 * The Android shell (GuestPad.kt) appends 16-byte type-7 records to
 * /tmp/vexa-pointer: (type=7, x=SDL_GameControllerButton or 100+axis,
 * y=state/value). This library makes every engine that enumerates Linux
 * evdev devices - Unity, Godot, non-thunked SDL, MonoGame - see a standard
 * wired Xbox 360 controller at /dev/input/event0, so each game applies its
 * own default bindings instead of relying on VEXA's keyboard fallback.
 *
 * Loaded through /etc/ld.so.preload: it interposes open/openat to swap the
 * marker node for a pipe whose writer thread translates pad records into
 * struct input_event, and interposes ioctl to answer the EVIOC* queries a
 * real input device would.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define VEXA_POINTER_PATH "/tmp/vexa-pointer"
#define VEXA_DEVNODE "/dev/input/event0"

#define PAD_NAME "Microsoft X-Box 360 pad"
#define PAD_PHYS "vexa/input0"
#define PAD_UNIQ ""

/* xpad-compatible button layout: type-7 button ids are SDL_GameControllerButton
 * order; translate to the evdev codes a real Xbox 360 pad reports. */
static const uint16_t kPadButtonCodes[] = {
    [0] = BTN_SOUTH, /* SDL A     */
    [1] = BTN_EAST,  /* SDL B     */
    [2] = BTN_NORTH, /* SDL X     */
    [3] = BTN_WEST,  /* SDL Y     */
    [4] = BTN_SELECT,
    [5] = BTN_MODE,
    [6] = BTN_START,
    [7] = BTN_THUMBL,
    [8] = BTN_THUMBR,
    [9] = BTN_TL,
    [10] = BTN_TR,
    /* 11..14 are d-pad directions; xpad reports them as ABS_HAT0X/Y. */
};

static const uint16_t kPadButtonBits[] = {
    BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST,
    BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR,
    BTN_TL, BTN_TR,
};

static int (*real_open)(const char*, int, ...);
static int (*real_open64)(const char*, int, ...);
static int (*real_openat)(int, const char*, int, ...);
static int (*real_openat64)(int, const char*, int, ...);
static int (*real_ioctl)(int, unsigned long, ...);
static int (*real_close)(int);
static int (*real_fcntl)(int, int, ...);

static int pad_fds[16];
static pthread_mutex_t pad_lock = PTHREAD_MUTEX_INITIALIZER;
static int pad_write_fd = -1;
static pthread_once_t feeder_once = PTHREAD_ONCE_INIT;

static bool is_pad_fd(int fd) {
    bool found = false;
    pthread_mutex_lock(&pad_lock);
    for (size_t i = 0; i < sizeof(pad_fds) / sizeof(pad_fds[0]); ++i) {
        if (pad_fds[i] == fd) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&pad_lock);
    return found;
}

static bool track_fd(int fd) {
    bool ok = false;
    pthread_mutex_lock(&pad_lock);
    for (size_t i = 0; i < sizeof(pad_fds) / sizeof(pad_fds[0]); ++i) {
        if (pad_fds[i] < 0) {
            pad_fds[i] = fd;
            ok = true;
            break;
        }
    }
    pthread_mutex_unlock(&pad_lock);
    return ok;
}

static void untrack_fd(int fd) {
    pthread_mutex_lock(&pad_lock);
    for (size_t i = 0; i < sizeof(pad_fds) / sizeof(pad_fds[0]); ++i) {
        if (pad_fds[i] == fd) {
            pad_fds[i] = -1;
        }
    }
    pthread_mutex_unlock(&pad_lock);
}

static void emit_event(struct input_event* ev, uint16_t type, uint16_t code, int32_t value) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ev->time.tv_sec = ts.tv_sec;
    ev->time.tv_usec = ts.tv_nsec / 1000;
    ev->type = type;
    ev->code = code;
    ev->value = value;
}

/* Tail /tmp/vexa-pointer and translate type-7 pad records into evdev events. */
static void* feeder_main(void* unused) {
    (void)unused;
    if (!real_open) {
        return NULL;
    }
    int hat_x = 0, hat_y = 0;
    for (;;) {
        int fd = real_open(VEXA_POINTER_PATH, O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            usleep(50000);
            continue;
        }
        off_t off = lseek(fd, 0, SEEK_END);
        if (off < 0) {
            off = 0;
        }
        unsigned char rec[16];
        for (;;) {
            ssize_t n = read(fd, rec, sizeof(rec));
            if (n != (ssize_t)sizeof(rec)) {
                /* No full record yet: wait and retry. Records are appended
                 * atomically, so a short read just means "caught up". The
                 * shell truncates the file when a new session starts. */
                off_t end = lseek(fd, 0, SEEK_END);
                if (end < off) {
                    off = end;
                }
                lseek(fd, off, SEEK_SET);
                usleep(4000);
                continue;
            }
            off += sizeof(rec);
            int32_t type, x, y, state;
            memcpy(&type, rec, 4);
            memcpy(&x, rec + 4, 4);
            memcpy(&y, rec + 8, 4);
            memcpy(&state, rec + 12, 4);
            if (type != 7) {
                continue;
            }
            int wfd;
            pthread_mutex_lock(&pad_lock);
            wfd = pad_write_fd;
            pthread_mutex_unlock(&pad_lock);
            if (wfd < 0) {
                continue;
            }
            struct input_event ev;
            if (x >= 11 && x <= 14) {
                /* D-pad directions come through as button ids; a real xpad
                 * reports them on the hat axes. */
                int nx = hat_x, ny = hat_y;
                if (x == 11) ny = y ? -1 : 0;
                if (x == 12) ny = y ? 1 : 0;
                if (x == 13) nx = y ? -1 : 0;
                if (x == 14) nx = y ? 1 : 0;
                if (nx == hat_x && ny == hat_y) {
                    continue;
                }
                if (nx != hat_x) {
                    emit_event(&ev, EV_ABS, ABS_HAT0X, nx);
                    write(wfd, &ev, sizeof(ev));
                    hat_x = nx;
                }
                if (ny != hat_y) {
                    emit_event(&ev, EV_ABS, ABS_HAT0Y, ny);
                    write(wfd, &ev, sizeof(ev));
                    hat_y = ny;
                }
            } else if (x >= 0 && x <= 10) {
                emit_event(&ev, EV_KEY, kPadButtonCodes[x], y ? 1 : 0);
                write(wfd, &ev, sizeof(ev));
            } else {
                int axis = x - 100;
                if (axis < 0 || axis > 5) {
                    continue;
                }
                switch (axis) {
                case 0:
                    emit_event(&ev, EV_ABS, ABS_X, y);
                    break;
                case 1:
                    emit_event(&ev, EV_ABS, ABS_Y, y);
                    break;
                case 2:
                    emit_event(&ev, EV_ABS, ABS_RX, y);
                    break;
                case 3:
                    emit_event(&ev, EV_ABS, ABS_RY, y);
                    break;
                case 4:
                    /* xpad triggers are 0..255. */
                    emit_event(&ev, EV_ABS, ABS_Z, y >> 7);
                    break;
                case 5:
                    emit_event(&ev, EV_ABS, ABS_RZ, y >> 7);
                    break;
                }
                write(wfd, &ev, sizeof(ev));
            }
            emit_event(&ev, EV_SYN, SYN_REPORT, 0);
            write(wfd, &ev, sizeof(ev));
        }
    }
    return NULL;
}

static void feeder_start(void) {
    for (size_t i = 0; i < sizeof(pad_fds) / sizeof(pad_fds[0]); ++i) {
        pad_fds[i] = -1;
    }
    pthread_t thread;
    if (pthread_create(&thread, NULL, feeder_main, NULL) == 0) {
        pthread_detach(thread);
    }
}

static bool is_pad_path(const char* path) {
    if (!path) {
        return false;
    }
    /* Only the marker node resolves to the virtual pad. Intercepting every
     * eventN would surface the same device 32 times and duplicate input. */
    return strcmp(path, VEXA_DEVNODE) == 0;
}

static int vexa_pad_open(int flags) {
    if (!real_open || !real_ioctl || !real_close) {
        errno = ENODEV;
        return -1;
    }
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) < 0) {
        errno = ENODEV;
        return -1;
    }
    if (!track_fd(pipefd[0])) {
        close(pipefd[0]);
        close(pipefd[1]);
        errno = EMFILE;
        return -1;
    }
    pthread_mutex_lock(&pad_lock);
    if (pad_write_fd >= 0) {
        close(pad_write_fd);
    }
    pad_write_fd = pipefd[1];
    pthread_mutex_unlock(&pad_lock);
    /* Honour the caller's relevant flags on the read end. */
    if (real_fcntl) {
        real_fcntl(pipefd[0], F_SETFL, flags & ~(O_CREAT | O_TRUNC | O_APPEND));
    }
    pthread_once(&feeder_once, feeder_start);
    return pipefd[0];
}

static int open_dispatch(const char* path, int flags, int mode, int which) {
    if (is_pad_path(path)) {
        return vexa_pad_open(flags);
    }
    switch (which) {
    case 0:
        return real_open(path, flags, mode);
    case 1:
        return real_open64(path, flags, mode);
    }
    errno = ENOSYS;
    return -1;
}

static int openat_dispatch(int dirfd, const char* path, int flags, int mode, int which) {
    if (is_pad_path(path)) {
        return vexa_pad_open(flags);
    }
    switch (which) {
    case 0:
        return real_openat(dirfd, path, flags, mode);
    case 1:
        return real_openat64(dirfd, path, flags, mode);
    }
    errno = ENOSYS;
    return -1;
}

int open(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return open_dispatch(path, flags, mode, 0);
}

int open64(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return open_dispatch(path, flags, mode, 1);
}

int __open(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return open_dispatch(path, flags, mode, 0);
}

int openat(int dirfd, const char* path, int flags, ...) {
    int mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return openat_dispatch(dirfd, path, flags, mode, 0);
}

int openat64(int dirfd, const char* path, int flags, ...) {
    int mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return openat_dispatch(dirfd, path, flags, mode, 1);
}

int close(int fd) {
    if (is_pad_fd(fd)) {
        untrack_fd(fd);
    }
    return real_close(fd);
}

/* Bitmap helpers for EVIOCGBIT. */
static size_t fill_bit(unsigned long* bits, size_t bytes, int bit) {
    size_t need = (size_t)bit / 8 + 1;
    if (need <= bytes) {
        bits[bit / (8 * sizeof(unsigned long))] |= 1UL << (bit % (8 * sizeof(unsigned long)));
    }
    return need;
}

static int pad_ioctl(int fd, unsigned long request, void* arg) {
    switch (_IOC_NR(request)) {
    case 0x01:
        if (arg) {
            *(int*)arg = EV_VERSION;
            return 0;
        }
        return -EINVAL;
    case 0x02: {
        if (!arg) {
            return -EINVAL;
        }
        struct input_id* id = (struct input_id*)arg;
        id->bustype = BUS_USB;
        id->vendor = 0x045e;
        id->product = 0x028e;
        id->version = 0x0114;
        return 0;
    }
    case 0x06:
    case 0x07:
    case 0x08: {
        /* EVIOCGNAME / EVIOCGPHYS / EVIOCGUNIQ */
        if (!arg) {
            return -EINVAL;
        }
        const char* str = PAD_NAME;
        if (_IOC_NR(request) == 0x07) {
            str = PAD_PHYS;
        } else if (_IOC_NR(request) == 0x08) {
            str = PAD_UNIQ;
        }
        size_t len = strlcpy((char*)arg, str, _IOC_SIZE(request)) + 1;
        return (int)len;
    }
    case 0x09:
        /* EVIOCGPROP: no special properties. */
        if (arg) {
            memset(arg, 0, _IOC_SIZE(request));
        }
        return 0;
    case 0x0a:
        /* EVIOCGMTSLOTS: not multitouch. */
        if (arg && _IOC_SIZE(request) >= sizeof(int)) {
            *(int*)arg = -1;
        }
        return 0;
    case 0x18: case 0x19: case 0x1a: case 0x1b:
        /* EVIOCGKEY/LED/SND/SW: report the current state bitmap. */
        if (arg) {
            memset(arg, 0, _IOC_SIZE(request));
        }
        return 0;
    default:
        break;
    }

    const unsigned int nr = _IOC_NR(request);
    if (nr >= 0x20 && nr < 0x40) {
        /* EVIOCGBIT(ev): capability bitmaps. */
        if (!arg) {
            return -EINVAL;
        }
        const unsigned int ev = nr - 0x20;
        unsigned long* bits = (unsigned long*)arg;
        size_t bytes = _IOC_SIZE(request);
        size_t needed = 0;
        memset(bits, 0, bytes);
        if (ev == 0) {
            needed = fill_bit(bits, bytes, EV_KEY);
            needed = fill_bit(bits, bytes, EV_ABS) > needed ? fill_bit(bits, bytes, EV_ABS) : needed;
            needed = fill_bit(bits, bytes, EV_SYN) > needed ? needed : needed;
            /* No EV_FF: we do not emulate rumble yet. */
        } else if (ev == EV_KEY) {
            for (size_t i = 0; i < sizeof(kPadButtonBits) / sizeof(kPadButtonBits[0]); ++i) {
                needed = fill_bit(bits, bytes, kPadButtonBits[i]);
            }
        } else if (ev == EV_ABS) {
            static const uint8_t axes[] = {
                ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y,
            };
            for (size_t i = 0; i < sizeof(axes); ++i) {
                needed = fill_bit(bits, bytes, axes[i]);
            }
        }
        return (int)needed;
    }
    if (nr >= 0x40 && nr < 0x80 && _IOC_TYPE(request) == 'E' &&
        _IOC_SIZE(request) == sizeof(struct input_absinfo)) {
        /* EVIOCGABS */
        if (!arg) {
            return -EINVAL;
        }
        struct input_absinfo* abs = (struct input_absinfo*)arg;
        memset(abs, 0, sizeof(*abs));
        const unsigned int axis = nr - 0x40;
        switch (axis) {
        case ABS_X: case ABS_Y: case ABS_RX: case ABS_RY:
            abs->minimum = -32768;
            abs->maximum = 32767;
            abs->fuzz = 16;
            abs->flat = 128;
            return 0;
        case ABS_Z: case ABS_RZ:
            abs->minimum = 0;
            abs->maximum = 255;
            return 0;
        case ABS_HAT0X: case ABS_HAT0Y:
            abs->minimum = -1;
            abs->maximum = 1;
            return 0;
        default:
            return -EINVAL;
        }
    }
    if (nr >= 0xc0 && nr < 0xc0 + 64) {
        /* EVIOCSABS: accept and ignore. */
        return 0;
    }
    switch (request) {
    case 0x40044590: /* EVIOCGRAB */
    case 0x40044591: /* EVIOCREVOKE */
    case 0x400445a0: /* EVIOCSCLOCKID */
        return 0;
    case 0x80084504: /* EVIOCGKEYCODE (legacy, 8-byte payload) */
    case 0x80284504: /* EVIOCGKEYCODE_V2 (struct input_keymap_entry) */
    case 0x40044504:
    case 0x40284504:
    case 0x80044584: /* EVIOCGEFFECTS */
        return -EINVAL;
    default:
        return -EINVAL;
    }
}

int ioctl(int fd, unsigned long request, ...) {
    void* arg = NULL;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void*);
    va_end(ap);
    if (is_pad_fd(fd)) {
        return pad_ioctl(fd, request, arg);
    }
    return real_ioctl(fd, request, arg);
}

__attribute__((constructor)) static void vexa_evdev_init(void) {
    real_open = dlsym(RTLD_NEXT, "open");
    real_open64 = dlsym(RTLD_NEXT, "open64");
    real_openat = dlsym(RTLD_NEXT, "openat");
    real_openat64 = dlsym(RTLD_NEXT, "openat64");
    real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    real_close = dlsym(RTLD_NEXT, "close");
    real_fcntl = dlsym(RTLD_NEXT, "fcntl");
}
