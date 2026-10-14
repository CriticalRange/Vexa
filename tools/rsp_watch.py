#!/usr/bin/env python3
# Poll FEX gdbserver thread snapshots as fast as possible until disconnect.
import socket, sys, time

HOST, PORT = "localhost", 5099

def csum(d): return f"{sum(d.encode()) % 256:02x}"

s = socket.create_connection((HOST, PORT), timeout=10)
s.settimeout(10)
buf = b""
nack = False

def read_packet():
    global buf
    while True:
        i = buf.find(b"$")
        if i == -1:
            chunk = s.recv(65536)
            if not chunk:
                raise EOFError
            buf = chunk
            continue
        j = buf.find(b"#", i)
        if j != -1 and len(buf) >= j + 3:
            pkt = buf[i + 1:j].decode(errors="replace")
            buf = buf[j + 3:]
            if not nack:
                s.sendall(b"+")
            return pkt
        chunk = s.recv(65536)
        if not chunk:
            raise EOFError
        buf += chunk

def send(pkt):
    s.sendall(f"${pkt}#{csum(pkt)}".encode())
    return read_packet()

send("QStartNoAckMode")
nack = True

t0 = time.time()
while True:
    try:
        r = send("qVexaThreads")
    except (EOFError, ConnectionError, socket.error) as e:
        print(f"[{time.time()-t0:7.3f}] DISCONNECTED {e}")
        break
    el = time.time() - t0
    # r = tid:rip:rsp[,tid:rip:rsp...]
    threads = r.split(",") if r and not r.startswith("E") else [r]
    print(f"[{el:7.3f}] {len(threads)} threads")
    for t in threads:
        print(f"    {t}")
    sys.stdout.flush()
    time.sleep(0.15)
