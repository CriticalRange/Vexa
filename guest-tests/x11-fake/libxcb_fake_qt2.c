
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
