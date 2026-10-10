/* Host preload. Runs before app_process main(), so before ART builds the heap.
 * Android 16 asks for the moving space at 0x2000000. Brotato is ET_EXEC and
 * MAP_FIXEDs its load segments through that range. Holding the VA first makes
 * ART's MapAnonymousPreferredAddress retry somewhere else. */
#include <android/log.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0
#endif

#define LOG_TAG "vexa-reserve"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* 32 MiB through 64 MiB. Brotato's RW segment ends near 0x27c3000 and FEX
 * maps an 8 MiB brk after it (~0x2fc3000). 64 MiB leaves headroom. */
static const uintptr_t kReserveBegin = 0x02000000u;
static const uintptr_t kReserveEnd = 0x04000000u;

static void log_cmdline(void) {
    char buf[128];
    FILE* fp = fopen("/proc/self/cmdline", "re");
    size_t n = 0;
    if (fp) {
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
    }
    buf[n] = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0) {
            buf[i] = ' ';
        }
    }
    LOGI("pid=%d cmd=%s", (int)getpid(), n ? buf : "?");
}

static void log_maps(void) {
    FILE* fp = fopen("/proc/self/maps", "re");
    char line[256];
    if (!fp) {
        LOGE("open maps errno=%d", errno);
        return;
    }
    while (fgets(line, sizeof(line), fp)) {
        unsigned long start = 0;
        unsigned long end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            continue;
        }
        if (end <= kReserveBegin || start >= kReserveEnd) {
            continue;
        }
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[len - 1] = 0;
        }
        LOGI("map %s", line);
    }
    fclose(fp);
}

static size_t reserve_span(uintptr_t at, size_t length) {
    void* p = mmap((void*)at, length, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE, -1, 0);
    if (p != (void*)at) {
        return 0;
    }
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, at, length, "vexa-guest-va");
    return length;
}

__attribute__((constructor(101))) static void vexa_reserve_low(void) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    const size_t chunk = 2u * 1024u * 1024u;
    size_t ok = 0;
    size_t blocked = 0;
    log_cmdline();
    if (page == 0 || (page & (page - 1)) != 0 || chunk < page) {
        LOGE("bad page size %zu", page);
        return;
    }
    ok = reserve_span(kReserveBegin, kReserveEnd - kReserveBegin);
    if (ok == 0) {
        for (uintptr_t at = kReserveBegin; at < kReserveEnd;) {
            size_t length = chunk;
            if (at + length > kReserveEnd) {
                length = (size_t)(kReserveEnd - at);
            }
            size_t got = reserve_span(at, length);
            if (got != 0) {
                ok += got;
                at += length;
                continue;
            }
            for (uintptr_t page_at = at; page_at < at + length; page_at += page) {
                if (reserve_span(page_at, page) != 0) {
                    ok += page;
                } else {
                    blocked += page;
                }
            }
            at += length;
        }
    }
    LOGI("reserve 0x%lx-0x%lx page=%zu ok=%zu blocked=%zu",
         (unsigned long)kReserveBegin, (unsigned long)kReserveEnd, page, ok, blocked);
    log_maps();
}
