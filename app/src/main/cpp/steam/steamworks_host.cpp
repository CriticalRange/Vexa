#include <jni.h>

#include <dlfcn.h>
#include <stdlib.h>

#include <string>

namespace {

using IsRunningFn = bool (*)();
using InitFn = bool (*)();
using ShutdownFn = void (*)();

std::string utf(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string copy = chars == nullptr ? std::string() : chars;
    if (chars != nullptr) {
        env->ReleaseStringUTFChars(value, chars);
    }
    return copy;
}

const char* symbol_state(void* handle, const char* name) {
    return dlsym(handle, name) == nullptr ? "no" : "yes";
}

}  // namespace

// Probes the official Steamworks androidarm64 libraries the user placed in
// app storage. SteamAPI_Init only attaches to a running client; it does not
// listen, so a negative result stays negative.
extern "C" JNIEXPORT jstring JNICALL
Java_com_critical_vexaemulator_steam_SteamworksHost_nativeProbe(
        JNIEnv* env,
        jobject /* thiz */,
        jstring api_path,
        jstring client_path,
        jstring app_id) {
    const std::string api = utf(env, api_path);
    const std::string client = utf(env, client_path);
    const std::string app = utf(env, app_id);
    if (!app.empty()) {
        setenv("SteamAppId", app.c_str(), 1);
    }

    std::string status;
    if (!client.empty()) {
        void* client_handle = dlopen(client.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (client_handle == nullptr) {
            const char* error = dlerror();
            status += "libsteamclient.so dlopen failed";
            if (error != nullptr) {
                status += ": ";
                status += error;
            }
        } else {
            status += "libsteamclient.so loaded, CreateInterface ";
            status += symbol_state(client_handle, "CreateInterface");
        }
    } else {
        status = "libsteamclient.so missing";
    }

    status += "; ";
    if (api.empty()) {
        status += "libsteam_api.so missing";
        return env->NewStringUTF(status.c_str());
    }

    void* api_handle = dlopen(api.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (api_handle == nullptr) {
        const char* error = dlerror();
        status += "libsteam_api.so dlopen failed";
        if (error != nullptr) {
            status += ": ";
            status += error;
        }
        return env->NewStringUTF(status.c_str());
    }

    auto is_running = reinterpret_cast<IsRunningFn>(dlsym(api_handle, "SteamAPI_IsSteamRunning"));
    auto init = reinterpret_cast<InitFn>(dlsym(api_handle, "SteamAPI_Init"));
    auto shutdown = reinterpret_cast<ShutdownFn>(dlsym(api_handle, "SteamAPI_Shutdown"));
    if (is_running == nullptr && init == nullptr) {
        status += "libsteam_api.so loaded, SteamAPI exports missing";
        return env->NewStringUTF(status.c_str());
    }
    if (is_running != nullptr && is_running()) {
        status += "libsteam_api.so loaded, Steam is running";
        return env->NewStringUTF(status.c_str());
    }
    if (init != nullptr && init()) {
        status += "libsteam_api.so SteamAPI_Init ok";
        if (shutdown != nullptr) {
            shutdown();
        }
        return env->NewStringUTF(status.c_str());
    }
    status += "libsteam_api.so loaded, Steam is not running";
    return env->NewStringUTF(status.c_str());
}
