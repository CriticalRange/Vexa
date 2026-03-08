//
// Created by critical on 9.03.2026.
//
#include <jni.h>
#include <android/log.h>

#define VEXA_TAG "VEXA"

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStartRuntime(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_INFO, VEXA_TAG, "startRuntime is called");
};

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeStopRuntime(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_INFO, VEXA_TAG, "stopRuntime is called");
}