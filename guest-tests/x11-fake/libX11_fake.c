/* Guest libX11.so.6 for programs that only need a display and a window id.
   Field stores use the Ubuntu Xlibint.h layout this file is compiled against. */
#define _GNU_SOURCE
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>
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
static Window g_input_window = 1;
static int g_focus_sent_win = 0;
static int g_input_selected = 0;
static int g_warp_x = 0;
static int g_warp_y = 0;
static int g_last_touch_x = -1;
static int g_last_touch_y = -1;
static Time g_last_event_time = 100;
static off_t g_pointer_off;
static int g_map[10];
static int g_map_on;
static KeySym g_sym_by_code[256];
static unsigned char g_key_down[32];
static unsigned int g_mod_state;
static int g_keys_ready;
static volatile int g_lock;

static Time vexa_next_time(void) {
  struct timespec ts;
  Time now = 0;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    now = (Time)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
  }
  if (now <= g_last_event_time) {
    now = g_last_event_time + 1;
  }
  g_last_event_time = now;
  return now;
}

/* X keycodes match the evdev map in libxkbcommon_x11_fake.c. SDL and Godot
   look these up by keycode, so ASCII (W = 119) is not a valid W. */
static const struct {
  unsigned int code;
  KeySym sym;
} k_key_table[] = {
    {9, 0xff1b},   /* Escape */
    {10, 0x31},    /* 1 */
    {11, 0x32},    /* 2 */
    {12, 0x33},    /* 3 */
    {13, 0x34},    /* 4 */
    {14, 0x35},    /* 5 */
    {15, 0x36},    /* 6 */
    {16, 0x37},    /* 7 */
    {17, 0x38},    /* 8 */
    {18, 0x39},    /* 9 */
    {19, 0x30},    /* 0 */
    {20, 0x2d},    /* minus */
    {21, 0x3d},    /* equal */
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
    {34, 0x5b},    /* bracketleft */
    {35, 0x5d},    /* bracketright */
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
    {47, 0x3b},    /* semicolon */
    {48, 0x27},    /* apostrophe */
    {49, 0x60},    /* grave */
    {50, 0xffe1},  /* Shift_L */
    {51, 0x5c},    /* backslash */
    {52, 0x7a},    /* z */
    {53, 0x78},    /* x */
    {54, 0x63},    /* c */
    {55, 0x76},    /* v */
    {56, 0x62},    /* b */
    {57, 0x6e},    /* n */
    {58, 0x6d},    /* m */
    {59, 0x2c},    /* comma */
    {60, 0x2e},    /* period */
    {61, 0x2f},    /* slash */
    {62, 0xffe2},  /* Shift_R */
    {64, 0xffe9},  /* Alt_L */
    {65, 0x20},    /* space */
    {66, 0xffe5},  /* Caps_Lock */
    {67, 0xffbe},  /* F1 */
    {68, 0xffbf},  /* F2 */
    {69, 0xffc0},  /* F3 */
    {70, 0xffc1},  /* F4 */
    {71, 0xffc2},  /* F5 */
    {72, 0xffc3},  /* F6 */
    {73, 0xffc4},  /* F7 */
    {74, 0xffc5},  /* F8 */
    {75, 0xffc6},  /* F9 */
    {76, 0xffc7},  /* F10 */
    {95, 0xffc8},  /* F11 */
    {96, 0xffc9},  /* F12 */
    {105, 0xffe4}, /* Control_R */
    {108, 0xffea}, /* Alt_R */
    {110, 0xff50}, /* Home */
    {111, 0xff52}, /* Up */
    {112, 0xff55}, /* Prior (Page_Up) */
    {113, 0xff51}, /* Left */
    {114, 0xff53}, /* Right */
    {115, 0xff57}, /* End */
    {116, 0xff54}, /* Down */
    {117, 0xff56}, /* Next (Page_Down) */
    {118, 0xff63}, /* Insert */
    {119, 0xffff}, /* Delete */
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
    case EnterNotify:
      return (mask & EnterWindowMask) != 0;
    case LeaveNotify:
      return (mask & LeaveWindowMask) != 0;
    case FocusIn:
    case FocusOut:
      return (mask & FocusChangeMask) != 0;
    case MapNotify:
    case UnmapNotify:
    case ConfigureNotify:
      return (mask & (StructureNotifyMask | SubstructureNotifyMask)) != 0;
    case PropertyNotify:
      return (mask & PropertyChangeMask) != 0;
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
    /* Resolution picked through XRRSetCrtcConfig overrides the surface size. */
    fd = open("/tmp/vexa-mode", O_RDONLY);
    if (fd >= 0) {
      char mbuf[64];
      int mn = (int)read(fd, mbuf, sizeof(mbuf) - 1);
      int mw = 0;
      int mh = 0;
      close(fd);
      if (mn > 0) {
        mbuf[mn] = 0;
        if (sscanf(mbuf, "%d %d", &mw, &mh) == 2 && mw > 1 && mh > 1) {
          width = mw;
          height = mh;
        }
      }
    }
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
  event.xconfigure.window = g_input_window;
  event.xconfigure.event = g_input_window;
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
/* Unity creates a small helper before its game window. Keep their geometry
   separate so queries on the game window never inherit the helper's size. */
struct vexa_window_geometry { int x, y, width, height, valid; };
static struct vexa_window_geometry g_windows[256];
static struct vexa_window_geometry* vexa_window(Window window) {
  return window < 256 && g_windows[window].valid ? &g_windows[window] : NULL;
}

static int g_cfg_sent;
static Window g_cfg_win;
static int g_cfg_x, g_cfg_y, g_cfg_w, g_cfg_h;

static void vexa_push_configure_win(Display* dpy, Window win, int x, int y, int w, int h) {
  XEvent event;
  if (win == 0) {
    win = g_input_window;
  }
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
  /* ETS2 answers every ConfigureNotify with another resize of the same
     window. The y nudge above would otherwise bounce 0 and 1 forever. */
  if (g_cfg_sent && g_cfg_win == win && g_cfg_x == x && g_cfg_y == y && g_cfg_w == w && g_cfg_h == h) {
    return;
  }
  g_cfg_sent = 1;
  g_cfg_win = win;
  g_cfg_x = x;
  g_cfg_y = y;
  g_cfg_w = w;
  g_cfg_h = h;
  fprintf(stderr, "[VEXA][X11] configure %d,%d %dx%d\n", x, y, w, h);
  memset(&event, 0, sizeof(event));
  event.type = ConfigureNotify;
  event.xconfigure.display = dpy;
  event.xconfigure.window = win;
  event.xconfigure.event = win;
  event.xconfigure.x = x;
  event.xconfigure.y = y;
  event.xconfigure.width = w;
  event.xconfigure.height = h;
  event.xconfigure.border_width = 0;
  event.xconfigure.above = None;
  event.xconfigure.override_redirect = False;
  vexa_q_push(&event);
}

static void vexa_push_configure(Display* dpy, int x, int y, int w, int h) {
  vexa_push_configure_win(dpy, g_input_window, x, y, w, h);
}

static void vexa_push_map_win(Display* dpy, Window win, int mapped) {
  XEvent event;
  if (win == 0) {
    win = g_input_window;
  }
  memset(&event, 0, sizeof(event));
  event.type = mapped ? MapNotify : UnmapNotify;
  if (mapped) {
    event.xmap.display = dpy;
    event.xmap.window = win;
    event.xmap.event = win;
    event.xmap.override_redirect = False;
  } else {
    event.xunmap.display = dpy;
    event.xunmap.window = win;
    event.xunmap.event = win;
  }
  vexa_q_push(&event);
}

static void vexa_push_map(Display* dpy, int mapped) {
  vexa_push_map_win(dpy, g_input_window, mapped);
}

static void vexa_push_focus(Display* dpy, Window win) {
  XEvent event;
  if (win == 0) {
    win = g_input_window;
  }
  memset(&event, 0, sizeof(event));
  event.type = EnterNotify;
  event.xcrossing.display = dpy;
  event.xcrossing.window = win;
  event.xcrossing.root = 1;
  event.xcrossing.subwindow = None;
  event.xcrossing.time = vexa_next_time();
  event.xcrossing.x = g_pointer_x;
  event.xcrossing.y = g_pointer_y;
  event.xcrossing.x_root = g_pointer_x;
  event.xcrossing.y_root = g_pointer_y;
  event.xcrossing.mode = NotifyNormal;
  event.xcrossing.detail = NotifyNonlinear;
  event.xcrossing.same_screen = True;
  event.xcrossing.focus = True;
  event.xcrossing.state = g_pointer_state | g_mod_state;
  vexa_q_push(&event);

  memset(&event, 0, sizeof(event));
  event.type = FocusIn;
  event.xfocus.display = dpy;
  event.xfocus.window = win;
  event.xfocus.mode = NotifyNormal;
  event.xfocus.detail = NotifyNonlinear;
  vexa_q_push(&event);
  g_focus_sent_win = (int)win;
}

static void vexa_push_property(Display* dpy, Atom atom, int state) {
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.type = PropertyNotify;
  event.xproperty.display = dpy;
  event.xproperty.window = g_input_window;
  event.xproperty.atom = atom;
  event.xproperty.state = state;
  event.xproperty.time = vexa_next_time();
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
  vexa_push_configure_win(dpy, g_input_window, x, y, w, h);
  if (g_focus_sent_win != (int)g_input_window) {
    vexa_push_focus(dpy, g_input_window);
  }
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

static KeySym vexa_shifted_sym(KeySym sym) {
  if (sym >= (KeySym)'a' && sym <= (KeySym)'z') {
    return sym - ('a' - 'A');
  }
  switch (sym) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case '[': return '{';
    case ']': return '}';
    case '\\': return '|';
    case ';': return ':';
    case '\'': return '"';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '`': return '~';
    default: return NoSymbol;
  }
}

static KeySym vexa_sym_for_code(unsigned int code, int level) {
  KeySym sym;
  vexa_init_keys();
  if (code >= 256) {
    return NoSymbol;
  }
  sym = g_sym_by_code[code];
  if (level == 1) {
    return vexa_shifted_sym(sym);
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
  event->xkey.window = g_input_window;
  event->xkey.root = 1;
  event->xkey.time = vexa_next_time();
  event->xkey.x = g_pointer_x;
  event->xkey.y = g_pointer_y;
  event->xkey.x_root = g_pointer_x;
  event->xkey.y_root = g_pointer_y;
  event->xkey.state = (state & 0x00ffffffu) | g_mod_state;
  event->xkey.keycode = vexa_code_for_sym(sym);
  event->xkey.subwindow = None;
  /* No evdev key for this symbol. Keep it on the event so text lookup can
     still return the character (Unicode keysyms are 0x01000000 + code point). */
  if (event->xkey.keycode == 0 && sym >= 0xa0) {
    event->xkey.subwindow = (Window)sym;
  }
  event->xkey.same_screen = True;
}

static int vexa_emit_char(char* buffer, int bytes, unsigned int cp) {
  if (!buffer || bytes <= 0 || cp < 32) {
    return 0;
  }
  if (cp < 128) {
    buffer[0] = (char)cp;
    return 1;
  }
  if (cp < 0x800) {
    if (bytes < 2) return 0;
    buffer[0] = (char)(0xc0 | (cp >> 6));
    buffer[1] = (char)(0x80 | (cp & 0x3f));
    return 2;
  }
  if (cp <= 0x10ffff) {
    if (bytes < 3) return 0;
    buffer[0] = (char)(0xe0 | (cp >> 12));
    buffer[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    buffer[2] = (char)(0x80 | (cp & 0x3f));
    return 3;
  }
  return 0;
}

static int vexa_lookup_key(XKeyEvent* event, char* buffer, int bytes, KeySym* keysym) {
  KeySym sym = NoSymbol;
  unsigned int cp = 0;
  if (event && event->keycode == 0 && event->subwindow >= 0xa0) {
    unsigned long packed = (unsigned long)event->subwindow;
    sym = (KeySym)packed;
    cp = packed >= 0x01000000ul ? (unsigned int)(packed & 0x1ffffful) : (unsigned int)packed;
  } else if (event) {
    int level = (event->state & ShiftMask) ? 1 : 0;
    sym = vexa_sym_for_code(event->keycode, level);
    cp = (unsigned int)sym;
  }
  if (keysym) {
    *keysym = sym;
  }
  /* Printable ASCII and Unicode keysyms become text. BackSpace, Return and
     the other 0xffxx controls stay key events, so lookup does not insert them. */
  {
    int printable = (sym >= 32 && sym < 127) || (sym >= 0x01000000);
    int written = printable ? vexa_emit_char(buffer, bytes, cp) : 0;
    if (written > 0) {
      return written;
    }
  }
  if (buffer && bytes > 0) {
    buffer[0] = 0;
  }
  return 0;
}

static unsigned int vexa_pointer_button(int state) {
  unsigned int button = ((unsigned int)state >> 24) & 0x3fu;
  if (button < 1 || button > 5) {
    return 1;
  }
  return button;
}

static void vexa_fill_pointer(XEvent* event, Display* dpy, int type, int x, int y, int state) {
  memset(event, 0, sizeof(*event));
  event->type = type;
  event->xbutton.display = dpy;
  event->xbutton.window = g_input_window;
  event->xbutton.root = 1;
  event->xbutton.time = vexa_next_time();
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

static int g_warp_centered = 0;
static int g_pointer_grabbed = 0;
/* One cursor shared by MotionNotify, clicks, and XQueryPointer. Guest
   warps move it exactly like a real X server does — pointer jumps to the
   target and a MotionNotify announces it — because clients mix two delta
   styles: diff consecutive events (needs the echo to not fake a jump) and
   diff against their own warp anchor (needs the pointer at the target). */
static int g_cursor_x = 0;
static int g_cursor_y = 0;
static int g_cursor_moved = 0;

static void vexa_step_cursor(int dx, int dy) {
  if (!g_cursor_moved) {
    /* 0,0 is a real warp target. Promoting it to the screen centre makes
       ETS2 add the centre again and the cursor can only travel the
       bottom-right quarter until the pointer is pushed off the screen. */
    g_cursor_x = g_pointer_x;
    g_cursor_y = g_pointer_y;
  }
  g_cursor_x += dx;
  g_cursor_y += dy;
  if (g_cursor_x < 0) g_cursor_x = 0;
  if (g_cursor_y < 0) g_cursor_y = 0;
  if (g_screen.width > 1 && g_cursor_x >= g_screen.width) g_cursor_x = g_screen.width - 1;
  if (g_screen.height > 1 && g_cursor_y >= g_screen.height) g_cursor_y = g_screen.height - 1;
  g_cursor_moved = 1;
  g_pointer_x = g_cursor_x;
  g_pointer_y = g_cursor_y;
}

static int vexa_absolute_pointer(void) {
  const char* mode = getenv("VEXA_X11_ABSOLUTE_POINTER");
  return mode && strcmp(mode, "1") == 0;
}

/* Opt-in, bounded trace of the Android/X11/guest-warp coordinate boundary. */
static void vexa_trace_pointer(const char* kind, int x, int y, int dx, int dy, int out_x, int out_y,
                               Window window) {
  int flag = open("/data/data/com.critical.vexaemulator/files/rootfs/tmp/vexa-input-trace.flag", O_RDONLY);
  static int checked;
  if (!checked++) fprintf(stderr, "[VEXA][INPUT-TRACE] flag_fd=%d errno=%d\n", flag, flag < 0 ? errno : 0);
  if (flag < 0) return;
  close(flag);
  int file = open("/data/data/com.critical.vexaemulator/files/rootfs/tmp/vexa-input-trace.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (file < 0) return;
  if (lseek(file, 0, SEEK_END) > 1024 * 1024) ftruncate(file, 0);
  char line[256];
  int n = snprintf(line, sizeof(line), "%s win=%lu input=%d,%d delta=%d,%d output=%d,%d pointer=%d,%d centered=%d screen=%dx%d\n",
          kind, (unsigned long)window, x, y, dx, dy, out_x, out_y,
          g_pointer_x, g_pointer_y, g_warp_centered, g_screen.width, g_screen.height);
  if (n > 0 && n < (int)sizeof(line)) write(file, line, (size_t)n);
  close(file);
}

static void vexa_relative_pointer(int dx, int dy, int* x, int* y) {
  *x = g_pointer_x + dx;
  *y = g_pointer_y + dy;
  if (*x < 0) *x = 0;
  if (*y < 0) *y = 0;
  if (*x >= g_screen.width) *x = g_screen.width - 1;
  if (*y >= g_screen.height) *y = g_screen.height - 1;
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
        fprintf(stderr, "[VEXA][X11] key %s sym=0x%x code=%u win=%lu\n", type == KeyPress ? "down" : "up", (unsigned)x,
                event.xkey.keycode, (unsigned long)event.xkey.window);
      }
      vexa_q_push_locked(&event);
      got++;
      continue;
    }
    if (type != ButtonPress && type != ButtonRelease && type != MotionNotify) {
      continue;
    }
    {
      int synth = (((uint32_t)state & 0x40000000u) != 0);
      int raw_x = synth && type == MotionNotify ? (int)(uint16_t)((uint32_t)x & 0xffffu) : (int)x;
      int raw_y = synth && type == MotionNotify ? (int)(uint16_t)((uint32_t)y & 0xffffu) : (int)y;
      int rel_dx = synth && type == MotionNotify ? (int)(int16_t)((uint32_t)x >> 16) : 0;
      int rel_dy = synth && type == MotionNotify ? (int)(int16_t)((uint32_t)y >> 16) : 0;
      int mx = raw_x;
      int my = raw_y;
      vexa_map_pointer(&mx, &my);
      if (!synth) {
        if (type == ButtonPress) {
          g_last_touch_x = mx;
          g_last_touch_y = my;
        } else if (type == MotionNotify) {
          if (g_last_touch_x >= 0 && g_last_touch_y >= 0) {
            rel_dx = mx - g_last_touch_x;
            rel_dy = my - g_last_touch_y;
          }
          g_last_touch_x = mx;
          g_last_touch_y = my;
        } else if (type == ButtonRelease) {
          g_last_touch_x = -1;
          g_last_touch_y = -1;
        }
      }
      if (type == MotionNotify && g_warp_centered) {
        if (rel_dx == 0 && rel_dy == 0) {
          continue;
        }
        /* ETS2 reads the motion as a delta from the last event or from
           XQueryPointer. Those two have to be the same cursor. Reporting
           the centre, or only the step, disagrees by about half the screen
           and the cursor clamps into a corner. */
        vexa_step_cursor(rel_dx, rel_dy);
        mx = g_pointer_x;
        my = g_pointer_y;
      } else if ((type == ButtonPress || type == ButtonRelease) && g_warp_centered) {
        mx = g_cursor_moved ? g_cursor_x : g_pointer_x;
        my = g_cursor_moved ? g_cursor_y : g_pointer_y;
      } else {
        g_cursor_x = mx;
        g_cursor_y = my;
      }
      vexa_fill_pointer(&event, dpy, (int)type, mx, my, (int)state);
      vexa_trace_pointer(synth ? "pad-motion" : "mouse-touch", raw_x, raw_y, rel_dx, rel_dy, mx, my, g_input_window);
      g_pointer_x = mx;
      g_pointer_y = my;
      g_pointer_state = (unsigned int)state & 0x00ffffffu;
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

#include "unity_native_mode.h"

Display* XOpenDisplay(const char* name) {
  /* Display is a pointer typedef, so sizeof(Display) is 8. Callers that keep
     the handle (Unity reads ScreenOfDisplay(dpy, 0)->width_in_pixels through
     the Xlib macros) walk the real struct, so allocate the whole thing. */
  Display* dpy = (Display*)calloc(1, sizeof(*dpy));
  if (!dpy) {
    return NULL;
  }
  setenv("SDL_MOUSE_RELATIVE_MODE_WARP", "1", 0);
  vexa_init_screen(dpy);
  vexa_unity_native_mode(__builtin_return_address(0));
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
  if (win < 256) g_windows[win] = (struct vexa_window_geometry){x, y, (int)width, (int)height, 1};
  /* Godot asks for the project size (1920x1080). The published surface is the
     real screen, and a later ConfigureNotify makes Godot adopt it. */
  if (win == 1 || (width > 32 && height > 32)) {
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
  if (!g_input_selected && width > 32 && height > 32) {
    g_input_window = win;
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
  struct vexa_window_geometry* geom = vexa_window(window);
  int cfg_w = (g_published && g_screen.width > 1) ? g_screen.width : (geom && geom->width > 1 ? geom->width : (g_win_w > 1 ? g_win_w : g_screen.width));
  int cfg_h = (g_published && g_screen.height > 1) ? g_screen.height : (geom && geom->height > 1 ? geom->height : (g_win_h > 1 ? g_win_h : g_screen.height));
  fprintf(stderr, "[VEXA][X11] map_window %lu\n", (unsigned long)window);
  if (window >= 1) {
    if (!g_input_selected && geom && geom->width > 32 && geom->height > 32) {
      g_input_window = window;
    }
    vexa_push_map_win(dpy, window, 1);
    vexa_push_configure_win(dpy, window, g_published ? 0 : g_win_x, g_published ? 0 : g_win_y, cfg_w, cfg_h);
    vexa_push_focus(dpy, window);
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
  vexa_push_map_win(dpy, window, 0);
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
  event->xexpose.window = g_input_window;
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
  if (g_focus_sent_win != (int)g_input_window) {
    vexa_push_focus(dpy, g_input_window);
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
  {
    static int logged;
    if (logged < 16) {
      logged++;
      fprintf(stderr, "[VEXA][X11] change_property w=%lu prop=%lu n=%d\n", (unsigned long)window,
              (unsigned long)property, nelements);
    }
  }
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
  const struct vexa_window_geometry* geometry = window > 1 ? vexa_window(window) : NULL;
  attrs->x = geometry ? geometry->x : (g_win_valid ? g_win_x : 0);
  attrs->y = geometry ? geometry->y : (g_win_valid ? g_win_y : 0);
  attrs->width = geometry ? geometry->width : (g_win_valid && g_win_w > 0 ? g_win_w : g_screen.width);
  attrs->height = geometry ? geometry->height : (g_win_valid && g_win_h > 0 ? g_win_h : g_screen.height);
  attrs->depth = 24;
  attrs->visual = &g_visual;
  attrs->class = TrueColor;
  attrs->map_state = IsViewable;
  attrs->screen = &g_screen;
  {
    static int logged;
    if (logged < 16) {
      logged++;
      fprintf(stderr, "[VEXA][X11] get_window_attributes w=%lu -> %dx%d (screen %dx%d)\n", (unsigned long)window,
              attrs->width, attrs->height, g_screen.width, g_screen.height);
    }
  }
  return 1;
}

int XGetInputFocus(Display* dpy, Window* focus, int* revert_to) {
  (void)dpy;
  /* Report the input target rather than the first helper/root window. */
  if (focus) {
    *focus = g_input_window;
  }
  if (revert_to) {
    *revert_to = 0;
  }
  return 0;
}

int XSetInputFocus(Display* dpy, Window window, int revert_to, Time time) {
  if (window > PointerRoot) {
    g_input_window = window;
    vexa_push_focus(dpy, window);
  }
  (void)revert_to;
  (void)time;
  return 0;
}

Status XGetGeometry(Display* dpy, Drawable drawable, Window* root, int* x, int* y, unsigned int* width, unsigned int* height,
                    unsigned int* border, unsigned int* depth) {
  (void)dpy;
  (void)drawable;
  vexa_read_window_size();
  const struct vexa_window_geometry* geometry = drawable > 1 ? vexa_window(drawable) : NULL;
  if (root) {
    *root = 1;
  }
  if (x) {
    *x = geometry ? geometry->x : (g_win_valid ? g_win_x : 0);
  }
  if (y) {
    *y = geometry ? geometry->y : (g_win_valid ? g_win_y : 0);
  }
  if (width) {
    *width = (unsigned int)(geometry ? geometry->width : (g_win_valid && g_win_w > 0 ? g_win_w : g_screen.width));
  }
  if (height) {
    *height = (unsigned int)(geometry ? geometry->height : (g_win_valid && g_win_h > 0 ? g_win_h : g_screen.height));
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
  struct vexa_window_geometry* geometry = vexa_window(window);
  if (geometry) { geometry->width = (int)width; geometry->height = (int)height; }
  fprintf(stderr, "[VEXA][X11] resize_window w=%lu %ux%u\n", (unsigned long)window, width, height);
  if (window != 1 && window != g_input_window) {
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
  struct vexa_window_geometry* geometry = vexa_window(window);
  if (geometry) { geometry->x = x; geometry->y = y; }
  (void)dpy;
  if (window != 1 && window != g_input_window) {
    return 1;
  }
  if (!g_published) {
    g_win_x = (x < -100000 || x > 100000) ? 0 : x;
    g_win_y = (y < -100000 || y > 100000) ? 0 : y;
  }
  return 1;
}

int XSelectInput(Display* dpy, Window window, long mask) {
  if (mask & (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask)) {
    g_input_window = window;
    g_input_selected = 1;
    fprintf(stderr, "[VEXA][X11] input_window=%lu mask=0x%lx\n", window, mask);
    vexa_push_focus(dpy, window);
  }
  return 1;
}

static int g_clear_configured = 0;

int XClearWindow(Display* dpy, Window window) {
  if ((window == 1 || window == g_input_window) && !g_clear_configured && g_published && g_screen.width > 1 && g_screen.height > 1) {
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
  g_pointer_grabbed = 1;
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
  struct vexa_window_geometry* geometry = vexa_window(window);
  if (geometry) { geometry->x = x; geometry->y = y; geometry->width = (int)width; geometry->height = (int)height; }
  fprintf(stderr, "[VEXA][X11] move_resize_window w=%lu %d,%d %ux%u\n", (unsigned long)window, x, y, width,
          height);
  if (window != 1 && window != g_input_window) {
    return 1;
  }
  vexa_configure(dpy, x, y, (int)width, (int)height);
  return 1;
}

int XConfigureWindow(Display* dpy, Window window, unsigned int valuemask, XWindowChanges* values) {
  struct vexa_window_geometry* geometry = vexa_window(window);
  if (geometry && values) {
    if (valuemask & CWX) geometry->x = values->x;
    if (valuemask & CWY) geometry->y = values->y;
    if (valuemask & CWWidth) geometry->width = values->width;
    if (valuemask & CWHeight) geometry->height = values->height;
  }
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
  if (window != 1 && window != g_input_window) {
    return 1;
  }
  vexa_configure(dpy, x, y, w, h);
  return 1;
}

/* Unity embeds SDL2. With XI2 present SDL waits for XI_RawMotion generic events
   in relative-mouse mode, which this fake never produces, so camera look is dead.
   Hiding XI2 from UnityPlayer.so makes SDL fall back to warp-based relative mode,
   which the MotionNotify/XWarpPointer path above feeds. */
static int vexa_caller_is_unity(void* caller) {
  Dl_info module;
  const char* name;
  if (!dladdr(caller, &module) || !module.dli_fname) {
    return 0;
  }
  name = strrchr(module.dli_fname, '/');
  return strcmp(name ? name + 1 : module.dli_fname, "UnityPlayer.so") == 0;
}

Bool XQueryExtension(Display* dpy, const char* name, int* major, int* event, int* error) {
  int unity = vexa_caller_is_unity(__builtin_return_address(0));
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
  if (name && !unity && strcmp(name, "XInputExtension") == 0) {
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
  /* Polling clients must see input even if they have not called XPending. */
  vexa_poll_pointer(dpy);
  vexa_trace_pointer("query", g_pointer_x, g_pointer_y, 0, 0, g_pointer_x, g_pointer_y, window);
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
    /* ETS2 recentering sends itself a ClientMessage carrying the warp
       target (window centre) and resyncs its motion baseline when it reads
       the message back. Swallowing it leaves the baseline stale, so every
       recenter warp reads as a delta of (centre - old) and the cursor
       snaps. Deliver only that self-message -- it targets the game's own
       window and its payload is the centre point. Delivering WM/EWMH
       messages back to the game can be read as a close request (the game
       quit shortly after launch when every ClientMessage was queued). */
    fprintf(stderr, "[VEXA][X11] client_message win=%lu type=%lu fmt=%d l0=%ld l1=%ld\n",
            (unsigned long)event->xclient.window, (unsigned long)event->xclient.message_type,
            event->xclient.format, (long)event->xclient.data.l[0], (long)event->xclient.data.l[1]);
    int centered = 0;
    if (g_screen.width > 1 && g_screen.height > 1 &&
        event->xclient.data.l[0] == g_screen.width / 2 && event->xclient.data.l[1] == g_screen.height / 2) {
      centered = 1;
    }
    if (g_win_w > 1 && g_win_h > 1 &&
        event->xclient.data.l[0] == g_win_w / 2 && event->xclient.data.l[1] == g_win_h / 2) {
      centered = 1;
    }
    if (event->xclient.format == 32 && event->xclient.window == g_input_window && centered) {
      vexa_q_push(event);
    }
    /* A real WM also consumes EWMH client messages (e.g. _NET_WM_STATE
       fullscreen toggles) and answers with _NET_WM_STATE property
       notification plus a ConfigureNotify at the display bounds. SDL waits
       on exactly that pair before it reports the window as
       fullscreen/on-display. */
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
  g_warp_centered = 0;
  g_pointer_grabbed = 0;
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
  XEvent event;
  Window target = dest ? dest : g_input_window;
  const struct vexa_window_geometry* geom = dest > 1 ? vexa_window(dest) : NULL;
  int wx = dest_x;
  int wy = dest_y;
  if (dest == None) {
    /* None means relative displacement, not root coordinates. In particular
       (0,0) must not reset the pointer or create a spurious motion event. */
    if (dest_x == 0 && dest_y == 0) return 1;
    wx += g_pointer_x;
    wy += g_pointer_y;
  }
  (void)src;
  (void)src_x;
  (void)src_y;
  (void)src_width;
  (void)src_height;
  if (geom && geom->width <= 2 && geom->height <= 2 && dest_x == 0 && dest_y == 0) {
    wx = g_screen.width > 1 ? g_screen.width / 2 : 0;
    wy = g_screen.height > 1 ? g_screen.height / 2 : 0;
  }
  {
    static int logged;
    if (logged < 4) {
      logged++;
      fprintf(stderr, "[VEXA][X11] warp_pointer src=%lu dest=%lu -> %d,%d (eff %d,%d)\n",
              (unsigned long)src, (unsigned long)dest, dest_x, dest_y, wx, wy);
    }
  }
  /* Absolute warps only move the anchor. Echoing one as MotionNotify is a
     delta of about half the screen and pins the cursor to the bottom-right. */
  {
    int cx = g_screen.width > 1 ? g_screen.width / 2 : 0;
    int cy = g_screen.height > 1 ? g_screen.height / 2 : 0;
    int near_center = abs(wx - cx) <= 2 && abs(wy - cy) <= 2;
    /* ETS2 drives its virtual cursor like SDL's RELATIVE_MODE_WARP: it
       re-centres the X pointer and measures deltas from that anchor. Real
       X semantics keep every consumer consistent: move the pointer to the
       target AND emit the MotionNotify — the client caused the warp, so it
       expects (and compensates) the event. Silently teleporting injects a
       fake delta for consecutive-diff clients (cursor snaps to centre),
       while ignoring the warp leaves query-vs-anchor clients a constant
       offset that drifts the reachable region every reset. */
    if (dest != None && ((wx == 0 && wy == 0) || near_center || g_warp_centered ||
                         (g_pointer_grabbed && vexa_absolute_pointer()))) {
      g_warp_centered = 1;
      g_warp_x = wx;
      g_warp_y = wy;
      g_cursor_x = wx;
      g_cursor_y = wy;
      g_pointer_x = wx;
      g_pointer_y = wy;
      vexa_fill_pointer(&event, dpy, MotionNotify, wx, wy, (int)g_pointer_state);
      event.xmotion.window = target;
      vexa_trace_pointer(wx == 0 && wy == 0 ? "warp-origin" : "warp-anchor",
                         dest_x, dest_y, 0, 0, wx, wy, target);
      vexa_q_push(&event);
      return 1;
    }
  }
  g_warp_x = wx;
  vexa_trace_pointer("warp", dest_x, dest_y, (int)src, (int)dest, wx, wy, target);
  g_warp_y = wy;
  g_warp_centered = 0;
  g_cursor_x = wx;
  g_cursor_y = wy;
  g_pointer_x = wx;
  g_pointer_y = wy;
  vexa_fill_pointer(&event, dpy, MotionNotify, wx, wy, (int)g_pointer_state);
  event.xmotion.window = target;
  vexa_q_push(&event);
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
  // Non-NULL dummy: ETS2 treats a NULL IM as fatal and tears down the display.
  // Opt-in only (VEXA_FAKE_IM=1): Unity/SDL games cope with NULL and crash
  // (strlen(NULL) in the Unity main thread) when handed a dummy handle.
  static int g_fake_im;
  const char* fake_im = getenv("VEXA_FAKE_IM");
  return (fake_im && fake_im[0] == '1') ? (XIM)&g_fake_im : NULL;
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
  static int g_fake_ic;
  const char* fake_im = getenv("VEXA_FAKE_IM");
  return (fake_im && fake_im[0] == '1') ? (XIC)&g_fake_ic : NULL;
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

/* ---- ETS2 extras ---- */

// XrmUniqueQuark: each call must return a new quark; fake atoms start at 1, so
// quarks live in a disjoint high range to avoid colliding with XInternAtom.
XrmQuark XrmUniqueQuark(void) {
  static XrmQuark next = 0x10000;
  return next++;
}

Status XQueryColors(Display* dpy, Colormap cmap, XColor* defs, int ncolors) {
  (void)dpy;
  (void)cmap;
  for (int i = 0; i < ncolors; ++i) {
    unsigned long p = defs[i].pixel;
    defs[i].red = (unsigned short)(((p >> 16) & 0xff) * 257);
    defs[i].green = (unsigned short)(((p >> 8) & 0xff) * 257);
    defs[i].blue = (unsigned short)((p & 0xff) * 257);
    defs[i].flags = DoRed | DoGreen | DoBlue;
  }
  return 1;
}

// XSaveContext/XDeleteContext back a (window,context)->data map. ETS2 saves a
// context on its window; keep a tiny real map so behavior stays sane.
typedef struct FakeXContextEntry {
  XID rid;
  XContext ctx;
  void* data;
} FakeXContextEntry;
static FakeXContextEntry g_xcontexts[64];
static int g_xcontext_count;

int XSaveContext(Display* dpy, XID rid, XContext context, _Xconst char* data) {
  (void)dpy;
  for (int i = 0; i < g_xcontext_count; ++i) {
    if (g_xcontexts[i].rid == rid && g_xcontexts[i].ctx == context) {
      g_xcontexts[i].data = (void*)data;
      return 0;
    }
  }
  if (g_xcontext_count >= 64) return XCNOMEM;
  g_xcontexts[g_xcontext_count++] = (FakeXContextEntry){rid, context, (void*)data};
  return 0;
}

int XFindContext(Display* dpy, XID rid, XContext context, char** data) {
  (void)dpy;
  for (int i = 0; i < g_xcontext_count; ++i) {
    if (g_xcontexts[i].rid == rid && g_xcontexts[i].ctx == context) {
      *data = (char*)g_xcontexts[i].data;
      return 0;
    }
  }
  return XCNOENT;
}

int XDeleteContext(Display* dpy, XID rid, XContext context) {
  (void)dpy;
  for (int i = 0; i < g_xcontext_count; ++i) {
    if (g_xcontexts[i].rid == rid && g_xcontexts[i].ctx == context) {
      g_xcontexts[i] = g_xcontexts[--g_xcontext_count];
      return 0;
    }
  }
  return XCNOENT;
}

void XkbFreeNames(XkbDescPtr xkb, unsigned int which, Bool free_map) {
  (void)xkb;
  (void)which;
  (void)free_map;
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
    if (desc->names->keys) {
      /* xkeyboard-config "evdev" key names. ETS2 maps physical keys by these
       * four-character names (AE01, AD01, SPCE, ...). */
      static const struct { unsigned char code; char name[5]; } k_evdev_names[] = {
        {9, "ESC"}, {10, "AE01"}, {11, "AE02"}, {12, "AE03"}, {13, "AE04"}, {14, "AE05"},
        {15, "AE06"}, {16, "AE07"}, {17, "AE08"}, {18, "AE09"}, {19, "AE10"}, {20, "AE11"},
        {21, "AE12"}, {22, "BKSP"}, {23, "TAB"}, {24, "AD01"}, {25, "AD02"}, {26, "AD03"},
        {27, "AD04"}, {28, "AD05"}, {29, "AD06"}, {30, "AD07"}, {31, "AD08"}, {32, "AD09"},
        {33, "AD10"}, {34, "AD11"}, {35, "AD12"}, {36, "RTRN"}, {37, "LCTL"}, {38, "AC01"},
        {39, "AC02"}, {40, "AC03"}, {41, "AC04"}, {42, "AC05"}, {43, "AC06"}, {44, "AC07"},
        {45, "AC08"}, {46, "AC09"}, {47, "AC10"}, {48, "AC11"}, {49, "TLDE"}, {50, "LFSH"},
        {51, "BKSL"}, {52, "AB01"}, {53, "AB02"}, {54, "AB03"}, {55, "AB04"}, {56, "AB05"},
        {57, "AB06"}, {58, "AB07"}, {59, "AB08"}, {60, "AB09"}, {61, "AB10"}, {62, "RTSH"},
        {63, "KPMU"}, {64, "LALT"}, {65, "SPCE"}, {66, "CAPS"}, {67, "FK01"}, {68, "FK02"},
        {69, "FK03"}, {70, "FK04"}, {71, "FK05"}, {72, "FK06"}, {73, "FK07"}, {74, "FK08"},
        {75, "FK09"}, {76, "FK10"}, {77, "NMLK"}, {78, "SCLK"}, {79, "KP7"}, {80, "KP8"},
        {81, "KP9"}, {82, "KPSU"}, {83, "KP4"}, {84, "KP5"}, {85, "KP6"}, {86, "KPAD"},
        {87, "KP1"}, {88, "KP2"}, {89, "KP3"}, {90, "KP0"}, {91, "KPDL"}, {94, "LSGT"},
        {95, "FK11"}, {96, "FK12"}, {104, "KPEN"}, {105, "RCTL"}, {106, "KPDV"}, {107, "PRSC"},
        {108, "RALT"}, {110, "HOME"}, {111, "UP"}, {112, "PGUP"}, {113, "LEFT"}, {114, "RGHT"},
        {115, "END"}, {116, "DOWN"}, {117, "PGDN"}, {118, "INS"}, {119, "DELE"}, {125, "KPEQ"},
        {127, "PAUS"}, {133, "LWIN"}, {134, "RWIN"}, {135, "COMP"},
      };
      size_t i;
      for (i = 0; i < sizeof(k_evdev_names) / sizeof(k_evdev_names[0]); ++i) {
        if ((int)k_evdev_names[i].code < count) {
          /* XkbKeyNameRec.name is char[4] and not NUL terminated. */
          memcpy(desc->names->keys[k_evdev_names[i].code].name, k_evdev_names[i].name, 4);
        }
      }
    }
  }
  return Success;
}

/* ETS2 reads the keyboard layout through XkbGetMap + XkbGetNames. The old
 * sdl-stubs XkbGetMap returned NULL, so the game dereferenced desc->names
 * (offset 0x30 of a NULL XkbDescRec) and crashed right after Steam init. */
XkbDescPtr XkbGetMap(Display* dpy, unsigned int which, unsigned int device_spec) {
  XkbDescPtr desc;
  (void)dpy;
  (void)which;
  desc = XkbAllocKeyboard();
  if (desc) {
    desc->device_spec = (unsigned short)device_spec;
  }
  return desc;
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
