#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
from pathlib import Path

ADDR_RE = re.compile(r"(host_pc|host_lr|guest_rip)=0x([0-9a-fA-F]+)")
MAP_RE = re.compile(r"^([0-9a-f]+)-([0-9a-f]+)\s+\S+\s+([0-9a-f]+)\s+\S+\s+\d+\s*(.*)$")


def _is_placeholder_path(p: str | None) -> bool:
    if not p:
        return True
    return p.startswith("/path/to/")


def _latest_existing(paths):
    existing = [p for p in paths if p.exists()]
    if not existing:
        return None
    existing.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return existing[0]


def discover_latest_log(logs_root: Path):
    return _latest_existing(logs_root.rglob("artifacts/fex_stderr.log"))


def discover_latest_maps(logs_root: Path):
    candidates = list(logs_root.rglob("artifacts/runtime_worker.maps"))
    candidates += list(logs_root.rglob("artifacts/runtime_worker.*.maps"))
    return _latest_existing(candidates)


def load_maps(path: str):
    out = []
    try:
        text = Path(path).read_text(errors="ignore")
    except OSError as e:
        print(f"[symbolize] warning: failed to read maps file '{path}': {e}")
        return out

    for line in text.splitlines():
        m = MAP_RE.match(line)
        if not m:
            continue
        start = int(m.group(1), 16)
        end = int(m.group(2), 16)
        off = int(m.group(3), 16)
        mod = m.group(4).strip()
        out.append((start, end, off, mod))
    return out


def find_map(maps, addr: int):
    for start, end, off, mod in maps:
        if start <= addr < end:
            return start, end, off, mod
    return None


def find_sym(sym_root: str, mod_path: str):
    if not sym_root:
        return None
    base = os.path.basename(mod_path)
    p = Path(sym_root) / base
    if p.exists():
        return p
    hits = list(Path(sym_root).rglob(base))
    return hits[0] if hits else None


def addr2line(sym: Path, rel: int):
    for tool in ("llvm-addr2line", "addr2line"):
        try:
            r = subprocess.run(
                [tool, "-Cfpe", str(sym), hex(rel)],
                capture_output=True,
                text=True,
                check=False,
            )
            if r.returncode == 0 and r.stdout.strip():
                return r.stdout.strip()
        except FileNotFoundError:
            pass
    return "<no addr2line tool>"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log")
    ap.add_argument("--host-maps")
    ap.add_argument("--host-syms")
    ap.add_argument("--guest-maps")
    ap.add_argument("--guest-syms")
    ap.add_argument("--logs-root", default="~/vexa/logs")
    args = ap.parse_args()

    logs_root = Path(args.logs_root).expanduser()

    log_path = Path(args.log).expanduser() if args.log else None
    if _is_placeholder_path(args.log) or (log_path and not log_path.exists()):
        auto_log = discover_latest_log(logs_root)
        if not auto_log:
            raise FileNotFoundError(f"Could not find fex_stderr.log under {logs_root}")
        log_path = auto_log
        print(f"[symbolize] auto log: {log_path}")
    elif log_path is None:
        auto_log = discover_latest_log(logs_root)
        if not auto_log:
            raise FileNotFoundError(f"Could not find fex_stderr.log under {logs_root}")
        log_path = auto_log
        print(f"[symbolize] auto log: {log_path}")

    host_maps_path = Path(args.host_maps).expanduser() if args.host_maps else None
    if _is_placeholder_path(args.host_maps) or (host_maps_path and not host_maps_path.exists()):
        auto_maps = discover_latest_maps(logs_root)
        if auto_maps:
            host_maps_path = auto_maps
            print(f"[symbolize] auto host maps: {host_maps_path}")
        else:
            host_maps_path = None
            print(
                f"[symbolize] warning: could not find runtime_worker.maps under {logs_root}. "
                f"Run pull_runtime_logs.sh --live for symbolization-ready output."
            )
    elif host_maps_path is None:
        auto_maps = discover_latest_maps(logs_root)
        if auto_maps:
            host_maps_path = auto_maps
            print(f"[symbolize] auto host maps: {host_maps_path}")
        else:
            print(
                f"[symbolize] warning: could not find runtime_worker.maps under {logs_root}. "
                f"Run pull_runtime_logs.sh --live for symbolization-ready output."
            )

    host_syms = str(Path(args.host_syms).expanduser()) if args.host_syms else ""
    if host_syms and not Path(host_syms).exists():
        print(f"[symbolize] warning: host symbol path does not exist: {host_syms}")
        host_syms = ""

    guest_maps_path = str(Path(args.guest_maps).expanduser()) if args.guest_maps else None
    guest_syms = str(Path(args.guest_syms).expanduser()) if args.guest_syms else ""
    if guest_maps_path and not Path(guest_maps_path).exists():
        print(f"[symbolize] warning: guest maps path does not exist: {guest_maps_path}")
        guest_maps_path = None
    if guest_syms and not Path(guest_syms).exists():
        print(f"[symbolize] warning: guest symbol path does not exist: {guest_syms}")
        guest_syms = ""

    host_maps = load_maps(str(host_maps_path)) if host_maps_path else []
    guest_maps = load_maps(guest_maps_path) if guest_maps_path else None

    seen = set()
    for line in Path(log_path).read_text(errors="ignore").splitlines():
        m = ADDR_RE.search(line)
        if not m:
            continue

        kind = m.group(1)
        addr = int(m.group(2), 16)
        key = (kind, addr)
        if key in seen:
            continue
        seen.add(key)

        use_guest = kind.startswith("guest_")
        maps = guest_maps if use_guest and guest_maps else host_maps
        sym_root = guest_syms if use_guest and guest_syms else host_syms

        mm = find_map(maps, addr) if maps else None
        if not mm:
            print(f"{kind} {hex(addr)} -> <no map>")
            continue

        start, _end, off, mod = mm
        rel = addr - start + off
        sym = find_sym(sym_root, mod) if mod else None

        if not sym:
            print(f"{kind} {hex(addr)} -> {mod} + {hex(rel)}")
            continue

        print(f"{kind} {hex(addr)} -> {mod} + {hex(rel)}")
        print("  " + addr2line(sym, rel))


if __name__ == "__main__":
    main()
