# RootFS Required Files (Tracking)

This file tracks the minimum RootFS content needed to launch targets under VEXA/FEX.
Update this file whenever a new missing dependency is discovered.

## Target: HytaleClient
Source binary:
`/home/critical/.var/app/com.hypixel.HytaleLauncher/data/Hytale/install/release/package/game/latest/Client/HytaleClient`

## Core Loader Requirements (from ELF)

- [x] `/lib64/ld-linux-x86-64.so.2` (`PT_INTERP`)
- [x] `/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2` (symlink target)
- [x] `/lib/x86_64-linux-gnu/libc.so.6` (`DT_NEEDED`)
- [x] `/lib/x86_64-linux-gnu/libm.so.6` (`DT_NEEDED`)

## Baseline Runtime Libraries Added (manual push)

- [x] `/lib/x86_64-linux-gnu/libdl.so.2`
- [x] `/lib/x86_64-linux-gnu/libpthread.so.0`
- [x] `/lib/x86_64-linux-gnu/libgcc_s.so.1`
- [x] `/usr/lib/x86_64-linux-gnu/libstdc++.so.6`
- [x] `/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.33`
- [x] `/etc/ld.so.cache`
- [x] `/etc/ld.so.conf`
- [x] `/etc/ld.so.conf.d`

## Observed Potential Runtime Libraries (from binary strings)

- [ ] `libssl.so` family (`libssl.so`, `libssl.so.1.0.0`, `libssl.so.1.0.2`, `libssl.so.1.1`, `libssl.so.10`, `libssl.so.3`)
- [ ] `libicui18n.so`
- [ ] `libicuuc.so`
- [ ] `libgssapi_krb5.so.2`

## Notes

- `DT_NEEDED` is only the static baseline.
- Additional libraries may be loaded dynamically (`dlopen`) at runtime.
- Add new entries here when loader/runtime logs report missing `.so` files.

## Verification Log

- 2026-03-13: `HytaleClient` analyzed with `readelf` and `strings`.
  - `PT_INTERP`: `/lib64/ld-linux-x86-64.so.2`
  - `DT_NEEDED`: `libm.so.6`, `libc.so.6`, `ld-linux-x86-64.so.2`
- 2026-03-13: Pushed missing glibc/runtime files to device rootfs path:
  `/data/user/0/com.critical.vexaemulator/files/rootfs`.
  - Fixed broken loader symlink by adding `/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2`.
