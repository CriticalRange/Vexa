//
// Created by critical on 15.03.2026.
//

#include "surface_bridge.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <mutex>

namespace {
    std::mutex g_mutex;
    ANativeWindow *g_window{};
    std::atomic<uint64_t> g_surface_serial{1};
}

namespace Vexa::Runtime::SurfaceBridge {
    void Set(JNIEnv *env, jobject surface) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_window) {
            ANativeWindow_release(g_window);
            g_window = nullptr;
        }
        if (surface) {
            g_window = ANativeWindow_fromSurface(env, surface);
        }
        g_surface_serial.fetch_add(1, std::memory_order_release);
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
int g_Vexa_SDLContextCreated = 0;

extern "C" __attribute__((visibility("default")))
int Vexa_GetSDLContextCreated();

extern "C" __attribute__((visibility("default")))
int Vexa_GetSDLContextCreated() {
    return g_Vexa_SDLContextCreated;
}