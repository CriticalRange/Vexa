/*
 * Stub libX11.so.6 for Android x86_64 guest runtime.
 *
 * libNoesis.so links against libX11.so.6 for clipboard probing.
 * Android has no X server, so these symbols provide safe no-op behavior.
 */

#include <stddef.h>
#include <stdint.h>

typedef void* Display;
typedef void Visual;
typedef unsigned long Window;
typedef unsigned long Atom;
typedef unsigned long XID;

typedef struct {
    int type;
    unsigned long serial;
    int pad[46];
} XEvent;

typedef struct {
    Visual* visual;
    unsigned long visualid;
    int screen;
    int depth;
    int c_class;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

typedef struct {
    int type;
    Display display;
    XID resourceid;
    unsigned long serial;
    unsigned char error_code;
    unsigned char request_code;
    unsigned char minor_code;
} XErrorEvent;

typedef int (*XErrorHandler)(Display, XErrorEvent*);

Display XOpenDisplay(const char* display_name) {
    (void)display_name;
    return NULL;
}

Window XDefaultRootWindow(Display dpy) {
    (void)dpy;
    return 0;
}

Window XCreateSimpleWindow(Display dpy,
                           Window parent,
                           int x,
                           int y,
                           unsigned int w,
                           unsigned int h,
                           unsigned int border_width,
                           unsigned long border,
                           unsigned long background) {
    (void)dpy;
    (void)parent;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)border_width;
    (void)border;
    (void)background;
    return 0;
}

Atom XInternAtom(Display dpy, const char* name, int only_if_exists) {
    (void)dpy;
    (void)name;
    (void)only_if_exists;
    return 0;
}

int XChangeProperty(Display dpy,
                    Window w,
                    Atom property,
                    Atom type,
                    int format,
                    int mode,
                    const unsigned char* data,
                    int nelements) {
    (void)dpy;
    (void)w;
    (void)property;
    (void)type;
    (void)format;
    (void)mode;
    (void)data;
    (void)nelements;
    return 0;
}

int XDeleteProperty(Display dpy, Window w, Atom property) {
    (void)dpy;
    (void)w;
    (void)property;
    return 0;
}

int XGetWindowProperty(Display dpy,
                       Window w,
                       Atom property,
                       long long_offset,
                       long long_length,
                       int delete_prop,
                       Atom req_type,
                       Atom* actual_type_return,
                       int* actual_format_return,
                       unsigned long* nitems_return,
                       unsigned long* bytes_after_return,
                       unsigned char** prop_return) {
    (void)dpy;
    (void)w;
    (void)property;
    (void)long_offset;
    (void)long_length;
    (void)delete_prop;
    (void)req_type;

    if (actual_type_return) *actual_type_return = 0;
    if (actual_format_return) *actual_format_return = 0;
    if (nitems_return) *nitems_return = 0;
    if (bytes_after_return) *bytes_after_return = 0;
    if (prop_return) *prop_return = NULL;

    return 1;
}

int XConvertSelection(Display dpy,
                      Atom selection,
                      Atom target,
                      Atom property,
                      Window requestor,
                      unsigned long time) {
    (void)dpy;
    (void)selection;
    (void)target;
    (void)property;
    (void)requestor;
    (void)time;
    return 0;
}

int XSelectInput(Display dpy, Window w, long event_mask) {
    (void)dpy;
    (void)w;
    (void)event_mask;
    return 0;
}

int XNextEvent(Display dpy, XEvent* event_return) {
    (void)dpy;
    if (event_return) {
        unsigned char* p = (unsigned char*)event_return;
        int i;
        for (i = 0; i < 192; ++i) {
            p[i] = 0;
        }
    }
    return 0;
}

int XSync(Display dpy, int discard) {
    (void)dpy;
    (void)discard;
    return 0;
}

char* XDisplayString(Display dpy) {
    (void)dpy;
    static char display_name[] = ":0";
    return display_name;
}

XVisualInfo* XGetVisualInfo(Display dpy,
                            long vinfo_mask,
                            XVisualInfo* vinfo_template,
                            int* nitems_return) {
    (void)dpy;
    (void)vinfo_mask;
    (void)vinfo_template;

    static XVisualInfo visual_info = {
            .visual = (Visual*)1,
            .visualid = 1,
            .screen = 0,
            .depth = 24,
            .c_class = 4,
            .red_mask = 0x00ff0000UL,
            .green_mask = 0x0000ff00UL,
            .blue_mask = 0x000000ffUL,
            .colormap_size = 256,
            .bits_per_rgb = 8,
    };

    if (nitems_return) *nitems_return = 1;
    return &visual_info;
}

XErrorHandler XSetErrorHandler(XErrorHandler handler) {
    static XErrorHandler current_handler = NULL;
    XErrorHandler previous = current_handler;
    current_handler = handler;
    return previous;
}

int XFree(void* data) {
    (void)data;
    return 0;
}
