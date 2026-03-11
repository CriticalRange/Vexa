//
// Created by critical on 10.03.2026.
//
#include <jni.h>
#include <android/log.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <string>
#include <type_traits>
#include <mutex>

#include "native_log.h"

#define VEXA_TAG "VEXA"

static JavaVM *g_vm = nullptr;
static jobject g_log_sink = nullptr; // GlobalRef
static jmethodID g_on_native_log = nullptr;
static std::mutex g_log_sink_mutex;

jint JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

namespace Vexa::Log {

    std::string AddFields(
            std::initializer_list<std::pair<std::string, std::string>> fields
    ) {
        rapidjson::Document d;
        d.SetObject();
        auto &a = d.GetAllocator();

        for (const auto &kv: fields) {
            rapidjson::Value key;
            rapidjson::Value value;
            key.SetString(kv.first.c_str(), static_cast<rapidjson::SizeType>(kv.first.size()), a);
            value.SetString(kv.second.c_str(), static_cast<rapidjson::SizeType>(kv.second.size()),
                            a);

            d.AddMember(key, value, a);
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        d.Accept(writer);
        return std::string(buffer.GetString());
    }

    void VexaNativeLog(JNIEnv *env, const char *level, const char *category, const char *msg,
                       const char *fieldsJson) {
        if (!env) return;


        jstring jLevel = env->NewStringUTF(level ? level : "INFO");
        jstring jCategory = env->NewStringUTF(category ? category : "RUNTIME");
        jstring jMsg = env->NewStringUTF(msg ? msg : "");
        jstring jFields = env->NewStringUTF(fieldsJson ? fieldsJson : "{}");

        if (!jLevel || !jCategory || !jMsg || !jFields) {
            __android_log_print(ANDROID_LOG_ERROR, VEXA_TAG,
                                "NewStringUTF failed (OOM or invalid input)");
            if (jLevel) env->DeleteLocalRef(jLevel);
            if (jCategory) env->DeleteLocalRef(jCategory);
            if (jMsg) env->DeleteLocalRef(jMsg);
            if (jFields) env->DeleteLocalRef(jFields);
            return;
        }

        jobject sink = nullptr;
        jmethodID methodId = nullptr;
        std::lock_guard<std::mutex> lock(g_log_sink_mutex);
        if (g_log_sink && g_on_native_log) {
            sink = g_log_sink;
            methodId = g_on_native_log;
            if (jLevel && jCategory && jMsg && jFields) {
                env->CallVoidMethod(
                        sink,
                        methodId,
                        jLevel,
                        jCategory,
                        jMsg,
                        jFields);
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
            }
        }

        env->DeleteLocalRef(jLevel);
        env->DeleteLocalRef(jCategory);
        env->DeleteLocalRef(jMsg);
        env->DeleteLocalRef(jFields);
    }

    void VexaNativeLogRaw(const char *level, const char *category, const char *message,
                          const char *fields) {
        if (!g_vm) return;

        JNIEnv *env = nullptr;
        bool didAttach = false;

        jint state = g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (state == JNI_EDETACHED) {
            if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
            didAttach = true;
        } else if (state != JNI_OK || !env) {
            return;
        }

        VexaNativeLog(
                env,
                level ? level : "INFO",
                category ? category : "RUNTIME",
                message ? message : "",
                fields ? fields : "{}"
        );
        if (didAttach) {
            g_vm->DetachCurrentThread();
        }
    }

} // namespace Vexa::Log

extern "C"
JNIEXPORT void JNICALL
Java_com_critical_vexaemulator_RuntimeBridge_nativeSetLogSink(JNIEnv *env, jobject thiz,
                                                              jobject sink) {
    std::lock_guard<std::mutex>
            lock(g_log_sink_mutex);

    if (g_log_sink) {
        env->DeleteGlobalRef(g_log_sink);
        g_log_sink = nullptr;
        g_on_native_log = nullptr;
    }

    if (!sink) return;

    jclass sinkCls = env->GetObjectClass(sink);
    if (!sinkCls) return;

    jmethodID mid = env->GetMethodID(
            sinkCls,
            "onNativeLog",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"
    );
    env->DeleteLocalRef(sinkCls);
    if (!mid) return;

    g_log_sink = env->NewGlobalRef(sink);
    g_on_native_log = mid;
}