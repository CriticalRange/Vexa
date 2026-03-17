//
// Created by critical on 9.03.2026.
//

#include <jni.h>
#include <vector>
#include <string>
#include <cstdlib>

#include "logging/crash_signals.h"
#include "common/paths.h"
#include "common/status.h"
#include "logging/native_log.h"
#include "runtime/init/preflight.h"
#include "runtime/launch.h"
#include "utils/jni_scoped.h"
#include "runtime/surface_bridge.h"

static std::vector<std::string>
JStringArrayToVector(JNIEnv *env, jobjectArray arr) {
    std::vector<std::string> out;
    if (!arr) return out;
    const jsize n = env->GetArrayLength(arr);
    out.reserve(static_cast<size_t>(n));
    for (jsize i = 0; i < n; ++i) {
        auto *jstr =
                static_cast<jstring>(env->GetObjectArrayElement(arr,
                                                                i));
        if (!jstr) continue;
        const char *raw =
                env->GetStringUTFChars(jstr, nullptr);
        if (raw) {
            out.emplace_back(raw);
            env->ReleaseStringUTFChars(jstr, raw);
        }
        env->DeleteLocalRef(jstr);
    }
    return out;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStartRuntime(JNIEnv *env, jobject thiz,
                                                                jstring executable,
                                                                jstring rootfsPath,
                                                                jstring thunkHostPath,
                                                                jstring thunkGuestPath,
                                                                jstring workingDirectory,
                                                                jstring artifactDirectory,
                                                                jobjectArray launchEnv,
                                                                jobjectArray launchArgs) {
    Vexa::Utils::ScopedUtfChars exec(env, executable);
    Vexa::Utils::ScopedUtfChars working(env, workingDirectory);
    Vexa::Utils::ScopedUtfChars rootfs(env, rootfsPath);
    Vexa::Utils::ScopedUtfChars thunkHost(env, thunkHostPath);
    Vexa::Utils::ScopedUtfChars thunkGuest(env, thunkGuestPath);
    Vexa::Utils::ScopedUtfChars artifactDir(env, artifactDirectory);

    Vexa::Log::InstallSignalHandlers();

    if (!exec.ok() || !rootfs.ok() || !thunkHost.ok() || !thunkGuest.ok() || !working.ok() ||
        !artifactDir.ok()) {
        VEXA_LOGE(env, "BOOT", "JNI string conversion failed", "{}");
        return static_cast<jint>(Vexa::Common::ToInt(Vexa::Common::Code::InternalError));
    }

    Vexa::Common::Paths paths{
            exec.get(),
            rootfs.get(),
            thunkHost.get(),
            thunkGuest.get(),
            working.get(),
            artifactDir.get()
    };

    auto launchEnvVec = JStringArrayToVector(env,
                                             launchEnv);
    auto launchArgsVec = JStringArrayToVector(env,
                                              launchArgs);

    // Runs preflight
    auto preflight = Vexa::Runtime::RunPreflight(paths);
    if (!preflight.Ok()) {
        auto fields = Vexa::Log::AddFields({
                                                   Vexa::Log::F("reason", preflight.reason),
                                                   Vexa::Log::F("detail", preflight.detail)
                                           });
        VEXA_LOGE(env, "BOOT", "Runtime preflight failed", fields.c_str());
        return static_cast<jint>(preflight.code);
    }
    // Launches FEX Runtime
    auto launch = Vexa::Runtime::StartRuntime(env, paths, launchEnvVec, launchArgsVec);
    if (!launch.Ok()) {
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
    Vexa::Log::UninstallSignalHandlers();
    Vexa::Runtime::SurfaceBridge::Clear();
    Vexa::Runtime::StopRuntime();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeSetRuntimeSurface(
        JNIEnv *env,
        jobject /*thiz*/,
        jobject surface
) {
    Vexa::Runtime::SurfaceBridge::Set(env, surface);
}