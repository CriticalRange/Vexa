"""Turn the gated Unity display-refresh hook into a permanent fix.

The hook used to be a diagnostic that only ran when /tmp/vexa-unity-refresh
existed in the guest. It is now the actual fix, so run it unconditionally and
keep the Unity build signature as the only guard. Also add an opt-in dump of the
display manager so the resolution list can be inspected from the outside.
"""
import io, sys

PATH = r"C:\Users\zeytu\Documents\vexa\guest-tests\x11-fake\libX11_fake.c"

NEW = '''  /* Unity caches its display list the first time it queries it, which happens
     before SDL video is up. Screen.resolutions then stays empty and any game
     that indexes Screen.resolutions[0] while restoring its settings dies with
     IndexOutOfRangeException before it ever loads a scene. Nudge the engine
     into re-enumerating once the window exists. The instruction bytes are
     verified so a different Unity build skips this instead of jumping into the
     wrong function. */
  static int refresh_done;
  if (!refresh_done) {
    Dl_info info;
    if (dladdr(__builtin_return_address(0), &info) && info.dli_fbase &&
        info.dli_fname && strstr(info.dli_fname, "UnityPlayer.so")) {
      unsigned char* base = info.dli_fbase;
      const unsigned char signature[] = {0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x38};
      if (memcmp(base + 0x15ea030, signature, sizeof(signature)) == 0) {
        g_diag_unity_base = base;
        refresh_done = 1;
        char* mgr = (char*)((void* (*)(void))(base + 0xa600f0))();
        if (mgr) {
          fprintf(stderr, "[VEXA][UNITY] display before refresh %dx%d entries=%lu\\n", *(int*)(mgr + 0xb4),
                  *(int*)(mgr + 0xb8), *(unsigned long*)(mgr + 0xd8));
          *(int*)(mgr + 0x110) = -1;
          ((void (*)(void*, int))(base + 0x15ea030))(mgr, 0);
          fprintf(stderr, "[VEXA][UNITY] display after  refresh %dx%d entries=%lu\\n", *(int*)(mgr + 0xb4),
                  *(int*)(mgr + 0xb8), *(unsigned long*)(mgr + 0xd8));
          if (getenv("VEXA_DUMP_UNITY_DISPLAY")) {
            for (int off = 0x40; off < 0x120; off += 8) {
              fprintf(stderr, "[VEXA][UNITY]   mgr+0x%02x = 0x%lx\\n", off, *(unsigned long*)(mgr + off));
            }
          }
        }
      }
    }
  }
'''

with io.open(PATH, "r", encoding="utf-8", newline="") as f:
    lines = f.readlines()

start = end = None
for idx, line in enumerate(lines):
    if "Temporary diagnostic, enabled only by a file" in line:
        start = idx - 1  # the 'static int refresh_done;' line above it
    if start is not None and "screen after native" in line:
        end = idx + 1
        break

if start is None or end is None:
    sys.exit("markers not found: start=%r end=%r" % (start, end))

print("replacing lines %d..%d" % (start + 1, end))
for i in range(start, end):
    print("   -", lines[i].rstrip())

out = lines[:start] + [NEW] + lines[end:]
with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.writelines(out)
print("ok")