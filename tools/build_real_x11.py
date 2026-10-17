#!/usr/bin/env python3
"""Fetch pinned Termux:X11 sources and build its Android arm64 libXlorie.

All downloads and generated files stay inside the workspace. No Android device
is required. The upstream checkout is never patched: build a disposable source
overlay so the exact source provenance remains auditable.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "tools/real_x11.lock.json").read_text(encoding="utf-8"))
PIN = LOCK["source_commit"]
URL = LOCK["source_repository"]
APK_URL = LOCK["download_url"]
APK_SHA256 = LOCK["apk_sha256"]
LIB_SHA256 = LOCK["library_sha256"]
MIRRORS = {
    "pixman": "https://github.com/freedesktop-unofficial-mirror/pixman.git",
    "libfontenc": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libfontenc.git",
    "libxau": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libXau.git",
    "libxdmcp": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libXdmcp.git",
    "libxfont": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libXfont.git",
    "libxkbfile": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libxkbfile.git",
    "libxshmfence": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libxshmfence.git",
    "libxtrans": "https://github.com/freedesktop-unofficial-mirror/xorg__lib__libxtrans.git",
    "libxcvt": "https://github.com/LizardByte-infrastructure/libxcvt.git",
    "libx11": "https://github.com/mirror/libX11.git",
    "xorgproto": "https://github.com/rwiggins/xorgproto.git",
    "xkbcomp": "https://github.com/freedesktop-unofficial-mirror/xorg__app__xkbcomp.git",
    "xserver": "https://github.com/mirror/xserver.git",
    "bzip2": "https://github.com/libarchive/bzip2.git",
}


def run(args, cwd=None, capture=False):
    process_env = dict(os.environ, GIT_TERMINAL_PROMPT="0", GCM_INTERACTIVE="never")
    result = subprocess.run([str(x) for x in args], cwd=cwd, text=True, env=process_env,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(map(str, args))}\n{result.stdout or ''}")
    return (result.stdout or "").strip()


def fetch_repo(path, url, commit, fallback=None):
    path.mkdir(parents=True, exist_ok=True)
    if not (path / ".git").exists():
        run(["git", "init", path], capture=True)
        run(["git", "-C", path, "config", "core.autocrlf", "false"])
    try:
        run(["git", "-C", path, "cat-file", "-e", commit + "^{commit}"], capture=True)
    except RuntimeError:
        errors = []
        for remote in dict.fromkeys([url, fallback]):
            if not remote:
                continue
            try:
                run(["git", "-C", path, "-c", "credential.interactive=never", "-c", "http.lowSpeedLimit=1000", "-c", "http.lowSpeedTime=30", "fetch", "--depth=1", remote, commit], capture=True)
                break
            except RuntimeError as exc:
                errors.append(str(exc))
        else:
            raise RuntimeError("\n".join(errors))
    # Do not reset or discard a modified existing checkout.
    if run(["git", "-C", path, "status", "--porcelain", "--ignore-submodules=all"], capture=True):
        raise RuntimeError(f"checkout has local changes: {path}")
    run(["git", "-C", path, "checkout", "--detach", commit], capture=True)
    actual = run(["git", "-C", path, "rev-parse", "HEAD"], capture=True)
    if actual != commit:
        raise RuntimeError(f"commit mismatch: {path}: {actual}")
    return {"path": str(path.relative_to(ROOT)), "upstream": url, "commit": actual,
            "fallback_mirror": fallback}


def fetch(source, workers):
    provenance = [fetch_repo(source, URL, PIN)]
    modules = run(["git", "-C", source, "config", "-f", ".gitmodules", "--get-regexp", "submodule.*.path"], capture=True)
    jobs = []
    for line in modules.splitlines():
        key, relative = line.split(None, 1)
        url = run(["git", "-C", source, "config", "-f", ".gitmodules", "--get", key[:-4] + "url"], capture=True)
        tree = run(["git", "-C", source, "ls-tree", PIN, relative], capture=True)
        commit = tree.split()[2]
        jobs.append((source / relative, url, commit, MIRRORS.get(Path(relative).name)))
    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(fetch_repo, *job): job for job in jobs}
        for future in concurrent.futures.as_completed(futures):
            try:
                item = future.result()
                provenance.append(item)
                print("FETCHED", item["path"], item["commit"], flush=True)
            except Exception as exc:
                failures.append(str(exc))
    if failures:
        raise RuntimeError("Some exact pinned submodules were not available:\n" + "\n".join(failures))
    return provenance


def executable(candidate, name):
    found = str(candidate) if candidate and Path(candidate).is_file() else shutil.which(name)
    if not found:
        raise RuntimeError(f"missing host tool {name}; provide its path explicitly")
    return found


def prebuilt(work, source):
    """Fail closed if the mutable nightly URL no longer serves the pinned APK."""
    checkout = run(["git", "-C", source, "rev-parse", "HEAD"], capture=True)
    if checkout != PIN:
        raise RuntimeError(f"JNI source mismatch: expected {PIN}, got {checkout}")
    archive = work / "termux-x11-universal-debug.apk"
    if not archive.is_file():
        temporary = archive.with_suffix(".download")
        with urllib.request.urlopen(APK_URL, timeout=60) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        temporary.replace(archive)
    apk_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
    if apk_hash != APK_SHA256:
        raise RuntimeError(f"nightly APK hash changed: {apk_hash}; pinned asset must be recovered or consciously repinned")
    with zipfile.ZipFile(archive) as package:
        commits = {value.decode() for name in package.namelist()
                   if re.fullmatch(r"classes[0-9]*\.dex", name)
                   for value in re.findall(rb"(?<![0-9a-f])[0-9a-f]{40}(?![0-9a-f])", package.read(name))}
        if commits != {PIN}:
            raise RuntimeError(f"APK commit evidence differs from pinned JNI source: {sorted(commits)}")
        binary = package.read("lib/arm64-v8a/libXlorie.so")
    if binary[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", binary, 18)[0] != 183:
        raise RuntimeError("expected ELF64 little-endian AArch64 library")
    digest = hashlib.sha256(binary).hexdigest()
    if digest != LIB_SHA256:
        raise RuntimeError(f"native library hash mismatch: {digest}")
    output = ROOT / "app/src/main/jniLibs/arm64-v8a/libXlorie.so"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    provenance = {"project": "Termux:X11", "source_commit": PIN,
                  "release_asset_id": 616219007, "download_url": APK_URL,
                  "apk_sha256": apk_hash, "artifact_sha256": digest,
                  "artifact_size": len(binary), "elf_machine": 183,
                  "artifact": output.relative_to(ROOT).as_posix(),
                  "commit_evidence": "unique 40-hex APK DEX string matches pinned source",
                  "license": "GPL-3.0; component notices apply",
                  "validation": "hash, source commit evidence and ELF architecture; device loading separate"}
    (work / "binary-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    license_dir = work / "licenses"
    license_dir.mkdir(exist_ok=True)
    shutil.copy2(source / "LICENSE", license_dir / "Termux-X11-GPL-3.0.txt")
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "third_party/termux-x11")
    parser.add_argument("--work", type=Path, default=ROOT / ".tools/real-x11")
    parser.add_argument("--fetch-only", action="store_true")
    parser.add_argument("--prebuilt", action="store_true", help="extract hash-pinned official APK library, requiring matching local source commit")
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--cmake", type=Path)
    parser.add_argument("--ninja", type=Path)
    parser.add_argument("--bison", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    args.source = args.source.resolve()
    args.work = args.work.resolve()
    args.source.relative_to(ROOT)
    args.work.relative_to(ROOT)
    args.work.mkdir(parents=True, exist_ok=True)
    if args.prebuilt:
        prebuilt(args.work, args.source)
        return
    provenance = fetch(args.source, args.jobs)
    manifest = {"project": "Termux:X11", "license": "GPL-3.0; preserve component notices",
                "source_commit": PIN, "sources": provenance, "built": False}
    manifest_path = args.work / "provenance.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if args.fetch_only:
        print(manifest_path)
        return
    cmake = executable(args.cmake, "cmake")
    ninja = executable(args.ninja, "ninja")
    bison = executable(args.bison, "bison")
    executable(None, "bash")
    executable(None, "patch")
    if not args.ndk or not (args.ndk / "build/cmake/android.toolchain.cmake").is_file():
        raise RuntimeError("--ndk must identify an extracted Android NDK")
    # The pinned upstream recipe executes /usr/bin/gcc for a build-host generator.
    # Keep this explicit rather than accidentally cross-compiling that helper.
    if os.name == "nt":
        raise RuntimeError("Windows source fetch succeeded, but pinned upstream requires /usr/bin/gcc for makekeys, POSIX shell redirection and CMake symlinks. Use this tool under Linux with host gcc/bison/bash/patch and a Linux-host NDK. Windows portability patch is not yet implemented.")
    if not Path("/usr/bin/gcc").is_file():
        raise RuntimeError("pinned upstream requires host /usr/bin/gcc for makekeys")
    overlay = args.work / "source"
    if overlay.exists():
        raise RuntimeError(f"build source overlay already exists; choose a new --work directory: {overlay}")
    shutil.copytree(args.source / "lorie/src/main/cpp", overlay,
                    ignore=shutil.ignore_patterns(".git"))
    build = args.work / "build"
    run([cmake, "-S", overlay, "-B", build, "-G", "Ninja",
         "-DCMAKE_MAKE_PROGRAM=" + ninja,
         "-DCMAKE_TOOLCHAIN_FILE=" + str(args.ndk.resolve() / "build/cmake/android.toolchain.cmake"),
         "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-30",
         "-DPython3_EXECUTABLE=" + sys.executable, "-DBISON_EXECUTABLE=" + bison])
    run([cmake, "--build", build, "--target", "Xlorie", "-j", args.jobs])
    binary = build / "libXlorie.so"
    if not binary.is_file():
        raise RuntimeError("build completed without expected libXlorie.so")
    license_dir = args.work / "licenses"
    license_dir.mkdir()
    shutil.copy2(args.source / "LICENSE", license_dir / "Termux-X11-GPL-3.0.txt")
    for item in provenance[1:]:
        component = ROOT / item["path"]
        for name in ("COPYING", "LICENSE", "COPYING.txt", "LICENSE.txt"):
            if (component / name).is_file():
                shutil.copy2(component / name, license_dir / (component.name + "-" + name))
    manifest.update(built=True, artifact=str(binary.relative_to(ROOT)),
                    sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(binary)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as exc:
        print("ERROR:", exc, file=sys.stderr)
        sys.exit(1)
