#!/usr/bin/env python3
"""Exercise real shared X11 state/PutImage-independent GDI-like drawing.

Run with guest Linux Python inside FEX. The real server's headless compositor
captures the window; no Android activity/window is needed for this client.
"""
import ctypes as C
import os
import time


def main():
    x = C.CDLL("libX11.so.6")
    ptr = C.c_void_p
    ulong = C.c_ulong
    uint = C.c_uint

    def bind(name, result, *args):
        function = getattr(x, name)
        function.restype = result
        function.argtypes = args
        return function

    open_display = bind("XOpenDisplay", ptr, C.c_char_p)
    root_window = bind("XDefaultRootWindow", ulong, ptr)
    create = bind("XCreateSimpleWindow", ulong, ptr, ulong, C.c_int, C.c_int,
                  uint, uint, uint, ulong, ulong)
    name = bind("XStoreName", C.c_int, ptr, ulong, C.c_char_p)
    select = bind("XSelectInput", C.c_int, ptr, ulong, C.c_long)
    map_window = bind("XMapWindow", C.c_int, ptr, ulong)
    gc_create = bind("XCreateGC", ptr, ptr, ulong, ulong, ptr)
    foreground = bind("XSetForeground", C.c_int, ptr, ptr, ulong)
    fill = bind("XFillRectangle", C.c_int, ptr, ulong, ptr, C.c_int, C.c_int, uint, uint)
    draw = bind("XDrawString", C.c_int, ptr, ulong, ptr, C.c_int, C.c_int, C.c_char_p, C.c_int)
    pending = bind("XPending", C.c_int, ptr)
    next_event = bind("XNextEvent", C.c_int, ptr, ptr)
    flush = bind("XFlush", C.c_int, ptr)
    destroy = bind("XDestroyWindow", C.c_int, ptr, ulong)
    free_gc = bind("XFreeGC", C.c_int, ptr, ptr)
    close = bind("XCloseDisplay", C.c_int, ptr)
    server_vendor = bind("XServerVendor", C.c_char_p, ptr)
    display = open_display(None)
    if not display:
        raise RuntimeError(f"real XOpenDisplay failed: DISPLAY={os.environ.get('DISPLAY')}")
    window = 0
    gc = None
    try:
        window = create(display, root_window(display), 40, 40, 640, 360, 0, 0, 0x122033)
        name(display, window, b"VEXA real X11 under FEX")
        select(display, window, (1 << 15) | (1 << 17))  # Exposure + StructureNotify
        gc = gc_create(display, window, 0, None)
        map_window(display, window)
        flush(display)
        event = (C.c_long * 24)()
        exposes = 0
        deadline = time.monotonic() + int(os.environ.get("VEXA_PROBE_SECONDS", "15"))
        while time.monotonic() < deadline:
            while pending(display):
                next_event(display, C.byref(event))
                if C.cast(C.byref(event), C.POINTER(C.c_int))[0] == 12:
                    exposes += 1
            foreground(display, gc, 0x122033)
            fill(display, window, gc, 0, 0, 640, 360)
            for index, color in enumerate((0xFF503C, 0x32D296, 0x378BFF)):
                foreground(display, gc, color)
                fill(display, window, gc, 40 + index * 190, 65, 160, 180)
            foreground(display, gc, 0xFFFFFF)
            text = b"VEXA REAL X11 - FEX client / Android server"
            draw(display, window, gc, 40, 300, text, len(text))
            flush(display)
            time.sleep(0.05)
        if not exposes:
            raise RuntimeError("No real X11 Expose event received")
        print(f"X11_REAL_PASS vendor={server_vendor(display).decode()} window={window} exposes={exposes}", flush=True)
    finally:
        if gc:
            free_gc(display, gc)
        if window:
            destroy(display, window)
        close(display)


if __name__ == "__main__":
    main()
