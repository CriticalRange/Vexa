"""Build an x86_64 libsteam_api.so stand-in for guests that insist on a running Steam client.

The export list is taken from the game's own libsteam_api.so so every flat-API
entry point Steamworks.NET (or any other binding) can ask for resolves. Calls
succeed and report an empty, offline-but-"logged in" Steam: initialisation and
ownership checks pass, everything else returns zero.

Usage: python build_steam_stub.py <real libsteam_api.so> [output.so]
"""

import os
import re
import subprocess
import sys

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


def exports(real):
    out = subprocess.check_output([NM, "-D", "--defined-only", real], text=True)
    names = {line.split()[-1].split("@")[0] for line in out.splitlines() if line.split() and line.split()[1] in "Tt"}
    return sorted(n for n in names if n.startswith("Steam"))


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
        "#define EXPORT __attribute__((visibility(\"default\")))",
    ]
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
            lines.append(f"EXPORT void* {n}(void) {{ trace(\"{n}\"); return dummy_object; }}")
            continue
        else:
            body = "return 0;"
        lines.append(f"EXPORT int64_t {n}(void) {{ trace(\"{n}\"); {body} }}")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    names = exports(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    os.makedirs(os.path.dirname(out), exist_ok=True)
    source = out + ".c"
    with open(source, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(render(names))
    subprocess.check_call([
        CLANG, "--target=x86_64-linux-gnu", f"--sysroot={SYSROOT}", "-fPIC", "-shared",
        "-fno-stack-protector", "-O2", "-fuse-ld=lld", "-Wl,-soname,libsteam_api.so",
        source, "-o", out, "-lc",
    ])
    os.remove(source)
    print(f"built {out} ({len(names)} exports)")


if __name__ == "__main__":
    main()
