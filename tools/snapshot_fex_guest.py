"""Read a running guest's register/stack/code snapshot over forwarded FEX RSP."""
import json, pathlib, socket, sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
sock = socket.create_connection(('localhost', 5099), timeout=3)
sock.settimeout(3)
buf = b''
def query(command):
    global buf
    raw = command.encode()
    sock.sendall(b'$'+raw+b'#'+f'{sum(raw)%256:02x}'.encode())
    while True:
        start = buf.find(b'$'); end = buf.find(b'#', start)
        if start >= 0 and end >= 0 and len(buf) >= end+3:
            answer = buf[start+1:end].decode()
            buf = buf[end+3:]
            sock.sendall(b'+')
            return answer
        chunk = sock.recv(65536)
        if not chunk: raise EOFError()
        buf += chunk
threads = query('qVexaThreads')
for request in sys.argv[2:]:
    address, length = request.split(':')
    response = query(f'm{int(address,16):x},{int(length,16):x}')
    out.joinpath('memory-'+address+'.txt').write_text(response)
out.joinpath('threads.json').write_text(json.dumps(threads))
for entry in threads.split(','):
    fields = entry.split(':')
    tid, rip, rsp = [int(x, 16) for x in fields[:3]]
    for name, address in [('code', rip), ('stack', rsp)]:
        response = query(f'm{address:x},400')
        if not response.startswith('E'):
            out.joinpath(f'{tid}-{name}-{address:x}.bin').write_bytes(bytes.fromhex(response))
sock.close()
print('Captured', len(threads.split(',')), 'threads to', out)
