"""Verify the built APK contains the exact validated guest/host runtime pairs."""
import argparse
import hashlib
from pathlib import Path
import zipfile
import stage_i386_runtime as i386
import stage_sdl3_thunks as thunks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('apk', type=Path)
    args = parser.parse_args()
    files = i386.payloads()
    for arch, source, relative in thunks.PAYLOADS:
        if thunks.machine(source) != thunks.ELF_MACHINES[arch]:
            raise ValueError(f'Wrong ELF class/machine: {source}')
        files.append((Path(source), relative))
    with zipfile.ZipFile(args.apk) as apk:
        for source, relative in files:
            expected = hashlib.sha256(source.read_bytes()).digest()
            staged = hashlib.sha256((i386.RUNTIME / relative).read_bytes()).digest()
            packed = hashlib.sha256(apk.read('assets/runtime/' + relative)).digest()
            if expected != staged or expected != packed:
                raise ValueError(f'Stale or mismatched runtime payload: {relative}')
        stamp = (i386.RUNTIME / 'STAMP').read_bytes()
        if apk.read('assets/runtime/STAMP') != stamp:
            raise ValueError('APK runtime stamp is stale')
    print(f'APK_RUNTIME_PASS: {len(files)} payloads, ELF classes, i386 dependency closure and stamp')


if __name__ == '__main__':
    main()
