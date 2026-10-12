"""Compare the X11 symbols a Unity player resolves at runtime with what the guest
fake libraries actually export.

Unity dlopen()s libX11.so.6 / libXrandr.so.2 / libXinerama.so.1 and dlsym()s a fixed
set of names. Anything missing resolves to NULL, which silently sends the engine's
display code down a fallback path - that is how Screen.resolutions ends up empty
while the desktop size still looks right.
"""
import re
import struct
import subprocess
import sys

READELF = r"C:\Users\zeytu\AppData\Local\Android\Sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-readelf.exe"
FAKE_DIR = r"C:\Users\zeytu\Documents\vexa\guest-tests\x11-fake"
FAKE_LIBS = ["libX11.so.6", "libXrandr.so.2", "libXinerama.so.1", "libXcursor.so.1",
             "libXfixes.so.3", "libXi.so.6", "libXrender.so.1", "libXext.so.6"]

PREFIXES = ("XQuery", "XGet", "XSet", "XCreate", "XDestroy", "XInit", "XDefault",
            "XDisplay", "XRRScreen", "XRRGet", "XRRFree", "XRRQuery", "XRRConfig",
            "XRRSet", "XRRFree", "XRRUpdate", "Xinerama", "Xkb", "_X", "XFree",
            "XOpen", "XClose", "XIntern", "XNext", "XPending", "XFlush", "XSync",
            "XMapWindow", "XUnmapWindow", "XStoreName", "XSelectInput", "XResize",
            "XMove", "XConfigure", "XTranslate", "XSendEvent", "XChangeProperty",
            "XDeleteProperty", "XAlloc", "XSetErrorHandler", "XSetIOErrorHandler",
            "XConnectionNumber", "XScreenCount", "XRoot", "XBell", "XWarpPointer",
            "XSetInputFocus", "XDefineCursor", "XUndefineCursor", "XCreatePixmap",
            "XFreePixmap", "XCreatePixmapCursor", "XFreeCursor", "XGrabServer",
            "XUngrabServer", "XReconfigureWMWindow", "XIconifyWindow")


def wanted_symbols(path):
    """Symbol-like strings Unity keeps so it can dlsym them later."""
    data = open(path, "rb").read()
    out = set()
    for m in re.finditer(rb"[ -~]{4,60}", data):
        s = m.group().decode("latin1")
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{3,55}", s):
            continue
        if s.startswith(PREFIXES):
            out.add(s)
    return out


def exported_symbols(lib):
    try:
        r = subprocess.run([READELF, "--dyn-symbols", "--wide", lib],
                           capture_output=True, text=True, timeout=60)
    except Exception:
        return set()
    out = set()
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[3] == "FUNC" and parts[6] != "UND":
            out.add(parts[7].split("@")[0])
    return out


def main():
    unity = sys.argv[1]
    want = wanted_symbols(unity)
    have = set()
    per_lib = {}
    for lib in FAKE_LIBS:
        syms = exported_symbols(FAKE_DIR + "\\" + lib)
        if syms:
            per_lib[lib] = syms
            have |= syms

    print(f"Unity player: {unity}")
    print(f"candidate X symbols in the binary : {len(want)}")
    print(f"symbols exported by the fakes    : {len(have)}")
    print()

    missing = sorted(want - have)
    print(f"=== Unity aranan ama fakes'te OLMAMIS ({len(missing)}) ===")
    for s in missing:
        print("   ", s)
    print()
    print("=== Unity aranan ve fakes'te OLAN ===")
    for s in sorted(want & have):
        libs = [l for l, v in per_lib.items() if s in v]
        print(f"    {s}   [{', '.join(libs)}]")


if __name__ == "__main__":
    main()
