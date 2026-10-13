"""Copy a small guest runtime into the APK assets.

The full noble rootfs stays on the host for thunk compilation. The device only
needs the loader, the shared libraries the graphics programs link, glmark2's
data, and the GL/EGL/GLES thunk libraries when those builds exist.
"""

import hashlib
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
ROOTFS = os.path.join(ROOT, "tree")
SYSROOT = os.path.join(ROOT, "rootfs")
VEXA = os.path.normpath(os.path.join(ROOT, "..", ".."))
ASSETS = os.path.join(VEXA, "app", "src", "main", "assets")
FEX_BUILD = os.path.normpath(os.path.join(VEXA, "..", "FEX", "build-android-arm64-ninja"))

SONAMES = (
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libm.so.6",
    "libdl.so.2",
    "librt.so.1",
    "libpthread.so.0",
    "libresolv.so.2",
    "libgcc_s.so.1",
    "libstdc++.so.6",
    "libz.so.1",
    "libjpeg.so.8",
    "libpng16.so.16",
    "libX11.so.6",
    "libXcursor.so.1",
    "libXinerama.so.1",
    "libXext.so.6",
    "libXrandr.so.2",
    "libXrender.so.1",
    "libXi.so.6",
    "libdbus-1.so.3",
    "libudev.so.1",
    "libsteam_api.so",
    "libvulkan.so.1",
    "libwayland-client.so.0",
    "libxcb.so.1",
    "libXau.so.6",
    "libXdmcp.so.6",
    "libffi.so.8",
    "libbsd.so.0",
    "libmd.so.0",
    "libxkbcommon.so.0",
    "libxkbcommon-x11.so.0",
    "libdecor-0.so.0",
    "libasound.so.2",
    "libogg.so.0",
    "libvorbis.so.0",
    "libvorbisfile.so.3",
    "libdrm.so.2",
    "libwayland-egl.so.1",
    "libwayland-cursor.so.0",
    "libpulse.so.0",
    "libXfixes.so.3",
    "libXss.so.1",
    "libXxf86vm.so.1",
    "libgbm.so.1",
)
REQUIRED = (
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libm.so.6",
    "libgcc_s.so.1",
    "libstdc++.so.6",
    "libz.so.1",
    "libjpeg.so.8",
    "libpng16.so.16",
    "libX11.so.6",
    "libXcursor.so.1",
    "libXinerama.so.1",
    "libXext.so.6",
    "libXrandr.so.2",
    "libXrender.so.1",
    "libXi.so.6",
    "libdbus-1.so.3",
    "libudev.so.1",
    "libsteam_api.so",
)
PROGRAMS = (
    "es2gears_x11.x86_64-linux-gnu",
    "glxgears.x86_64-linux-gnu",
    "eglgears_x11.x86_64-linux-gnu",
    "glxinfo.x86_64-linux-gnu",
    "glmark2",
    "glmark2-es2",
    "vkcube",
    "vkcubepp",
    "vulkaninfo",
    "vkgears.x86_64-linux-gnu",
)
THUNKS = (
    "libGL-host.so",
    "libEGL-host.so",
    "libGLESv2-host.so",
    "libSDL3-host.so",
    "libSDL3_image-host.so",
    "libGL-guest.so",
    "libEGL-guest.so",
    "libGLESv2-guest.so",
    "libSDL3-guest.so",
    "libSDL3_image-guest.so",
    "libvulkan-host.so",
    "libvulkan-guest.so",
    "libopenal-host.so",
    "libopenal-guest.so",
)


def is_elf(path):
    try:
        with open(path, "rb") as handle:
            return handle.read(4) == b"\x7fELF"
    except OSError:
        return False


def find_elf(soname):
    best = None
    best_size = -1
    for dirpath, _, files in os.walk(SYSROOT):
        if "llvm" in dirpath.lower():
            continue
        for name in files:
            if name != soname and not name.startswith(soname + "."):
                continue
            path = os.path.join(dirpath, name)
            if not is_elf(path):
                continue
            size = os.path.getsize(path)
            if size > best_size:
                best = path
                best_size = size
    return best


# Guest libs that must be the VEXA shims (compile_fake_* in stage_rootfs.py),
# not the real Ubuntu socket clients. The shims stamp "VEXA" into their
# vendor/setup bytes; the real libs never carry that string.
FAKE_LIB_MARKERS = {
    "libudev.so.1": b"VEXA empty udev backend",
    "libxcb.so.1": b"VEXA",
    "libX11.so.6": b"VEXA",
}


def has_marker(path, marker):
    try:
        with open(path, "rb") as handle:
            return marker in handle.read()
    except OSError:
        return False


def copy_file(src, dest):
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    shutil.copy2(src, dest)


def main():
    runtime = os.path.join(ASSETS, "runtime")
    if os.environ.get("RESET_RUNTIME_ASSETS") == "1" and os.path.isdir(runtime):
        shutil.rmtree(runtime)
    os.makedirs(runtime, exist_ok=True)
    lib_dest = os.path.join(runtime, "rootfs", "usr", "lib", "x86_64-linux-gnu")
    missing = []
    for soname in SONAMES:
        found = find_elf(soname)
        if not found:
            if soname in REQUIRED:
                missing.append(soname)
            else:
                print(f"skip {soname}", flush=True)
            continue
        marker = FAKE_LIB_MARKERS.get(soname)
        if marker and not has_marker(found, marker):
            raise SystemExit(
                f"staging real {soname} socket client instead of the VEXA shim: "
                f"{found}\n"
                "find_elf prefers the largest match, so a stale guest rootfs "
                "silently breaks XCB/X11 guests (vkcube: 'Cannot connect to XCB').\n"
                "Rebuild the rootfs so compile_fake_*() replaces the lib:\n"
                "  rm -f guest-tests/graphics/rootfs/.vexa-rootfs-ready\n"
                "  rm -rf guest-tests/graphics/rootfs\n"
                "  python guest-tests/graphics/stage_rootfs.py"
            )
        copy_file(found, os.path.join(lib_dest, soname))
        print(f"lib {soname} <= {found}", flush=True)
    if missing:
        raise SystemExit("missing rootfs libraries: " + ", ".join(missing))

    for relative in (
        os.path.join("lib64", "ld-linux-x86-64.so.2"),
        os.path.join("lib", "x86_64-linux-gnu", "ld-linux-x86-64.so.2"),
    ):
        copy_file(os.path.join(lib_dest, "ld-linux-x86-64.so.2"), os.path.join(runtime, "rootfs", relative))

    data_src = os.path.join(ROOTFS, "usr", "share", "glmark2")
    if not os.path.isdir(data_src):
        raise SystemExit(f"missing glmark2 data: {data_src}")
    shutil.copytree(
        data_src,
        os.path.join(runtime, "rootfs", "usr", "share", "glmark2"),
        dirs_exist_ok=True,
    )

    bin_dir = os.path.join(ROOTFS, "usr", "bin")
    guest_assets = os.path.join(ASSETS, "guests")
    os.makedirs(guest_assets, exist_ok=True)
    for name in PROGRAMS:
        src = os.path.join(bin_dir, name)
        if not is_elf(src):
            raise SystemExit(f"missing program ELF: {src}")
        copy_file(src, os.path.join(guest_assets, name))
        print(f"program {name}", flush=True)

    found_thunks = {}
    for dirpath, _, files in os.walk(FEX_BUILD):
        for name in files:
            if name in THUNKS and name not in found_thunks and is_elf(os.path.join(dirpath, name)):
                found_thunks[name] = os.path.join(dirpath, name)
    require = os.environ.get("REQUIRE_THUNKS") == "1"
    for name in THUNKS:
        kind = "host" if name.endswith("-host.so") else "guest"
        dest = os.path.join(runtime, "thunks", kind, name)
        src = found_thunks.get(name)
        if not src:
            if require:
                raise SystemExit(f"missing thunk {name}")
            print(f"skip thunk {name}", flush=True)
            continue
        copy_file(src, dest)
        print(f"thunk {name}", flush=True)

    openal_host = os.path.normpath(
        os.path.join(VEXA, "third_party", "openal-soft-android", "build-android-arm64", "libopenal.so")
    )
    if is_elf(openal_host):
        copy_file(openal_host, os.path.join(runtime, "thunks", "host", "libopenal.so"))
        print("host libopenal.so", flush=True)
    else:
        print("skip host libopenal.so", flush=True)

    sdl3_host_libraries = (
        ("sdl3/lib/libSDL3.so", "libSDL3.so"),
        ("sdl3/lib/libSDL3.so", "libSDL3.so.1"),
        ("sdl3_image/lib/libSDL3_image.so", "libSDL3_image.so"),
    )
    host_thunk_dir = os.path.join(runtime, "thunks", "host")
    for relative, name in sdl3_host_libraries:
        source = os.path.join(VEXA, "third_party", "install-android-arm64", relative)
        if is_elf(source):
            copy_file(source, os.path.join(host_thunk_dir, name))
            print(f"host {name}", flush=True)
        elif require:
            raise SystemExit(f"missing Android host library: {source}")
        else:
            print(f"skip host {name}", flush=True)

    # The installer re-copies the runtime whenever this content hash changes.
    digest = hashlib.sha256()
    for dirpath, dirs, files in os.walk(runtime):
        dirs.sort()
        for name in sorted(files):
            if dirpath == runtime and name == "STAMP":
                continue
            path = os.path.join(dirpath, name)
            digest.update(os.path.relpath(path, runtime).replace(os.sep, "/").encode())
            with open(path, "rb") as handle:
                digest.update(handle.read())
    with open(os.path.join(runtime, "STAMP"), "w", encoding="utf-8") as handle:
        handle.write(f"vexa-graphics-1:{digest.hexdigest()[:16]}\n")
    print("ASSETS_READY", flush=True)


if __name__ == "__main__":
    main()
