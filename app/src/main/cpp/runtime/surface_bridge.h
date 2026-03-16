//
// Created by critical on 15.03.2026.
//

#ifndef VEXA_EMULATOR_SURFACE_BRIDGE_H
#define VEXA_EMULATOR_SURFACE_BRIDGE_H

#include <jni.h>

struct ANativeWindow;

namespace Vexa::Runtime::SurfaceBridge {
    void Set(JNIEnv *env, jobject surface);

    ANativeWindow *Get();

    void Clear();
}

extern "C" __attribute__((visibility("default")))
ANativeWindow *Vexa_GetRuntimeNativeWindow();

#endif //VEXA_EMULATOR_SURFACE_BRIDGE_H
