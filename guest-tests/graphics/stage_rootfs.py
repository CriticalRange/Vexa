"""Download an Ubuntu noble x86_64 sysroot and install the fake guest libX11."""

import gzip
import os
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = os.path.dirname(os.path.abspath(__file__))
ROOTFS = os.path.join(ROOT, "rootfs")
DEBS = os.path.join(ROOT, "debs-rootfs")
MARKER = os.path.join(ROOTFS, ".vexa-rootfs-ready")
INDEXES = [
    "http://archive.ubuntu.com/ubuntu/dists/noble/main/binary-amd64/Packages.gz",
    "http://archive.ubuntu.com/ubuntu/dists/noble/universe/binary-amd64/Packages.gz",
]
SEEDS = [
    "libc6",
    "libc6-dev",
    "linux-libc-dev",
    "libgcc-s1",
    "libstdc++6",
    "libstdc++-13-dev",
    "libgcc-13-dev",
    "gcc-13-base",
    "zlib1g",
    "libjpeg-turbo8",
    "libpng16-16t64",
    "libx11-dev",
    "libegl-dev",
    "libgles-dev",
    "libgl-dev",
    "vulkan-tools",
    "mesa-utils-bin",
    "libxkbcommon0",
    "libxkbcommon-x11-0",
    "libdecor-0-0",
    "libwayland-client0",
    "libvulkan1",
]
MIRROR = "http://archive.ubuntu.com/ubuntu/"
SDK = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME") or os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk")
CLANG = os.environ.get("VEXA_GUEST_CC") or os.path.join(SDK, "ndk", "29.0.14206865", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "clang.exe")
TAR = r"C:\Windows\System32\tar.exe" if os.name == "nt" else "tar"
FAKE_X11 = os.path.normpath(os.path.join(ROOT, "..", "x11-fake", "libX11_fake.c"))
FAKE_XCB = os.path.normpath(os.path.join(ROOT, "..", "x11-fake", "libxcb_fake.c"))
GLMARK = os.path.join(ROOT, "tree", "usr", "share", "glmark2")


def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        print(f"cached {os.path.basename(dest)}", flush=True)
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    print(f"get {os.path.basename(dest)}", flush=True)
    urllib.request.urlretrieve(url, dest)


def parse_packages(text):
    pkgs = {}
    cur = {}
    last = None
    for line in text.splitlines():
        if line == "":
            name = cur.get("Package")
            if name and "Filename" in cur:
                pkgs[name] = cur
            cur = {}
            last = None
            continue
        if line.startswith((" ", "\t")):
            if last:
                cur[last] = cur.get(last, "") + " " + line.strip()
            continue
        if ":" in line:
            key, value = line.split(":", 1)
            last = key
            cur[key] = value.strip()
    name = cur.get("Package")
    if name and "Filename" in cur:
        pkgs[name] = cur
    return pkgs


def dep_names(depends):
    names = []
    for part in (depends or "").split(","):
        choice = part.split("|")[0].strip()
        if not choice:
            continue
        name = choice.split()[0]
        if name and name[0].isalnum():
            names.append(name)
    return names


def extract_data(data, root, links):
    # Windows cannot create the package symlinks without extra privileges, so record them and copy later.
    with tarfile.open(data) as archive:
        for member in archive:
            if member.issym() or member.islnk():
                target = member.linkname if member.issym() else "/" + member.linkname.lstrip("./")
                links.append((member.name, target))
                continue
            try:
                archive.extract(member, root, filter="fully_trusted")
            except OSError as error:
                print(f"skip {member.name}: {error}", flush=True)


def materialize_links(root, links):
    pending = links
    while pending:
        left = []
        for name, target in pending:
            dest = os.path.normpath(os.path.join(root, name))
            if os.path.lexists(dest):
                continue
            base = root if target.startswith("/") else os.path.dirname(dest)
            src = os.path.normpath(os.path.join(base, target.lstrip("/")))
            if os.path.isfile(src):
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(src, dest)
            elif os.path.isdir(src):
                shutil.copytree(src, dest, dirs_exist_ok=True)
            else:
                left.append((name, target))
        if len(left) == len(pending):
            for name, target in left:
                print(f"dangling {name} -> {target}", flush=True)
            break
        pending = left


def rewrite_linker_scripts(root):
    for dirpath, _, files in os.walk(root):
        for name in files:
            path = os.path.join(dirpath, name)
            try:
                if os.path.getsize(path) > 65536:
                    continue
                with open(path, "rb") as handle:
                    text = handle.read()
                if text.startswith(b"\x7fELF"):
                    continue
            except OSError:
                continue
            stripped = text.lstrip()
            if not (stripped.startswith(b"/*") or stripped.startswith(b"OUTPUT_FORMAT") or stripped.startswith(b"GROUP") or stripped.startswith(b"INPUT")):
                continue
            # lld does not treat a leading '=' as the sysroot. Point multiarch paths at /usr/lib, where the ELFs live.
            updated = text
            for old, new in (
                (b"=/lib/x86_64-linux-gnu/", b"/usr/lib/x86_64-linux-gnu/"),
                (b" /lib/x86_64-linux-gnu/", b" /usr/lib/x86_64-linux-gnu/"),
                (b"=/usr/", b"/usr/"),
                (b"=/lib64/", b"/lib64/"),
                (b" /lib64/", b" /lib64/"),
            ):
                updated = updated.replace(old, new)
            if updated != text:
                with open(path, "wb") as handle:
                    handle.write(updated)
                print(f"script {os.path.relpath(path, root)}", flush=True)


def ensure_loader(root):
    candidates = []
    for dirpath, _, files in os.walk(root):
        for name in files:
            if name.startswith("ld-linux") or name.startswith("ld-2"):
                path = os.path.join(dirpath, name)
                try:
                    with open(path, "rb") as handle:
                        if handle.read(4) == b"\x7fELF":
                            candidates.append(path)
                except OSError:
                    pass
    if not candidates:
        raise SystemExit("ld-linux ELF was not found in the rootfs")
    source = max(candidates, key=os.path.getsize)
    for relative in (
        os.path.join("lib64", "ld-linux-x86-64.so.2"),
        os.path.join("lib", "x86_64-linux-gnu", "ld-linux-x86-64.so.2"),
        os.path.join("usr", "lib", "x86_64-linux-gnu", "ld-linux-x86-64.so.2"),
    ):
        dest = os.path.join(root, relative)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        if os.path.abspath(dest) == os.path.abspath(source):
            continue
        if os.path.lexists(dest):
            os.remove(dest)
        shutil.copy2(source, dest)
    print(f"loader {source}", flush=True)


def compile_fake_x11(root):
    dest_dir = os.path.join(root, "usr", "lib", "x86_64-linux-gnu")
    os.makedirs(dest_dir, exist_ok=True)
    for name in os.listdir(dest_dir):
        if name.startswith("libX11.so"):
            os.remove(os.path.join(dest_dir, name))
    dest = os.path.join(dest_dir, "libX11.so.6")
    cmd = [
        CLANG,
        "--target=x86_64-linux-gnu",
        f"--sysroot={root}",
        "-fPIC",
        "-shared",
        "-fno-stack-protector",
        "-O2",
        "-fuse-ld=lld",
        "-Wl,-soname,libX11.so.6",
        FAKE_X11,
        "-o",
        dest,
        "-lc",
    ]
    print("compile libX11.so.6", flush=True)
    subprocess.check_call(cmd)
    with open(dest, "rb") as handle:
        if handle.read(4) != b"\x7fELF":
            raise SystemExit("fake libX11 is not an ELF")


def compile_fake_xcb(root):
    """Replace Ubuntu libxcb with the VEXA connection shim.

    find_elf() prefers the largest libxcb.so.1* file, so the real
    libxcb.so.1.1.0 has to go or the APK would keep the socket client.
    """
    dest_dir = os.path.join(root, "usr", "lib", "x86_64-linux-gnu")
    os.makedirs(dest_dir, exist_ok=True)
    for name in os.listdir(dest_dir):
        if name == "libxcb.so" or name.startswith("libxcb.so."):
            os.remove(os.path.join(dest_dir, name))
    dest = os.path.join(dest_dir, "libxcb.so.1")
    cmd = [
        CLANG,
        "--target=x86_64-linux-gnu",
        f"--sysroot={root}",
        "-fPIC",
        "-shared",
        "-fno-stack-protector",
        "-O2",
        "-fuse-ld=lld",
        "-Wl,-soname,libxcb.so.1",
        "-Wl,--no-undefined",
        FAKE_XCB,
        "-o",
        dest,
        "-lc",
    ]
    print("compile libxcb.so.1", flush=True)
    subprocess.check_call(cmd)
    with open(dest, "rb") as handle:
        if handle.read(4) != b"\x7fELF":
            raise SystemExit("fake libxcb is not an ELF")


def copy_glmark(root):
    dest = os.path.join(root, "usr", "share", "glmark2")
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    shutil.copytree(GLMARK, dest)
    print("glmark2 data", flush=True)


def main():
    if os.path.exists(MARKER):
        print("ROOTFS_READY", flush=True)
        return
    os.makedirs(DEBS, exist_ok=True)
    packages = {}
    for url in INDEXES:
        dest = os.path.join(DEBS, os.path.basename(os.path.dirname(os.path.dirname(url))) + "-" + os.path.basename(url))
        fetch(url, dest)
        with gzip.open(dest, "rt", encoding="utf-8", errors="replace") as handle:
            packages.update(parse_packages(handle.read()))
    print(f"index {len(packages)}", flush=True)

    wanted = []
    seen = set()

    def add(name):
        if name in seen:
            return
        seen.add(name)
        info = packages.get(name)
        if not info:
            print(f"skip {name}", flush=True)
            return
        wanted.append(name)
        for dep in dep_names(info.get("Depends", "")):
            add(dep)

    for seed in SEEDS:
        add(seed)
    print(f"packages {len(wanted)}", flush=True)

    if os.path.isdir(ROOTFS):
        shutil.rmtree(ROOTFS)
    os.makedirs(ROOTFS)
    links = []
    for name in wanted:
        filename = packages[name]["Filename"]
        deb = os.path.join(DEBS, os.path.basename(filename))
        fetch(MIRROR + filename, deb)
        staging = os.path.join(DEBS, "_stage")
        if os.path.isdir(staging):
            shutil.rmtree(staging)
        os.makedirs(staging)
        subprocess.check_call([TAR, "-xf", deb, "-C", staging])
        data = None
        for entry in os.listdir(staging):
            if entry.startswith("data.tar"):
                data = os.path.join(staging, entry)
                break
        if not data:
            raise SystemExit(f"no data.tar in {name}")
        extract_data(data, ROOTFS, links)
        shutil.rmtree(staging)
        print(f"extracted {name}", flush=True)

    materialize_links(ROOTFS, links)
    rewrite_linker_scripts(ROOTFS)
    ensure_loader(ROOTFS)
    copy_glmark(ROOTFS)
    compile_fake_x11(ROOTFS)
    compile_fake_xcb(ROOTFS)
    with open(MARKER, "w", encoding="utf-8") as handle:
        handle.write("vexa-graphics-1\n")
    print("ROOTFS_READY", flush=True)


if __name__ == "__main__":
    main()
