#!/usr/bin/env python3
"""Write the build identity (sources, patch set, FEX options, payload hashes) packaged as assets/build-manifest.json."""

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apply_fex_compat as compat  # noqa: E402

VEXA_ROOT = compat.VEXA_ROOT
RUNTIME = VEXA_ROOT / "app/src/main/assets/runtime"
# Linked statically into libvexa.so; their hashes identify the FEX core in a run.
FEX_STATIC_LIBS = (
    "FEXCore/Source/libFEXCore.a", "FEXCore/Source/libFEXCore_Base.a",
    "Source/Tools/LinuxEmulation/libLinuxEmulation.a", "Source/Common/libCommon.a",
    "Source/Tools/CommonTools/libCommonTools.a",
)
# Guest shims staged into the rootfs next to the thunks (stage_sdl3_thunks.PAYLOADS).
ROOTFS_PAYLOADS = (
    "rootfs/usr/lib/x86_64-linux-gnu/libasound.so.2", "rootfs/usr/lib/x86_64-linux-gnu/libX11.so.6",
    "rootfs/usr/lib/x86_64-linux-gnu/libvexa_evdev.so", "rootfs/usr/lib/x86_64-linux-gnu/libudev.so.1",
)
CACHE_PREFIXES = ("CMAKE_BUILD_TYPE", "CMAKE_CXX_FLAGS", "CMAKE_C_FLAGS", "ENABLE_", "BUILD_", "TUNE_", "VEXA_", "ANDROID_PLATFORM", "ANDROID_ABI", "ANDROID_STL")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def git_identity(root):
    git = lambda *args: compat.run(root, *args)
    head = git("rev-parse", "HEAD")
    if head.returncode:
        return {"error": head.stderr.strip()}
    dirty = git("status", "--porcelain", "--untracked-files=no").stdout.splitlines()
    identity = {
        "commit": head.stdout.strip(),
        "describe": git("describe", "--always", "--dirty", "--abbrev=12").stdout.strip(),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip(),
        "dirtyFiles": len(dirty),
    }
    if dirty:
        # Same commit with different local edits must not look identical.
        identity["diffSha256"] = hashlib.sha256(git("diff", "HEAD", "--binary").stdout.encode()).hexdigest()
    return identity


def cmake_options(build):
    cache = build / "CMakeCache.txt"
    options = {}
    if not cache.is_file():
        return options
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(("#", "//")) or ":" not in line or "=" not in line:
            continue
        key, rest = line.split(":", 1)
        kind, value = rest.split("=", 1)
        # Skip host paths: they identify the build machine, not the build.
        if key.startswith(CACHE_PREFIXES) and kind not in ("PATH", "FILEPATH", "INTERNAL", "UNINITIALIZED") or key == "ANDROID_PLATFORM":
            options[key] = value
    return options


def hash_tree(base, relative):
    root = base / relative
    if not root.is_dir():
        return {}
    return {path.relative_to(base).as_posix(): sha256(path) for path in sorted(root.rglob("*")) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fex-root", required=True, type=Path)
    parser.add_argument("--fex-build", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    fex_root, fex_build = args.fex_root.resolve(), args.fex_build.resolve()

    fex = git_identity(fex_root)
    if "commit" in fex:
        _, pinned, contains = compat.pin_state(fex_root)
        fex.update(pinnedCommit=pinned, containsPin=contains)
    patches = []
    for patch in compat.PATCHES:
        state, _, _ = compat.patch_state(fex_root, patch)
        patches.append({"name": patch.name, "sha256": sha256(patch), "state": state})

    payloads = hash_tree(RUNTIME, "thunks")
    payloads.update({rel: sha256(RUNTIME / rel) for rel in ROOTFS_PAYLOADS if (RUNTIME / rel).is_file()})
    stamp = RUNTIME / "STAMP"
    manifest = {
        "schema": "vexa-build-manifest-1",
        "generatedAt": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "vexa": git_identity(VEXA_ROOT),
        "fex": fex,
        "patches": patches,
        "fexBuild": {
            "cmake": cmake_options(fex_build),
            "staticLibs": {rel: sha256(fex_build / rel) for rel in FEX_STATIC_LIBS if (fex_build / rel).is_file()},
        },
        "runtime": {
            "stamp": stamp.read_text(encoding="utf-8").strip() if stamp.is_file() else None,
            "payloads": payloads,
        },
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print(f"BUILD_MANIFEST {args.out} vexa={manifest['vexa'].get('describe')} fex={fex.get('describe')} payloads={len(payloads)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
