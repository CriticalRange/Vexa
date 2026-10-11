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
#include <xcb/xcbext.h>
#include <sys/uio.h>

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
  {
    int sw = 1280, sh = 720;
    FILE* f = fopen("/tmp/vexa-window-size", "r");
    if (f) {
      int w = 0, h = 0;
      if (fscanf(f, "%d %d", &w, &h) == 2 && w > 1 && h > 1 && w <= 65535 && h <= 65535) {
        sw = w;
        sh = h;
      }
      fclose(f);
    }
    screen->width_in_pixels = (uint16_t)sw;
    screen->height_in_pixels = (uint16_t)sh;
    /* ~96 DPI so Qt keeps devicePixelRatio at 1; scaling comes from
       QT_SCALE_FACTOR alone. */
    screen->width_in_millimeters = (uint16_t)(sw * 254 / 960);
    screen->height_in_millimeters = (uint16_t)(sh * 254 / 960);
  }
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
  (void)border_width;
  (void)_class;
  (void)visual;
  (void)value_mask;
  (void)value_list;
  fprintf(stderr, "[VEXA][XCB] create_window wid=%u x=%d y=%d %ux%u\n", (unsigned)wid, (int)x, (int)y,
          (unsigned)width, (unsigned)height);
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
  struct timespec delay;
  delay.tv_sec = 0;
  delay.tv_nsec = 16000000L;
  /* No X server pushes input, but a null return is defined as an I/O
     failure and makes Qt's xcb event-reader thread tear the connection
     down. Block like a socket read until an event or a real error. */
  while (c && !c->error) {
    xcb_generic_event_t* event = xcb_poll_for_event(c);
    if (event) {
      return event;
    }
    nanosleep(&delay, NULL);
  }
  return NULL;
}

/* ---- Qt xcb platform plugin: remaining core-protocol surface ---- */

static void* vexa_new_reply(unsigned int sequence, size_t min_size) {
  xcb_generic_reply_t* reply = (xcb_generic_reply_t*)calloc(1, min_size < 4096 ? 4096 : min_size);
  if (!reply) {
    return NULL;
  }
  reply->response_type = 1;
  reply->sequence = (uint16_t)sequence;
  return reply;
}

xcb_void_cookie_t
xcb_change_gc (xcb_connection_t *c, xcb_gcontext_t gc, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_change_gc_checked (xcb_connection_t *c, xcb_gcontext_t gc, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_change_keyboard_control (xcb_connection_t *c, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_change_property_checked (xcb_connection_t *c, uint8_t mode, xcb_window_t window, xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_change_window_attributes (xcb_connection_t *c, xcb_window_t window, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_change_window_attributes_checked (xcb_connection_t *c, xcb_window_t window, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_clear_area (xcb_connection_t *c, uint8_t exposures, xcb_window_t window, int16_t x, int16_t y, uint16_t width, uint16_t height) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_gc (xcb_connection_t *c, xcb_gcontext_t cid, xcb_drawable_t drawable, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_gc_checked (xcb_connection_t *c, xcb_gcontext_t cid, xcb_drawable_t drawable, uint32_t value_mask, const void *value_list) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_glyph_cursor (xcb_connection_t *c, xcb_cursor_t cid, xcb_font_t source_font, xcb_font_t mask_font, uint16_t source_char, uint16_t mask_char, uint16_t fore_red, uint16_t fore_green, uint16_t fore_blue, uint16_t back_red, uint16_t back_green, uint16_t back_blue) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_pixmap (xcb_connection_t *c, uint8_t depth, xcb_pixmap_t pid, xcb_drawable_t drawable, uint16_t width, uint16_t height) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_pixmap_checked (xcb_connection_t *c, uint8_t depth, xcb_pixmap_t pid, xcb_drawable_t drawable, uint16_t width, uint16_t height) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_free_cursor (xcb_connection_t *c, xcb_cursor_t cursor) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_free_gc (xcb_connection_t *c, xcb_gcontext_t gc) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_free_pixmap (xcb_connection_t *c, xcb_pixmap_t pixmap) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_free_pixmap_checked (xcb_connection_t *c, xcb_pixmap_t pixmap) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_no_operation (xcb_connection_t *c) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_no_operation_checked (xcb_connection_t *c) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_open_font (xcb_connection_t *c, xcb_font_t fid, uint16_t name_len, const char *name) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_put_image (xcb_connection_t *c, uint8_t format, xcb_drawable_t drawable, xcb_gcontext_t gc, uint16_t width, uint16_t height, int16_t dst_x, int16_t dst_y, uint8_t left_pad, uint8_t depth, uint32_t data_len, const uint8_t *data) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_window_checked (xcb_connection_t *c, uint8_t depth, xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t border_width, uint16_t _class, xcb_visualid_t visual, uint32_t value_mask, const void *value_list) {
  fprintf(stderr, "[VEXA][XCB] create_window_checked wid=%u x=%d y=%d %ux%u\n", (unsigned)wid, (int)x, (int)y,
          (unsigned)width, (unsigned)height);
  if (c && !c->error) {
    c->last_window = wid;
  }
  return vexa_cookie(c);
}

xcb_get_geometry_cookie_t
xcb_get_geometry (xcb_connection_t *c, xcb_drawable_t drawable) {
  xcb_get_geometry_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_property_cookie_t
xcb_get_property (xcb_connection_t *c, uint8_t _delete, xcb_window_t window, xcb_atom_t property, xcb_atom_t type, uint32_t long_offset, uint32_t long_length) {
  xcb_get_property_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_property_cookie_t
xcb_get_property_unchecked (xcb_connection_t *c, uint8_t _delete, xcb_window_t window, xcb_atom_t property, xcb_atom_t type, uint32_t long_offset, uint32_t long_length) {
  xcb_get_property_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_image_cookie_t
xcb_get_image (xcb_connection_t *c, uint8_t format, xcb_drawable_t drawable, int16_t x, int16_t y, uint16_t width, uint16_t height, uint32_t plane_mask) {
  xcb_get_image_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_input_focus_cookie_t
xcb_get_input_focus (xcb_connection_t *c) {
  xcb_get_input_focus_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_keyboard_mapping_cookie_t
xcb_get_keyboard_mapping (xcb_connection_t *c, xcb_keycode_t first_keycode, uint8_t count) {
  xcb_get_keyboard_mapping_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_geometry_reply_t *
xcb_get_geometry_reply (xcb_connection_t *c, xcb_get_geometry_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_geometry_reply_t* reply = (xcb_get_geometry_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_geometry_reply_t));
  if (reply) {
    reply->root = 1;
    reply->depth = 24;
    reply->width = (uint16_t)(g_announced_width > 0 ? g_announced_width : 1280);
    reply->height = (uint16_t)(g_announced_height > 0 ? g_announced_height : 720);
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_property_reply_t *
xcb_get_property_reply (xcb_connection_t *c, xcb_get_property_cookie_t cookie , xcb_generic_error_t **e) {
  if (e) { *e = NULL; }
  return (xcb_get_property_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_property_reply_t));
}

xcb_get_image_reply_t *
xcb_get_image_reply (xcb_connection_t *c, xcb_get_image_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_image_reply_t* reply = (xcb_get_image_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_image_reply_t));
  if (reply) { reply->depth = 24; reply->visual = 1; }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_input_focus_reply_t *
xcb_get_input_focus_reply (xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_input_focus_reply_t* reply = (xcb_get_input_focus_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_input_focus_reply_t));
  if (reply && c) { reply->focus = c->last_window ? c->last_window : 1; }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_keyboard_mapping_reply_t *
xcb_get_keyboard_mapping_reply (xcb_connection_t *c, xcb_get_keyboard_mapping_cookie_t cookie , xcb_generic_error_t **e) {
  if (e) { *e = NULL; }
  return (xcb_get_keyboard_mapping_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_keyboard_mapping_reply_t));
}

void* xcb_get_property_value (const xcb_get_property_reply_t* R) {
  return R ? (void*)(R + 1) : NULL;
}

int xcb_get_property_value_length (const xcb_get_property_reply_t* R) {
  if (!R) { return 0; }
  return (int)(R->length * (R->format / 8));
}

uint8_t* xcb_get_image_data (const xcb_get_image_reply_t* R) {
  return R ? (uint8_t*)(R + 1) : NULL;
}

int xcb_get_image_data_length (const xcb_get_image_reply_t* R) {
  return R ? (int)(R->length * 4) : 0;
}

xcb_keysym_t* xcb_get_keyboard_mapping_keysyms (const xcb_get_keyboard_mapping_reply_t* R) {
  return R ? (xcb_keysym_t*)(R + 1) : NULL;
}

xcb_depth_iterator_t xcb_screen_allowed_depths_iterator (const xcb_screen_t* R) {
  xcb_depth_iterator_t i;
  memset(&i, 0, sizeof(i));
  if (!R) { return i; }
  i.data = (xcb_depth_t*)(R + 1);
  i.rem = R->allowed_depths_len;
  return i;
}

void xcb_depth_next (xcb_depth_iterator_t* i) {
  const xcb_depth_t* d;
  if (!i || !i->data || i->rem <= 0) { return; }
  d = i->data;
  i->data = (xcb_depth_t*)((const char*)d + sizeof(xcb_depth_t) + (size_t)d->visuals_len * sizeof(xcb_visualtype_t));
  i->rem--;
}

xcb_visualtype_iterator_t xcb_depth_visuals_iterator (const xcb_depth_t* R) {
  xcb_visualtype_iterator_t i;
  memset(&i, 0, sizeof(i));
  if (!R) { return i; }
  i.data = (xcb_visualtype_t*)(R + 1);
  i.rem = R->visuals_len;
  return i;
}

void xcb_visualtype_next (xcb_visualtype_iterator_t* i) {
  if (!i || !i->data || i->rem <= 0) { return; }
  i->data = i->data + 1;
  i->rem--;
}

xcb_format_t* xcb_setup_pixmap_formats (const xcb_setup_t* R) {
  return R ? (xcb_format_t*)vexa_after_vendor(R) : NULL;
}

int xcb_setup_pixmap_formats_length (const xcb_setup_t* R) {
  return R ? R->pixmap_formats_len : 0;
}

int xcb_str_sizeof (const void* str) {
  const xcb_str_t* s = (const xcb_str_t*)str;
  return s ? (int)(sizeof(xcb_str_t) + vexa_pad4(s->name_len)) : 0;
}

int xcb_popcount (uint32_t x) {
  return __builtin_popcount(x);
}

unsigned int xcb_send_request (xcb_connection_t* c, int flags, struct iovec* vector, const xcb_protocol_request_t* request) {
  (void)flags; (void)vector; (void)request;
  if (!c || c->error) { return 0; }
  return c->sequence++;
}

uint64_t xcb_send_request64 (xcb_connection_t* c, int flags, struct iovec* vector, const xcb_protocol_request_t* request) {
  return xcb_send_request(c, flags, vector, request);
}

unsigned int xcb_send_request_with_fds (xcb_connection_t* c, int flags, struct iovec* vector, const xcb_protocol_request_t* request, unsigned int num_fds, int* fds) {
  (void)num_fds; (void)fds;
  return xcb_send_request(c, flags, vector, request);
}

uint64_t xcb_send_request_with_fds64 (xcb_connection_t* c, int flags, struct iovec* vector, const xcb_protocol_request_t* request, unsigned int num_fds, int* fds) {
  return xcb_send_request_with_fds(c, flags, vector, request, num_fds, fds);
}

void* xcb_wait_for_reply (xcb_connection_t* c, unsigned int request, xcb_generic_error_t** e) {
  (void)c;
  if (e) { *e = NULL; }
  return vexa_new_reply(request, 64);
}

void* xcb_wait_for_reply64 (xcb_connection_t* c, uint64_t request, xcb_generic_error_t** e) {
  return xcb_wait_for_reply(c, (unsigned int)request, e);
}

int xcb_poll_for_reply (xcb_connection_t* c, unsigned int request, void** reply, xcb_generic_error_t** e) {
  (void)c;
  if (e) { *e = NULL; }
  if (reply) { *reply = vexa_new_reply(request, 64); return 1; }
  return 0;
}

int* xcb_get_reply_fds (xcb_connection_t* c, void* reply, size_t replylen) {
  (void)c; (void)reply; (void)replylen;
  return NULL;
}

xcb_generic_error_t* xcb_request_check (xcb_connection_t* c, xcb_void_cookie_t request) {
  (void)c; (void)request;
  return NULL;
}

const xcb_query_extension_reply_t* xcb_get_extension_data (xcb_connection_t* c, xcb_extension_t* ext) {
  static const xcb_query_extension_reply_t absent;
  (void)c; (void)ext;
  return &absent;
}

int xcb_get_file_descriptor (xcb_connection_t* c) {
  (void)c;
  return -1;
}

/* ---- second pass: core requests the bundled Qt/XcbQpa stack imports ---- */

xcb_void_cookie_t
xcb_bell (xcb_connection_t *c, int8_t percent) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_close_font (xcb_connection_t *c, xcb_font_t font) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_convert_selection (xcb_connection_t *c, xcb_window_t requestor, xcb_atom_t selection, xcb_atom_t target, xcb_atom_t property, xcb_timestamp_t time) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_copy_area (xcb_connection_t *c, xcb_drawable_t src_drawable, xcb_drawable_t dst_drawable, xcb_gcontext_t gc, int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t width, uint16_t height) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_colormap (xcb_connection_t *c, uint8_t alloc, xcb_colormap_t mid, xcb_window_t window, xcb_visualid_t visual) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_create_cursor (xcb_connection_t *c, xcb_cursor_t cid, xcb_pixmap_t source, xcb_pixmap_t mask, uint16_t fore_red, uint16_t fore_green, uint16_t fore_blue, uint16_t back_red, uint16_t back_green, uint16_t back_blue, uint16_t x, uint16_t y) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_delete_property (xcb_connection_t *c, xcb_window_t window, xcb_atom_t property) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_free_colormap (xcb_connection_t *c, xcb_colormap_t cmap) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_grab_server (xcb_connection_t *c) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_reparent_window (xcb_connection_t *c, xcb_window_t window, xcb_window_t parent, int16_t x, int16_t y) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_set_clip_rectangles (xcb_connection_t *c, uint8_t ordering, xcb_gcontext_t gc, int16_t clip_x_origin, int16_t clip_y_origin, uint32_t rectangles_len, const xcb_rectangle_t *rectangles) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_set_input_focus (xcb_connection_t *c, uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_set_selection_owner (xcb_connection_t *c, xcb_window_t owner, xcb_atom_t selection, xcb_timestamp_t time) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_ungrab_keyboard (xcb_connection_t *c, xcb_timestamp_t time) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_ungrab_pointer (xcb_connection_t *c, xcb_timestamp_t time) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_ungrab_server (xcb_connection_t *c) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_unmap_window (xcb_connection_t *c, xcb_window_t window) {
  return vexa_cookie(c);
}

xcb_void_cookie_t
xcb_warp_pointer (xcb_connection_t *c, xcb_window_t src_window, xcb_window_t dst_window, int16_t src_x, int16_t src_y, uint16_t src_width, uint16_t src_height, int16_t dst_x, int16_t dst_y) {
  return vexa_cookie(c);
}

xcb_get_atom_name_cookie_t
xcb_get_atom_name (xcb_connection_t *c, xcb_atom_t atom) {
  xcb_get_atom_name_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_geometry_cookie_t
xcb_get_geometry_unchecked (xcb_connection_t *c, xcb_drawable_t drawable) {
  xcb_get_geometry_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_image_cookie_t
xcb_get_image_unchecked (xcb_connection_t *c, uint8_t format, xcb_drawable_t drawable, int16_t x, int16_t y, uint16_t width, uint16_t height, uint32_t plane_mask) {
  xcb_get_image_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_modifier_mapping_cookie_t
xcb_get_modifier_mapping (xcb_connection_t *c) {
  xcb_get_modifier_mapping_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_selection_owner_cookie_t
xcb_get_selection_owner (xcb_connection_t *c, xcb_atom_t selection) {
  xcb_get_selection_owner_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_window_attributes_cookie_t
xcb_get_window_attributes (xcb_connection_t *c, xcb_window_t window) {
  xcb_get_window_attributes_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_window_attributes_cookie_t
xcb_get_window_attributes_unchecked (xcb_connection_t *c, xcb_window_t window) {
  xcb_get_window_attributes_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_grab_keyboard_cookie_t
xcb_grab_keyboard (xcb_connection_t *c, uint8_t owner_events, xcb_window_t grab_window, xcb_timestamp_t time, uint8_t pointer_mode, uint8_t keyboard_mode) {
  xcb_grab_keyboard_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_grab_pointer_cookie_t
xcb_grab_pointer (xcb_connection_t *c, uint8_t owner_events, xcb_window_t grab_window, uint16_t event_mask, uint8_t pointer_mode, uint8_t keyboard_mode, xcb_window_t confine_to, xcb_cursor_t cursor, xcb_timestamp_t time) {
  xcb_grab_pointer_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_query_pointer_cookie_t
xcb_query_pointer (xcb_connection_t *c, xcb_window_t window) {
  xcb_query_pointer_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_query_tree_cookie_t
xcb_query_tree (xcb_connection_t *c, xcb_window_t window) {
  xcb_query_tree_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_query_tree_cookie_t
xcb_query_tree_unchecked (xcb_connection_t *c, xcb_window_t window) {
  xcb_query_tree_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_translate_coordinates_cookie_t
xcb_translate_coordinates (xcb_connection_t *c, xcb_window_t src_window, xcb_window_t dst_window, int16_t src_x, int16_t src_y) {
  xcb_translate_coordinates_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_translate_coordinates_cookie_t
xcb_translate_coordinates_unchecked (xcb_connection_t *c, xcb_window_t src_window, xcb_window_t dst_window, int16_t src_x, int16_t src_y) {
  xcb_translate_coordinates_cookie_t cookie;
  cookie.sequence = vexa_cookie(c).sequence;
  return cookie;
}

xcb_get_atom_name_reply_t *
xcb_get_atom_name_reply (xcb_connection_t *c, xcb_get_atom_name_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_atom_name_reply_t* reply = (xcb_get_atom_name_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_atom_name_reply_t));
  if (reply) {
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_modifier_mapping_reply_t *
xcb_get_modifier_mapping_reply (xcb_connection_t *c, xcb_get_modifier_mapping_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_modifier_mapping_reply_t* reply = (xcb_get_modifier_mapping_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_modifier_mapping_reply_t));
  if (reply) {
    reply->keycodes_per_modifier = 0;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_selection_owner_reply_t *
xcb_get_selection_owner_reply (xcb_connection_t *c, xcb_get_selection_owner_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_selection_owner_reply_t* reply = (xcb_get_selection_owner_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_selection_owner_reply_t));
  if (reply) {
    reply->owner = 0;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_get_window_attributes_reply_t *
xcb_get_window_attributes_reply (xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_get_window_attributes_reply_t* reply = (xcb_get_window_attributes_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_get_window_attributes_reply_t));
  if (reply) {
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_grab_keyboard_reply_t *
xcb_grab_keyboard_reply (xcb_connection_t *c, xcb_grab_keyboard_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_grab_keyboard_reply_t* reply = (xcb_grab_keyboard_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_grab_keyboard_reply_t));
  if (reply) {
    reply->status = XCB_GRAB_STATUS_SUCCESS;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_grab_pointer_reply_t *
xcb_grab_pointer_reply (xcb_connection_t *c, xcb_grab_pointer_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_grab_pointer_reply_t* reply = (xcb_grab_pointer_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_grab_pointer_reply_t));
  if (reply) {
    reply->status = XCB_GRAB_STATUS_SUCCESS;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_query_pointer_reply_t *
xcb_query_pointer_reply (xcb_connection_t *c, xcb_query_pointer_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_query_pointer_reply_t* reply = (xcb_query_pointer_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_query_pointer_reply_t));
  if (reply) {
    reply->same_screen = 1;
    reply->root = 1;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_query_tree_reply_t *
xcb_query_tree_reply (xcb_connection_t *c, xcb_query_tree_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_query_tree_reply_t* reply = (xcb_query_tree_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_query_tree_reply_t));
  if (reply) {
    reply->root = 1;
    reply->parent = 1;
  }
  if (e) { *e = NULL; }
  return reply;
}

xcb_translate_coordinates_reply_t *
xcb_translate_coordinates_reply (xcb_connection_t *c, xcb_translate_coordinates_cookie_t cookie , xcb_generic_error_t **e) {
  xcb_translate_coordinates_reply_t* reply = (xcb_translate_coordinates_reply_t*)vexa_new_reply(cookie.sequence, sizeof(xcb_translate_coordinates_reply_t));
  if (reply) {
    reply->same_screen = 1;
  }
  if (e) { *e = NULL; }
  return reply;
}

char* xcb_get_atom_name_name (const xcb_get_atom_name_reply_t* R) {
  return R ? (char*)(R + 1) : NULL;
}

int xcb_get_atom_name_name_length (const xcb_get_atom_name_reply_t* R) {
  return R ? R->name_len : 0;
}

xcb_keycode_t* xcb_get_modifier_mapping_keycodes (const xcb_get_modifier_mapping_reply_t* R) {
  return R ? (xcb_keycode_t*)(R + 1) : NULL;
}

int xcb_get_modifier_mapping_keycodes_length (const xcb_get_modifier_mapping_reply_t* R) {
  return R ? (int)(R->length * 4) : 0;
}

xcb_window_t* xcb_query_tree_children (const xcb_query_tree_reply_t* R) {
  return R ? (xcb_window_t*)(R + 1) : NULL;
}

int xcb_query_tree_children_length (const xcb_query_tree_reply_t* R) {
  return R ? R->children_len : 0;
}

xcb_format_iterator_t xcb_setup_pixmap_formats_iterator (const xcb_setup_t* R) {
  xcb_format_iterator_t i;
  memset(&i, 0, sizeof(i));
  if (!R) { return i; }
  i.data = (xcb_format_t*)vexa_after_vendor(R);
  i.rem = R->pixmap_formats_len;
  return i;
}

void xcb_format_next (xcb_format_iterator_t* i) {
  if (!i || !i->data || i->rem <= 0) { return; }
  i->data = i->data + 1;
  i->rem--;
}

uint32_t xcb_get_maximum_request_length (xcb_connection_t* c) {
  const xcb_setup_t* setup = c ? xcb_get_setup(c) : NULL;
  return setup ? setup->maximum_request_length : 65535;
}

void xcb_prefetch_extension_data (xcb_connection_t* c, xcb_extension_t* ext) {
  (void)c; (void)ext;
}

xcb_pixmap_t xcb_create_pixmap_from_bitmap_data (xcb_connection_t* c, xcb_drawable_t d, uint8_t* data, uint32_t width, uint32_t height, uint32_t depth, uint32_t fg, uint32_t bg, xcb_gcontext_t* gcp) {
  (void)d; (void)data; (void)width; (void)height; (void)depth; (void)fg; (void)bg;
  if (gcp) { *gcp = xcb_generate_id(c); }
  return xcb_generate_id(c);
}
