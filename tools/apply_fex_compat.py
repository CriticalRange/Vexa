#!/usr/bin/env python3
"""Check/apply VEXA's FEX hooks without resetting any source changes."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

VEXA_ROOT = Path(__file__).resolve().parents[1]
PATCH_DIR = VEXA_ROOT / "scripts/patches"
LOCK_FILE = VEXA_ROOT / "tools/fex.lock.json"
PATCHES = [PATCH_DIR / name for name in ("fex-vexa-exec.patch", "fex-vexa-x11-sockets.patch", "fex-vexa-fd-ownership.patch", "fex-vexa-proton-fs.patch", "fex-vexa-rootfs-open.patch", "fex-vexa-android-fsync.patch", "fex-vexa-android-pid.patch", "fex-vexa-android-mmap.patch", "fex-vexa-android-seccomp.patch")]


def run(root, *args):
    return subprocess.run(
        ["git", "-C", str(root), *args], text=True, capture_output=True,
        encoding="utf-8", errors="replace",
    )


def pinned_commit():
    return json.loads(LOCK_FILE.read_text(encoding="utf-8"))["commit"]


def pin_state(root):
    """Returns (head, pinned, contains_pin) for the FEX checkout."""
    head = run(root, "rev-parse", "HEAD").stdout.strip()
    pinned = pinned_commit()
    contains = run(root, "merge-base", "--is-ancestor", pinned, "HEAD").returncode == 0
    return head, pinned, contains


def patch_state(root, patch):
    """Returns ("applied" | "applicable" | "conflict", reverse, forward)."""
    reverse = run(root, "apply", "--reverse", "--check", str(patch))
    if reverse.returncode == 0:
        return "applied", reverse, None
    forward = run(root, "apply", "--check", str(patch))
    return ("applicable" if forward.returncode == 0 else "conflict"), reverse, forward


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fex-root", required=True, type=Path)
    parser.add_argument("--check", action="store_true", help="Validate only; do not modify FEX")
    args = parser.parse_args()
    root = args.fex_root.resolve()
    for patch in PATCHES:
        if not patch.is_file():
            parser.error(f"Patch is missing: {patch}")
    if not root.is_dir():
        parser.error(f"FEX source directory is missing: {root}")
    try:
        head, pinned, contains = pin_state(root)
        if not contains:
            print(f"FEX checkout {head or '?'} does not contain pinned commit {pinned} ({LOCK_FILE.name})", file=sys.stderr)
            return 1
        print(f"FEX_PIN_OK {head}" + ("" if head == pinned else f" (descends from {pinned[:12]})"))
        pending = []
        for patch in PATCHES:
            # Inspect every patch before modifying anything. Mixed states across
            # independent patches are fine; partial application within one is not.
            state, reverse, forward = patch_state(root, patch)
            if state == "applied":
                print(f"ALREADY_APPLIED {patch.name}")
                continue
            if state == "conflict":
                print(f"FEX compatibility patch is neither applied nor applicable: {patch.name}", file=sys.stderr)
                print("Forward check:\n" + forward.stderr, file=sys.stderr, end="")
                print("Reverse check:\n" + reverse.stderr, file=sys.stderr, end="")
                return forward.returncode
            pending.append(str(patch))
        if not pending:
            print("FEX_COMPAT_ALREADY_APPLIED")
            return 0
        # Git checks/applies the complete pending set in one operation.
        forward = run(root, "apply", "--check", *pending)
        if forward.returncode:
            print("FEX compatibility patch is neither applied nor applicable.", file=sys.stderr)
            print("Forward check:\n" + forward.stderr, file=sys.stderr, end="")
            return forward.returncode
        if args.check:
            print("FEX_COMPAT_APPLICABLE")
            return 0
        applied = run(root, "apply", *pending)
        if applied.returncode:
            print(applied.stderr, file=sys.stderr, end="")
            return applied.returncode
        print("FEX_COMPAT_APPLIED")
        return 0
    except OSError as error:
        print(f"Cannot execute git: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
