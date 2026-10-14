#!/usr/bin/env python3
# Poll FEX gdbserver; for each live thread grab rip/rsp + read 0x100 stack bytes.
import socket, sys, time, struct

HOST, PORT = "localhost", 5099

def csum(d): return f"{sum(d.encode()) % 256:02x}"

s = socket.create_connection((HOST, PORT), timeout=10)
s.settimeout(10)
buf = b""

def read_packet():
    global buf
    while True:
        i = buf.find(b"$")
        if i == -1:
            c = s.recv(65536)
            if not c: raise EOFError
            buf = c
            continue
        j = buf.find(b"#", i)
        if j != -1 and len(buf) >= j + 3:
            pkt = buf[i + 1:j].decode(errors="replace")
            buf = buf[j + 3:]
            s.sendall(b"+")
            return pkt
        c = s.recv(65536)
        if not c: raise EOFError
        buf += c

def send(p):
    s.sendall(f"${p}#{csum(p)}".encode())
    return read_packet()

def read_mem(addr, ln):
    r = send(f"m{addr:x},{ln:x}")
    if r.startswith("E"): return None
    try: return bytes.fromhex(r)
    except ValueError: return None

send("QStartNoAckMode")

last = []
while True:
    try:
        r = send("qVexaThreads")
    except Exception as e:
        print(f"DISCONNECT {e}")
        break
    if r.startswith("E"):
        break
    snap = []
    for t in r.split(","):
        f = t.split(":")
        # observed format: tid:rip:rsp:?:?:?:?  -> take fields heuristically
        try:
            tid = int(f[0], 16)
            rip = int(f[1], 16)
            rsp = int(f[2], 16)
        except Exception:
            continue
        snap.append((tid, rip, rsp, t))
    last = snap
    time.sleep(0.1)

print(f"=== last snapshot: {len(last)} threads ===")
for tid, rip, rsp, raw in last:
    print(f"tid={tid:x} rip={rip:x} rsp={rsp:x}")
for tid, rip, rsp, raw in last:
    print(f"--- tid {tid:x} stack @ {rsp:x} ---")
    try:
        # reconnect impossible; read via fresh conn not possible after crash.
        pass
    except Exception:
        pass
