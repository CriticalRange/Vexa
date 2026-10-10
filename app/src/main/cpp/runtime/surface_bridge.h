//
// Created by critical on 15.03.2026.
//

#ifndef VEXA_EMULATOR_SURFACE_BRIDGE_H
#define VEXA_EMULATOR_SURFACE_BRIDGE_H

#include <cstdint>
#include <jni.h>

struct ANativeWindow;

namespace Vexa::Runtime::SurfaceBridge {
    void Set(JNIEnv *env, jobject surface);

    ANativeWindow *Get();

    ANativeWindow *GetRetained();

    void Clear();
}

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindow();

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindowRetained();

extern "C" __attribute__((visibility("default")))
uint64_t Vexa_GetRuntimeSurfaceSerial();

extern "C" __attribute__((visibility("default")))
void Vexa_SetGuestRootfs(const char *rootfs);

extern "C" __attribute__((visibility("default")))
void Vexa_PublishGuestWindowSize(int width, int height);

#endif //VEXA_EMULATOR_SURFACE_BRIDGE_H
