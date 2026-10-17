#!/usr/bin/env python3
"""Check/apply VEXA's optional FEX hooks without resetting any source changes."""

import argparse
from pathlib import Path
import subprocess
import sys


def run(root, *args):
    return subprocess.run(
        ["git", "-C", str(root), *args], text=True, capture_output=True,
        encoding="utf-8", errors="replace",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fex-root", required=True, type=Path)
    parser.add_argument("--check", action="store_true", help="Validate only; do not modify FEX")
    parser.add_argument("--vulkan-x11", action="store_true", help="Include the optional real X11 Vulkan WSI thunk hooks")
    args = parser.parse_args()
    root = args.fex_root.resolve()
    directory = Path(__file__).resolve().parents[1] / "scripts/patches"
    patches = [directory / name for name in ("fex-vexa-exec.patch", "fex-vexa-x11-sockets.patch", "fex-vexa-fd-ownership.patch", "fex-vexa-proton-fs.patch", "fex-vexa-rootfs-open.patch", "fex-vexa-android-fsync.patch", "fex-vexa-android-pid.patch", "fex-vexa-android-mmap.patch", "fex-vexa-android-seccomp.patch")]
    if args.vulkan_x11:
        patches.append(directory / "fex-vexa-vulkan-x11.patch")
    for patch in patches:
        if not patch.is_file():
            parser.error(f"Patch is missing: {patch}")
    if not root.is_dir():
        parser.error(f"FEX source directory is missing: {root}")
    try:
        pending = []
        for patch in patches:
            # Inspect every patch before modifying anything. Mixed states across
            # independent patches are fine; partial application within one is not.
            reverse = run(root, "apply", "--reverse", "--check", str(patch))
            if reverse.returncode == 0:
                print(f"ALREADY_APPLIED {patch.name}")
                continue
            forward = run(root, "apply", "--check", str(patch))
            if forward.returncode:
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
