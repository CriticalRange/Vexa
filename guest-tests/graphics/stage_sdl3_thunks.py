"""Stage paired GL/SDL/OpenAL thunks, preserving the guest rootfs assets."""

import hashlib
import os
import shutil
import struct

ROOT = os.path.dirname(os.path.abspath(__file__))
VEXA = os.path.normpath(os.path.join(ROOT, "..", ".."))
RUNTIME = os.path.join(VEXA, "app", "src", "main", "assets", "runtime")
FEX_BUILD = os.environ.get("FEX_BUILD_DIR", os.path.normpath(os.path.join(VEXA, "..", "FEX", "build-android-arm64-ninja")))
THIRD_PARTY = os.path.join(VEXA, "third_party", "install-android-arm64")

PAYLOADS = (
    ("x86_64", os.path.join(FEX_BUILD, "Guest", "libopenal-guest.so"), "thunks/guest/libopenal-guest.so"),
    ("aarch64", os.path.join(FEX_BUILD, "HostLibs_64", "libopenal-host.so"), "thunks/host/libopenal-host.so"),
    ("x86_64", os.path.join(FEX_BUILD, "Guest", "libGL-guest.so"), "thunks/guest/libGL-guest.so"),
    ("aarch64", os.path.join(FEX_BUILD, "HostLibs_64", "libGL-host.so"), "thunks/host/libGL-host.so"),
    ("x86_64", os.path.join(FEX_BUILD, "Guest", "libvulkan-guest.so"), "thunks/guest/libvulkan-guest.so"),
    ("aarch64", os.path.join(FEX_BUILD, "HostLibs_64", "libvulkan-host.so"), "thunks/host/libvulkan-host.so"),
    ("x86_64", os.path.join(FEX_BUILD, "Guest", "libSDL3-guest.so"), "thunks/guest/libSDL3-guest.so"),
    (
        "x86_64",
        os.path.join(FEX_BUILD, "Guest", "libSDL3_image-guest.so"),
        "thunks/guest/libSDL3_image-guest.so",
    ),
    ("aarch64", os.path.join(FEX_BUILD, "HostLibs_64", "libSDL3-host.so"), "thunks/host/libSDL3-host.so"),
    (
        "aarch64",
        os.path.join(FEX_BUILD, "HostLibs_64", "libSDL3_image-host.so"),
        "thunks/host/libSDL3_image-host.so",
    ),
    (
        "aarch64",
        os.path.join(THIRD_PARTY, "sdl3", "lib", "libSDL3.so"),
        "thunks/host/libSDL3.so",
    ),
    (
        "aarch64",
        os.path.join(THIRD_PARTY, "sdl3", "lib", "libSDL3.so"),
        "thunks/host/libSDL3.so.1",
    ),
    (
        "aarch64",
        os.path.join(THIRD_PARTY, "sdl3_image", "lib", "libSDL3_image.so"),
        "thunks/host/libSDL3_image.so",
    ),
    ("x86_64", os.path.join(FEX_BUILD, "Guest", "libSDL2-guest.so"), "thunks/guest/libSDL2-guest.so"),
    ("aarch64", os.path.join(FEX_BUILD, "HostLibs_64", "libSDL2-host.so"), "thunks/host/libSDL2-host.so"),
    (
        # Self-copy of the host thunk: satisfies fexldr_init's dlopen("libSDL2.so")
        # and resolves its FEX_SDL2_* exports.
        "aarch64",
        os.path.join(FEX_BUILD, "HostLibs_64", "libSDL2-host.so"),
        "thunks/host/libSDL2.so",
    ),
)

ELF_MACHINES = {"x86_64": (2, 62), "aarch64": (2, 183)}


def machine(path):
    with open(path, "rb") as source:
        header = source.read(20)
    if len(header) != 20 or header[:4] != b"\x7fELF" or header[5] != 1:
        return None
    return header[4], struct.unpack_from("<H", header, 18)[0]


def update_stamp():
    digest = hashlib.sha256()
    for dirpath, dirs, files in os.walk(RUNTIME):
        dirs.sort()
        for name in sorted(files):
            if dirpath == RUNTIME and name == "STAMP":
                continue
            path = os.path.join(dirpath, name)
            relative = os.path.relpath(path, RUNTIME).replace(os.sep, "/")
            digest.update(relative.encode())
            with open(path, "rb") as source:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(block)
    stamp = os.path.join(RUNTIME, "STAMP")
    with open(stamp, "w", encoding="utf-8") as output:
        output.write(f"vexa-graphics-1:{digest.hexdigest()[:16]}\n")


def main():
    i386 = None
    if os.environ.get('VEXA_BUILD_32BIT_THUNKS', 'ON') == 'ON':
        import stage_i386_runtime as i386
        i386.payloads()  # Validate before modifying either architecture.
    # Validate every source before modifying assets, including both GL sides.
    for expected_arch, source, relative in PAYLOADS:
        if not os.path.isfile(source):
            raise SystemExit(f"missing thunk payload: {source}")
        if machine(source) != ELF_MACHINES[expected_arch]:
            raise SystemExit(f"unexpected ELF architecture for {source}; expected {expected_arch}")
    os.makedirs(RUNTIME, exist_ok=True)
    for expected_arch, source, relative in PAYLOADS:
        destination = os.path.join(RUNTIME, *relative.split("/"))
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        shutil.copy2(source, destination)
        print(f"staged {relative}", flush=True)

    if i386:
        i386.stage()
    update_stamp()
    print("SDL3_THUNKS_READY", flush=True)


if __name__ == "__main__":
    main()
