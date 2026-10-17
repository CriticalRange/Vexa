#!/usr/bin/env python3
"""Stage a user-supplied Proton (modern files/ layout), without downloading it.

Example:
  python tools/stage_proton.py --source /downloads/Proton --version proton-10 \
    --destination app/src/main/assets/runtime/rootfs/opt/vexa/proton/proton-10 \
    --rootfs app/src/main/assets/runtime/rootfs

Run on a filesystem supporting symbolic links (Windows may require Developer
Mode). The destination must not exist. A failure may leave a partial destination;
inspect and remove it manually before retrying. No source files are modified.
The manifest inventories SHA256 file contents and link targets, not signatures.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import sys


def guest_resolve(root, guest_path):
    """Resolve links with absolute targets interpreted inside the guest root."""
    pending = list(PurePosixPath(guest_path).parts)
    resolved = []
    links = 0
    while pending:
        part = pending.pop(0)
        if part in ("/", ".", ""):
            continue
        if part == "..":
            if not resolved:
                raise ValueError(f"guest path escapes rootfs: {guest_path}")
            resolved.pop()
            continue
        candidate = root.joinpath(*resolved, part)
        if candidate.is_symlink():
            links += 1
            if links > 40:
                raise ValueError(f"too many symbolic links: {guest_path}")
            target = os.readlink(candidate).replace("\\", "/")
            if target.startswith("/"):
                resolved = []
            pending = list(PurePosixPath(target).parts) + pending
        else:
            resolved.append(part)
    return root.joinpath(*resolved)


def require_amd64(path):
    with path.open("rb") as stream:
        header = stream.read(20)
    if (len(header) < 20 or header[:6] != b"\x7fELF\x02\x01"
            or struct.unpack_from("<H", header, 18)[0] != 62):
        raise ValueError(f"expected little-endian x86-64 ELF: {path}")


def inventory(source):
    entries = []
    # Do not follow directory links: validate and inventory the link itself.
    for directory, dirs, files in os.walk(source, followlinks=False):
        for name in sorted(dirs + files):
            path = Path(directory) / name
            relative = path.relative_to(source).as_posix()
            if path.is_symlink():
                target = os.readlink(path)
                if os.path.isabs(target) or PurePosixPath(target).is_absolute():
                    raise ValueError(f"absolute bundle symlink {relative} -> {target}; use a relocatable Proton archive")
                try:
                    path.resolve(strict=True).relative_to(source)
                except (ValueError, OSError, RuntimeError) as exc:
                    raise ValueError(f"dangling, cyclic or escaping bundle symlink {relative} -> {target}") from exc
                entries.append({"path": relative, "type": "symlink", "target": target})
            elif path.is_file():
                digest = hashlib.sha256()
                with path.open("rb") as stream:
                    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(chunk)
                entries.append({"path": relative, "type": "file", "size": path.stat().st_size,
                                "sha256": digest.hexdigest()})
            elif not path.is_dir():
                raise ValueError(f"unsupported special file: {relative}")
    return sorted(entries, key=lambda entry: entry["path"])


def check_rootfs(root):
    python = guest_resolve(root, "/usr/bin/python3")
    if not python.is_file():
        raise ValueError(f"guest Python missing: {root}/usr/bin/python3; install x86-64 Python 3 in the rootfs")
    require_amd64(python)
    # Static checks only: the host must never execute guest binaries here.
    lib = guest_resolve(root, "/usr/lib")
    versions = sorted(lib.glob("python3.*"))
    required = ("os", "sysconfig", "json", "subprocess", "fcntl", "ctypes", "_ctypes")
    missing = []
    for module in required:
        found = False
        for version in versions:
            candidates = [version / (module + ".py"), version / module / "__init__.py"]
            candidates += list((version / "lib-dynload").glob(module + ".*.so"))
            if any(guest_resolve(root, "/" + candidate.relative_to(root).as_posix()).is_file()
                   for candidate in candidates):
                found = True
                break
        if not found:
            missing.append(module)
    if missing:
        raise ValueError("guest Python stdlib files not found: " + ", ".join(missing)
                         + "; install the full matching Python stdlib (static check, built-in/frozen modules may require manual verification)")
    return {"python": "/usr/bin/python3", "stdlib_static_check": "passed",
            "runtime_execution": "not performed"}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", required=True, type=Path, help="unpacked, trusted Proton distribution")
    parser.add_argument("--destination", required=True, type=Path, help="new version directory; must not exist")
    parser.add_argument("--version", required=True, help="stable installation identifier, e.g. proton-10")
    parser.add_argument("--rootfs", type=Path, help="optionally require guest x86-64 Python and standard library files")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.version):
        parser.error("--version must contain only letters, digits, dots, underscores and hyphens")
    try:
        source = args.source.resolve(strict=True)
        destination = args.destination.absolute()
        if not source.is_dir():
            raise ValueError("--source must be a directory")
        if os.path.lexists(destination):
            raise ValueError("destination already exists; choose a new version directory")
        if destination.name != args.version:
            raise ValueError("destination directory name must match --version")
        try:
            destination.resolve().relative_to(source)
        except ValueError:
            pass
        else:
            raise ValueError("destination must be outside the source directory")
        for relative in ("proton", "filelock.py", "version", "files/bin/wineserver"):
            if not (source / relative).is_file():
                raise ValueError(f"missing {relative}; expected modern Proton files/ layout, not legacy dist/")
        wines = [source / "files/bin" / name for name in ("wine64", "wine")]
        wine = next((path for path in wines if path.is_file()), None)
        if wine is None:
            raise ValueError("missing files/bin/wine64 or files/bin/wine")
        entries = inventory(source)
        if any(entry["path"] == "vexa-proton-manifest.json" for entry in entries):
            raise ValueError("source already contains reserved vexa-proton-manifest.json")
        require_amd64(wine)
        require_amd64(source / "files/bin/wineserver")
        dependencies = check_rootfs(args.rootfs.resolve(strict=True)) if args.rootfs else {"status": "not checked"}
        shutil.copytree(source, destination, symlinks=True, copy_function=shutil.copy2)
        # Detect interrupted/corrupt copies before recording an installation.
        if inventory(destination) != entries:
            raise ValueError("staged files differ from source; destination is incomplete")
        manifest = {"schema_version": 1, "version": args.version,
                    "upstream_version": (source / "version").read_text(encoding="utf-8").strip(),
                    "layout": "files", "architecture": "x86_64", "dependencies": dependencies,
                    "entries": entries}
        (destination / "vexa-proton-manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"Staged {args.version} at {destination} ({len(entries)} manifest entries)")
        print("Guest runtime validation and device testing are still required.")
    except (ValueError, OSError, RuntimeError) as exc:
        parser.exit(1, f"error: {exc}\n")


if __name__ == "__main__":
    main()
