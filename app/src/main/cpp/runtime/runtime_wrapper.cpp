//
// Created by critical on 9.03.2026.
//
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

#include "vexa_native_log.h"

#define VEXA_TAG "VEXA"

static bool IsReadableDir(const char *path) {
    if (!path) return false;
    struct stat st{};
    if (stat(path, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return false;
    return access(path, R_OK | X_OK) == 0;
}

static bool IsExecutable(const char *path) {
    if (!path) return false;
    struct stat st{};
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return access(path, R_OK | X_OK) == 0;
}

extern "C"
// Preflight codes (Determines whether FEX will run or not
// 0 = OK
// 1 = Working directory missing/unreadable
// 2 = rootfs missing/unreadable
// 3 = Host thunk directory missing/unreadable
// 4 = Guest thunk directory missing/unreadable
// 5 = Executable path missing/unreadable
// 6 = Artifacts directory missing/unreadable
// 7 = libFEXCore load failed
// 8 = JNI/internal error

JNIEXPORT jint JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStartRuntime(JNIEnv *env, jobject thiz,
                                                                jstring executable,
                                                                jstring rootfsPath,
                                                                jstring thunkHostPath,
                                                                jstring thunkGuestPath,
                                                                jstring workingDirectory,
                                                                jstring artifactDirectory) {
    const char *workingDirectoryTranslated = env->GetStringUTFChars(workingDirectory, nullptr);
    if (!workingDirectoryTranslated || !IsReadableDir(workingDirectoryTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 1: Failed to read working Directory!",
                  nullptr);
        return 1;
    }
    const char *rootfsPathTranslated = env->GetStringUTFChars(rootfsPath, nullptr);
    if (!rootfsPathTranslated || !IsReadableDir(rootfsPathTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 2: Failed to read Rootfs Path!",
                  nullptr);
        env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
        // return 2;
    }
    const char *thunkHostPathTranslated = env->GetStringUTFChars(thunkHostPath, nullptr);
    if (!thunkHostPathTranslated || !IsReadableDir(thunkHostPathTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 3: Failed to read Host Thunk Path!",
                  nullptr);
        env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
        env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
        // return 3;
    }
    const char *thunkGuestPathTranslated = env->GetStringUTFChars(thunkGuestPath, nullptr);
    if (!thunkGuestPathTranslated || !IsReadableDir(thunkGuestPathTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 4: Failed to read Guest Thunk Path!",
                  nullptr);
        env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
        env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
        env->ReleaseStringUTFChars(thunkHostPath, thunkHostPathTranslated);
        // return 4;
    }
    const char *executableTranslated = env->GetStringUTFChars(executable, nullptr);
    if (!executableTranslated || !IsExecutable(executableTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 5: Failed to launch Executable!",
                  nullptr);
        env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
        env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
        env->ReleaseStringUTFChars(thunkHostPath, thunkHostPathTranslated);
        env->ReleaseStringUTFChars(thunkGuestPath, thunkGuestPathTranslated);
        return 5;
    }
    const char *artifactDirectoryTranslated = env->GetStringUTFChars(artifactDirectory, nullptr);
    if (!artifactDirectoryTranslated || !IsReadableDir(artifactDirectoryTranslated)) {
        VEXA_LOGE(env, "BOOT",
                  "Application failed with code 6: Failed to read Artifacts Directory!",
                  nullptr);
        env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
        env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
        env->ReleaseStringUTFChars(thunkHostPath, thunkHostPathTranslated);
        env->ReleaseStringUTFChars(thunkGuestPath, thunkGuestPathTranslated);
        env->ReleaseStringUTFChars(executable, executableTranslated);
        // return 6;
    }

    if (IsExecutable(executableTranslated)) {
        dlopen("libFEXCore.so", RTLD_NOW);
    }


    VEXA_LOGI(env, "BOOT", "startRuntime is called from JNI",
              Vexa::Log::AddFields({{
                                            Vexa::Log::F("workingDirectoryTranslated",
                                                         workingDirectoryTranslated)
                                    },
                                    {
                                            Vexa::Log::F("rootfsPathTranslated",
                                                         rootfsPathTranslated)
                                    },
                                    {
                                            Vexa::Log::F("thunkHostPathTranslated",
                                                         thunkHostPathTranslated)
                                    },
                                    {
                                            Vexa::Log::F("thunkGuestPathTranslated",
                                                         thunkGuestPathTranslated)
                                    },
                                    {
                                            Vexa::Log::F("executablePathTranslated",
                                                         executableTranslated)
                                    },
                                    {
                                            Vexa::Log::F("artifactDirectoryTranslated",
                                                         artifactDirectoryTranslated)
                                    }}).c_str());
    env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
    env->ReleaseStringUTFChars(thunkHostPath, thunkHostPathTranslated);
    env->ReleaseStringUTFChars(thunkGuestPath, thunkGuestPathTranslated);
    env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
    env->ReleaseStringUTFChars(executable, executableTranslated);
    env->ReleaseStringUTFChars(artifactDirectory, artifactDirectoryTranslated);
    return 0;
};

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStopRuntime(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_INFO, VEXA_TAG, "stopRuntime is called");
}