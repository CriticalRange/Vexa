
/* ---- Qt xcb platform plugin: remaining core-protocol surface ---- */

static void* vexa_new_reply(unsigned int sequence, size_t min_size) {
  xcb_generic_reply_t* reply = (xcb_generic_reply_t*)calloc(1, min_size < 4096 ? 4096 : min_size);
  if (!reply) {
    return NULL;
  }
  reply->response_type = XCB_REPLY;
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

void* xcb_poll_for_reply (xcb_connection_t* c, unsigned int request, void** reply, xcb_generic_error_t** e) {
  (void)c;
  if (e) { *e = NULL; }
  if (reply) { *reply = vexa_new_reply(request, 64); }
  return reply ? *reply : NULL;
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
