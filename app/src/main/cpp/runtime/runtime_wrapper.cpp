//
// Created by critical on 9.03.2026.
//
#include <jni.h>
#include <android/log.h>

#include "vexa_native_log.h"

#define VEXA_TAG "VEXA"

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStartRuntime(JNIEnv *env, jobject thiz,
                                                                jstring executablePath,
                                                                jstring rootfsPath,
                                                                jstring thunkHostPath,
                                                                jstring thunkGuestPath,
                                                                jstring workingDirectory,
                                                                jstring artifactDirectory) {
    const char *executableTranslated = env->GetStringUTFChars(executablePath, nullptr);
    if (!executableTranslated) {
    }
    const char *rootfsPathTranslated = env->GetStringUTFChars(rootfsPath, nullptr);
    if (!rootfsPathTranslated) {
        __android_log_print(ANDROID_LOG_ERROR, "VEXA",
                            "Failed to read rootfsPathTranslated from JNI");
        return;
    }
    const char *thunkHostPathTranslated = env->GetStringUTFChars(thunkHostPath, nullptr);
    if (!thunkHostPathTranslated) {
        __android_log_print(ANDROID_LOG_ERROR, "VEXA",
                            "Failed to read thunkHostPathTranslated from JNI");
        return;
    }
    const char *thunkGuestPathTranslated = env->GetStringUTFChars(thunkGuestPath, nullptr);
    if (!thunkGuestPathTranslated) {
        __android_log_print(ANDROID_LOG_ERROR, "VEXA",
                            "Failed to read thunkGuestPathTranslated from JNI");
        return;
    }
    const char *workingDirectoryTranslated = env->GetStringUTFChars(workingDirectory, nullptr);
    if (!workingDirectoryTranslated) {
        __android_log_print(ANDROID_LOG_ERROR, "VEXA",
                            "Failed to read workingDirectoryTranslated from JNI");
        return;
    }
    const char *artifactDirectoryTranslated = env->GetStringUTFChars(artifactDirectory, nullptr);
    if (!artifactDirectoryTranslated) {
        __android_log_print(ANDROID_LOG_ERROR, "VEXA",
                            "Failed to read artifactDirectoryTranslated from JNI");
        return;
    }
    VEXA_LOGI(env, "BOOT", "startRuntime is called from JNI",
              Vexa::Log::AddFields({{
                                            Vexa::Log::F("executableTranslated",
                                                         executableTranslated)
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
                                            Vexa::Log::F("workingDirectoryTranslated",
                                                         workingDirectoryTranslated)
                                    },
                                    {
                                            Vexa::Log::F("artifactDirectoryTranslated",
                                                         artifactDirectoryTranslated)
                                    }}).c_str());
    env->ReleaseStringUTFChars(executablePath, executableTranslated);
    env->ReleaseStringUTFChars(rootfsPath, rootfsPathTranslated);
    env->ReleaseStringUTFChars(thunkHostPath, thunkHostPathTranslated);
    env->ReleaseStringUTFChars(thunkGuestPath, thunkGuestPathTranslated);
    env->ReleaseStringUTFChars(workingDirectory, workingDirectoryTranslated);
    env->ReleaseStringUTFChars(artifactDirectory, artifactDirectoryTranslated);
};

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStopRuntime(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_INFO, VEXA_TAG, "stopRuntime is called");
}