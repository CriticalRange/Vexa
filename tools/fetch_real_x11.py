#!/usr/bin/env python3
"""Extract the hash-pinned official Xlorie library; never install its APK.

The upstream nightly URL is mutable. A different release fails closed instead
of updating the lock or mixing Java/JNI versions. --apk permits an archived copy
of the exact pinned APK. All validation finishes before replacing the output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import urllib.request
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, help="existing copy of the exact pinned official APK")
    args = parser.parse_args()
    lock = json.loads((root / "tools/real_x11.lock.json").read_text(encoding="utf-8"))
    work = root / ".tools/real-x11"
    work.mkdir(parents=True, exist_ok=True)
    archive = args.apk or work / "termux-x11-universal-debug.apk"
    if not archive.is_file():
        if args.apk:
            raise ValueError(f"APK does not exist: {archive}")
        temporary = archive.with_suffix(".download")
        with urllib.request.urlopen(lock["download_url"], timeout=60) as response, temporary.open("wb") as target:
            shutil.copyfileobj(response, target)
        temporary.replace(archive)
    if digest(archive.read_bytes()) != lock["apk_sha256"]:
        raise ValueError("APK SHA256 differs from lock; retrieve the archived pinned asset or consciously update all matching JNI sources and lock")
    with zipfile.ZipFile(archive) as package:
        commits = {value.decode() for name in package.namelist()
                   if re.fullmatch(r"classes[0-9]*\.dex", name)
                   for value in re.findall(rb"(?<![0-9a-f])[0-9a-f]{40}(?![0-9a-f])", package.read(name))}
        if commits != {lock["source_commit"]}:
            raise ValueError(f"APK DEX source commit mismatch: {sorted(commits)}")
        library = package.read(lock["library_entry"])
    if digest(library) != lock["library_sha256"]:
        raise ValueError("library SHA256 differs from lock")
    if len(library) < 20 or library[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", library, 18)[0] != lock["elf_machine"]:
        raise ValueError("expected ELF64 little-endian AArch64 library")
    output = root / lock["output"]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".verified")
    temporary.write_bytes(library)
    temporary.replace(output)
    provenance = dict(lock, validation="APK and library SHA256, unique DEX source commit, ELF64/AArch64 verified; device test separate")
    (work / "binary-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(f"X11_FETCH_VERIFIED {output} sha256={lock['library_sha256']}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
