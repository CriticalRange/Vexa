//
// Created by critical on 9.03.2026.
//

#include <jni.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <memory>
#include <atomic>

#include "logging/crash_signals.h"
#include "common/paths.h"
#include "common/status.h"
#include "logging/native_log.h"
#include "runtime/init/preflight.h"
#include "runtime/launch.h"
#include "utils/jni_scoped.h"
#include "runtime/surface_bridge.h"

namespace {
    std::atomic<uint64_t> g_RuntimeLaunchSerial{0};

    struct RuntimeLaunchPayload {
        JavaVM *vm{};
        Vexa::Common::Paths paths{};
        std::vector<std::string> launchEnv{};
        std::vector<std::string> launchArgs{};
        jint result{
                static_cast<jint>(Vexa::Common::ToInt(Vexa::Common::Code::InternalError))
        };
    };

    void *RuntimeLaunchThreadMain(void *opaque) {
        auto *payload =
                static_cast<RuntimeLaunchPayload *>(opaque);

        if (!payload || !payload->vm) return nullptr;

        pthread_setname_np(pthread_self(), "runtime_worker");

        bool attached = false;
        JNIEnv *env = nullptr;

        const jint envState =
                payload->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (envState == JNI_EDETACHED) {
            if (payload->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
                Vexa::Log::VexaNativeLogRaw("ERROR", "THREAD", "AttachCurrentThread failed", "{}");
                return nullptr;
            }
            attached = true;
        } else if (envState != JNI_OK || !env) {
            Vexa::Log::VexaNativeLogRaw("ERROR", "THREAD", "GetEnv failed on runtime pthread",
                                        "{}");
            return nullptr;
        }

        pthread_attr_t selfAttr{};
        if (pthread_getattr_np(pthread_self(), &selfAttr) == 0) {
            void *stackAddr = nullptr;
            size_t stackSize = 0;
            if (pthread_attr_getstack(&selfAttr, &stackAddr, &stackSize) == 0) {
                VEXA_LOGI(
                        env,
                        "THREAD", "runtime_worker pthread stack",
                        Vexa::Log::AddFields({}).c_str());
            }
            pthread_attr_destroy(&selfAttr);
        }

        auto preflight = Vexa::Runtime::RunPreflight(payload->paths);
        if (!preflight.Ok()) {
            VEXA_LOGE(env, "BOOT", "Runtime preflight failed", Vexa::Log::AddFields({
                                                                                            Vexa::Log::F(
                                                                                                    "reason",
                                                                                                    preflight.reason),
                                                                                            Vexa::Log::F(
                                                                                                    "detail",
                                                                                                    preflight.detail)
                                                                                    }).c_str());
            payload->result = static_cast<jint>(preflight.code);
        } else {
            auto launch = Vexa::Runtime::StartRuntime(env, payload->paths, payload->launchEnv,
                                                      payload->launchArgs);
            if (!launch.Ok()) {
                VEXA_LOGE(env, "FEX", "FEX Launch failed", Vexa::Log::AddFields({
                                                                                        Vexa::Log::F(
                                                                                                "reason",
                                                                                                launch.reason),
                                                                                        Vexa::Log::F(
                                                                                                "detail",
                                                                                                launch.detail),
                                                                                }).c_str());
                payload->result = static_cast<jint>(launch.code);
            } else {
                payload->result = 0;
            }
        }

        if (attached) payload->vm->DetachCurrentThread();
        return nullptr;
    }
}

extern "C" __attribute__((visibility("default")))
uint64_t Vexa_GetRuntimeLaunchSerial() {
    return g_RuntimeLaunchSerial.load(std::memory_order_acquire);
}

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

    JavaVM *vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || !vm) {
        VEXA_LOGE(env, "THREAD", "GetJavaVM failed", "{}");
        return static_cast<jint>(Vexa::Common::ToInt(Vexa::Common::Code::InternalError));
    }

    auto payload = std::make_unique<RuntimeLaunchPayload>();
    payload->vm = vm;
    payload->paths = paths;
    payload->launchEnv = std::move(launchEnvVec);
    payload->launchArgs = std::move(launchArgsVec);

    pthread_attr_t attr{};
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16 * 1024 * 1024);
    pthread_attr_setguardsize(&attr, 1024 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    sched_param sp{};
    sp.sched_priority = 0;
    pthread_attr_setschedparam(&attr, &sp);

    pthread_t tid{};
    const int rc = pthread_create(&tid, &attr, RuntimeLaunchThreadMain, payload.get());
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        VEXA_LOGE(env, "THREAD", "pthread_create failed",
                  Vexa::Log::AddFields({Vexa::Log::F("errno",
                                                     rc), Vexa::Log::F("error",
                                                                       std::strerror(
                                                                               rc))}).c_str());
        return static_cast<jint>(Vexa::Common::ToInt(Vexa::Common::Code::InternalError));
    }
    pthread_setname_np(tid, "runtime_worker");

    (void) pthread_join(tid, nullptr);
    const jint result = payload->result;
    payload.reset();
    return result;

    VEXA_LOGI(env, "BOOT", "Runtime started", "{}");
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