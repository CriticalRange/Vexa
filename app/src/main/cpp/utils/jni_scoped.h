//
// Created by critical on 10.03.2026.
//

#ifndef VEXA_EMULATOR_JNI_SCOPED_H
#define VEXA_EMULATOR_JNI_SCOPED_H

#include <jni.h>
#include <string>

namespace Vexa::Utils {
    class ScopedUtfChars {
    public:
        ScopedUtfChars(JNIEnv *env, jstring str) : env_(env), str_(str) {
            if (env_ && str_) {
                chars_ = env_->GetStringUTFChars(str_, nullptr);
            }
        }

        ~ScopedUtfChars() {
            if (env_ && str_ && chars_) {
                env_->ReleaseStringUTFChars(str_, chars_);
            }
        }

        const char *get() const { return chars_; }

        bool ok() const { return chars_ != nullptr; }

        ScopedUtfChars(const ScopedUtfChars &) = delete;

        ScopedUtfChars &operator=(const ScopedUtfChars &) = delete;

    private:
        JNIEnv *env_{nullptr};
        jstring str_{nullptr};
        const char *chars_{nullptr};
    };

    template<typename T>
    class ScopedLocalRef {
    public:
        ScopedLocalRef(JNIEnv *env, T obj) : env_(env), obj_(obj) {}

        ~ScopedLocalRef() {
            if (env_ && obj_) {
                env_->DeleteLocalRef(obj_);
            }
        }

        T get() const { return obj_; }

        bool ok() const { return obj_ != nullptr; }

        ScopedLocalRef(const ScopedLocalRef &) = delete;

        ScopedLocalRef &operator=(const ScopedLocalRef &) = delete;

    private:
        JNIEnv *env_{nullptr};
        T obj_{nullptr};
    };
} // namespace Vexa::Utils

#endif //VEXA_EMULATOR_JNI_SCOPED_H