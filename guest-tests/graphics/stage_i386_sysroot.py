"""Add Ubuntu noble i386 development/runtime libraries to the multiarch sysroot.

Unlike stage_rootfs.py this never replaces the existing amd64 tree.
Downloaded packages are checked against the repository index's SHA256.
"""
import gzip
import hashlib
from pathlib import Path
import subprocess
import tempfile
import shutil

import stage_rootfs as base

SEEDS = ('libc6-dev', 'libgcc-13-dev', 'libstdc++-13-dev', 'libgl-dev',
         'libegl-dev', 'libgles-dev', 'libx11-dev', 'libsdl2-2.0-0')


def main():
    cache = Path(base.ROOT) / 'debs-i386'
    cache.mkdir(exist_ok=True)
    packages = {}
    for component in ('main', 'universe'):
        index = cache / f'{component}-Packages.gz'
        base.fetch(f'https://archive.ubuntu.com/ubuntu/dists/noble/{component}/binary-i386/Packages.gz', str(index))
        with gzip.open(index, 'rt', encoding='utf-8') as stream:
            packages.update(base.parse_packages(stream.read()))
    wanted = {}

    def add(name):
        name = name.split(':')[0]
        if name in wanted:
            return
        if name not in packages:
            raise RuntimeError(f'No i386 package for dependency {name}')
        info = wanted[name] = packages[name]
        for field in ('Pre-Depends', 'Depends'):
            for dependency in base.dep_names(info.get(field, '')):
                add(dependency)

    for name in SEEDS:
        add(name)
    links = []
    for name, info in wanted.items():
        deb = cache / Path(info['Filename']).name
        base.fetch('https://archive.ubuntu.com/ubuntu/' + info['Filename'], str(deb))
        if hashlib.sha256(deb.read_bytes()).hexdigest() != info['SHA256']:
            raise RuntimeError(f'Package hash mismatch: {deb}')
        with tempfile.TemporaryDirectory(dir=cache) as staging:
            subprocess.run([base.TAR, '-xf', str(deb), '-C', staging], check=True)
            data = next(Path(staging).glob('data.tar*'))
            base.extract_data(str(data), base.ROOTFS, links)
        print(f'extracted {name}', flush=True)
    base.materialize_links(base.ROOTFS, links)
    root = Path(base.ROOTFS)
    (root / 'lib').mkdir(exist_ok=True)
    shutil.copy2(root / 'usr/lib/i386-linux-gnu/ld-linux.so.2', root / 'lib/ld-linux.so.2')
    for path in (root / 'usr/lib/i386-linux-gnu').glob('*.so'):
        if path.stat().st_size > 65536 or path.read_bytes().startswith(b'\x7fELF'):
            continue
        data = path.read_bytes()
        if b'GROUP' in data or b'INPUT' in data:
            path.write_bytes(data.replace(b'/lib/i386-linux-gnu/', b'/usr/lib/i386-linux-gnu/').replace(b'/usr/usr/', b'/usr/'))
    print('I386_SYSROOT_READY', flush=True)


if __name__ == '__main__':
    main()
