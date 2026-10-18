//
// Created by critical on 15.03.2026.
//

#include "surface_bridge.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/stat.h>

extern "C" void Vexa_PublishGuestWindowSize(int width, int height);

namespace {
    std::mutex g_mutex;
    ANativeWindow *g_window{};
    std::atomic<uint64_t> g_surface_serial{1};
    std::string g_guest_rootfs;
}

namespace Vexa::Runtime::SurfaceBridge {
    void Set(JNIEnv *env, jobject surface) {
        int w = 0;
        int h = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_window) {
                ANativeWindow_release(g_window);
                g_window = nullptr;
            }
            if (surface) {
                g_window = ANativeWindow_fromSurface(env, surface);
                if (g_window) {
                    w = ANativeWindow_getWidth(g_window);
                    h = ANativeWindow_getHeight(g_window);
                }
            }
            g_surface_serial.fetch_add(1, std::memory_order_release);
        }
        // Keep the guest-visible size file in sync: the fake X11/RandR/Xinerama
        // libs and the GLX thunk all read it. A stale or missing size is how a
        // guest ends up believing the screen is 0x0.
        if (w > 1 && h > 1) {
            Vexa_PublishGuestWindowSize(w, h);
        }
    }

    ANativeWindow *GetRetained() {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_window) {
            ANativeWindow_acquire(g_window);
        }
        return g_window;
    }

    ANativeWindow *Get() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_window;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_window) {
            ANativeWindow_release(g_window);
            g_window = nullptr;
        }
        g_surface_serial.fetch_add(1, std::memory_order_release);
    }
}

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindow() {
    return Vexa::Runtime::SurfaceBridge::Get();
}

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindowRetained() {
    return Vexa::Runtime::SurfaceBridge::GetRetained();
}

extern "C" __attribute__((visibility("default")))
uint64_t Vexa_GetRuntimeSurfaceSerial() {
    return g_surface_serial.load(std::memory_order_acquire);
}

extern "C" __attribute__((visibility("default")))
void Vexa_SetGuestRootfs(const char *rootfs) {
    if (!rootfs || !rootfs[0]) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_guest_rootfs = rootfs;
}

extern "C" __attribute__((visibility("default")))
void Vexa_PublishGuestWindowSize(int width, int height) {
    if (width <= 1 || height <= 1) {
        return;
    }
    std::string root;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        root = g_guest_rootfs;
    }
    if (root.empty()) {
        std::fprintf(stderr, "[VEXA][WINDOW] no rootfs for window size\n");
        return;
    }
    const std::string dir = root + "/tmp";
    mkdir(dir.c_str(), 0755);
    const std::string path = dir + "/vexa-window-size";
    FILE *file = std::fopen(path.c_str(), "w");
    if (!file) {
        std::fprintf(stderr, "[VEXA][WINDOW] cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(file, "%d %d\n", width, height);
    std::fclose(file);
    std::fprintf(stderr, "[VEXA][WINDOW] guest size %dx%d\n", width, height);
}

// Where the GL host placed the guest frame, in the guest rootfs. Ten integers:
// content x0 y0 cw ch, crop sx sy src_w src_h, surface sw sh.
// Identity (src == surface, origin 0) means a touch is already a window pixel.
extern "C" __attribute__((visibility("default")))
void Vexa_PublishPresentMap(int x0, int y0, int cw, int ch, int sx, int sy, int src_w, int src_h, int sw, int sh) {
    if (sw < 2 || sh < 2 || cw < 1 || ch < 1 || src_w < 1 || src_h < 1) {
        return;
    }
    std::string root;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        root = g_guest_rootfs;
    }
    if (root.empty()) {
        return;
    }
    const std::string dir = root + "/tmp";
    mkdir(dir.c_str(), 0755);
    const std::string path = dir + "/vexa-present";
    const std::string tmp = path + ".tmp";
    FILE *file = std::fopen(tmp.c_str(), "w");
    if (!file) {
        return;
    }
    std::fprintf(file, "%d %d %d %d %d %d %d %d %d %d\n", x0, y0, cw, ch, sx, sy, src_w, src_h, sw, sh);
    std::fclose(file);
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
    }
}

// Display preferences shared with the GL host: fit mode (/tmp/vexa-fit, "aspect"
// letterboxes, anything else stretches) and the refresh rate of the selected
// RandR mode (3rd field of /tmp/vexa-mode). Re-read at most every 250 ms.
extern "C" __attribute__((visibility("default")))
void Vexa_GetDisplayPrefs(int *aspect, int *rate_hz) {
    static std::mutex mu;
    static std::chrono::steady_clock::time_point last{};
    static int cached_aspect = 0;
    static int cached_rate = 0;
    std::lock_guard<std::mutex> lock(mu);
    const auto now = std::chrono::steady_clock::now();
    if (last.time_since_epoch().count() == 0 || now - last > std::chrono::milliseconds(250)) {
        last = now;
        std::string root;
        {
            std::lock_guard<std::mutex> l2(g_mutex);
            root = g_guest_rootfs;
        }
        cached_aspect = 0;
        cached_rate = 0;
        if (!root.empty()) {
            char buf[64] = {};
            if (FILE *f = std::fopen((root + "/tmp/vexa-fit").c_str(), "r")) {
                if (std::fgets(buf, sizeof(buf), f)) cached_aspect = std::strncmp(buf, "aspect", 6) == 0;
                std::fclose(f);
            }
            int w = 0, h = 0, r = 0;
            if (FILE *f = std::fopen((root + "/tmp/vexa-mode").c_str(), "r")) {
                if (std::fscanf(f, "%d %d %d", &w, &h, &r) == 3) cached_rate = r;
                std::fclose(f);
            }
        }
    }
    if (aspect) *aspect = cached_aspect;
    if (rate_hz) *rate_hz = cached_rate;
}

extern "C" __attribute__((visibility("default")))
int g_Vexa_SDLContextCreated = 0;

extern "C" __attribute__((visibility("default")))
int Vexa_GetSDLContextCreated();

extern "C" __attribute__((visibility("default")))
int Vexa_GetSDLContextCreated() {
    return g_Vexa_SDLContextCreated;
}