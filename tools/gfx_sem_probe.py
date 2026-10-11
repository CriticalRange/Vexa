#!/usr/bin/env python3
"""Pause-free probe of Unity's GfxDeviceClient "present" semaphore (Megabonk).

Static analysis (UnityPlayer.so, Megabonk build, file offsets == vaddrs):
  GfxDeviceClient (X):
    +0x2540  GfxDeviceWorker*  W
    +0x2551  threaded flag
    +0x2558  ThreadedStreamBuffer* (command queue)
    +0x26a4  m_PresentFrameID (u32, ++ per PresentFrame)
    +0x26a8  m_PresentPending (u8)
  e2f6d0 GfxDeviceClient::PresentFrame : signal = !PresentPending; PresentPending=1;
         FrameID++; queue cmd 0x2776 {u8 signal, u32 frameId}; submit
  e2f320 GfxDeviceClient::BeginFrame -> e2f3d0 -> if PresentPending:
         15eae60(W, 0)  = semaphore wait  (W+0x68 count, W+0x28 futex tokens)
  Worker cmd 0x2776 handler (15edcc0 -> 15f19cf): realDevice->PresentFrame (vt 0x698),
         if signal: lock xadd +1 W+0x68 (old<0 -> W+0x28 += n, futex_wake) ;
         W+0x254 = frameId
  15eaf40 GfxDeviceWorker run loop: one initial post on W+0x68 if caps(vt 0xb48)&4

Main thread blocked in 15eae60 has rbx == W+0x28 (lea/add before the futex loop).

Usage:  python gfx_sem_probe.py maps.txt   (maps of runtime_worker; adb forward tcp:5099 first)
"""
import socket, sys, re, struct

HOST, PORT = "localhost", 5099
s = socket.create_connection((HOST, PORT), timeout=15)
s.settimeout(15)
buf = b""

def csum(d): return f"{sum(d.encode()) % 256:02x}"

def send(p):
    global buf
    s.sendall(f"${p}#{csum(p)}".encode())
    while True:
        i = buf.find(b"$")
        if i != -1:
            j = buf.find(b"#", i)
            if j != -1 and len(buf) >= j + 3:
                r = buf[i + 1:j]; buf = buf[j + 3:]
                s.sendall(b"+")
                return r.decode(errors="replace")
        c = s.recv(65536)
        if not c: return "<EOF>"
        buf += c

def mem(addr, n):
    r = send(f"m{addr:x},{n:x}")
    if not r or r[0] == "E" or r == "<EOF>": return None
    try: return bytes.fromhex(r)
    except ValueError: return None

def u32(a):
    b = mem(a, 4); return None if b is None else struct.unpack("<i", b)[0]
def u64(a):
    b = mem(a, 8); return None if b is None else struct.unpack("<Q", b)[0]
def u8(a):
    b = mem(a, 1); return None if b is None else b[0]

maps = []
if len(sys.argv) > 1:
    rawm = open(sys.argv[1], "rb").read()
    txt = rawm.decode("utf-16") if rawm[:2] in (b"\xff\xfe", b"\xfe\xff") else rawm.decode("utf-8", "replace")
    for ln in txt.splitlines():
        m = re.match(r"([0-9a-f]+)-([0-9a-f]+)\s+(\S+)\s+([0-9a-f]+)\s+\S+\s+\d+\s*(.*)", ln)
        if m:
            maps.append((int(m[1], 16), int(m[2], 16), m[3], int(m[4], 16), m[5].strip()))

def sym(a):
    for lo, hi, perm, off, name in maps:
        if lo <= a < hi and "x" in perm:
            return f"{name.rsplit('/', 1)[-1]}+0x{off + a - lo:x}"
    return "?"

def up_off(a):
    for lo, hi, perm, off, name in maps:
        if lo <= a < hi and name.endswith("UnityPlayer.so"):
            return off + a - lo
    return None

r = send("qVexaThreads")
threads = []
for ent in r.split(","):
    p = ent.split(":")
    if len(p) >= 3:
        threads.append([int(x, 16) for x in p])
print(f"{len(threads)} threads")

for t in threads:
    tid, rip, rsp = t[:3]
    rbx = t[3] if len(t) > 3 else None
    stk = mem(rsp, 0x200) or b""
    chain = []
    for i in range(0, len(stk), 8):
        q = struct.unpack_from("<Q", stk, i)[0]
        o = up_off(q)
        if o is not None: chain.append(f"UP+{o:x}")
    print(f"tid={tid} rip={rip:#x} <{sym(rip)}> rbx={rbx:#x}" if rbx is not None else f"tid={tid} rip={rip:#x}")
    if chain: print("   UP chain:", " ".join(chain[:8]))
    in_wait = any(c in ("UP+15eae91", "UP+e2f45b", "UP+e2f432") for c in chain)
    if in_wait and rbx:
        W = rbx - 0x28
        print(f"   >>> present-semaphore waiter. W={W:#x}")
        print(f"       W+0x68 count   = {u32(W + 0x68)}")
        print(f"       W+0x28 tokens  = {u32(W + 0x28)}")
        print(f"       W+0x250        = {u32(W + 0x250)}")
        print(f"       W+0x254 lastPresentedFrameId = {u32(W + 0x254)}")
        print(f"       W+0x230 realDevice = {u64(W + 0x230)!s}")
        # X (GfxDeviceClient): scan the waiter's stack for a pointer whose +0x2540 == W
        for i in range(0, len(stk), 8):
            X = struct.unpack_from("<Q", stk, i)[0]
            if X > 0x10000 and u64(X + 0x2540) == W:
                print(f"       X(GfxDeviceClient)={X:#x} FrameID(+0x26a4)={u32(X + 0x26a4)} "
                      f"PresentPending(+0x26a8)={u8(X + 0x26a8)} threaded(+0x2551)={u8(X + 0x2551)}")
                q = u64(X + 0x2558)
                if q:
                    print(f"       cmdQ={q:#x} wpos(+0x108)={u32(q + 0x108)} +0x80={u32(q + 0x80)}")
                break
s.close()
