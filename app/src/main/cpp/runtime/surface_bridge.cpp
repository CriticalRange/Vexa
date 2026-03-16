//
// Created by critical on 15.03.2026.
//

#include "surface_bridge.h"

#include <android/native_window_jni.h>
#include <mutex>

namespace {
    std::mutex g_mutex;
    ANativeWindow *g_window{};
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
    }
}

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindow() {
    return Vexa::Runtime::SurfaceBridge::Get();
}