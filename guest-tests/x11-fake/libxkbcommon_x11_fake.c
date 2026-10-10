/* Guest libxkbcommon-x11.so.0.
 *
 * The real library asks the X server for the core keyboard through libxcb-xkb.
 * There is no X server here, and the VEXA libxcb connection is not the real
 * xcb_connection_t, so those requests cannot be forwarded. vkgears still needs
 * a keymap: a later key event calls xkb_state_key_get_one_sym on the state
 * created here. The keymap is compiled by the real libxkbcommon from the text
 * below. The connection pointer is ignored.
 */
#include <stdint.h>
#include <stdio.h>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

struct xkb_keymap* xkb_keymap_new_from_string(struct xkb_context* context, const char* string, int format, int flags);
struct xkb_state* xkb_state_new(struct xkb_keymap* keymap);

/* XKB_KEYMAP_FORMAT_TEXT_V1. No include directives: the device rootfs does not
   ship xkb-data, and these keysyms are compiled into libxkbcommon. */
static const char k_keymap[] =
    "xkb_keymap {\n"
    "xkb_keycodes {\n"
    "minimum = 8;\n"
    "maximum = 255;\n"
    "<ESC> = 9;\n"
    "<AC01> = 38;\n"
    "<UP> = 111;\n"
    "<LEFT> = 113;\n"
    "<RGHT> = 114;\n"
    "<DOWN> = 116;\n"
    "};\n"
    "xkb_types {\n"
    "type \"ONE_LEVEL\" {\n"
    "modifiers = none;\n"
    "level_name[Level1] = \"Any\";\n"
    "};\n"
    "type \"TWO_LEVEL\" {\n"
    "modifiers = Shift;\n"
    "map[Shift] = Level2;\n"
    "level_name[Level1] = \"Base\";\n"
    "level_name[Level2] = \"Shift\";\n"
    "};\n"
    "};\n"
    "xkb_compatibility {\n"
    "};\n"
    "xkb_symbols {\n"
    "name[group1] = \"English (US)\";\n"
    "key <ESC> { [ Escape ] };\n"
    "key <AC01> { type=\"TWO_LEVEL\", [ a, A ] };\n"
    "key <UP> { [ Up ] };\n"
    "key <LEFT> { [ Left ] };\n"
    "key <RGHT> { [ Right ] };\n"
    "key <DOWN> { [ Down ] };\n"
    "};\n"
    "};\n";

int xkb_x11_setup_xkb_extension(void* connection, uint16_t major, uint16_t minor, int flags, uint16_t* major_out,
                                uint16_t* minor_out, uint8_t* base_event_out, uint8_t* base_error_out) {
  (void)connection;
  (void)major;
  (void)minor;
  (void)flags;
  if (major_out) {
    *major_out = 1;
  }
  if (minor_out) {
    *minor_out = 0;
  }
  if (base_event_out) {
    *base_event_out = 0;
  }
  if (base_error_out) {
    *base_error_out = 0;
  }
  return 1;
}

int32_t xkb_x11_get_core_keyboard_device_id(void* connection) {
  (void)connection;
  /* Any non-error id. Callers only pass it back into the two functions below. */
  return 3;
}

struct xkb_keymap* xkb_x11_keymap_new_from_device(struct xkb_context* context, void* connection, int32_t device_id, int flags) {
  struct xkb_keymap* keymap;
  (void)connection;
  (void)device_id;
  (void)flags;
  if (!context) {
    return NULL;
  }
  keymap = xkb_keymap_new_from_string(context, k_keymap, 1, 0);
  if (!keymap) {
    fprintf(stderr, "[VEXA][XKB] keymap compile failed\n");
  }
  return keymap;
}

struct xkb_state* xkb_x11_state_new_from_device(struct xkb_keymap* keymap, void* connection, int32_t device_id) {
  (void)connection;
  (void)device_id;
  if (!keymap) {
    return NULL;
  }
  return xkb_state_new(keymap);
}
