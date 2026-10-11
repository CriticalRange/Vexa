#!/usr/bin/env python3
# Minimal gdb RSP client to dump guest thread states from FEX's gdbserver.
import socket, sys, re

HOST, PORT = "localhost", 5099

def csum(data: str) -> str:
    return f"{sum(data.encode()) % 256:02x}"

s = socket.create_connection((HOST, PORT), timeout=15)
s.settimeout(15)
buf = b""

def read_packet():
    global buf
    while True:
        # ack stray bytes
        i = buf.find(b"$")
        if i == -1:
            buf = b""
            buf += s.recv(65536)
            continue
        j = buf.find(b"#", i)
        if j != -1 and len(buf) >= j + 3:
            pkt = buf[i + 1:j].decode(errors="replace")
            buf = buf[j + 3:]
            s.sendall(b"+")
            return pkt
        buf += s.recv(65536)

def send(pkt: str) -> str:
    s.sendall(f"${pkt}#{csum(pkt)}".encode())
    r = read_packet()
    return r

def read_mem(addr, length):
    r = send(f"m{addr:x},{length:x}")
    if r.startswith("E"):
        return None
    try:
        return bytes.fromhex(r)
    except ValueError:
        return None

# handshake-ish
print("qSupported ->", send("qSupported:swbreak+;hwbreak+;vContSupported+")[:200])
send("QStartNoAckMode")

# threads
r = send("qfThreadInfo")
tids = []
if r.startswith("m"):
    tids = [t for t in r[1:].split(",") if t]
send("qsThreadInfo")
print(f"threads: {len(tids)}")

# library map
libs = []
off = 0
while True:
    r = send(f"qXfer:libraries:read::{off:x},4000")
    if not r or r[0] == "E":
        break
    chunk = r[1:]
    done = r[0] == "l"
    m = re.findall(r'<library[^>]*name="([^"]*)"[^>]*>\s*<segment[^>]*address="0x([0-9a-fA-F]+)"', chunk)
    if not m:  # segments may come as separate elements
        pass
    # simpler: collect names and segment addresses separately
    for name, addr in re.findall(r'name="([^"]+)"[^>]*>(.*?)</library>', chunk, re.S):
        segs = re.findall(r'address="0x([0-9a-fA-F]+)"', addr)
        for sa in segs:
            libs.append((int(sa, 16), name))
    if done:
        break
    off += len(chunk.encode())

print(f"libs: {len(libs)} mapped segments")

def sym(rip):
    best = None
    for base, name in libs:
        if rip >= base and (best is None or base > best[0]):
            best = (base, name)
    if best:
        return f"{best[1]}+0x{rip - best[0]:x}"
    return "?"

for tid in tids:
    try:
        name_hex = send(f"qThreadExtraInfo,{tid}")
        try:
            name = bytes.fromhex(name_hex).decode(errors="replace")
        except Exception:
            name = name_hex
        send(f"Hg{tid}")
        g = send("g")
        rip = None
        if not g.startswith("E") and len(g) >= (128 + 8) * 2:
            rip = int.from_bytes(bytes.fromhex(g[128 * 2:136 * 2]), "little")
        rsp = None
        if not g.startswith("E") and len(g) >= 64 * 2:
            rsp = int.from_bytes(bytes.fromhex(g[56 * 2:64 * 2]), "little")  # gregs[7]=rsp
        where = sym(rip) if rip else "n/a"
        print(f"tid={tid} name={name!r} rip={('#' + hex(rip)) if rip else 'n/a'} -> {where}")
        # crude unwinding: read a couple of stack qwords
        if rsp:
            stk = read_mem(rsp, 128)
            if stk:
                rets = []
                for i in range(0, len(stk), 8):
                    q = int.from_bytes(stk[i:i + 8], "little")
                    if 0x400000 <= q <= 0x8000000000:
                        rets.append(f"{q:#x}<{sym(q)}>")
                if rets:
                    print("   stack:", " | ".join(rets[:6]))
    except Exception as e:
        print(f"tid={tid} error: {e}")

s.close()
