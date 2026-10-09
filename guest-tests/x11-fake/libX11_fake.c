/* Guest libX11.so.6 for programs that only need a display and a window id.
   Field stores use the Ubuntu Xlibint.h layout this file is compiled against. */
#define _GNU_SOURCE
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

static Visual g_visual;
static Depth g_depth;
static Screen g_screen;
static char g_vendor[] = "VEXA";
static char g_display_name[] = ":0";
static int g_ready;

static void vexa_init_screen(Display* dpy) {
  if (g_ready) {
    return;
  }
  memset(&g_visual, 0, sizeof(g_visual));
  g_visual.visualid = 1;
  g_visual.class = TrueColor;
  g_visual.red_mask = 0x00ff0000;
  g_visual.green_mask = 0x0000ff00;
  g_visual.blue_mask = 0x000000ff;
  g_visual.bits_per_rgb = 8;
  g_visual.map_entries = 256;

  memset(&g_depth, 0, sizeof(g_depth));
  g_depth.depth = 24;
  g_depth.nvisuals = 1;
  g_depth.visuals = &g_visual;

  memset(&g_screen, 0, sizeof(g_screen));
  g_screen.display = dpy;
  g_screen.root = 1;
  g_screen.width = 1280;
  g_screen.height = 720;
  g_screen.mwidth = 300;
  g_screen.mheight = 170;
  g_screen.ndepths = 1;
  g_screen.depths = &g_depth;
  g_screen.root_depth = 24;
  g_screen.root_visual = &g_visual;
  g_screen.white_pixel = 0x00ffffff;
  g_screen.black_pixel = 0;
  g_screen.cmap = 1;
  g_ready = 1;
}

Display* XOpenDisplay(const char* name) {
  Display* dpy = (Display*)calloc(1, sizeof(Display));
  if (!dpy) {
    return NULL;
  }
  vexa_init_screen(dpy);
  dpy->fd = -1;
  dpy->default_screen = 0;
  dpy->nscreens = 1;
  dpy->screens = &g_screen;
  dpy->vendor = g_vendor;
  dpy->display_name = name && name[0] ? (char*)name : g_display_name;
  dpy->min_keycode = 8;
  dpy->max_keycode = 255;
  g_screen.display = dpy;
  return dpy;
}

int XCloseDisplay(Display* dpy) {
  free(dpy);
  return 0;
}

Window XCreateWindow(Display* dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border,
                     int depth, unsigned int klass, Visual* visual, unsigned long valuemask, XSetWindowAttributes* attrs) {
  (void)dpy;
  (void)parent;
  (void)x;
  (void)y;
  (void)border;
  (void)depth;
  (void)klass;
  (void)visual;
  (void)valuemask;
  (void)attrs;
  if (width > 1 && height > 1) {
    g_screen.width = (int)width;
    g_screen.height = (int)height;
  }
  return 1;
}

Window XCreateSimpleWindow(Display* dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border,
                           unsigned long background, unsigned long border_color) {
  (void)background;
  (void)border_color;
  return XCreateWindow(dpy, parent, x, y, width, height, border, 24, InputOutput, &g_visual, 0, NULL);
}

Colormap XCreateColormap(Display* dpy, Window window, Visual* visual, int alloc) {
  (void)dpy;
  (void)window;
  (void)visual;
  (void)alloc;
  return 1;
}

int XMapWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XMapRaised(Display* dpy, Window window) {
  return XMapWindow(dpy, window);
}

int XUnmapWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XDestroyWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XFree(void* data) {
  free(data);
  return 1;
}

static void vexa_expose(XEvent* event) {
  if (!event) {
    return;
  }
  memset(event, 0, sizeof(*event));
  event->type = Expose;
  event->xexpose.window = 1;
  event->xexpose.width = g_screen.width;
  event->xexpose.height = g_screen.height;
  event->xexpose.count = 0;
}

int XNextEvent(Display* dpy, XEvent* event) {
  (void)dpy;
  vexa_expose(event);
  return 0;
}

int XPeekEvent(Display* dpy, XEvent* event) {
  return XNextEvent(dpy, event);
}

int XWindowEvent(Display* dpy, Window window, long mask, XEvent* event) {
  (void)window;
  (void)mask;
  return XNextEvent(dpy, event);
}

int XMaskEvent(Display* dpy, long mask, XEvent* event) {
  (void)mask;
  return XNextEvent(dpy, event);
}

Bool XCheckMaskEvent(Display* dpy, long mask, XEvent* event) {
  (void)dpy;
  (void)mask;
  (void)event;
  return False;
}

Bool XCheckWindowEvent(Display* dpy, Window window, long mask, XEvent* event) {
  (void)dpy;
  (void)window;
  (void)mask;
  (void)event;
  return False;
}

Bool XCheckTypedEvent(Display* dpy, int type, XEvent* event) {
  (void)dpy;
  (void)type;
  (void)event;
  return False;
}

int XPending(Display* dpy) {
  struct timespec delay = {0, 8000000};
  (void)dpy;
  nanosleep(&delay, NULL);
  return 0;
}

int XEventsQueued(Display* dpy, int mode) {
  (void)dpy;
  (void)mode;
  return 0;
}

int XFlush(Display* dpy) {
  (void)dpy;
  return 1;
}

int XSync(Display* dpy, Bool discard) {
  (void)dpy;
  (void)discard;
  return 1;
}

Atom XInternAtom(Display* dpy, const char* name, Bool only_if_exists) {
  (void)dpy;
  (void)name;
  (void)only_if_exists;
  return 1;
}

int XChangeProperty(Display* dpy, Window window, Atom property, Atom type, int format, int mode, const unsigned char* data, int nelements) {
  (void)dpy;
  (void)window;
  (void)property;
  (void)type;
  (void)format;
  (void)mode;
  (void)data;
  (void)nelements;
  return 1;
}

int XDeleteProperty(Display* dpy, Window window, Atom property) {
  (void)dpy;
  (void)window;
  (void)property;
  return 1;
}

int XStoreName(Display* dpy, Window window, const char* name) {
  (void)dpy;
  (void)window;
  (void)name;
  return 1;
}

int XSetStandardProperties(Display* dpy, Window window, const char* name, const char* icon, Pixmap pixmap, char** argv, int argc, XSizeHints* hints) {
  (void)dpy;
  (void)window;
  (void)name;
  (void)icon;
  (void)pixmap;
  (void)argv;
  (void)argc;
  (void)hints;
  return 1;
}

void XSetWMProperties(Display* dpy, Window window, XTextProperty* name, XTextProperty* icon, char** argv, int argc, XSizeHints* normal,
                      XWMHints* wm, XClassHint* klass) {
  (void)dpy;
  (void)window;
  (void)name;
  (void)icon;
  (void)argv;
  (void)argc;
  (void)normal;
  (void)wm;
  (void)klass;
}

void Xutf8SetWMProperties(Display* dpy, Window window, const char* name, const char* icon, char** argv, int argc, XSizeHints* normal,
                          XWMHints* wm, XClassHint* klass) {
  (void)dpy;
  (void)window;
  (void)name;
  (void)icon;
  (void)argv;
  (void)argc;
  (void)normal;
  (void)wm;
  (void)klass;
}

void XmbSetWMProperties(Display* dpy, Window window, const char* name, const char* icon, char** argv, int argc, XSizeHints* normal, XWMHints* wm,
                        XClassHint* klass) {
  Xutf8SetWMProperties(dpy, window, name, icon, argv, argc, normal, wm, klass);
}

int XSetNormalHints(Display* dpy, Window window, XSizeHints* hints) {
  (void)dpy;
  (void)window;
  (void)hints;
  return 1;
}

void XSetWMNormalHints(Display* dpy, Window window, XSizeHints* hints) {
  (void)dpy;
  (void)window;
  (void)hints;
}

Status XSetWMProtocols(Display* dpy, Window window, Atom* protocols, int count) {
  (void)dpy;
  (void)window;
  (void)protocols;
  (void)count;
  return 1;
}

int XLookupString(XKeyEvent* event, char* buffer, int bytes, KeySym* keysym, XComposeStatus* status) {
  (void)event;
  (void)buffer;
  (void)bytes;
  (void)status;
  if (keysym) {
    *keysym = 0;
  }
  return 0;
}

KeySym XLookupKeysym(XKeyEvent* event, int index) {
  (void)event;
  (void)index;
  return 0;
}

int XParseGeometry(const char* string, int* x, int* y, unsigned int* width, unsigned int* height) {
  (void)string;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  return 0;
}

XVisualInfo* XGetVisualInfo(Display* dpy, long mask, XVisualInfo* template, int* count) {
  XVisualInfo* info = (XVisualInfo*)calloc(1, sizeof(XVisualInfo));
  (void)dpy;
  (void)mask;
  (void)template;
  if (!info) {
    if (count) {
      *count = 0;
    }
    return NULL;
  }
  info->visual = &g_visual;
  info->visualid = g_visual.visualid;
  info->screen = 0;
  info->depth = 24;
  info->class = TrueColor;
  info->red_mask = g_visual.red_mask;
  info->green_mask = g_visual.green_mask;
  info->blue_mask = g_visual.blue_mask;
  info->colormap_size = 256;
  info->bits_per_rgb = 8;
  if (count) {
    *count = 1;
  }
  return info;
}

Status XMatchVisualInfo(Display* dpy, int screen, int depth, int klass, XVisualInfo* info) {
  (void)dpy;
  (void)screen;
  (void)depth;
  (void)klass;
  if (!info) {
    return 0;
  }
  memset(info, 0, sizeof(*info));
  info->visual = &g_visual;
  info->visualid = g_visual.visualid;
  info->screen = 0;
  info->depth = 24;
  info->class = TrueColor;
  info->red_mask = g_visual.red_mask;
  info->green_mask = g_visual.green_mask;
  info->blue_mask = g_visual.blue_mask;
  info->colormap_size = 256;
  info->bits_per_rgb = 8;
  return 1;
}

Status XGetWindowAttributes(Display* dpy, Window window, XWindowAttributes* attrs) {
  (void)dpy;
  (void)window;
  if (!attrs) {
    return 0;
  }
  memset(attrs, 0, sizeof(*attrs));
  attrs->x = 0;
  attrs->y = 0;
  attrs->width = g_screen.width;
  attrs->height = g_screen.height;
  attrs->depth = 24;
  attrs->visual = &g_visual;
  attrs->class = TrueColor;
  attrs->map_state = IsViewable;
  attrs->screen = &g_screen;
  return 1;
}

Status XGetGeometry(Display* dpy, Drawable drawable, Window* root, int* x, int* y, unsigned int* width, unsigned int* height,
                    unsigned int* border, unsigned int* depth) {
  (void)dpy;
  (void)drawable;
  if (root) {
    *root = 1;
  }
  if (x) {
    *x = 0;
  }
  if (y) {
    *y = 0;
  }
  if (width) {
    *width = (unsigned int)g_screen.width;
  }
  if (height) {
    *height = (unsigned int)g_screen.height;
  }
  if (border) {
    *border = 0;
  }
  if (depth) {
    *depth = 24;
  }
  return 1;
}

int XResizeWindow(Display* dpy, Window window, unsigned int width, unsigned int height) {
  (void)dpy;
  (void)window;
  if (width > 1 && height > 1) {
    g_screen.width = (int)width;
    g_screen.height = (int)height;
  }
  return 1;
}

int XMoveWindow(Display* dpy, Window window, int x, int y) {
  (void)dpy;
  (void)window;
  (void)x;
  (void)y;
  return 1;
}

int XSelectInput(Display* dpy, Window window, long mask) {
  (void)dpy;
  (void)window;
  (void)mask;
  return 1;
}

int XClearWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

Status XInitThreads(void) {
  return 1;
}

XErrorHandler XSetErrorHandler(XErrorHandler handler) {
  (void)handler;
  return NULL;
}

XIOErrorHandler XSetIOErrorHandler(XIOErrorHandler handler) {
  (void)handler;
  return NULL;
}

int XIfEvent(Display* dpy, XEvent* event, Bool (*predicate)(Display*, XEvent*, XPointer), XPointer arg) {
  (void)dpy;
  (void)predicate;
  (void)arg;
  vexa_expose(event);
  return 0;
}

void XrmInitialize(void) {}

char* XDisplayName(const char* string) {
  return (string && string[0]) ? (char*)string : g_display_name;
}
