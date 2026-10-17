#!/usr/bin/env python3
"""Run upstream Proton unchanged and retain the session until wineserver exits.

Executed by guest Linux Python under FEX, never by Android's host interpreter.
The native supervisor owns cancellation of the complete tagged process tree.
"""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    if len(sys.argv) < 3:
        raise RuntimeError("usage: proton_session.py PROTON_DIRECTORY GAME [ARG ...]")
    proton = Path(sys.argv[1])
    game = sys.argv[2]
    data = Path(os.environ["STEAM_COMPAT_DATA_PATH"])
    # This is deliberately outside pfx: Proton owns and may migrate that tree.
    data.mkdir(parents=True, exist_ok=True)
    with (data / ".vexa-session.lock").open("a+") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("VEXA: this game's Proton session is already running", file=sys.stderr)
            return 75
        result_path = Path(os.environ["VEXA_ARTIFACTS"]) / "proton-session.json"

        def record(state, **fields):
            value = {"state": state, "pid": os.getpid(), "time": time.time(), **fields}
            temporary = result_path.with_suffix(".json.tmp")
            temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
            os.replace(temporary, result_path)

        # No shell splitting: spaces, empty arguments and metacharacters survive.
        record("starting")
        started = time.monotonic()
        rc = subprocess.call([sys.executable, str(proton / "proton"), "run", game, *sys.argv[3:]])
        record("waiting-for-wineserver", proton_exit=rc)
        # waitforexitandrun waits BEFORE launching. Explicitly waiting AFTER the
        # official launcher exits keeps forked game/helper processes in-session.
        wait_env = os.environ.copy()
        wait_env["WINEPREFIX"] = str(data / "pfx")
        wait_env["LD_LIBRARY_PATH"] = ":".join(filter(None, (
            str(proton / "files/lib64"), str(proton / "files/lib"),
            wait_env.get("LD_LIBRARY_PATH", ""),
        )))
        waited = subprocess.call([str(proton / "files/bin/wineserver"), "-w"], env=wait_env)
        exit_code = rc if rc else waited
        if exit_code < 0:
            exit_code = 128 - exit_code
        record("exited", proton_exit=rc, wineserver_exit=waited,
               exit_code=exit_code, seconds=round(time.monotonic() - started, 3))
        return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"VEXA Proton session: {type(error).__name__}: {error}", file=sys.stderr)
        sys.exit(70)
