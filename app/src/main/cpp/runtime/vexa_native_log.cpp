//
// Created by critical on 10.03.2026.
//
#include <jni.h>
#include <android/log.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <type_traits>

#include "vexa_native_log.h"

#define VEXA_TAG "VEXA"

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

        jclass bridge = env->FindClass("com/critical/vexaemulator/RuntimeBridge");
        if (!bridge) return;

        jmethodID mid = env->GetStaticMethodID(
                bridge,
                "logFromNative",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"

        );
        if (!mid) {
            env->DeleteLocalRef(bridge);
            return;
        }

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
            env->DeleteLocalRef(bridge);
            return;
        }

        env->CallStaticVoidMethod(bridge, mid, jLevel, jCategory, jMsg, jFields);

        env->DeleteLocalRef(jLevel);
        env->DeleteLocalRef(jCategory);
        env->DeleteLocalRef(jMsg);
        env->DeleteLocalRef(bridge);
        env->DeleteLocalRef(jFields);
    }

} // namespace Vexa::Log