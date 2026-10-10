/* Guest libX11.so.6 for programs that only need a display and a window id.
   Field stores use the Ubuntu Xlibint.h layout this file is compiled against. */
#define _GNU_SOURCE
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>
#include <X11/XKBlib.h>
#include <X11/Xregion.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static Visual g_visual;
static Depth g_depth;
static Screen g_screen;
static char g_vendor[] = "VEXA";
static char g_display_name[] = ":0";
static int g_ready;
static int g_published;
static int g_configure_sent;
static int g_wake_r = -1;
static int g_wake_w = -1;
static int g_pointer_x;
static int g_pointer_y;
static unsigned int g_pointer_state;
static int g_pointer_logs;
static off_t g_pointer_off;
static int g_map[10];
static int g_map_on;
static volatile int g_lock;

#define VEXA_Q 64
static XEvent g_q[VEXA_Q];
static int g_q_head;
static int g_q_count;

static void vexa_lock(void) {
  while (__sync_lock_test_and_set(&g_lock, 1)) {
  }
}

static void vexa_unlock(void) {
  __sync_lock_release(&g_lock);
}

static void vexa_wake(void) {
  char byte = 1;
  if (g_wake_w < 0) {
    return;
  }
  if (write(g_wake_w, &byte, 1) < 0) {
    /* The pipe is full. The event is still in the queue. */
  }
}

static void vexa_drain_wake(void) {
  char buf[64];
  if (g_wake_r < 0) {
    return;
  }
  while (read(g_wake_r, buf, sizeof(buf)) > 0) {
  }
}

static void vexa_q_push_locked(const XEvent* event) {
  if (g_q_count == VEXA_Q) {
    g_q_head = (g_q_head + 1) % VEXA_Q;
    g_q_count--;
  }
  g_q[(g_q_head + g_q_count) % VEXA_Q] = *event;
  g_q_count++;
}

static void vexa_q_push(const XEvent* event) {
  vexa_lock();
  vexa_q_push_locked(event);
  vexa_unlock();
  vexa_wake();
}

static int vexa_q_count_of(void) {
  int count;
  vexa_lock();
  count = g_q_count;
  vexa_unlock();
  return count;
}

static int vexa_q_pop(XEvent* event) {
  vexa_lock();
  if (g_q_count == 0) {
    vexa_unlock();
    return 0;
  }
  *event = g_q[g_q_head];
  g_q_head = (g_q_head + 1) % VEXA_Q;
  g_q_count--;
  vexa_unlock();
  return 1;
}

static int vexa_q_peek(XEvent* event) {
  vexa_lock();
  if (g_q_count == 0) {
    vexa_unlock();
    return 0;
  }
  *event = g_q[g_q_head];
  vexa_unlock();
  return 1;
}

static int vexa_event_matches(const XEvent* event, long mask, Window window, int type) {
  if (window != 0 && event->xany.window != window) {
    return 0;
  }
  if (type != 0 && event->type != type) {
    return 0;
  }
  if (mask == 0) {
    return 1;
  }
  switch (event->type) {
    case ButtonPress:
      return (mask & ButtonPressMask) != 0;
    case ButtonRelease:
      return (mask & ButtonReleaseMask) != 0;
    case MotionNotify:
      if (mask & PointerMotionMask) {
        return 1;
      }
      if ((mask & ButtonMotionMask) && (event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask))) {
        return 1;
      }
      if ((mask & Button1MotionMask) && (event->xmotion.state & Button1Mask)) {
        return 1;
      }
      return 0;
    case ConfigureNotify:
      return (mask & StructureNotifyMask) != 0;
    case Expose:
      return (mask & ExposureMask) != 0;
    default:
      return 0;
  }
}

static int vexa_q_take(Display* dpy, XEvent* event, long mask, Window window, int type,
                       Bool (*predicate)(Display*, XEvent*, XPointer), XPointer arg) {
  int index;
  vexa_lock();
  for (index = 0; index < g_q_count; index++) {
    int slot = (g_q_head + index) % VEXA_Q;
    int next;
    if (!vexa_event_matches(&g_q[slot], mask, window, type)) {
      continue;
    }
    if (predicate && !predicate(dpy, &g_q[slot], arg)) {
      continue;
    }
    *event = g_q[slot];
    for (next = index; next < g_q_count - 1; next++) {
      g_q[(g_q_head + next) % VEXA_Q] = g_q[(g_q_head + next + 1) % VEXA_Q];
    }
    g_q_count--;
    vexa_unlock();
    return 1;
  }
  vexa_unlock();
  return 0;
}

static void vexa_read_window_size(void) {
  char buf[64];
  int fd;
  int n;
  int width = 0;
  int height = 0;
  fd = open("/tmp/vexa-window-size", O_RDONLY);
  if (fd < 0) {
    return;
  }
  n = (int)read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) {
    return;
  }
  buf[n] = 0;
  if (sscanf(buf, "%d %d", &width, &height) == 2 && width > 1 && height > 1) {
    g_screen.width = width;
    g_screen.height = height;
    g_screen.mwidth = width * 254 / 960;
    g_screen.mheight = height * 254 / 960;
    if (g_screen.mwidth < 1) {
      g_screen.mwidth = 1;
    }
    if (g_screen.mheight < 1) {
      g_screen.mheight = 1;
    }
    g_published = 1;
  }
}

static void vexa_enqueue_configure(Display* dpy) {
  XEvent event;
  if (g_configure_sent || g_screen.width <= 1 || g_screen.height <= 1) {
    return;
  }
  memset(&event, 0, sizeof(event));
  event.type = ConfigureNotify;
  event.xconfigure.display = dpy;
  event.xconfigure.window = 1;
  event.xconfigure.event = 1;
  event.xconfigure.x = 0;
  event.xconfigure.y = 0;
  event.xconfigure.width = g_screen.width;
  event.xconfigure.height = g_screen.height;
  event.xconfigure.border_width = 0;
  event.xconfigure.above = None;
  event.xconfigure.override_redirect = False;
  g_configure_sent = 1;
  fprintf(stderr, "[VEXA][X11] configure %dx%d\n", g_screen.width, g_screen.height);
  vexa_q_push(&event);
}

static void vexa_read_present(void) {
  char buf[160];
  int fd;
  int n;
  int v[10];
  fd = open("/tmp/vexa-present", O_RDONLY);
  if (fd < 0) {
    g_map_on = 0;
    return;
  }
  n = (int)read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) {
    g_map_on = 0;
    return;
  }
  buf[n] = 0;
  if (sscanf(buf, "%d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
             &v[9]) != 10 ||
      v[6] < 1 || v[7] < 1 || v[8] < 2 || v[9] < 2) {
    g_map_on = 0;
    return;
  }
  memcpy(g_map, v, sizeof(g_map));
  g_map_on = 1;
}

/* Inverse of the swap-time cover. Touch is top-left on the surface; the
   published rect is the GL content that was cropped onto that surface. */
static void vexa_map_pointer(int* x, int* y) {
  int tx;
  int ty;
  int sw;
  int sh;
  int tex_x;
  int tex_y;
  int fb_x;
  int fb_y;
  int wx;
  int wy;
  vexa_read_present();
  if (!g_map_on) {
    return;
  }
  sw = g_map[8];
  sh = g_map[9];
  tx = *x;
  ty = *y;
  if (tx < 0) {
    tx = 0;
  }
  if (ty < 0) {
    ty = 0;
  }
  if (tx >= sw) {
    tx = sw - 1;
  }
  if (ty >= sh) {
    ty = sh - 1;
  }
  tex_x = g_map[4] + (int)(((int64_t)tx * g_map[6]) / sw);
  tex_y = g_map[5] + (int)(((int64_t)(sh - 1 - ty) * g_map[7]) / sh);
  fb_x = g_map[0] + tex_x;
  fb_y = g_map[1] + tex_y;
  wx = fb_x;
  wy = (sh - 1) - fb_y;
  if (wx < 0) {
    wx = 0;
  }
  if (wy < 0) {
    wy = 0;
  }
  if (g_screen.width > 1 && wx >= g_screen.width) {
    wx = g_screen.width - 1;
  }
  if (g_screen.height > 1 && wy >= g_screen.height) {
    wy = g_screen.height - 1;
  }
  *x = wx;
  *y = wy;
}

static void vexa_fill_pointer(XEvent* event, Display* dpy, int type, int x, int y, int state) {
  memset(event, 0, sizeof(*event));
  event->type = type;
  event->xbutton.display = dpy;
  event->xbutton.window = 1;
  event->xbutton.root = 1;
  event->xbutton.x = x;
  event->xbutton.y = y;
  event->xbutton.x_root = x;
  event->xbutton.y_root = y;
  if (type == MotionNotify) {
    event->xmotion.state = (unsigned int)state;
    event->xmotion.is_hint = NotifyNormal;
    event->xmotion.same_screen = True;
  } else {
    event->xbutton.state = (unsigned int)state;
    event->xbutton.button = 1;
    event->xbutton.same_screen = True;
  }
}

static void vexa_poll_pointer(Display* dpy) {
  unsigned char buf[16];
  int fd;
  int got = 0;
  off_t end;
  fd = open("/tmp/vexa-pointer", O_RDONLY);
  if (fd < 0) {
    return;
  }
  vexa_lock();
  end = lseek(fd, 0, SEEK_END);
  if (end < g_pointer_off) {
    g_pointer_off = 0;
  }
  if (lseek(fd, g_pointer_off, SEEK_SET) < 0) {
    vexa_unlock();
    close(fd);
    return;
  }
  while (got < 32) {
    int32_t type = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t state = 0;
    XEvent event;
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n != (ssize_t)sizeof(buf)) {
      break;
    }
    memcpy(&type, buf, 4);
    memcpy(&x, buf + 4, 4);
    memcpy(&y, buf + 8, 4);
    memcpy(&state, buf + 12, 4);
    g_pointer_off += (off_t)sizeof(buf);
    if (type != ButtonPress && type != ButtonRelease && type != MotionNotify) {
      continue;
    }
    {
      int raw_x = (int)x;
      int raw_y = (int)y;
      int mx = raw_x;
      int my = raw_y;
      vexa_map_pointer(&mx, &my);
      vexa_fill_pointer(&event, dpy, (int)type, mx, my, (int)state);
      g_pointer_x = mx;
      g_pointer_y = my;
      g_pointer_state = (unsigned int)state;
      if (type == ButtonPress && g_pointer_logs < 8) {
        g_pointer_logs++;
        fprintf(stderr, "[VEXA][X11] pointer press raw %d,%d -> %d,%d state=%d\n", raw_x, raw_y, mx, my, (int)state);
      }
      vexa_q_push_locked(&event);
    }
    got++;
  }
  vexa_unlock();
  close(fd);
  if (got > 0) {
    vexa_wake();
  }
}

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
  vexa_read_window_size();
  g_ready = 1;
}

Display* XOpenDisplay(const char* name) {
  Display* dpy = (Display*)calloc(1, sizeof(Display));
  if (!dpy) {
    return NULL;
  }
  vexa_init_screen(dpy);
  dpy->fd = -1;
  {
    int fds[2];
    if (pipe(fds) == 0) {
      fcntl(fds[0], F_SETFL, O_NONBLOCK);
      fcntl(fds[1], F_SETFL, O_NONBLOCK);
      g_wake_r = fds[0];
      g_wake_w = fds[1];
      dpy->fd = g_wake_r;
    }
  }
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
  vexa_read_window_size();
  /* Godot asks for the project size (1920x1080). The published surface is the
     real screen, and a later ConfigureNotify makes Godot adopt it. */
  if (!g_published && width > 1 && height > 1) {
    g_screen.width = (int)width;
    g_screen.height = (int)height;
  }
  vexa_enqueue_configure(dpy);
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

static void vexa_poll_input(Display* dpy) {
  vexa_poll_pointer(dpy);
  vexa_drain_wake();
}

int XNextEvent(Display* dpy, XEvent* event) {
  vexa_poll_input(dpy);
  if (event && vexa_q_pop(event)) {
    return 0;
  }
  vexa_expose(event);
  return 0;
}

int XPeekEvent(Display* dpy, XEvent* event) {
  vexa_poll_input(dpy);
  if (event && vexa_q_peek(event)) {
    return 0;
  }
  vexa_expose(event);
  return 0;
}

int XWindowEvent(Display* dpy, Window window, long mask, XEvent* event) {
  vexa_poll_input(dpy);
  if (event && vexa_q_take(dpy, event, mask, window, 0, NULL, NULL)) {
    return 0;
  }
  vexa_expose(event);
  return 0;
}

int XMaskEvent(Display* dpy, long mask, XEvent* event) {
  vexa_poll_input(dpy);
  if (event && vexa_q_take(dpy, event, mask, 0, 0, NULL, NULL)) {
    return 0;
  }
  vexa_expose(event);
  return 0;
}

Bool XCheckMaskEvent(Display* dpy, long mask, XEvent* event) {
  vexa_poll_input(dpy);
  if (!event) {
    return False;
  }
  return vexa_q_take(dpy, event, mask, 0, 0, NULL, NULL) ? True : False;
}

Bool XCheckWindowEvent(Display* dpy, Window window, long mask, XEvent* event) {
  vexa_poll_input(dpy);
  if (!event) {
    return False;
  }
  return vexa_q_take(dpy, event, mask, window, 0, NULL, NULL) ? True : False;
}

Bool XCheckTypedEvent(Display* dpy, int type, XEvent* event) {
  vexa_poll_input(dpy);
  if (!event) {
    return False;
  }
  return vexa_q_take(dpy, event, 0, 0, type, NULL, NULL) ? True : False;
}

// Like real Xlib, return immediately. Render loops call this once per frame and
// eglSwapBuffers already paces them, so sleeping here only capped the frame rate.
int XPending(Display* dpy) {
  vexa_poll_input(dpy);
  return vexa_q_count_of();
}

int XEventsQueued(Display* dpy, int mode) {
  (void)mode;
  return XPending(dpy);
}

int XFlush(Display* dpy) {
  vexa_poll_input(dpy);
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
  vexa_read_window_size();
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
  vexa_read_window_size();
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
  if (!g_published && width > 1 && height > 1) {
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
  vexa_poll_input(dpy);
  if (event && vexa_q_take(dpy, event, 0, 0, 0, predicate, arg)) {
    return 0;
  }
  vexa_expose(event);
  return 0;
}

void XrmInitialize(void) {}

char* XDisplayName(const char* string) {
  return (string && string[0]) ? (char*)string : g_display_name;
}

/* Godot binds these at load. Optional extensions report themselves absent.
 * Allocations go through malloc so the caller's XFree can release them. */

static char k_im_unsupported[] = "XNQueryInputStyle";

static Region vexa_region(void) {
  REGION* region = (REGION*)calloc(1, sizeof(REGION));
  if (region) {
    region->size = 1;
  }
  return region;
}

XSizeHints* XAllocSizeHints(void) {
  return (XSizeHints*)calloc(1, sizeof(XSizeHints));
}

XClassHint* XAllocClassHint(void) {
  return (XClassHint*)calloc(1, sizeof(XClassHint));
}

int XAutoRepeatOn(Display* dpy) {
  (void)dpy;
  return 1;
}

int XChangeWindowAttributes(Display* dpy, Window window, unsigned long valuemask, XSetWindowAttributes* attrs) {
  (void)dpy;
  (void)window;
  (void)valuemask;
  (void)attrs;
  return 1;
}

Bool XCheckIfEvent(Display* dpy, XEvent* event, Bool (*predicate)(Display*, XEvent*, XPointer), XPointer arg) {
  vexa_poll_input(dpy);
  if (!event) {
    return False;
  }
  return vexa_q_take(dpy, event, 0, 0, 0, predicate, arg) ? True : False;
}

int XConvertSelection(Display* dpy, Atom selection, Atom target, Atom property, Window requestor, Time time) {
  (void)dpy;
  (void)selection;
  (void)target;
  (void)property;
  (void)requestor;
  (void)time;
  return 1;
}

GC XCreateGC(Display* dpy, Drawable drawable, unsigned long valuemask, XGCValues* values) {
  (void)dpy;
  (void)drawable;
  (void)valuemask;
  (void)values;
  return (GC)calloc(1, 64);
}

Pixmap XCreatePixmap(Display* dpy, Drawable drawable, unsigned int width, unsigned int height, unsigned int depth) {
  (void)dpy;
  (void)drawable;
  (void)width;
  (void)height;
  (void)depth;
  return 2;
}

Cursor XCreatePixmapCursor(Display* dpy, Pixmap source, Pixmap mask, XColor* foreground, XColor* background, unsigned int x, unsigned int y) {
  (void)dpy;
  (void)source;
  (void)mask;
  (void)foreground;
  (void)background;
  (void)x;
  (void)y;
  return 1;
}

Region XCreateRegion(void) {
  return vexa_region();
}

int XDefineCursor(Display* dpy, Window window, Cursor cursor) {
  (void)dpy;
  (void)window;
  (void)cursor;
  return 1;
}

int XDestroyRegion(Region region) {
  if (region) {
    free(region->rects);
    free(region);
  }
  return 1;
}

int XFillRectangle(Display* dpy, Drawable drawable, GC gc, int x, int y, unsigned int width, unsigned int height) {
  (void)dpy;
  (void)drawable;
  (void)gc;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  return 1;
}

int XFreeCursor(Display* dpy, Cursor cursor) {
  (void)dpy;
  (void)cursor;
  return 1;
}

int XFreeGC(Display* dpy, GC gc) {
  (void)dpy;
  free(gc);
  return 1;
}

int XFreePixmap(Display* dpy, Pixmap pixmap) {
  (void)dpy;
  (void)pixmap;
  return 1;
}

char* XGetAtomName(Display* dpy, Atom atom) {
  const char* name = "VEXA";
  size_t bytes;
  char* copy;
  (void)dpy;
  (void)atom;
  bytes = strlen(name) + 1;
  copy = (char*)malloc(bytes);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, name, bytes);
  return copy;
}

int XGetWindowProperty(Display* dpy, Window window, Atom property, long long_offset, long long_length, Bool delete, Atom req_type,
                       Atom* actual_type_return, int* actual_format_return, unsigned long* nitems_return, unsigned long* bytes_after_return,
                       unsigned char** prop_return) {
  (void)dpy;
  (void)window;
  (void)property;
  (void)long_offset;
  (void)long_length;
  (void)delete;
  (void)req_type;
  if (actual_type_return) {
    *actual_type_return = None;
  }
  if (actual_format_return) {
    *actual_format_return = 0;
  }
  if (nitems_return) {
    *nitems_return = 0;
  }
  if (bytes_after_return) {
    *bytes_after_return = 0;
  }
  if (prop_return) {
    *prop_return = NULL;
  }
  return Success;
}

Window XGetSelectionOwner(Display* dpy, Atom selection) {
  (void)dpy;
  (void)selection;
  return None;
}

int XGrabPointer(Display* dpy, Window grab_window, Bool owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
                 Window confine_to, Cursor cursor, Time time) {
  (void)dpy;
  (void)grab_window;
  (void)owner_events;
  (void)event_mask;
  (void)pointer_mode;
  (void)keyboard_mode;
  (void)confine_to;
  (void)cursor;
  (void)time;
  return GrabSuccess;
}

int XMoveResizeWindow(Display* dpy, Window window, int x, int y, unsigned int width, unsigned int height) {
  (void)x;
  (void)y;
  return XResizeWindow(dpy, window, width, height);
}

Bool XQueryExtension(Display* dpy, const char* name, int* major, int* event, int* error) {
  (void)dpy;
  if (major) {
    *major = 0;
  }
  if (event) {
    *event = 0;
  }
  if (error) {
    *error = 0;
  }
  // Godot 3.7 refuses to start unless XInput2 is present. Other extensions
  // stay absent so their reply structs are never read.
  if (name && strcmp(name, "XInputExtension") == 0) {
    if (major) {
      *major = 131;
    }
    return True;
  }
  return False;
}

Bool XQueryPointer(Display* dpy, Window window, Window* root, Window* child, int* root_x, int* root_y, int* win_x, int* win_y,
                   unsigned int* mask) {
  (void)dpy;
  (void)window;
  if (root) {
    *root = 1;
  }
  if (child) {
    *child = None;
  }
  if (root_x) {
    *root_x = 0;
  }
  if (root_y) {
    *root_y = 0;
  }
  if (win_x) {
    *win_x = g_pointer_x;
  }
  if (win_y) {
    *win_y = g_pointer_y;
  }
  if (root_x) {
    *root_x = g_pointer_x;
  }
  if (root_y) {
    *root_y = g_pointer_y;
  }
  if (mask) {
    *mask = g_pointer_state;
  }
  return True;
}

Region XPolygonRegion(XPoint* points, int n, int fill_rule) {
  (void)points;
  (void)n;
  (void)fill_rule;
  return vexa_region();
}

Status XSendEvent(Display* dpy, Window window, Bool propagate, long event_mask, XEvent* event) {
  (void)dpy;
  (void)window;
  (void)propagate;
  (void)event_mask;
  (void)event;
  return 1;
}

int XSetClassHint(Display* dpy, Window window, XClassHint* hints) {
  (void)dpy;
  (void)window;
  (void)hints;
  return 1;
}

int XSetSelectionOwner(Display* dpy, Atom selection, Window owner, Time time) {
  (void)dpy;
  (void)selection;
  (void)owner;
  (void)time;
  return 1;
}

Bool XTranslateCoordinates(Display* dpy, Window src, Window dest, int src_x, int src_y, int* dest_x, int* dest_y, Window* child) {
  (void)dpy;
  (void)src;
  (void)dest;
  if (dest_x) {
    *dest_x = src_x;
  }
  if (dest_y) {
    *dest_y = src_y;
  }
  if (child) {
    *child = None;
  }
  return True;
}

int XUndefineCursor(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XUngrabPointer(Display* dpy, Time time) {
  (void)dpy;
  (void)time;
  return 1;
}

int XUnionRectWithRegion(XRectangle* rectangle, Region src, Region dest) {
  (void)rectangle;
  (void)src;
  (void)dest;
  return 0;
}

int XWarpPointer(Display* dpy, Window src, Window dest, int src_x, int src_y, unsigned int src_width, unsigned int src_height, int dest_x,
                 int dest_y) {
  (void)dpy;
  (void)src;
  (void)dest;
  (void)src_x;
  (void)src_y;
  (void)src_width;
  (void)src_height;
  (void)dest_x;
  (void)dest_y;
  return 1;
}

Bool XFilterEvent(XEvent* event, Window window) {
  (void)event;
  (void)window;
  return False;
}

Bool XGetEventData(Display* dpy, XGenericEventCookie* cookie) {
  (void)dpy;
  (void)cookie;
  return False;
}

void XFreeEventData(Display* dpy, XGenericEventCookie* cookie) {
  (void)dpy;
  (void)cookie;
}

XIM XOpenIM(Display* dpy, struct _XrmHashBucketRec* rdb, char* res_name, char* res_class) {
  (void)dpy;
  (void)rdb;
  (void)res_name;
  (void)res_class;
  return NULL;
}

Status XCloseIM(XIM im) {
  (void)im;
  return 0;
}

char* XGetIMValues(XIM im, ...) {
  (void)im;
  return k_im_unsupported;
}

char* XSetIMValues(XIM im, ...) {
  (void)im;
  return k_im_unsupported;
}

XIC XCreateIC(XIM im, ...) {
  (void)im;
  return NULL;
}

void XDestroyIC(XIC ic) {
  (void)ic;
}

void XSetICFocus(XIC ic) {
  (void)ic;
}

void XUnsetICFocus(XIC ic) {
  (void)ic;
}

char* XGetICValues(XIC ic, ...) {
  (void)ic;
  return k_im_unsupported;
}

char* XSetICValues(XIC ic, ...) {
  (void)ic;
  return k_im_unsupported;
}

char* XSetLocaleModifiers(const char* modifiers) {
  (void)modifiers;
  return g_display_name;
}

XVaNestedList XVaCreateNestedList(int unused, ...) {
  (void)unused;
  return NULL;
}

int Xutf8LookupString(XIC ic, XKeyPressedEvent* event, char* buffer, int bytes, KeySym* keysym, Status* status) {
  (void)ic;
  (void)event;
  (void)buffer;
  (void)bytes;
  if (keysym) {
    *keysym = NoSymbol;
  }
  if (status) {
    *status = XLookupNone;
  }
  return 0;
}

XkbDescPtr XkbAllocKeyboard(void) {
  XkbDescPtr desc = (XkbDescPtr)calloc(1, sizeof(XkbDescRec));
  if (!desc) {
    return NULL;
  }
  desc->device_spec = XkbUseCoreKbd;
  desc->min_key_code = 8;
  desc->max_key_code = 255;
  return desc;
}

void XkbFreeKeyboard(XkbDescPtr xkb, unsigned int which, Bool free_desc) {
  (void)which;
  if (!xkb) {
    return;
  }
  if (xkb->names) {
    free(xkb->names->keys);
    free(xkb->names->key_aliases);
    free(xkb->names->radio_groups);
    free(xkb->names);
    xkb->names = NULL;
  }
  free(xkb->ctrls);
  xkb->ctrls = NULL;
  if (free_desc) {
    free(xkb);
  }
}

Status XkbGetControls(Display* dpy, unsigned long which, XkbDescPtr desc) {
  (void)dpy;
  (void)which;
  if (!desc) {
    return BadAlloc;
  }
  if (!desc->ctrls) {
    desc->ctrls = (XkbControlsPtr)calloc(1, sizeof(XkbControlsRec));
    if (!desc->ctrls) {
      return BadAlloc;
    }
    desc->ctrls->num_groups = 1;
  }
  return Success;
}

Status XkbGetNames(Display* dpy, unsigned int which, XkbDescPtr desc) {
  int count;
  (void)dpy;
  (void)which;
  if (!desc) {
    return BadAlloc;
  }
  if (!desc->names) {
    desc->names = (XkbNamesPtr)calloc(1, sizeof(XkbNamesRec));
    if (!desc->names) {
      return BadAlloc;
    }
    count = (int)desc->max_key_code + 1;
    if (count < 1) {
      count = 1;
    }
    if (count > 256) {
      count = 256;
    }
    desc->names->keys = (XkbKeyNamePtr)calloc((size_t)count, sizeof(XkbKeyNameRec));
  }
  return Success;
}

Status XkbGetState(Display* dpy, unsigned int device, XkbStatePtr state) {
  (void)dpy;
  (void)device;
  if (state) {
    memset(state, 0, sizeof(*state));
  }
  return Success;
}

KeySym XkbKeycodeToKeysym(Display* dpy, KeyCode keycode, int group, int level) {
  (void)dpy;
  (void)keycode;
  (void)group;
  (void)level;
  return NoSymbol;
}

Bool XkbLockGroup(Display* dpy, unsigned int device, unsigned int group) {
  (void)dpy;
  (void)device;
  (void)group;
  return True;
}

Bool XkbSetDetectableAutoRepeat(Display* dpy, Bool detectable, Bool* supported) {
  (void)dpy;
  (void)detectable;
  if (supported) {
    *supported = False;
  }
  return True;
}
