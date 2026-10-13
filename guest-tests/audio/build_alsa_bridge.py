"""Build/stage the x86_64 ALSA bridge and refresh runtime asset stamp."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME") or str(Path(os.environ["LOCALAPPDATA"]) / "Android/Sdk"))
CC = os.environ.get("VEXA_GUEST_CC") or str(SDK / "ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe")
SYSROOT = ROOT / "guest-tests/graphics/rootfs"
OUTPUT = ROOT / "guest-tests/audio/libasound.so.2"
subprocess.run([CC, "--target=x86_64-linux-gnu", f"--sysroot={SYSROOT}", "-fPIC", "-shared", "-fno-stack-protector", "-O2", "-fuse-ld=lld", "-Wl,-soname,libasound.so.2", str(ROOT / "guest-tests/audio/libasound_openal.c"), str(ROOT / "guest-tests/audio/libasound_stubs.c"), "-o", str(OUTPUT), "-lc", "-ldl"], check=True)
for base in (SYSROOT, ROOT / "app/src/main/assets/runtime/rootfs"):
    shutil.copy2(OUTPUT, base / "usr/lib/x86_64-linux-gnu/libasound.so.2")
sys.path.insert(0, str(ROOT / "guest-tests/graphics"))
from stage_sdl3_thunks import update_stamp
update_stamp()
print("ALSA_BRIDGE_READY")
