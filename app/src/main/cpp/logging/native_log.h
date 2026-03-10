//
// Created by critical on 10.03.2026.
//
#pragma once

#include <jni.h>
#include <initializer_list>
#include <string>
#include <type_traits>

namespace Vexa::Log {
    using Field = std::pair<std::string, std::string>;

    inline std::string ToString(const std::string &v) { return v; }

    inline std::string ToString(const char *v) { return v ? v : ""; }

    inline std::string ToString(bool v) { return v ? "true" : "false"; }

    template<typename T>
    inline std::string ToString(const T &v) {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(v);
        } else {
            return "";
        }
    }

    template<typename T>
    inline Field F(const std::string &key, const T &value) {
        return Field(key, ToString(value));
    }

    std::string AddFields(std::initializer_list<Field> fields);

    void VexaNativeLog(JNIEnv *env, const char *level, const char *category, const char *msg,
                       const char *fieldsJson);

#define VEXA_LOGD(env, category, msg, fieldsJson) Vexa::Log::VexaNativeLog((env), "DEBUG", (category), (msg), (fieldsJson))
#define VEXA_LOGI(env, category, msg, fieldsJson) Vexa::Log::VexaNativeLog((env), "INFO", (category), (msg), (fieldsJson))
#define VEXA_LOGW(env, category, msg, fieldsJson) Vexa::Log::VexaNativeLog((env), "WARN", (category), (msg), (fieldsJson))
#define VEXA_LOGE(env, category, msg, fieldsJson) Vexa::Log::VexaNativeLog((env), "ERROR", (category), (msg), (fieldsJson))
#define VEXA_LOGF(env, category, msg, fieldsJson) Vexa::Log::VexaNativeLog((env), "FATAL", (category), (msg), (fieldsJson))

} // namespace Vexa::Log