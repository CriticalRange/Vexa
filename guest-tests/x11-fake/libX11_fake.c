/* Guest libX11.so.6 for programs that only need a display and a window id.
   Field stores use the Ubuntu Xlibint.h layout this file is compiled against. */
#define _GNU_SOURCE
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>
#include <X11/XKBlib.h>
#include <X11/Xregion.h>

#include <errno.h>
#include <dlfcn.h>
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
static KeySym g_sym_by_code[256];
static unsigned char g_key_down[32];
static unsigned int g_mod_state;
static int g_keys_ready;
static volatile int g_lock;

/* X keycodes match the evdev map in libxkbcommon_x11_fake.c. SDL and Godot
   look these up by keycode, so ASCII (W = 119) is not a valid W. */
static const struct {
  unsigned int code;
  KeySym sym;
} k_key_table[] = {
    {9, 0xff1b},   /* Escape */
    {22, 0xff08},  /* BackSpace */
    {23, 0xff09},  /* Tab */
    {24, 0x71},    /* q */
    {25, 0x77},    /* w */
    {26, 0x65},    /* e */
    {27, 0x72},    /* r */
    {28, 0x74},    /* t */
    {29, 0x79},    /* y */
    {30, 0x75},    /* u */
    {31, 0x69},    /* i */
    {32, 0x6f},    /* o */
    {33, 0x70},    /* p */
    {36, 0xff0d},  /* Return */
    {37, 0xffe3},  /* Control_L */
    {38, 0x61},    /* a */
    {39, 0x73},    /* s */
    {40, 0x64},    /* d */
    {41, 0x66},    /* f */
    {42, 0x67},    /* g */
    {43, 0x68},    /* h */
    {44, 0x6a},    /* j */
    {45, 0x6b},    /* k */
    {46, 0x6c},    /* l */
    {50, 0xffe1},  /* Shift_L */
    {52, 0x7a},    /* z */
    {53, 0x78},    /* x */
    {54, 0x63},    /* c */
    {55, 0x76},    /* v */
    {56, 0x62},    /* b */
    {57, 0x6e},    /* n */
    {58, 0x6d},    /* m */
    {62, 0xffe2},  /* Shift_R */
    {64, 0xffe9},  /* Alt_L */
    {65, 0x20},    /* space */
    {105, 0xffe4}, /* Control_R */
    {108, 0xffea}, /* Alt_R */
    {111, 0xff52}, /* Up */
    {113, 0xff51}, /* Left */
    {114, 0xff53}, /* Right */
    {116, 0xff54}, /* Down */
};

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
    case KeyPress:
      return (mask & KeyPressMask) != 0;
    case KeyRelease:
      return (mask & KeyReleaseMask) != 0;
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

/* Tracked client-window geometry and WM-style acknowledgements. A real server
   answers every configure/map/property request with the matching event; SDL's
   window bookkeeping (and therefore Unity's Screen.resolutions /
   currentResolution) only advances when those events arrive. */
static int g_win_x;
static int g_win_y;
static int g_win_w;
static int g_win_h;
static int g_win_valid;

static void vexa_push_configure(Display* dpy, int x, int y, int w, int h) {
  XEvent event;
  /* SDL only emits SDL_WINDOWEVENT_MOVED (which is what Unity listens to for
     refreshing Screen.resolutions / the native desktop resolution cache) when
     the reported position actually changes from its zero-initialized tracking
     state. A (0,0) report is a no-op for it, so nudge the position by one
     pixel — the window stays inside the display bounds either way. */
  if (x == 0 && y == 0) {
    y = 1;
  }
  if (w < 1) {
    w = g_screen.width;
  }
  if (h < 1) {
    h = g_screen.height;
  }
  memset(&event, 0, sizeof(event));
  event.type = ConfigureNotify;
  event.xconfigure.display = dpy;
  event.xconfigure.window = 1;
  event.xconfigure.event = 1;
  event.xconfigure.x = x;
  event.xconfigure.y = y;
  event.xconfigure.width = w;
  event.xconfigure.height = h;
  event.xconfigure.border_width = 0;
  event.xconfigure.above = None;
  event.xconfigure.override_redirect = False;
  vexa_q_push(&event);
}

static void vexa_push_map(Display* dpy, int mapped) {
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.type = mapped ? MapNotify : UnmapNotify;
  if (mapped) {
    event.xmap.display = dpy;
    event.xmap.window = 1;
    event.xmap.event = 1;
    event.xmap.override_redirect = False;
  } else {
    event.xunmap.display = dpy;
    event.xunmap.window = 1;
    event.xunmap.event = 1;
  }
  vexa_q_push(&event);
}

static void vexa_push_property(Display* dpy, Atom atom, int state) {
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.type = PropertyNotify;
  event.xproperty.display = dpy;
  event.xproperty.window = 1;
  event.xproperty.atom = atom;
  event.xproperty.state = state;
  event.xproperty.time = 1;
  vexa_q_push(&event);
}

static void vexa_configure(Display* dpy, int x, int y, int w, int h) {
  vexa_read_window_size();
  if (x < -100000 || x > 100000) {
    x = 0;
  }
  if (y < -100000 || y > 100000) {
    y = 0;
  }
  /* On Android single-surface mode (g_published), the native window covers the
     entire screen surface. Keep the main window at full screen dimensions so
     windowed-mode room resets (e.g. GameMaker / Undertale calling
     XConfigureWindow(640, 480) after room change) do not shrink the viewport to
     the bottom-left corner of the screen. */
  if (g_published && g_screen.width > 1 && g_screen.height > 1) {
    x = 0;
    y = 0;
    w = g_screen.width;
    h = g_screen.height;
  } else {
    if (w < 1) {
      w = g_screen.width;
    }
    if (h < 1) {
      h = g_screen.height;
    }
  }
  g_win_x = x;
  g_win_y = y;
  g_win_w = w;
  g_win_h = h;
  g_win_valid = 1;
  fprintf(stderr, "[VEXA][X11] win_configure %d,%d %dx%d\n", x, y, w, h);
  vexa_push_configure(dpy, x, y, w, h);
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

static void vexa_init_keys(void) {
  unsigned int i;
  if (g_keys_ready) {
    return;
  }
  for (i = 0; i < sizeof(k_key_table) / sizeof(k_key_table[0]); i++) {
    unsigned int code = k_key_table[i].code;
    if (code < 256) {
      g_sym_by_code[code] = k_key_table[i].sym;
    }
  }
  g_keys_ready = 1;
}

static unsigned int vexa_code_for_sym(KeySym sym) {
  unsigned int i;
  vexa_init_keys();
  for (i = 0; i < sizeof(k_key_table) / sizeof(k_key_table[0]); i++) {
    if (k_key_table[i].sym == sym) {
      return k_key_table[i].code;
    }
  }
  return 0;
}

static KeySym vexa_sym_for_code(unsigned int code, int level) {
  KeySym sym;
  vexa_init_keys();
  if (code >= 256) {
    return NoSymbol;
  }
  sym = g_sym_by_code[code];
  if (level == 1 && sym >= (KeySym)'a' && sym <= (KeySym)'z') {
    return sym - ('a' - 'A');
  }
  if (level != 0) {
    return NoSymbol;
  }
  return sym;
}

static unsigned int vexa_mod_bit(unsigned int code) {
  if (code == 50 || code == 62) {
    return ShiftMask;
  }
  if (code == 37 || code == 105) {
    return ControlMask;
  }
  if (code == 64 || code == 108) {
    return Mod1Mask;
  }
  return 0;
}

static void vexa_note_key(unsigned int code, int down) {
  unsigned int bit;
  if (code == 0 || code >= 256) {
    return;
  }
  bit = vexa_mod_bit(code);
  if (down) {
    g_key_down[code >> 3] |= (unsigned char)(1u << (code & 7));
    g_mod_state |= bit;
  } else {
    g_key_down[code >> 3] &= (unsigned char)~(1u << (code & 7));
    g_mod_state &= ~bit;
  }
}

static void vexa_fill_key(XEvent* event, Display* dpy, int type, KeySym sym, unsigned int state) {
  memset(event, 0, sizeof(*event));
  event->type = type;
  event->xkey.display = dpy;
  event->xkey.window = 1;
  event->xkey.root = 1;
  event->xkey.state = state | g_mod_state;
  event->xkey.keycode = vexa_code_for_sym(sym);
  event->xkey.same_screen = True;
}

static int vexa_lookup_key(XKeyEvent* event, char* buffer, int bytes, KeySym* keysym) {
  KeySym sym = event ? vexa_sym_for_code(event->keycode, 0) : NoSymbol;
  if (keysym) {
    *keysym = sym;
  }
  if (buffer && bytes > 0) {
    if (sym >= 32 && sym < 127) {
      buffer[0] = (char)sym;
      return 1;
    }
    buffer[0] = 0;
  }
  return 0;
}

static unsigned int vexa_pointer_button(int state) {
  unsigned int button = ((unsigned int)state >> 24) & 0xffu;
  if (button < 1 || button > 5) {
    return 1;
  }
  return button;
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
    event->xmotion.state = (unsigned int)state & 0x00ffffffu;
    event->xmotion.is_hint = NotifyNormal;
    event->xmotion.same_screen = True;
  } else {
    event->xbutton.state = (unsigned int)state & 0x00ffffffu;
    event->xbutton.button = vexa_pointer_button(state);
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
    if (type == KeyPress || type == KeyRelease) {
      vexa_fill_key(&event, dpy, (int)type, (KeySym)x, (unsigned int)state);
      vexa_note_key(event.xkey.keycode, type == KeyPress);
      if (g_pointer_logs < 8) {
        g_pointer_logs++;
        fprintf(stderr, "[VEXA][X11] key %s sym=0x%x code=%u\n", type == KeyPress ? "down" : "up", (unsigned)x,
                event.xkey.keycode);
      }
      vexa_q_push_locked(&event);
      got++;
      continue;
    }
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
  /* Display is a pointer typedef, so sizeof(Display) is 8. Callers that keep
     the handle (Unity reads ScreenOfDisplay(dpy, 0)->width_in_pixels through
     the Xlib macros) walk the real struct, so allocate the whole thing. */
  Display* dpy = (Display*)calloc(1, sizeof(*dpy));
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
  fprintf(stderr, "[VEXA][X11] open_display screens=%d default=%d screen=%dx%d depth=%d sizeof=%zu\n", dpy->nscreens,
          dpy->default_screen, g_screen.width, g_screen.height, g_screen.root_depth, sizeof(*dpy));
  return dpy;
}

int XCloseDisplay(Display* dpy) {
  free(dpy);
  return 0;
}

static Window g_next_window = 1;

Window XCreateWindow(Display* dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border,
                     int depth, unsigned int klass, Visual* visual, unsigned long valuemask, XSetWindowAttributes* attrs) {
  Window win;
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
  win = g_next_window++;
  /* Godot asks for the project size (1920x1080). The published surface is the
     real screen, and a later ConfigureNotify makes Godot adopt it. */
  if (win == 1) {
    if (!g_published && width > 1 && height > 1) {
      g_screen.width = (int)width;
      g_screen.height = (int)height;
    }
    g_win_x = (x < -100000 || x > 100000) ? 0 : x;
    g_win_y = (y < -100000 || y > 100000) ? 0 : y;
    g_win_w = (int)width;
    g_win_h = (int)height;
    g_win_valid = 1;
  }
  fprintf(stderr, "[VEXA][X11] create_window -> %lu at %d,%d %ux%u depth=%d\n", (unsigned long)win, x, y, width, height,
          depth);
  if (win == 1) {
    vexa_enqueue_configure(dpy);
  }
  return win;
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
  fprintf(stderr, "[VEXA][X11] map_window %lu\n", (unsigned long)window);
  if (window == 1) {
    int cfg_w = (g_published && g_screen.width > 1) ? g_screen.width : (g_win_w > 1 ? g_win_w : g_screen.width);
    int cfg_h = (g_published && g_screen.height > 1) ? g_screen.height : (g_win_h > 1 ? g_win_h : g_screen.height);
    vexa_push_map(dpy, 1);
    vexa_push_configure(dpy, g_published ? 0 : g_win_x, g_published ? 0 : g_win_y, cfg_w, cfg_h);
  }
  return 1;
}

int XMapRaised(Display* dpy, Window window) {
  return XMapWindow(dpy, window);
}

int XRaiseWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XLowerWindow(Display* dpy, Window window) {
  (void)dpy;
  (void)window;
  return 1;
}

int XRestackWindows(Display* dpy, Window* windows, int nwindows) {
  (void)dpy;
  (void)windows;
  (void)nwindows;
  return 1;
}

int XCirculateSubwindows(Display* dpy, Window window, int direction) {
  (void)dpy;
  (void)window;
  (void)direction;
  return 1;
}

Status XQueryTree(Display* dpy, Window window, Window* root, Window* parent, Window** children, unsigned int* nchildren) {
  (void)dpy;
  (void)window;
  if (root) {
    *root = 1;
  }
  if (parent) {
    *parent = 1;
  }
  if (children) {
    *children = NULL;
  }
  if (nchildren) {
    *nchildren = 0;
  }
  return 1;
}

int XIconifyWindow(Display* dpy, Window window, int screen) {
  (void)dpy;
  (void)window;
  (void)screen;
  return 1;
}

int XWithdrawWindow(Display* dpy, Window window, int screen) {
  (void)dpy;
  (void)window;
  (void)screen;
  return 1;
}

int XUnmapWindow(Display* dpy, Window window) {
  (void)window;
  vexa_push_map(dpy, 0);
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
static int g_pending_calls = 0;

int XPending(Display* dpy) {
  if (g_published && g_screen.width > 1 && g_screen.height > 1 && g_pending_calls < 200) {
    g_pending_calls++;
    if (g_pending_calls == 5 || g_pending_calls == 200) {
      vexa_configure(dpy, 0, 0, g_screen.width, g_screen.height);
    }
  }
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
  (void)window;
  (void)type;
  (void)format;
  (void)mode;
  (void)data;
  fprintf(stderr, "[VEXA][X11] change_property w=%lu prop=%lu n=%d\n", (unsigned long)window,
          (unsigned long)property, nelements);
  vexa_push_property(dpy, property, PropertyNewValue);
  return 1;
}

int XDeleteProperty(Display* dpy, Window window, Atom property) {
  (void)window;
  vexa_push_property(dpy, property, PropertyDelete);
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
  (void)status;
  return vexa_lookup_key(event, buffer, bytes, keysym);
}

KeySym XLookupKeysym(XKeyEvent* event, int index) {
  if (!event || index < 0) {
    return NoSymbol;
  }
  return vexa_sym_for_code(event->keycode, index);
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

/* ---- Unity Linux player fixups -------------------------------------------
   Unity queries its display list before SDL video is up, caches the empty
   answer, and never looks again. Two consequences on a single-surface guest:
   Screen.resolutions is empty (games that restore a saved resolution index die
   with IndexOutOfRangeException before loading a scene) and there is no
   display, so there is no camera and the scene never renders.

   Both are recoverable from inside this library because it already runs in the
   guest process. The engine is stripped, so each entry point is guarded by a
   byte signature and a wrong Unity build simply skips the call instead of
   jumping into the wrong function. */

Status XGetWindowAttributes(Display* dpy, Window window, XWindowAttributes* attrs) {
  (void)dpy;
  (void)window;
  vexa_read_window_size();
  if (!attrs) {
    return 0;
  }
  memset(attrs, 0, sizeof(*attrs));
  attrs->x = g_win_valid ? g_win_x : 0;
  attrs->y = g_win_valid ? g_win_y : 0;
  attrs->width = g_win_valid && g_win_w > 0 ? g_win_w : g_screen.width;
  attrs->height = g_win_valid && g_win_h > 0 ? g_win_h : g_screen.height;
  attrs->depth = 24;
  attrs->visual = &g_visual;
  attrs->class = TrueColor;
  attrs->map_state = IsViewable;
  attrs->screen = &g_screen;
  fprintf(stderr, "[VEXA][X11] get_window_attributes w=%lu -> %dx%d (screen %dx%d)\n", (unsigned long)window,
          attrs->width, attrs->height, g_screen.width, g_screen.height);
  return 1;
}

int XGetInputFocus(Display* dpy, Window* focus, int* revert_to) {
  (void)dpy;
  /* Unity and SDL both poll focus. Reporting the root window keeps them from
     waiting for a window that will never be activated. */
  if (focus) {
    *focus = 1;
  }
  if (revert_to) {
    *revert_to = 0;
  }
  return 0;
}

int XSetInputFocus(Display* dpy, Window window, int revert_to, Time time) {
  (void)dpy;
  (void)window;
  (void)revert_to;
  (void)time;
  return 0;
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
    *x = g_win_valid ? g_win_x : 0;
  }
  if (y) {
    *y = g_win_valid ? g_win_y : 0;
  }
  if (width) {
    *width = (unsigned int)(g_win_valid && g_win_w > 0 ? g_win_w : g_screen.width);
  }
  if (height) {
    *height = (unsigned int)(g_win_valid && g_win_h > 0 ? g_win_h : g_screen.height);
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
  fprintf(stderr, "[VEXA][X11] resize_window w=%lu %ux%u\n", (unsigned long)window, width, height);
  if (window != 1) {
    return 1;
  }
  if (!g_published && width > 1 && height > 1) {
    g_screen.width = (int)width;
    g_screen.height = (int)height;
  }
  vexa_configure(dpy, g_win_x, g_win_y, (int)width, (int)height);
  return 1;
}

int XMoveWindow(Display* dpy, Window window, int x, int y) {
  (void)dpy;
  if (window != 1) {
    return 1;
  }
  if (!g_published) {
    g_win_x = (x < -100000 || x > 100000) ? 0 : x;
    g_win_y = (y < -100000 || y > 100000) ? 0 : y;
  }
  return 1;
}

int XSelectInput(Display* dpy, Window window, long mask) {
  (void)dpy;
  (void)window;
  (void)mask;
  return 1;
}

static int g_clear_configured = 0;

int XClearWindow(Display* dpy, Window window) {
  if (window == 1 && !g_clear_configured && g_published && g_screen.width > 1 && g_screen.height > 1) {
    g_clear_configured = 1;
    vexa_configure(dpy, 0, 0, g_screen.width, g_screen.height);
  }
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
  /* All interned atoms collapse to 1 in this fake, so a one-atom XA_ATOM
     property always "contains" whatever atom the caller is looking for —
     _NET_SUPPORTED lists every EWMH feature as supported and _NET_WM_STATE
     lists _NET_WM_STATE_FULLSCREEN as set. */
  if (req_type == XA_ATOM || req_type == 0) {
    Atom* atoms = (Atom*)malloc(sizeof(Atom));
    if (atoms) {
      atoms[0] = 1;
      if (actual_type_return) {
        *actual_type_return = XA_ATOM;
      }
      if (actual_format_return) {
        *actual_format_return = 32;
      }
      if (nitems_return) {
        *nitems_return = 1;
      }
      if (prop_return) {
        *prop_return = (unsigned char*)atoms;
      } else {
        free(atoms);
      }
    }
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
  fprintf(stderr, "[VEXA][X11] move_resize_window w=%lu %d,%d %ux%u\n", (unsigned long)window, x, y, width,
          height);
  if (window != 1) {
    return 1;
  }
  vexa_configure(dpy, x, y, (int)width, (int)height);
  return 1;
}

int XConfigureWindow(Display* dpy, Window window, unsigned int valuemask, XWindowChanges* values) {
  int x = g_win_x;
  int y = g_win_y;
  int w = g_win_w;
  int h = g_win_h;
  if (values) {
    if (valuemask & CWX) {
      x = values->x;
    }
    if (valuemask & CWY) {
      y = values->y;
    }
    if (valuemask & CWWidth) {
      w = values->width;
    }
    if (valuemask & CWHeight) {
      h = values->height;
    }
  }
  fprintf(stderr, "[VEXA][X11] configure_window w=%lu %d,%d %dx%d\n", (unsigned long)window, x, y, w, h);
  if (window != 1) {
    return 1;
  }
  vexa_configure(dpy, x, y, w, h);
  return 1;
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
  // stay absent so their reply structs are never read. RANDR and XINERAMA are
  // backed by the fake client libs, so report them present for clients that
  // gate display enumeration on XQueryExtension.
  if (name && strcmp(name, "XInputExtension") == 0) {
    if (major) {
      *major = 131;
    }
    return True;
  }
  if (name && (strcmp(name, "RANDR") == 0 || strcmp(name, "XINERAMA") == 0)) {
    if (major) {
      *major = 1;
    }
    fprintf(stderr, "[VEXA][X11] query_extension %s -> yes\n", name);
    return True;
  }
  if (name) {
    fprintf(stderr, "[VEXA][X11] query_extension %s -> no\n", name);
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
  (void)window;
  (void)propagate;
  (void)event_mask;
  fprintf(stderr, "[VEXA][X11] send_event type=%d\n", event ? event->type : -1);
  if (event && event->type == ClientMessage) {
    /* A real WM consumes EWMH client messages (e.g. _NET_WM_STATE fullscreen
       toggles) and answers with _NET_WM_STATE property notification plus a
       ConfigureNotify at the display bounds. SDL waits on exactly that pair
       before it reports the window as fullscreen/on-display. */
    fprintf(stderr, "[VEXA][X11] wm_ack client_message fmt=%d l0=%ld\n", event->xclient.format,
            (long)event->xclient.data.l[0]);
    vexa_push_property(dpy, event->xclient.message_type, PropertyNewValue);
    vexa_read_window_size();
    vexa_configure(dpy, 0, 0, g_screen.width, g_screen.height);
  }
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
  int n;
  (void)ic;
  n = vexa_lookup_key((XKeyEvent*)event, buffer, bytes, keysym);
  if (status) {
    *status = n > 0 ? XLookupBoth : XLookupNone;
  }
  return n;
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
  if (group != 0) {
    return NoSymbol;
  }
  return vexa_sym_for_code(keycode, level);
}

KeySym XKeycodeToKeysym(Display* dpy, KeyCode keycode, int index) {
  return XkbKeycodeToKeysym(dpy, keycode, 0, index);
}

KeyCode XKeysymToKeycode(Display* dpy, KeySym keysym) {
  (void)dpy;
  return (KeyCode)vexa_code_for_sym(keysym);
}

int XDisplayKeycodes(Display* dpy, int* min_keycodes_return, int* max_keycodes_return) {
  (void)dpy;
  if (min_keycodes_return) {
    *min_keycodes_return = 8;
  }
  if (max_keycodes_return) {
    *max_keycodes_return = 255;
  }
  return 1;
}

KeySym* XGetKeyboardMapping(Display* dpy, KeyCode first_keycode, int keycode_count, int* keysyms_per_keycode_return) {
  int i;
  KeySym* out;
  (void)dpy;
  if (keysyms_per_keycode_return) {
    *keysyms_per_keycode_return = 2;
  }
  if (keycode_count < 0) {
    keycode_count = 0;
  }
  out = (KeySym*)calloc((size_t)keycode_count * 2 + 1, sizeof(KeySym));
  if (!out) {
    return NULL;
  }
  for (i = 0; i < keycode_count; i++) {
    out[i * 2] = vexa_sym_for_code((unsigned int)first_keycode + (unsigned int)i, 0);
    out[i * 2 + 1] = vexa_sym_for_code((unsigned int)first_keycode + (unsigned int)i, 1);
  }
  return out;
}

int XQueryKeymap(Display* dpy, char keys_return[32]) {
  (void)dpy;
  vexa_lock();
  memcpy(keys_return, g_key_down, 32);
  vexa_unlock();
  return 1;
}

XModifierKeymap* XGetModifierMapping(Display* dpy) {
  XModifierKeymap* map;
  (void)dpy;
  map = (XModifierKeymap*)calloc(1, sizeof(*map));
  if (!map) {
    return NULL;
  }
  map->max_keypermod = 2;
  map->modifiermap = (KeyCode*)calloc(8 * 2, sizeof(KeyCode));
  if (!map->modifiermap) {
    free(map);
    return NULL;
  }
  map->modifiermap[0] = 50;  /* Shift_L */
  map->modifiermap[1] = 62;  /* Shift_R */
  map->modifiermap[4] = 37;  /* Control_L */
  map->modifiermap[5] = 105; /* Control_R */
  map->modifiermap[6] = 64;  /* Alt_L */
  map->modifiermap[7] = 108; /* Alt_R */
  return map;
}

int XFreeModifiermap(XModifierKeymap* map) {
  if (!map) {
    return 0;
  }
  free(map->modifiermap);
  free(map);
  return 1;
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

/* GameMaker 1.x (Undertale runner) calls the function forms rather than the
   Xlib display-macros. Single-screen fake: answer straight from g_screen. */
int XDisplayWidth(Display* dpy, int screen_number) {
  (void)dpy;
  (void)screen_number;
  vexa_read_window_size();
  return g_screen.width;
}

int XDisplayHeight(Display* dpy, int screen_number) {
  (void)dpy;
  (void)screen_number;
  vexa_read_window_size();
  return g_screen.height;
}

int XDisplayWidthMM(Display* dpy, int screen_number) {
  (void)dpy;
  (void)screen_number;
  vexa_read_window_size();
  return g_screen.mwidth;
}

int XDisplayHeightMM(Display* dpy, int screen_number) {
  (void)dpy;
  (void)screen_number;
  vexa_read_window_size();
  return g_screen.mheight;
}

int XDrawLine(Display* dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2) {
  (void)dpy;
  (void)d;
  (void)gc;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
  return 0;
}

int XSetFillStyle(Display* dpy, GC gc, int fill_style) {
  (void)dpy;
  (void)gc;
  (void)fill_style;
  return 0;
}

int XSetFont(Display* dpy, GC gc, Font font) {
  (void)dpy;
  (void)gc;
  (void)font;
  return 0;
}

Status XStringListToTextProperty(char** list, int count, XTextProperty* text_prop) {
  if (!text_prop) {
    return 0;
  }
  memset(text_prop, 0, sizeof(*text_prop));
  text_prop->encoding = XA_STRING;
  text_prop->format = 8;
  if (list && count > 0 && list[0]) {
    size_t len = strlen(list[0]);
    unsigned char* copy = (unsigned char*)malloc(len + 1);
    if (!copy) {
      return 0;
    }
    memcpy(copy, list[0], len + 1);
    text_prop->value = copy;
    text_prop->nitems = (unsigned long)len;
  }
  return 1;
}

void XSetWMName(Display* dpy, Window w, XTextProperty* text_prop) {
  (void)dpy;
  (void)w;
  if (text_prop && text_prop->value && text_prop->format == 8) {
    fprintf(stderr, "[VEXA][X11] wm_name %s\n", (const char*)text_prop->value);
  }
}

XFontStruct* XLoadQueryFont(Display* dpy, const char* name) {
  XFontStruct* font;
  (void)dpy;
  (void)name;
  font = (XFontStruct*)calloc(1, sizeof(XFontStruct));
  if (!font) {
    return NULL;
  }
  font->fid = 1;
  font->ascent = 12;
  font->descent = 4;
  font->min_bounds.width = 8;
  font->max_bounds.width = 8;
  font->max_bounds.ascent = 12;
  font->max_bounds.descent = 4;
  return font;
}

int XFreeFont(Display* dpy, XFontStruct* font_struct) {
  (void)dpy;
  free(font_struct);
  return 1;
}

Pixmap XCreateBitmapFromData(Display* dpy, Drawable d, const char* data, unsigned int width, unsigned int height) {
  (void)dpy;
  (void)d;
  (void)data;
  (void)width;
  (void)height;
  return 2;
}

Cursor XCreateFontCursor(Display* dpy, unsigned int shape) {
  (void)dpy;
  (void)shape;
  return 1;
}

int XTextWidth(XFontStruct* font, const char* string, int count) {
  int advance = font && font->max_bounds.width > 0 ? font->max_bounds.width : 8;
  (void)string;
  return count > 0 ? count * advance : 0;
}
