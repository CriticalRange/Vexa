"""Capture live FEX registers and stack bytes without pausing the guest."""
import json, pathlib, socket, subprocess, time

out = pathlib.Path('logs/diagnosis-20261013/deadcells')
out.mkdir(parents=True, exist_ok=True)
def adb(*args):
    return subprocess.run(['adb', *args], capture_output=True).stdout
old = adb('shell', 'pidof', 'com.critical.vexaemulator:runtime_worker').strip()
adb('shell', 'am', 'force-stop', 'com.critical.vexaemulator')
adb('shell', 'am', 'start', '-n', 'com.critical.vexaemulator/.GameActivity', '--es', 'programId', 'deadcells')
deadline = time.monotonic() + 15
while time.monotonic() < deadline:
    pid = adb('shell', 'pidof', 'com.critical.vexaemulator:runtime_worker').strip()
    if pid and pid != old:
        break
    time.sleep(.05)
else:
    raise SystemExit('No fresh worker')
adb('forward', 'tcp:5099', 'localabstract:fexgdb' + pid.decode())
for attempt in range(30):
    try:
        sock = socket.create_connection(('localhost', 5099), timeout=.5)
        sock.settimeout(.5)
        sock.sendall(b'$qVexaThreads#d0')
        data = sock.recv(65536)
        if b'$' in data:
            break
        sock.close()
    except OSError:
        pass
    time.sleep(.03)
else:
    raise SystemExit('No GDB response')
buf = data
def reply():
    global buf
    while True:
        start = buf.find(b'$'); end = buf.find(b'#', start)
        if start >= 0 and end >= 0 and len(buf) >= end + 3:
            result = buf[start+1:end].decode()
            buf = buf[end+3:]
            sock.sendall(b'+')
            return result
        chunk = sock.recv(65536)
        if not chunk:
            raise EOFError()
        buf += chunk
def query(value):
    raw = value.encode()
    sock.sendall(b'$' + raw + b'#' + f'{sum(raw)%256:02x}'.encode())
    return reply()
out.joinpath('live.maps').write_bytes(adb('exec-out', 'run-as', 'com.critical.vexaemulator', 'cat', '/proc/'+pid.decode()+'/maps'))
with out.joinpath('stack-samples.jsonl').open('w') as dest:
    first = reply()
    for n in range(120):
        try:
            threads = first if n == 0 else query('qVexaThreads')
            fields = threads.split(',')[0].split(':')
            rsp = int(fields[2], 16)
            stack = query(f'm{rsp:x},200')
            dest.write(json.dumps({'n': n, 'threads': threads, 'stack': stack})+'\n')
            dest.flush()
            time.sleep(.03)
        except (OSError, EOFError, ValueError, IndexError) as err:
            print('Capture ended:', type(err).__name__, 'samples', n)
            break
sock.close()
print('Captured', pid.decode(), 'to', out)
