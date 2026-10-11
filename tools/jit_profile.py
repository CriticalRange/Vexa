#!/usr/bin/env python3
"""Aggregate a simpleperf report of a FEX guest into guest functions.

Inputs (pulled from the device):
  report  : `simpleperf report --sort symbol -n --percent-limit 0.005` output
  perfmap : /data/local/tmp/perf-<pid>.map written by FEX (BlockJITNaming)
  maps    : /proc/<pid>/maps of the runtime_worker

JIT symbols look like `JIT_0x<guest_rip>_0x<host>`; entries simpleperf could not
name (`perf-N.map[+host]`, `unknown[+host]`) are resolved through the perf map.
Guest RIPs are mapped to `lib+fileoff`, then to the enclosing ELF symbol via
llvm-nm when the library is available locally (--libdir).
"""
import argparse, bisect, os, re, subprocess, sys
from collections import defaultdict

ap = argparse.ArgumentParser()
ap.add_argument("report"); ap.add_argument("perfmap"); ap.add_argument("maps")
ap.add_argument("--libdir", action="append", default=[])
ap.add_argument("--nm", default="")
ap.add_argument("--top", type=int, default=40)
a = ap.parse_args()

# perf map: host -> guest rip
hs, he, hg = [], [], []
for line in open(a.perfmap, errors="replace"):
    p = line.split()
    if len(p) < 3:
        continue
    m = re.match(r"JIT_0x([0-9a-f]+)_", p[2])
    if not m:
        continue
    hs.append(int(p[0], 16)); he.append(int(p[0], 16) + int(p[1], 16)); hg.append(int(m.group(1), 16))
order = sorted(range(len(hs)), key=hs.__getitem__)
hs = [hs[i] for i in order]; he = [he[i] for i in order]; hg = [hg[i] for i in order]

def host_to_guest(h):
    i = bisect.bisect_right(hs, h) - 1
    while i >= 0 and i >= len(hs) - 1 - 10**9:
        if hs[i] <= h < he[i]:
            return hg[i]
        if h - hs[i] > 0x10000:
            break
        i -= 1
    return None

# guest maps: rip -> (lib, fileoff)
segs = []
for line in open(a.maps, errors="replace"):
    p = line.split()
    if len(p) < 6 or "x" not in p[1]:
        continue
    lo, hi = (int(x, 16) for x in p[0].split("-"))
    segs.append((lo, hi, int(p[2], 16), p[5]))
segs.sort(); ss = [s[0] for s in segs]

def where(rip):
    i = bisect.bisect_right(ss, rip) - 1
    if i >= 0 and segs[i][0] <= rip < segs[i][1]:
        lo, hi, off, path = segs[i]
        return path, rip - lo + off
    return None, rip

# optional ELF symbolization (file offset == vaddr for these libs is assumed
# only after correcting with PT_LOAD below)
symcache = {}
def load_syms(path):
    base = os.path.basename(path)
    if base in symcache:
        return symcache[base]
    local = next((os.path.join(d, base) for d in a.libdir if os.path.exists(os.path.join(d, base))), None)
    res = None
    if local and a.nm:
        out = subprocess.run([a.nm, "-n", "--defined-only", "-C", local], capture_output=True, text=True).stdout
        if not out.strip():
            out = subprocess.run([a.nm, "-n", "-D", "--defined-only", "-C", local], capture_output=True, text=True).stdout
        addrs, names = [], []
        for l in out.splitlines():
            p = l.split(None, 2)
            if len(p) == 3 and p[1] in "tTwW":
                addrs.append(int(p[0], 16)); names.append(p[2])
        res = (addrs, names) if addrs else None
    symcache[base] = res
    return res

def fname(path, off):
    syms = load_syms(path) if path else None
    if not syms:
        return None
    addrs, names = syms
    i = bisect.bisect_right(addrs, off) - 1
    return f"{names[i][:90]}+{off - addrs[i]:#x}" if i >= 0 else None

tot = 0
by_lib = defaultdict(int); by_fn = defaultdict(int); unresolved = 0
for line in open(a.report, errors="replace"):
    m = re.match(r"\s*([\d.]+)%\s+(\d+)\s+(.*)$", line)
    if not m:
        continue
    n = int(m.group(2)); sym = m.group(3).strip(); tot += n
    g = None
    mm = re.match(r"JIT_0x([0-9a-f]+)_", sym)
    if mm:
        g = int(mm.group(1), 16)
    else:
        mh = re.search(r"\[\+([0-9a-f]+)\]", sym)
        if mh:
            g = host_to_guest(int(mh.group(1), 16))
    if g is None:
        by_lib["<host> " + sym[:60]] += n
        continue
    path, off = where(g)
    lib = os.path.basename(path) if path else "<guest-anon>"
    by_lib[lib] += n
    fn = fname(path, off) or f"{off:#x}"
    by_fn[f"{lib}!{fn}" if path else f"rip={g:#x}"] += n

print(f"samples={tot}")
print("\n== by library ==")
for k, v in sorted(by_lib.items(), key=lambda x: -x[1])[:20]:
    print(f"{100*v/tot:6.2f}%  {k}")
print("\n== by guest function ==")
for k, v in sorted(by_fn.items(), key=lambda x: -x[1])[:a.top]:
    print(f"{100*v/tot:6.2f}%  {k}")
