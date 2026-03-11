//
// Created by critical on 9.03.2026.
//
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common/preflight_paths.h"
#include "logging/native_log.h"
#include "runtime/init/preflight.h"
#include "runtime/launch.h"
#include "utils/jni_scoped.h"

extern "C"
JNIEXPORT jint JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStartRuntime(JNIEnv *env, jobject thiz,
                                                                jstring executable,
                                                                jstring rootfsPath,
                                                                jstring thunkHostPath,
                                                                jstring thunkGuestPath,
                                                                jstring workingDirectory,
                                                                jstring artifactDirectory) {
    Vexa::Utils::ScopedUtfChars exec(env, executable);
    Vexa::Utils::ScopedUtfChars working(env, workingDirectory);
    Vexa::Utils::ScopedUtfChars rootfs(env, rootfsPath);
    Vexa::Utils::ScopedUtfChars thunkHost(env, thunkHostPath);
    Vexa::Utils::ScopedUtfChars thunkGuest(env, thunkGuestPath);
    Vexa::Utils::ScopedUtfChars artifactDir(env, artifactDirectory);

    if (!exec.ok() || !rootfs.ok() || !thunkHost.ok() || !thunkGuest.ok() || !working.ok() ||
        !artifactDir.ok()) {
        VEXA_LOGE(env, "BOOT", "JNI string conversion failed", "{}");
        return static_cast<jint>(Vexa::Runtime::PreflightCode::InternalError);
    }

    Vexa::Common::PreflightPaths paths{
            exec.get(),
            rootfs.get(),
            thunkHost.get(),
            thunkGuest.get(),
            working.get(),
            artifactDir.get()
    };

    // Runs preflight
    auto preflight = Vexa::Runtime::RunPreflight(paths);
    if (preflight.code != Vexa::Runtime::PreflightCode::Ok) {
        auto fields = Vexa::Log::AddFields({
                                                   Vexa::Log::F("reason", preflight.reason),
                                                   Vexa::Log::F("detail", preflight.detail)
                                           });
        VEXA_LOGE(env, "BOOT", "Runtime preflight failed", fields.c_str());
        return static_cast<jint>(preflight.code);
    }
    // Launches FEX Runtime
    auto launch = Vexa::Runtime::StartRuntime(env, paths);
    if (launch.code != 0) {
        auto fields = Vexa::Log::AddFields({
                                                   Vexa::Log::F("reason", launch.reason),
                                                   Vexa::Log::F("detail", launch.detail)
                                           });
        VEXA_LOGE(env, "FEX", "FEX Launch failed", fields.c_str());
        return static_cast<jint>(launch.code);
    }

    auto boot_status_fields = Vexa::Log::AddFields({
                                                           Vexa::Log::F("Preflight", "OK"),
                                                           Vexa::Log::F("FEX", "OK")
                                                   });
    VEXA_LOGI(env, "BOOT", "Runtime started", boot_status_fields.c_str());
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStopRuntime(JNIEnv *env, jobject thiz) {
    VEXA_LOGI(env, "BOOT", "native stopRuntime is called", "{}");
    Vexa::Runtime::StopRuntime();
}