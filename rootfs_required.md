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
- [x] `/usr/lib/x86_64-linux-gnu/libmvec.so.1` (required by `libNoesis.so`)
- [x] `/usr/lib/x86_64-linux-gnu/libdbus-1.so.3` (with `libdbus-1.so.3.32.4`)
- [x] `/etc/ld.so.cache`
- [x] `/etc/ld.so.conf`
- [x] `/etc/ld.so.conf.d`
- [x] `/usr/lib/x86_64-linux-gnu/libssl.so.3` (with `libssl.so` symlink)
- [x] `/usr/lib/x86_64-linux-gnu/libcrypto.so.3` (with `libcrypto.so` symlink)
- [x] `/usr/lib/i386-linux-gnu/libssl.so.3` (with `libssl.so` symlink)
- [x] `/usr/lib/i386-linux-gnu/libcrypto.so.3` (with `libcrypto.so` symlink)

## X11 Compatibility Stub Libraries (Noesis dependency chain)

These are intentionally no-op compatibility stubs for Android guest runtime.
They are required so `libNoesis.so` can resolve X11/XCB SONAMEs without a full X11 stack.

- [x] `/usr/lib/x86_64-linux-gnu/libX11.so.6` (stub)
- [x] `/usr/lib/x86_64-linux-gnu/libxcb.so.1` (stub)
- [x] `/usr/lib/x86_64-linux-gnu/libXau.so.6` (stub)
- [x] `/usr/lib/x86_64-linux-gnu/libXdmcp.so.6` (stub)

Build + overlay workflow:

```bash
./scripts/libx11-stub/build.sh
./scripts/push_fex_rootfs.sh
```

Manual verify:

```bash
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libX11.so.6
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libxcb.so.1
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libXau.so.6
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libXdmcp.so.6
```

## Hytale Auth/JWKS Network Requirements (minimal push set)

These are required for guest-side DNS + HTTPS verification used during
JWKS fetch from `https://sessions.hytale.com`.

- [ ] `/usr/lib/x86_64-linux-gnu/libnss_dns.so.2`
- [ ] `/usr/lib/x86_64-linux-gnu/libnss_files.so.2`
- [ ] `/lib/x86_64-linux-gnu/libresolv.so.2`
- [ ] `/etc/nsswitch.conf` (must include `hosts: files dns`)
- [ ] `/etc/resolv.conf` (working nameservers)
- [ ] `/etc/hosts` (localhost mappings)
- [ ] `/etc/ssl/certs/ca-certificates.crt`

### Minimal adb push commands (only required files)

```bash
PKG=com.critical.vexaemulator
SRC=/home/critical/vexa/.fex-emu/RootFS

# Stage only required files to temp.
adb shell mkdir -p /data/local/tmp/vexa-minroot/etc/ssl/certs
adb shell mkdir -p /data/local/tmp/vexa-minroot/usr/lib/x86_64-linux-gnu
adb shell mkdir -p /data/local/tmp/vexa-minroot/lib/x86_64-linux-gnu

adb push "$SRC/usr/lib/x86_64-linux-gnu/libnss_dns.so.2" /data/local/tmp/vexa-minroot/usr/lib/x86_64-linux-gnu/
adb push "$SRC/usr/lib/x86_64-linux-gnu/libnss_files.so.2" /data/local/tmp/vexa-minroot/usr/lib/x86_64-linux-gnu/
adb push "$SRC/lib/x86_64-linux-gnu/libresolv.so.2" /data/local/tmp/vexa-minroot/lib/x86_64-linux-gnu/
adb push "$SRC/etc/nsswitch.conf" /data/local/tmp/vexa-minroot/etc/
adb push "$SRC/etc/ssl/certs/ca-certificates.crt" /data/local/tmp/vexa-minroot/etc/ssl/certs/

# resolv.conf + hosts (explicit values, independent of host rootfs contents)
adb shell sh -c 'cat > /data/local/tmp/vexa-minroot/etc/resolv.conf <<EOF
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF'
adb shell sh -c 'cat > /data/local/tmp/vexa-minroot/etc/hosts <<EOF
127.0.0.1 localhost
::1 localhost ip6-localhost ip6-loopback
EOF'

# Install into app-private rootfs.
adb shell run-as "$PKG" mkdir -p files/rootfs/etc/ssl/certs
adb shell run-as "$PKG" mkdir -p files/rootfs/usr/lib/x86_64-linux-gnu
adb shell run-as "$PKG" mkdir -p files/rootfs/lib/x86_64-linux-gnu
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/usr/lib/x86_64-linux-gnu/libnss_dns.so.2 files/rootfs/usr/lib/x86_64-linux-gnu/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/usr/lib/x86_64-linux-gnu/libnss_files.so.2 files/rootfs/usr/lib/x86_64-linux-gnu/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/lib/x86_64-linux-gnu/libresolv.so.2 files/rootfs/lib/x86_64-linux-gnu/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/etc/nsswitch.conf files/rootfs/etc/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/etc/resolv.conf files/rootfs/etc/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/etc/hosts files/rootfs/etc/
adb shell run-as "$PKG" cp /data/local/tmp/vexa-minroot/etc/ssl/certs/ca-certificates.crt files/rootfs/etc/ssl/certs/

# Cleanup temp.
adb shell rm -rf /data/local/tmp/vexa-minroot
```

### Verify after push

```bash
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/etc/resolv.conf
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/etc/nsswitch.conf
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libnss_dns.so.2
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/usr/lib/x86_64-linux-gnu/libnss_files.so.2
adb shell run-as com.critical.vexaemulator ls -la files/rootfs/etc/ssl/certs/ca-certificates.crt
```

## Observed Potential Runtime Libraries (from binary strings)

- [x] `libssl.so` family (`libssl.so`, `libssl.so.1.0.0`, `libssl.so.1.0.2`, `libssl.so.1.1`,
  `libssl.so.10`, `libssl.so.3`) (`.so.3` + unversioned symlink added)
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
- 2026-03-17: Auth failure root cause confirmed in client logs:
  `Failed to fetch JWKS ... Resource temporarily unavailable (sessions.hytale.com:443)`.
  Device rootfs lacked NSS/DNS/TLS files required for hostname resolution and HTTPS trust.
- 2026-03-18: Added `libmvec.so.1` into app rootfs:
  `/data/user/0/com.critical.vexaemulator/files/rootfs/usr/lib/x86_64-linux-gnu/libmvec.so.1`
  to satisfy `libNoesis.so` dynamic dependency resolution.
- 2026-03-26: Added `libdbus-1.so.3` (and backing
  `libdbus-1.so.3.32.4`) into app rootfs:
  `/data/user/0/com.critical.vexaemulator/files/rootfs/usr/lib/x86_64-linux-gnu/`
  after `ALSOFT` logged:
  `Failed to load libdbus-1.so.3`.
- 2026-03-27: Added X11 compatibility stubs (`libX11.so.6`, `libxcb.so.1`,
  `libXau.so.6`, `libXdmcp.so.6`) via `scripts/libx11-stub/build.sh` and
  integrated overlay in `scripts/push_fex_rootfs.sh`.
  Reason: `libNoesis.so` load path requires X11 SONAME resolution on Android.
