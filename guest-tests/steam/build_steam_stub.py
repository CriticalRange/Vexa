"""Build an x86_64 libsteam_api.so stand-in for guests that insist on a running Steam client.

The export list is taken from the game's own libsteam_api.so so every flat-API
entry point Steamworks.NET (or any other binding) can ask for resolves. Calls
report an offline-but-"logged in" Steam: initialisation and ownership checks
pass. Flat RemoteStorage reads/writes use local files, and asynchronous writes
return typed completed results. Unimplemented API calls retain their empty
responses; unknown async handles report failure rather than waiting forever.

Usage: python build_steam_stub.py <real libsteam_api.so>... [-o output.so]

Multiple input libraries may be passed; the generated stub exports the union of
all Steam API symbols found across them.
"""

import argparse
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.abspath(__file__))
VEXA = os.path.normpath(os.path.join(ROOT, "..", ".."))
SYSROOT = os.path.join(VEXA, "guest-tests", "graphics", "rootfs")
SDK = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME") or os.path.join(
    os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk"
)
NDK_BIN = os.environ.get("VEXA_NDK_BIN") or os.path.join(
    SDK, "ndk", "29.0.14206865", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin"
)
EXE = ".exe" if os.name == "nt" else ""
CLANG = os.environ.get("VEXA_GUEST_CC") or os.path.join(NDK_BIN, "clang" + EXE)
NM = os.path.join(NDK_BIN, "llvm-nm" + EXE)
DEFAULT_OUT = os.path.join(VEXA, "app", "src", "main", "assets", "guests", "libsteam_api_stub.so")

RETURN_ONE = {
    "SteamAPI_Init", "SteamAPI_InitSafe", "SteamAPI_IsSteamRunning",
    "SteamAPI_GetHSteamPipe", "SteamAPI_GetHSteamUser",
    "SteamAPI_ISteamUser_BLoggedOn", "SteamAPI_ISteamApps_BIsSubscribed",
    "SteamAPI_ISteamApps_BIsSubscribedApp", "SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_",
    "SteamAPI_ISteamUserStats_RequestCurrentStats", "SteamAPI_ISteamUserStats_StoreStats",
    "SteamAPI_ISteamUtils_IsOverlayEnabled_", "SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_",
    "SteamAPI_ISteamApps_BIsAppInstalled", "SteamAPI_ISteamClient_CreateSteamPipe",
    "SteamAPI_ISteamClient_ConnectToGlobalUser", "SteamAPI_ISteamClient_CreateLocalUser",
}
STRINGS = {
    "SteamAPI_ISteamFriends_GetPersonaName": "VEXA",
    "SteamAPI_ISteamApps_GetCurrentGameLanguage": "english",
    "SteamAPI_ISteamApps_GetAvailableGameLanguages": "english",
    "SteamAPI_ISteamUtils_GetIPCountry": "US",
    "SteamAPI_GetSteamInstallPath": "",
}

FAKE_STEAM_ID = 76561198000000000

CUSTOM = {
    "SteamAPI_ISteamRemoteStorage_FileWrite":
        "EXPORT bool SteamAPI_ISteamRemoteStorage_FileWrite(void* self, const char* name, const void* data, int32_t size) { return size >= 0 && storage_write(name, data, (uint32_t)size); }",
    "SteamAPI_ISteamRemoteStorage_FileWriteAsync":
        "EXPORT uint64_t SteamAPI_ISteamRemoteStorage_FileWriteAsync(void* self, const char* name, const void* data, uint32_t size) { return storage_write_async(name, data, size); }",
    "SteamAPI_ISteamRemoteStorage_GetFileSize":
        "EXPORT int32_t SteamAPI_ISteamRemoteStorage_GetFileSize(void* self, const char* name) { return storage_size(name); }",
    "SteamAPI_ISteamRemoteStorage_FileRead":
        "EXPORT int32_t SteamAPI_ISteamRemoteStorage_FileRead(void* self, const char* name, void* data, int32_t size) { return storage_read(name, data, size); }",
    "SteamAPI_ISteamRemoteStorage_FileExists":
        "EXPORT bool SteamAPI_ISteamRemoteStorage_FileExists(void* self, const char* name) { char path[PATH_MAX]; struct stat st; return storage_path(name, path) && !stat(path, &st) && S_ISREG(st.st_mode); }",
    "SteamAPI_ISteamUtils_IsAPICallCompleted":
        "EXPORT bool SteamAPI_ISteamUtils_IsAPICallCompleted(void* self, uint64_t call, bool* failed) { return storage_completed(call, failed); }",
    "SteamAPI_ISteamUtils_GetAPICallResult":
        "EXPORT bool SteamAPI_ISteamUtils_GetAPICallResult(void* self, uint64_t call, void* data, int size, int callback, bool* failed) { return storage_result(call, data, size, callback, failed); }",
    "SteamInternal_ContextInit": (
        "EXPORT void* SteamInternal_ContextInit(void** ctx) {"
        " trace(\"SteamInternal_ContextInit\");"
        " if (!ctx || !ctx[0]) return 0;"
        " if (!ctx[2]) { ((void (*)(void**))ctx[0])(&ctx[2]); ctx[1] = (void*)1; }"
        " return &ctx[2]; }"
    ),
    "SteamInternal_FindOrCreateUserInterface": (
        "EXPORT void* SteamInternal_FindOrCreateUserInterface(int64_t user, const char* version) {"
        " trace(\"SteamInternal_FindOrCreateUserInterface\"); return fake_interface_for_version(version); }"
    ),
    "SteamInternal_FindOrCreateGameServerInterface": (
        "EXPORT void* SteamInternal_FindOrCreateGameServerInterface(int64_t user, const char* version) {"
        " trace(\"SteamInternal_FindOrCreateGameServerInterface\"); return fake_interface_for_version(version); }"
    ),
    "SteamInternal_CreateInterface": (
        "EXPORT void* SteamInternal_CreateInterface(const char* version) {"
        " trace(\"SteamInternal_CreateInterface\"); return fake_interface_for_version(version); }"
    ),
    "SteamAPI_RequestCurrentStatsMarker": "",
    "SteamAPI_ISteamUserStats_RequestCurrentStats": (
        "EXPORT int64_t SteamAPI_ISteamUserStats_RequestCurrentStats(void* self) {"
        " trace(\"SteamAPI_ISteamUserStats_RequestCurrentStats\"); pending_stats = 1; return 1; }"
    ),
    "SteamAPI_ManualDispatch_GetNextCallback": (
        "EXPORT int64_t SteamAPI_ManualDispatch_GetNextCallback(int pipe, callback_msg* msg) {"
        " if (!pending_stats) return 0; pending_stats = 0;"
        " stats_param.game_id = appid(); stats_param.result = 1; stats_param.user = %dULL;"
        " msg->user = 1; msg->callback = 1101; msg->param = (uint8_t*)&stats_param; msg->size = sizeof(stats_param);"
        " trace(\"deliver UserStatsReceived_t\"); return 1; }" % FAKE_STEAM_ID
    ),
    "SteamAPI_ISteamUser_GetSteamID": (
        "EXPORT uint64_t SteamAPI_ISteamUser_GetSteamID(void* self) { return %dULL; }" % FAKE_STEAM_ID
    ),
}

DUMMY_PTR = re.compile(r"^SteamAPI_Steam\w+_v\d+$|^SteamGameServer\w*_v\d+$|^SteamInternal_(CreateInterface|ContextInit|FindOrCreate\w+)$|^SteamAPI_ISteamClient_GetISteam\w+$|^SteamAPI_ISteamClient_GetIClient\w+$")


def exports(paths):
    names = set()
    for path in paths:
        out = subprocess.check_output([NM, "-D", "--defined-only", path], text=True)
        names.update(
            line.split()[-1].split("@")[0]
            for line in out.splitlines()
            if line.split() and line.split()[1] in "Tt"
        )
    return sorted(
        n for n in names
        if n.startswith("Steam") or n.startswith("GetHSteam")
    )


def render(names):
    lines = [
        "#include <stdint.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        "#include <unistd.h>",
        "static int trace_on(void) { static int on = -1; if (on < 0) on = getenv(\"VEXA_STEAM_TRACE\") != 0; return on; }",
        "static void trace(const char* n) { if (trace_on()) { write(2, \"[steam-stub] \", 13); write(2, n, strlen(n)); write(2, \"\\n\", 1); } }",
        "static char dummy_object[8192] __attribute__((aligned(16)));",
        "typedef struct { int32_t user; int32_t callback; uint8_t* param; int32_t size; } callback_msg;",
        "typedef struct __attribute__((packed, aligned(4))) { uint64_t game_id; int32_t result; uint64_t user; } stats_received;",
        "static stats_received stats_param;",
        "static int pending_stats;",
        "static int64_t appid(void) { const char* v = getenv(\"SteamAppId\"); return v ? atoll(v) : 0; }",
        "static int64_t vret0(void) { return 0; }",
        "static int64_t vret1(void) { return 1; }",
        "static int64_t vret_ff(void) { return 255; }",
        "static uint64_t vret_steamid(void) { return %dULL; }" % FAKE_STEAM_ID,
        "static int64_t vret_appid(void) { return appid(); }",
        "static const char *vret_empty(void) { return \"\"; }",
        "static const char *vret_name(void) { return \"VEXA\"; }",
        "static const char *vret_lang(void) { return \"english\"; }",
        "static const char *vret_country(void) { return \"US\"; }",
        "#define VT_COUNT 512",
        "static void *vt_generic[VT_COUNT];",
        "static void *vt_client[VT_COUNT];",
        "static void *vt_user[VT_COUNT];",
        "static void *vt_userstats[VT_COUNT];",
        "static void *vt_utils[VT_COUNT];",
        "static void *vt_friends[VT_COUNT];",
        "static void *vt_apps[VT_COUNT];",
        "static void *vt_remote[VT_COUNT];",
        "static void *vt_controller[VT_COUNT];",
        "static void *vt_matchmaking[VT_COUNT];",
        "static void *vt_matchservers[VT_COUNT];",
        "static void *vt_networking[VT_COUNT];",
        "static void *vt_gameserver[VT_COUNT];",
        "static void *vt_ugc[VT_COUNT];",
        "static void *obj_generic[1];",
        "static void *obj_client[1];",
        "static void *obj_user[1];",
        "static void *obj_userstats[1];",
        "static void *obj_utils[1];",
        "static void *obj_friends[1];",
        "static void *obj_apps[1];",
        "static void *obj_remote[1];",
        "static void *obj_controller[1];",
        "static void *obj_matchmaking[1];",
        "static void *obj_matchservers[1];",
        "static void *obj_networking[1];",
        "static void *obj_gameserver[1];",
        "static void *obj_ugc[1];",
        "static int vt_ready;",
        "static void vfill(void **vt) { int i; for (i = 0; i < VT_COUNT; i++) vt[i] = (void*)vret0; }",
        "static void init_vtables(void) {",
        " if (vt_ready) return; vt_ready = 1;",
        " vfill(vt_generic); vfill(vt_client); vfill(vt_user); vfill(vt_userstats);",
        " vfill(vt_utils); vfill(vt_friends); vfill(vt_apps); vfill(vt_remote);",
        " vfill(vt_controller); vfill(vt_matchmaking); vfill(vt_matchservers);",
        " vfill(vt_networking); vfill(vt_gameserver); vfill(vt_ugc);",
        " vt_user[1] = vret1; vt_user[2] = vret_steamid; vt_user[13] = vret1;",
        " vt_userstats[0] = vret1; vt_userstats[3] = vret1; vt_userstats[4] = vret1;",
        " vt_userstats[7] = vret1; vt_userstats[8] = vret1; vt_userstats[10] = vret1;",
        " vt_userstats[12] = vret_empty; vt_userstats[13] = vret1; vt_userstats[15] = vret_empty;",
        " vt_utils[4] = vret_country; vt_utils[8] = vret_ff; vt_utils[9] = vret_appid;",
        " vt_apps[4] = vret_lang; vt_apps[5] = vret_lang;",
        " vt_friends[0] = vret_name; vt_friends[6] = vret1; vt_friends[7] = vret_name;",
        " vt_remote[21] = vret0; vt_remote[22] = vret0;",
        " vt_controller[2] = vret1; vt_controller[4] = vret1; vt_controller[11] = vret1;",
        " vt_controller[14] = vret1; vt_controller[17] = vret_empty; vt_controller[18] = vret_empty;",
        " obj_generic[0] = vt_generic; obj_client[0] = vt_client; obj_user[0] = vt_user;",
        " obj_userstats[0] = vt_userstats; obj_utils[0] = vt_utils; obj_friends[0] = vt_friends;",
        " obj_apps[0] = vt_apps; obj_remote[0] = vt_remote; obj_controller[0] = vt_controller;",
        " obj_matchmaking[0] = vt_matchmaking; obj_matchservers[0] = vt_matchservers;",
        " obj_networking[0] = vt_networking; obj_gameserver[0] = vt_gameserver; obj_ugc[0] = vt_ugc; }",
        "static void *fake_interface_for_version(const char *v) {",
        " init_vtables(); if (!v) return obj_generic;",
        " if (strstr(v, \"UserStats\") || strstr(v, \"USERSTATS\")) return obj_userstats;",
        " if (strstr(v, \"RemoteStorage\") || strstr(v, \"REMOTESTORAGE\")) return obj_remote;",
        " if (strstr(v, \"GameServerNetworking\")) return obj_networking;",
        " if (strstr(v, \"SteamGameServer\") || strstr(v, \"STEAMGAMESERVER\")) return obj_gameserver;",
        " if (strstr(v, \"MatchmakingServers\") || strstr(v, \"MatchMakingServers\")) return obj_matchservers;",
        " if (strstr(v, \"Matchmaking\") || strstr(v, \"MatchMaking\") || strstr(v, \"MATCHMAKING\")) return obj_matchmaking;",
        " if (strstr(v, \"Networking\") || strstr(v, \"NETWORKING\")) return obj_networking;",
        " if (strstr(v, \"SteamUser\") || strstr(v, \"STEAMUSER\")) return obj_user;",
        " if (strstr(v, \"Utils\") || strstr(v, \"UTILS\")) return obj_utils;",
        " if (strstr(v, \"Friends\") || strstr(v, \"FRIENDS\")) return obj_friends;",
        " if (strstr(v, \"Apps\") || strstr(v, \"APPS\")) return obj_apps;",
        " if (strstr(v, \"Controller\") || strstr(v, \"CONTROLLER\")) return obj_controller;",
        " if (strstr(v, \"UGC\")) return obj_ugc;",
        " if (strstr(v, \"Client\") || strstr(v, \"CLIENT\")) return obj_client;",
        " return obj_generic; }",
        "static void *fake_interface_for_export(const char *n) { return fake_interface_for_version(n); }",
        "#define EXPORT __attribute__((visibility(\"default\")))",
    ]
    with open(os.path.join(ROOT, "remote_storage.c"), encoding="utf-8") as helper:
        lines.append(helper.read())
    for n in names:
        if n in CUSTOM:
            lines.append(CUSTOM[n])
            continue
        if n == "SteamAPI_InitFlat":
            body = "return 0;"
        elif n == "SteamAPI_RestartAppIfNecessary":
            body = "return 0;"
        elif n == "SteamAPI_ISteamUtils_GetAppID":
            body = "return (int64_t)appid();"
        elif n in STRINGS:
            lines.append(f'EXPORT const char* {n}(void) {{ trace("{n}"); return "{STRINGS[n]}"; }}')
            continue
        elif n in RETURN_ONE:
            body = "return 1;"
        elif DUMMY_PTR.match(n):
            lines.append(f"EXPORT void* {n}(void) {{ trace(\"{n}\"); return fake_interface_for_export(\"{n}\"); }}")
            continue
        else:
            body = "return 0;"
        lines.append(f"EXPORT int64_t {n}(void) {{ trace(\"{n}\"); {body} }}")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("real", nargs="+", help="real libsteam_api.so files to union")
    parser.add_argument("-o", "--output", default=DEFAULT_OUT)
    args = parser.parse_args()

    names = exports(args.real)
    out = args.output
    tmp_out = out + ".tmp"
    os.makedirs(os.path.dirname(out), exist_ok=True)
    source = tmp_out + ".c"
    with open(source, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(render(names))
    try:
        subprocess.check_call([
            CLANG, "--target=x86_64-linux-gnu", f"--sysroot={SYSROOT}", "-fPIC", "-shared",
            "-fno-stack-protector", "-O2", "-fuse-ld=lld", "-Wl,-soname,libsteam_api.so",
            source, "-o", tmp_out, "-lc",
        ])
        os.replace(tmp_out, out)
    finally:
        if os.path.exists(source):
            os.remove(source)
        if os.path.exists(tmp_out):
            os.remove(tmp_out)
    print(f"built {out} ({len(names)} exports)")


if __name__ == "__main__":
    main()
