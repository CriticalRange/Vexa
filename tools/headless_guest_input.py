"""Send a click/key to a headless guest's X11 stream, selecting the live ADB device."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("--serial")
parser.add_argument("--click", nargs=2, type=int)
parser.add_argument("--key", type=lambda s: int(s, 0))
a = parser.parse_args()
if bool(a.click) == (a.key is not None): parser.error("choose --click x y or --key keysym")
serial = a.serial
if not serial:
    devices = [line.split()[0] for line in subprocess.check_output(["adb", "devices"]).decode().splitlines()[1:] if len(line.split()) > 1 and line.split()[1] == "device"]
    if len(devices) != 1: parser.error("need exactly one live device or --serial")
    serial = devices[0]
adb = ["adb", "-s", serial]
def send(records):
    with tempfile.TemporaryDirectory(prefix="vexa-headless-") as directory:
        local = Path(directory) / "input.bin"
        local.write_bytes(b"".join(struct.pack("<iiii", *r) for r in records))
        subprocess.run(adb + ["push", str(local), "/data/local/tmp/vexa-headless-input.bin"], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(adb + ["shell", "run-as com.critical.vexaemulator sh -c 'cat /data/local/tmp/vexa-headless-input.bin >> files/rootfs/tmp/vexa-pointer'"], check=True)
if a.click:
    x, y = a.click
    send([(6, x, y, 0), (4, x, y, 0)])
    time.sleep(0.15)
    send([(5, x, y, 256)])
else:
    send([(2, a.key, 0, 0)])
    time.sleep(0.15)
    send([(3, a.key, 0, 0)])
print("sent to", serial)
