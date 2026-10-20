#!/usr/bin/env python3
"""Dump the device's Vulkan capabilities (libvexavkcaps.so over adb) and check them against a DXVK profile."""

import argparse
import datetime
import json
import os
from pathlib import Path
import subprocess
import sys

VEXA_ROOT = Path(__file__).resolve().parents[1]
REQUIREMENTS = VEXA_ROOT / "tools/dxvk_requirements.json"
LOCAL_BINARY = VEXA_ROOT / "app/src/main/jniLibs/arm64-v8a/libvexavkcaps.so"
REMOTE_TMP = "/data/local/tmp/vexavkcaps"
# Git Bash would otherwise rewrite /data/... arguments into Windows paths.
ADB_ENV = {**os.environ, "MSYS_NO_PATHCONV": "1"}


def adb(*args, check=True):
    result = subprocess.run(["adb", *args], capture_output=True, text=True, env=ADB_ENV,
                            encoding="utf-8", errors="replace")
    if check and result.returncode:
        raise SystemExit(f"adb {' '.join(args)} failed: {result.stderr.strip()}")
    return result


def installed_binary(package):
    """The APK's extracted copy, so the dump matches the installed build."""
    out = adb("shell", "pm", "path", package, check=False).stdout.strip().splitlines()
    apk = next((line[len("package:"):] for line in out if line.endswith("/base.apk")), None)
    if not apk:
        return None
    path = apk.rsplit("/", 1)[0] + "/lib/arm64/libvexavkcaps.so"
    return path if adb("shell", "test", "-x", path, check=False).returncode == 0 else None


def dump_device(package, binary):
    remote = None if binary else installed_binary(package)
    if not remote:
        local = binary or LOCAL_BINARY
        if not local.is_file():
            raise SystemExit(f"No installed libvexavkcaps.so and no local binary at {local}; build the app first")
        adb("push", str(local), REMOTE_TMP)
        adb("shell", "chmod", "755", REMOTE_TMP)
        remote = REMOTE_TMP
    result = adb("shell", remote)
    caps = json.loads(result.stdout)
    props = adb("shell", "getprop", check=False).stdout
    wanted = ("ro.product.model", "ro.product.manufacturer", "ro.soc.model", "ro.hardware", "ro.build.fingerprint")
    pairs = (line.strip().strip("[]").split("]: [", 1) for line in props.splitlines() if "]: [" in line)
    caps["androidProps"] = {pair[0]: pair[1] for pair in pairs if pair[0] in wanted}
    caps["binary"] = remote
    return caps


def version_tuple(text):
    return tuple(int(part) for part in text.split(".")[:3])


def lookup(device, dotted):
    """Resolves "group.field" in device features, or a top-level properties object."""
    group, field = dotted.split(".", 1)
    source = device.get("features", {}).get(group)
    if source is None:
        source = device.get(group)
    return None if source is None else source.get(field)


def check(device, profile):
    rows = []
    def add(kind, name, ok, detail=""):
        if kind == "optional":
            state = "YES" if ok else "NO"
        else:
            state = "PASS" if ok else ("UNKNOWN" if ok is None else "FAIL")
        rows.append((kind, name, state, detail))

    have = version_tuple(device["apiVersion"])
    add("required", f"apiVersion >= {profile['minApiVersion']}", have >= version_tuple(profile["minApiVersion"]),
        device["apiVersion"])
    extensions = device.get("extensions", {})
    for req in profile["requiredExtensions"]:
        found = [name for name in req["any"] if name in extensions]
        via_core = bool(req.get("orFeature")) and lookup(device, req["orFeature"]) is True
        add("required", " | ".join(req["any"]), bool(found) or via_core,
            ", ".join(found) or (f"core {req['orFeature']}" if via_core else "not exposed"))
    for dotted in profile["requiredFeatures"]:
        value = lookup(device, dotted)
        add("required", dotted, value if value is not None else False,
            "not queried (group unavailable)" if value is None else "")
    for limit, minimum in profile["minLimits"].items():
        value = device.get("limits", {}).get(limit)
        add("required", f"{limit} >= {minimum}", None if value is None else value >= minimum, str(value))
    for name in profile["optionalExtensions"]:
        found = [alias for alias in name.split("|") if alias in extensions]
        add("optional", name, bool(found), ", ".join(found))
    for dotted in profile["optionalFeatures"]:
        add("optional", dotted, lookup(device, dotted))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--input", type=Path, help="Use a saved capability JSON instead of the device")
    source.add_argument("--binary", type=Path, help="Push this vexavkcaps build instead of the installed one")
    parser.add_argument("--package", default="com.critical.vexaemulator")
    parser.add_argument("--profile", default="dxvk-3.x")
    parser.add_argument("--save", type=Path, help="Where to store the device dump (default logs/vkcaps/...)")
    args = parser.parse_args()

    requirements = json.loads(REQUIREMENTS.read_text(encoding="utf-8"))
    profile = requirements["profiles"].get(args.profile)
    if profile is None:
        parser.error(f"Unknown profile {args.profile}; known: {', '.join(requirements['profiles'])}")

    if args.input:
        caps = json.loads(args.input.read_text(encoding="utf-8"))
    else:
        caps = dump_device(args.package, args.binary)
        model = caps.get("androidProps", {}).get("ro.product.model", "device").replace(" ", "_")
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        save = args.save or VEXA_ROOT / "logs/vkcaps" / f"{model}-{stamp}.json"
        save.parent.mkdir(parents=True, exist_ok=True)
        save.write_text(json.dumps(caps, indent=1) + "\n", encoding="utf-8")
        print(f"saved {save}")

    failed = 0
    for device in caps["devices"]:
        print(f"\n{device['name']}  Vulkan {device['apiVersion']}  driver {device.get('driverName', '?')} "
              f"{device.get('driverInfo', '')}  (instance {caps['instanceVersion']})")
        print(f"profile {args.profile}  ({requirements['source']}, retrieved {requirements['retrieved']})")
        for kind, name, state, detail in check(device, profile):
            failed += kind == "required" and state != "PASS"
            print(f"  {state:7} {kind:8} {name}" + (f"  [{detail}]" if detail else ""))
        for note in profile.get("notes", []):
            print(f"  note: {note}")
    print(f"\n{'DXVK_REQUIREMENTS_MET' if not failed else f'DXVK_REQUIREMENTS_MISSING {failed}'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
