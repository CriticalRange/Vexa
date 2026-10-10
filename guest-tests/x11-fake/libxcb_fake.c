/* Guest libxcb.so.1.
 *
 * DISPLAY is an environment variable, not an entry point. xcb_connect reads it
 * and, on a desktop, opens a socket to an X server. There is no X server here.
 * A set DISPLAY means "this connection's screen is the VEXA Android surface".
 * The guest later passes the connection and window id to vkCreateXcbSurfaceKHR,
 * and the host Vulkan thunk presents on the ANativeWindow. Core requests use
 * the Ubuntu xcb/xproto.h layouts so cookies, setup bytes, and replies match
 * the real library. This is not an X protocol implementation: XPutImage, the
 * XKB extension, and a socket fd are not forwarded to Android.
 */
#include <xcb/xcb.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Static_assert(sizeof(xcb_setup_t) == 40, "xcb_setup_t layout");
_Static_assert(sizeof(xcb_format_t) == 8, "xcb_format_t layout");
_Static_assert(sizeof(xcb_screen_t) == 40, "xcb_screen_t layout");
_Static_assert(sizeof(xcb_depth_t) == 8, "xcb_depth_t layout");
_Static_assert(sizeof(xcb_visualtype_t) == 24, "xcb_visualtype_t layout");
_Static_assert(sizeof(xcb_void_cookie_t) == sizeof(unsigned int), "cookie layout");
_Static_assert(sizeof(xcb_intern_atom_cookie_t) == sizeof(unsigned int), "atom cookie");
_Static_assert(sizeof(xcb_configure_notify_event_t) <= 32, "configure event fits a wire event");
_Static_assert(offsetof(xcb_configure_notify_event_t, width) == 20, "configure width offset");

#define VEXA_XID_BASE 0x00200000u
#define VEXA_XID_MASK 0x001fffffu
#define VEXA_ATOM_BASE 256u
#define VEXA_MAX_ATOMS 64
#define VEXA_MAX_EVENTS 32
#define VEXA_MAX_PENDING 16

struct vexa_atom {
  xcb_atom_t id;
  uint16_t len;
  char name[64];
};

struct vexa_event_node {
  xcb_generic_event_t event;
};

struct vexa_pending_atom {
  unsigned int sequence;
  uint8_t only_if_exists;
  uint16_t name_len;
  char name[64];
};

struct xcb_connection_t {
  int error;
  int screen;
  unsigned int sequence;
  uint32_t next_id;
  unsigned char* setup_bytes;
  struct vexa_event_node events[VEXA_MAX_EVENTS];
  int event_head;
  int event_count;
  struct vexa_pending_atom pending[VEXA_MAX_PENDING];
  int pending_count;
  xcb_window_t last_window;
};

static int g_announced_width;
static int g_announced_height;

static struct vexa_atom g_atoms[VEXA_MAX_ATOMS];
static int g_atom_count;
static uint32_t g_next_atom = VEXA_ATOM_BASE;

static unsigned int vexa_pad4(unsigned int n) {
  return (n + 3u) & ~3u;
}

static char* vexa_after_vendor(const xcb_setup_t* setup) {
  return (char*)(setup + 1) + vexa_pad4(setup->vendor_len);
}

static const char* vexa_screen_end(const xcb_screen_t* screen) {
  const char* cursor = (const char*)(screen + 1);
  int remaining = screen->allowed_depths_len;
  while (remaining-- > 0) {
    const xcb_depth_t* depth = (const xcb_depth_t*)cursor;
    cursor += sizeof(*depth);
    cursor += (size_t)depth->visuals_len * sizeof(xcb_visualtype_t);
  }
  return cursor;
}

static unsigned char* vexa_build_setup(void) {
  unsigned char* bytes = (unsigned char*)calloc(1, 256);
  xcb_setup_t* setup;
  xcb_screen_t* screen;
  xcb_depth_t* depth;
  xcb_visualtype_t* visual;
  char* cursor;
  size_t total;
  if (!bytes) {
    return NULL;
  }
  setup = (xcb_setup_t*)bytes;
  setup->status = 1;
  setup->protocol_major_version = 11;
  setup->protocol_minor_version = 0;
  setup->release_number = 1;
  setup->resource_id_base = VEXA_XID_BASE;
  setup->resource_id_mask = VEXA_XID_MASK;
  setup->motion_buffer_size = 256;
  setup->vendor_len = 4;
  setup->maximum_request_length = 65535;
  setup->roots_len = 1;
  setup->pixmap_formats_len = 0;
  setup->image_byte_order = XCB_IMAGE_ORDER_LSB_FIRST;
  setup->bitmap_format_bit_order = XCB_IMAGE_ORDER_LSB_FIRST;
  setup->bitmap_format_scanline_unit = 32;
  setup->bitmap_format_scanline_pad = 32;
  setup->min_keycode = 8;
  setup->max_keycode = 255;
  memcpy(bytes + sizeof(*setup), "VEXA", 4);

  cursor = vexa_after_vendor(setup);
  screen = (xcb_screen_t*)cursor;
  screen->root = 1;
  screen->default_colormap = 2;
  screen->white_pixel = 0x00ffffff;
  screen->black_pixel = 0;
  screen->width_in_pixels = 1280;
  screen->height_in_pixels = 720;
  screen->width_in_millimeters = 300;
  screen->height_in_millimeters = 170;
  screen->min_installed_maps = 1;
  screen->max_installed_maps = 1;
  screen->root_visual = 1;
  screen->backing_stores = XCB_BACKING_STORE_NOT_USEFUL;
  screen->root_depth = 24;
  screen->allowed_depths_len = 1;

  depth = (xcb_depth_t*)(screen + 1);
  depth->depth = 24;
  depth->visuals_len = 1;
  visual = (xcb_visualtype_t*)(depth + 1);
  visual->visual_id = 1;
  visual->_class = XCB_VISUAL_CLASS_TRUE_COLOR;
  visual->bits_per_rgb_value = 8;
  visual->colormap_entries = 256;
  visual->red_mask = 0x00ff0000;
  visual->green_mask = 0x0000ff00;
  visual->blue_mask = 0x000000ff;

  total = (size_t)((unsigned char*)(visual + 1) - bytes);
  setup->length = (uint16_t)((total - 8u) / 4u);
  return bytes;
}

static int vexa_parse_screen(const char* name) {
  const char* colon = strrchr(name, ':');
  const char* dot;
  if (!colon || !isdigit((unsigned char)colon[1])) {
    return -1;
  }
  dot = strchr(colon, '.');
  if (!dot || dot[1] == '\0') {
    return 0;
  }
  return atoi(dot + 1);
}

static xcb_connection_t* vexa_connection(int error, int screen) {
  xcb_connection_t* connection = (xcb_connection_t*)calloc(1, sizeof(*connection));
  if (!connection) {
    return NULL;
  }
  connection->error = error;
  connection->screen = screen;
  connection->sequence = 1;
  connection->next_id = VEXA_XID_BASE;
  if (!error) {
    connection->setup_bytes = vexa_build_setup();
    if (!connection->setup_bytes) {
      connection->error = XCB_CONN_CLOSED_MEM_INSUFFICIENT;
    }
  }
  return connection;
}

static xcb_void_cookie_t vexa_cookie(xcb_connection_t* connection) {
  xcb_void_cookie_t cookie;
  cookie.sequence = 0;
  if (!connection || connection->error) {
    return cookie;
  }
  cookie.sequence = connection->sequence++;
  return cookie;
}

static void vexa_enqueue(xcb_connection_t* connection, const xcb_generic_event_t* event);

static void vexa_enqueue_configure(xcb_connection_t* connection, uint16_t width, uint16_t height) {
  xcb_configure_notify_event_t configure;
  xcb_generic_event_t event;
  memset(&configure, 0, sizeof(configure));
  configure.response_type = XCB_CONFIGURE_NOTIFY;
  configure.event = connection->last_window;
  configure.window = connection->last_window;
  configure.width = width;
  configure.height = height;
  memset(&event, 0, sizeof(event));
  memcpy(&event, &configure, sizeof(configure));
  vexa_enqueue(connection, &event);
}

/* The host writes this after the Android window has a real size. vkgears
   creates its swapchain at the size passed to xcb_create_window (300x300) and
   only looks at ConfigureNotify once the frame loop is running. */
static void vexa_note_window_size(xcb_connection_t* connection) {
  FILE* file;
  int width = 0;
  int height = 0;
  if (!connection || connection->error) {
    return;
  }
  file = fopen("/tmp/vexa-window-size", "r");
  if (!file) {
    return;
  }
  if (fscanf(file, "%d %d", &width, &height) != 2) {
    fclose(file);
    return;
  }
  fclose(file);
  if (width <= 1 || height <= 1 || width > 65535 || height > 65535) {
    return;
  }
  if (width == g_announced_width && height == g_announced_height) {
    return;
  }
  g_announced_width = width;
  g_announced_height = height;
  vexa_enqueue_configure(connection, (uint16_t)width, (uint16_t)height);
}

static void vexa_enqueue(xcb_connection_t* connection, const xcb_generic_event_t* event) {
  int slot;
  if (!connection || connection->error || connection->event_count >= VEXA_MAX_EVENTS) {
    return;
  }
  slot = (connection->event_head + connection->event_count) % VEXA_MAX_EVENTS;
  connection->events[slot].event = *event;
  connection->event_count++;
}

xcb_connection_t* xcb_connect(const char* displayname, int* screenp) {
  const char* name = displayname;
  int screen = 0;
  int error = 0;
  if (!name || name[0] == '\0') {
    name = getenv("DISPLAY");
  }
  /* Unset or empty DISPLAY is "no display", same rule vulkaninfo uses. */
  if (!name || name[0] == '\0') {
    error = XCB_CONN_CLOSED_PARSE_ERR;
  } else {
    screen = vexa_parse_screen(name);
    if (screen < 0) {
      error = XCB_CONN_CLOSED_PARSE_ERR;
    } else if (screen != 0) {
      error = XCB_CONN_CLOSED_INVALID_SCREEN;
      screen = 0;
    }
  }
  if (screenp) {
    *screenp = screen;
  }
  return vexa_connection(error, screen);
}

xcb_connection_t* xcb_connect_to_fd(int fd, xcb_auth_info_t* auth_info) {
  (void)fd;
  (void)auth_info;
  /* A raw socket is an X server transport. This process has none. */
  return vexa_connection(XCB_CONN_ERROR, 0);
}

xcb_connection_t* xcb_connect_to_display_with_auth_info(const char* display, xcb_auth_info_t* auth, int* screen) {
  (void)auth;
  return xcb_connect(display, screen);
}

void xcb_disconnect(xcb_connection_t* c) {
  if (!c) {
    return;
  }
  free(c->setup_bytes);
  free(c);
}

int xcb_connection_has_error(xcb_connection_t* c) {
  if (!c) {
    return XCB_CONN_ERROR;
  }
  return c->error;
}

const struct xcb_setup_t* xcb_get_setup(xcb_connection_t* c) {
  if (!c || c->error) {
    return NULL;
  }
  return (const xcb_setup_t*)c->setup_bytes;
}

int xcb_flush(xcb_connection_t* c) {
  if (!c || c->error) {
    return 0;
  }
  return 1;
}

uint32_t xcb_generate_id(xcb_connection_t* c) {
  uint32_t id;
  if (!c || c->error) {
    return (uint32_t)-1;
  }
  id = c->next_id;
  if ((id & ~VEXA_XID_MASK) != VEXA_XID_BASE || id > (VEXA_XID_BASE | VEXA_XID_MASK)) {
    c->error = XCB_CONN_CLOSED_EXT_NOTSUPPORTED;
    return (uint32_t)-1;
  }
  c->next_id = id + 1;
  return id;
}

xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t* R) {
  xcb_screen_iterator_t iterator;
  char* cursor;
  memset(&iterator, 0, sizeof(iterator));
  if (!R) {
    return iterator;
  }
  cursor = vexa_after_vendor(R);
  cursor += (size_t)R->pixmap_formats_len * sizeof(xcb_format_t);
  iterator.data = (xcb_screen_t*)cursor;
  iterator.rem = R->roots_len;
  iterator.index = (int)(cursor - (char*)R);
  return iterator;
}

void xcb_screen_next(xcb_screen_iterator_t* i) {
  const char* next;
  if (!i || !i->data || i->rem <= 0) {
    return;
  }
  next = vexa_screen_end(i->data);
  i->index += (int)(next - (char*)i->data);
  i->data = (xcb_screen_t*)next;
  i->rem--;
}

xcb_void_cookie_t xcb_create_window(xcb_connection_t* c, uint8_t depth, xcb_window_t wid, xcb_window_t parent, int16_t x,
                                    int16_t y, uint16_t width, uint16_t height, uint16_t border_width, uint16_t _class,
                                    xcb_visualid_t visual, uint32_t value_mask, const void* value_list) {
  (void)depth;
  (void)parent;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)border_width;
  (void)_class;
  (void)visual;
  (void)value_mask;
  (void)value_list;
  if (c && !c->error) {
    c->last_window = wid;
  }
  return vexa_cookie(c);
}

xcb_void_cookie_t xcb_destroy_window(xcb_connection_t* c, xcb_window_t window) {
  (void)window;
  return vexa_cookie(c);
}

xcb_void_cookie_t xcb_configure_window(xcb_connection_t* c, xcb_window_t window, uint16_t value_mask, const void* value_list) {
  (void)window;
  (void)value_mask;
  (void)value_list;
  return vexa_cookie(c);
}

xcb_void_cookie_t xcb_map_window(xcb_connection_t* c, xcb_window_t window) {
  xcb_generic_event_t event;
  if (c && !c->error) {
    c->last_window = window;
  }
  memset(&event, 0, sizeof(event));
  event.response_type = XCB_MAP_NOTIFY;
  vexa_enqueue(c, &event);
  return vexa_cookie(c);
}

xcb_void_cookie_t xcb_change_property(xcb_connection_t* c, uint8_t mode, xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
                                     uint8_t format, uint32_t data_len, const void* data) {
  (void)mode;
  (void)window;
  (void)property;
  (void)type;
  (void)format;
  (void)data_len;
  (void)data;
  return vexa_cookie(c);
}

xcb_void_cookie_t xcb_send_event(xcb_connection_t* c, uint8_t propagate, xcb_window_t destination, uint32_t event_mask,
                                const char* event) {
  xcb_generic_event_t copy;
  (void)propagate;
  (void)destination;
  (void)event_mask;
  if (event) {
    memset(&copy, 0, sizeof(copy));
    memcpy(&copy, event, 32);
    copy.response_type |= 0x80;
    vexa_enqueue(c, &copy);
  }
  return vexa_cookie(c);
}

static xcb_atom_t vexa_find_atom(const char* name, uint16_t name_len, int create) {
  int i;
  for (i = 0; i < g_atom_count; i++) {
    if (g_atoms[i].len == name_len && memcmp(g_atoms[i].name, name, name_len) == 0) {
      return g_atoms[i].id;
    }
  }
  if (!create || g_atom_count >= VEXA_MAX_ATOMS || name_len >= sizeof(g_atoms[0].name)) {
    return XCB_ATOM_NONE;
  }
  g_atoms[g_atom_count].id = g_next_atom++;
  g_atoms[g_atom_count].len = name_len;
  memcpy(g_atoms[g_atom_count].name, name, name_len);
  g_atoms[g_atom_count].name[name_len] = '\0';
  return g_atoms[g_atom_count++].id;
}

xcb_intern_atom_cookie_t xcb_intern_atom(xcb_connection_t* c, uint8_t only_if_exists, uint16_t name_len, const char* name) {
  xcb_intern_atom_cookie_t cookie;
  struct vexa_pending_atom* pending;
  cookie.sequence = vexa_cookie(c).sequence;
  if (!c || c->error || cookie.sequence == 0 || !name || name_len >= sizeof(pending->name)) {
    cookie.sequence = 0;
    return cookie;
  }
  if (c->pending_count >= VEXA_MAX_PENDING) {
    cookie.sequence = 0;
    return cookie;
  }
  pending = &c->pending[c->pending_count++];
  pending->sequence = cookie.sequence;
  pending->only_if_exists = only_if_exists;
  pending->name_len = name_len;
  memcpy(pending->name, name, name_len);
  pending->name[name_len] = '\0';
  return cookie;
}

xcb_intern_atom_cookie_t xcb_intern_atom_unchecked(xcb_connection_t* c, uint8_t only_if_exists, uint16_t name_len, const char* name) {
  return xcb_intern_atom(c, only_if_exists, name_len, name);
}

xcb_intern_atom_reply_t* xcb_intern_atom_reply(xcb_connection_t* c, xcb_intern_atom_cookie_t cookie, xcb_generic_error_t** e) {
  xcb_intern_atom_reply_t* reply;
  int i;
  struct vexa_pending_atom request;
  int found = 0;
  if (e) {
    *e = NULL;
  }
  if (!c || c->error || cookie.sequence == 0) {
    return NULL;
  }
  for (i = 0; i < c->pending_count; i++) {
    if (c->pending[i].sequence != cookie.sequence) {
      continue;
    }
    request = c->pending[i];
    c->pending[i] = c->pending[c->pending_count - 1];
    c->pending_count--;
    found = 1;
    break;
  }
  if (!found) {
    return NULL;
  }
  reply = (xcb_intern_atom_reply_t*)calloc(1, sizeof(*reply));
  if (!reply) {
    return NULL;
  }
  reply->response_type = 1;
  reply->sequence = (uint16_t)cookie.sequence;
  reply->atom = vexa_find_atom(request.name, request.name_len, request.only_if_exists ? 0 : 1);
  return reply;
}

xcb_generic_event_t* xcb_poll_for_event(xcb_connection_t* c) {
  xcb_generic_event_t* event;
  vexa_note_window_size(c);
  if (!c || c->error || c->event_count == 0) {
    return NULL;
  }
  event = (xcb_generic_event_t*)malloc(sizeof(*event));
  if (!event) {
    return NULL;
  }
  *event = c->events[c->event_head].event;
  c->event_head = (c->event_head + 1) % VEXA_MAX_EVENTS;
  c->event_count--;
  return event;
}

xcb_generic_event_t* xcb_poll_for_queued_event(xcb_connection_t* c) {
  return xcb_poll_for_event(c);
}

xcb_generic_event_t* xcb_wait_for_event(xcb_connection_t* c) {
  xcb_generic_event_t* event = xcb_poll_for_event(c);
  struct timespec delay;
  if (event || !c || c->error) {
    return event;
  }
  /* No X server pushes input. A null return is defined as an I/O failure,
     which makes callers tear the connection down, so this waits one frame
     and stays quiet instead. */
  delay.tv_sec = 0;
  delay.tv_nsec = 16000000L;
  nanosleep(&delay, NULL);
  return NULL;
}
