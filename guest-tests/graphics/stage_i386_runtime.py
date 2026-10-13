"""Collect a closed i386 runtime and the paired Android GL thunk.

All dependencies are resolved and ELF classes checked before any asset changes.
The 32-bit SDL library runs as guest code; Android SDL/audio thunks remain 64-bit.
"""
import os
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parent
VEXA = ROOT.parent.parent
RUNTIME = VEXA / 'app/src/main/assets/runtime'
BUILD = Path(os.environ.get('FEX_BUILD_DIR', str(VEXA.parent / 'FEX/build-android-arm64-ninja')))


def elf(path):
    data = Path(path).read_bytes()
    if data[:4] != b'\x7fELF' or len(data) < 52 or data[5] != 1:
        raise ValueError(f'Not a little-endian ELF: {path}')
    return data, (data[4], struct.unpack_from('<H', data, 18)[0])


def needed(path):
    data, arch = elf(path)
    if arch != (1, 3):
        raise ValueError(f'Not ELF32 i386: {path}: {arch}')
    phoff = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    loads, dynamic = [], None
    for i in range(count):
        kind, offset, va, _, filesz, _, _, _ = struct.unpack_from('<8I', data, phoff + i * size)
        if kind == 1:
            loads.append((va, offset, filesz))
        elif kind == 2:
            dynamic = (offset, filesz)
    if dynamic is None:
        return []
    offsets, strings = [], None
    for offset in range(dynamic[0], sum(dynamic), 8):
        tag, value = struct.unpack_from('<II', data, offset)
        if tag == 0:
            break
        if tag == 1:
            offsets.append(value)
        elif tag == 5:
            strings = value
    if not offsets:
        return []
    for va, offset, length in loads:
        if strings is not None and va <= strings < va + length:
            start = offset + strings - va
            return [data[start + n:data.index(b'\0', start + n)].decode('ascii') for n in offsets]
    raise ValueError(f'Missing dynamic string table: {path}')


def payloads():
    libdir = ROOT / 'rootfs/usr/lib/i386-linux-gnu'
    fakes = VEXA / 'guest-tests/x11-fake/i386'
    if not (fakes / 'libX11.so.6').is_file():
        raise ValueError('Build i386 X11 shims before staging')
    # PulseAudio stores its private library in a subdirectory. Flatten it into
    # the multiarch directory so no host-specific RPATH is needed on Android.
    candidates = {p.name: p for p in sorted(libdir.rglob('*')) if p.is_file()}
    candidates.update({p.name: p for p in fakes.glob('*.so*')})
    seeds = ['ld-linux.so.2', 'libc.so.6', 'libm.so.6', 'libdl.so.2',
             'libpthread.so.0', 'librt.so.1', 'libresolv.so.2',
             'libgcc_s.so.1', 'libstdc++.so.6', 'libSDL2-2.0.so.0']
    seeds += [p.name for p in fakes.glob('*.so*')]
    resolved = {}

    def add(name):
        if name in resolved:
            return
        path = candidates.get(name)
        if path is None:
            raise ValueError(f'Missing i386 dependency: {name}')
        deps = needed(path)
        resolved[name] = path
        for dep in deps:
            add(dep)

    for name in seeds:
        add(name)
    guest = BUILD / 'Guest_32/libGL-guest.so'
    for name in needed(guest):
        add(name)
    host = BUILD / 'HostLibs_32/libGL-host.so'
    if elf(host)[1] != (2, 183):
        raise ValueError('The i386 host thunk must be ELF64 AArch64')
    result = [(source, f'rootfs/usr/lib/i386-linux-gnu/{name}') for name, source in resolved.items()]
    result += [(resolved['ld-linux.so.2'], 'rootfs/lib/ld-linux.so.2'),
               (guest, 'thunks/guest_32/libGL-guest.so'),
               (host, 'thunks/host_32/libGL-host.so')]
    return result


def stage():
    files = payloads()
    for source, relative in files:
        destination = RUNTIME / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    print(f'I386_RUNTIME_READY: {len(files)} validated files', flush=True)


if __name__ == '__main__':
    stage()
    from stage_sdl3_thunks import update_stamp
    update_stamp()
