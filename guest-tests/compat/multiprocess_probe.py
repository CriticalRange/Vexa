#!/usr/bin/env python3
"""Check FEX's actual exec/re-exec boundary without Proton dependencies."""
import subprocess
import sys

print("PARENT_PASS", flush=True)
result = subprocess.run([sys.executable, "-c", "print('CHILD_PASS', flush=True)"])
print(f"CHILD_EXIT={result.returncode}", flush=True)
sys.exit(result.returncode if result.returncode >= 0 else 128 - result.returncode)
