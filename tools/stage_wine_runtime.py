#!/usr/bin/env python3
"""Build an isolated AMD64 Debian Wine/Proton support rootfs; never touches app assets.

  python tools/stage_wine_runtime.py --work third_party/wine-runtime

First run resolves Depends and Pre-Depends from Debian bookworm/main over HTTPS,
then saves a version/SHA256 lock. Subsequent runs replay that lock and verify all
cached archives. --resolve refreshes the lock; use a new --work for a new build.
This is extraction, not dpkg installation: maintainer scripts are never executed.
Recommends are excluded. Missing alternatives/version constraints fail explicitly.
Manual/info pages and systemd units are omitted (names unsupported on Windows).
Repository metadata is transported with TLS and hash-pinned in the local lock;
OpenPGP signature verification is NOT performed by this helper. For production,
review/check the lock against authenticated Debian Release metadata separately.

proton-rootfs.tar preserves Unix modes and symbolic links for Android extraction.
The Windows proton-rootfs directory materializes symlinks for host-side inspection;
use the tar, not adb push of that directory, to deploy the original Unix layout.
No adb, guest binary, user shell command, package script or emulator is executed.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import io
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import urllib.request


MIRROR = "https://deb.debian.org/debian/"
INDEX = MIRROR + "dists/bookworm/main/binary-amd64/Packages.xz"
SEEDS = """base-files bash dash coreutils findutils grep sed tar gzip xz-utils
python3 python3-venv libgcc-s1 libstdc++6 libc6 libnss-resolve
libx11-6 libx11-xcb1 libxcb1 libxcb-render0 libxcb-shm0 libxcb-randr0
libxcb-xfixes0 libxcb-xinput0 libxcb-sync1 libxcb-present0 libxcb-dri3-0
libxcb-dri2-0 libxcb-glx0 libxcb-shape0 libxkbcommon0 libxkbcommon-x11-0
libxext6 libxrandr2 libxrender1 libxcursor1 libxinerama1 libxi6 libxfixes3
libxcomposite1 libxxf86vm1 libxdamage1 libxss1 xkb-data xfonts-base
fonts-dejavu-core fontconfig libfreetype6 libfontconfig1 libpng16-16
libgl1 libglx-mesa0 libgl1-mesa-dri libegl1 libegl-mesa0 libglu1-mesa
libvulkan1 mesa-vulkan-drivers mesa-utils x11-utils x11-xserver-utils
libfaudio0 libpulse0 libasound2 libasound2-plugins libgnutls30 libkrb5-3
libgssapi-krb5-2 libldap-2.5-0 libdbus-1-3 libudev1 libusb-1.0-0
libunwind8 libgstreamer1.0-0 libgstreamer-plugins-base1.0-0
gstreamer1.0-plugins-base ca-certificates""".split()
EXCLUDED = ("usr/share/man/", "usr/share/info/", "lib/systemd/system/",
            "usr/lib/systemd/system/", "lib/systemd/user/", "usr/lib/systemd/user/")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(url, path, expected=None):
    if path.is_file() and (expected is None or sha256(path) == expected):
        return path
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".part")
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=90) as response, temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
            if expected and sha256(temporary) != expected:
                raise ValueError(f"SHA256 mismatch: {url}")
            temporary.replace(path)
            return path
        except (OSError, ValueError):
            if attempt == 2:
                raise
    raise AssertionError("unreachable")


def parse_packages(text):
    packages = {}
    for stanza in text.split("\n\n"):
        fields = {}
        key = None
        for line in stanza.splitlines():
            if line.startswith(" ") and key:
                fields[key] += " " + line.strip()
            elif ": " in line:
                key, value = line.split(": ", 1)
                fields[key] = value
        if "Filename" in fields and fields.get("Architecture") in ("amd64", "all"):
            packages[fields["Package"]] = fields
    return packages


def version_compare(a, b):
    """Debian epoch/upstream/revision ordering, including the special tilde."""
    def split(value):
        epoch, _, rest = value.partition(":")
        if not rest:
            epoch, rest = "0", epoch
        upstream, separator, revision = rest.rpartition("-")
        return int(epoch), upstream if separator else rest, revision if separator else "0"

    def order(char):
        if char == "~":
            return -1
        if not char:
            return 0
        if char.isalpha():
            return ord(char)
        return ord(char) + 256

    def part(x, y):
        while x or y:
            while (x and not x[0].isdigit()) or (y and not y[0].isdigit()):
                cx = x[0] if x and not x[0].isdigit() else ""
                cy = y[0] if y and not y[0].isdigit() else ""
                if order(cx) != order(cy):
                    return 1 if order(cx) > order(cy) else -1
                if cx:
                    x = x[1:]
                if cy:
                    y = y[1:]
            nx = re.match(r"\d*", x).group()
            ny = re.match(r"\d*", y).group()
            vx, vy = nx.lstrip("0"), ny.lstrip("0")
            if len(vx) != len(vy):
                return 1 if len(vx) > len(vy) else -1
            if vx != vy:
                return 1 if vx > vy else -1
            x, y = x[len(nx):], y[len(ny):]
        return 0

    ea, ua, ra = split(a)
    eb, ub, rb = split(b)
    return (1 if ea > eb else -1) if ea != eb else (part(ua, ub) or part(ra, rb))


RELATION = re.compile(r"^([a-z0-9][a-z0-9+.-]*)(?::(?:any|native|amd64))?(?:\s*\((<<|<=|=|>=|>>)\s*([^()]+)\))?$")


def relation(expression):
    match = RELATION.fullmatch(expression.strip())
    if not match:
        raise ValueError(f"unsupported binary dependency syntax: {expression}")
    return match.groups()


def satisfies(actual, operator, wanted):
    if not operator:
        return True
    compared = version_compare(actual, wanted)
    return {"<<": compared < 0, "<=": compared <= 0, "=": compared == 0,
            ">=": compared >= 0, ">>": compared > 0}[operator]


def resolve(packages):
    selected = {}
    providers = {}
    for package in packages.values():
        for entry in package.get("Provides", "").split(","):
            if entry.strip():
                name, _, version = relation(entry)
                providers.setdefault(name, []).append((package, version))

    def choose(expression):
        name, operator, version = relation(expression)
        package = packages.get(name)
        if package and satisfies(package["Version"], operator, version):
            return package
        for provider, provided_version in providers.get(name, []):
            if not operator or (provided_version and satisfies(provided_version, operator, version)):
                return provider
        return None

    def add(package):
        if package["Package"] in selected:
            return
        selected[package["Package"]] = package
        for field in ("Pre-Depends", "Depends"):
            for clause in package.get(field, "").split(","):
                if not clause.strip():
                    continue
                candidates = [choose(expression) for expression in clause.split("|")]
                available = [candidate for candidate in candidates if candidate]
                if not available:
                    raise ValueError(f"unresolved {package['Package']} {field}: {clause}")
                add(next((candidate for candidate in available if candidate["Package"] in selected), available[0]))

    for name in SEEDS:
        if name not in packages:
            raise ValueError(f"seed not in index: {name}")
        add(packages[name])
    # Essential packages can be omitted from Depends by Debian policy.
    for package in packages.values():
        if package.get("Essential") == "yes":
            add(package)
    return [selected[name] for name in sorted(selected)]


def deb_data(path):
    with path.open("rb") as stream:
        if stream.read(8) != b"!<arch>\n":
            raise ValueError(f"not a Debian ar archive: {path}")
        while header := stream.read(60):
            if len(header) != 60 or header[58:] != b"`\n":
                raise ValueError(f"invalid ar header: {path}")
            name = header[:16].decode("ascii").strip().rstrip("/")
            size = int(header[48:58])
            payload = stream.read(size)
            if len(payload) != size:
                raise ValueError(f"truncated ar member: {path}")
            if size % 2:
                stream.read(1)
            if name.startswith("data.tar"):
                return payload
    raise ValueError(f"no data.tar member in {path}")


def clean_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name:
        raise ValueError(f"unsafe archive path: {name}")
    return path.as_posix().removeprefix("./")


def unpack(path, root, members):
    with tarfile.open(fileobj=io.BytesIO(deb_data(path)), mode="r:*") as archive:
        for member in archive:
            if member.name.lstrip("./").startswith(EXCLUDED):
                continue
            name = clean_name(member.name)
            if name in ("", "."):
                continue
            if not (member.isdir() or member.isfile() or member.issym() or member.islnk()):
                continue  # Package special devices cannot be recreated in app storage.
            member.name = name
            if member.islnk():
                member.linkname = clean_name(member.linkname)
            destination = root / name
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
                os.chmod(destination, member.mode & 0o777)
            members[name] = member


def resolve_link(name, members):
    current = list(PurePosixPath(name).parts)
    resolved = []
    count = 0
    while current:
        part = current.pop(0)
        if part in ("/", ".", ""):
            continue
        if part == "..":
            if not resolved:
                raise ValueError(f"link escapes rootfs: {name}")
            resolved.pop()
            continue
        key = "/".join(resolved + [part])
        entry = members.get(key)
        if entry and (entry.issym() or entry.islnk()):
            count += 1
            if count > 40:
                raise ValueError(f"cyclic rootfs link: {name}")
            if entry.islnk() or entry.linkname.startswith("/"):
                resolved = []
            current = list(PurePosixPath(entry.linkname).parts) + current
        else:
            resolved.append(part)
    return "/".join(resolved)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=Path, required=True, help="isolated build/cache directory")
    parser.add_argument("--resolve", action="store_true", help="resolve fresh package index (existing rootfs is refused)")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    lock_path = work / "debian-bookworm-amd64.lock.json"
    root = work / "proton-rootfs"
    if root.exists():
        parser.error(f"refusing to overwrite existing rootfs: {root}; use a new --work directory")
    if args.resolve or not lock_path.exists():
        index_path = fetch(INDEX, work / "Packages.xz")
        packages = resolve(parse_packages(lzma.decompress(index_path.read_bytes()).decode("utf-8")))
        lock = {"schema_version": 1, "distribution": "Debian bookworm", "architecture": "amd64",
                "index_url": INDEX, "index_sha256": sha256(index_path), "transport": "HTTPS",
                "signature_verified": False, "seeds": SEEDS,
                "packages": [{key: package[key] for key in ("Package", "Version", "Architecture", "Filename", "SHA256", "Size")}
                             for package in packages]}
        lock_path.write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")
    else:
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        if lock.get("schema_version") != 1 or lock.get("architecture") != "amd64":
            parser.error("unsupported lock schema/architecture")
    packages = lock["packages"]
    print(f"Locked {len(packages)} packages, {sum(int(p['Size']) for p in packages) / 1048576:.1f} MiB compressed", flush=True)

    def download(package):
        relative = clean_name(package["Filename"])
        if not relative.startswith("pool/"):
            raise ValueError("package path must be under Debian pool/")
        path = fetch(MIRROR + relative, work / "debs" / PurePosixPath(relative).name, package["SHA256"])
        print("verified " + package["Package"] + " " + package["Version"], flush=True)
        return path

    with ThreadPoolExecutor(max_workers=8) as pool:
        archives = list(pool.map(download, packages))
    root.mkdir()
    members = {}
    for package, archive in zip(packages, archives):
        unpack(archive, root, members)
        print("extracted " + package["Package"], flush=True)
    # Writable runtime directories are real directories in the deployment tar.
    for name, mode in (("tmp", 0o1777), ("run", 0o755), ("root", 0o700), ("proc", 0o755), ("dev", 0o755)):
        (root / name).mkdir(exist_ok=True)
        if name not in members:
            info = tarfile.TarInfo(name)
            info.type = tarfile.DIRTYPE
            info.mode = mode
            members[name] = info
    links = {}
    unresolved = []
    for name, member in list(members.items()):
        if member.issym() or member.islnk():
            target = resolve_link(name, members)
            links[name] = {"target": member.linkname, "resolved": target,
                           "type": "hardlink" if member.islnk() else "symlink"}
            source, destination = root / target, root / name
            if source.is_file():
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
            elif source.is_dir():
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copytree(source, destination, dirs_exist_ok=True)
            else:
                unresolved.append(name)
    (work / "rootfs-links.json").write_text(json.dumps(links, indent=2) + "\n", encoding="utf-8")
    output = work / "proton-rootfs.tar"
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for name, member in sorted(members.items(), key=lambda item: (
                0 if item[1].isdir() else 2 if item[1].islnk() else 1,
                item[0].count("/"), item[0])):
            member.uid = member.gid = 0
            member.uname = member.gname = "root"
            member.mtime = 0
            member.pax_headers = {}
            if member.islnk():
                target = resolve_link(name, members)
                rel = os.path.relpath(target, PurePosixPath(name).parent.as_posix()).replace("\\", "/")
                member.type = tarfile.SYMTYPE
                member.linkname = rel
                member.size = 0
            if member.isfile():
                with (root / name).open("rb") as stream:
                    archive.addfile(member, stream)
            else:
                archive.addfile(member)
    report = {"rootfs": str(root), "archive": str(output), "archive_sha256": sha256(output),
              "package_count": len(packages), "file_count": len(members), "unresolved_symlinks": unresolved,
              "maintainer_scripts_executed": False, "device_runtime_validated": False,
              "excluded_prefixes": list(EXCLUDED),
              "lock_sha256": sha256(lock_path)}
    (work / "staging-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
