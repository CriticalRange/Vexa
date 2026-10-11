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
/* Keycodes are the evdev X codes. libX11_fake.c uses the same numbers. */
static const char k_keymap[] =
    "xkb_keymap {\n"
    "xkb_keycodes {\n"
    "minimum = 8;\n"
    "maximum = 255;\n"
    "<ESC> = 9;\n"
    "<BKSP> = 22;\n"
    "<TAB> = 23;\n"
    "<AD01> = 24;\n"
    "<AD02> = 25;\n"
    "<AD03> = 26;\n"
    "<AD04> = 27;\n"
    "<AD05> = 28;\n"
    "<AD06> = 29;\n"
    "<AD07> = 30;\n"
    "<AD08> = 31;\n"
    "<AD09> = 32;\n"
    "<AD10> = 33;\n"
    "<RTRN> = 36;\n"
    "<LCTL> = 37;\n"
    "<AC01> = 38;\n"
    "<AC02> = 39;\n"
    "<AC03> = 40;\n"
    "<AC04> = 41;\n"
    "<AC05> = 42;\n"
    "<AC06> = 43;\n"
    "<AC07> = 44;\n"
    "<AC08> = 45;\n"
    "<AC09> = 46;\n"
    "<LFSH> = 50;\n"
    "<AB01> = 52;\n"
    "<AB02> = 53;\n"
    "<AB03> = 54;\n"
    "<AB04> = 55;\n"
    "<AB05> = 56;\n"
    "<AB06> = 57;\n"
    "<AB07> = 58;\n"
    "<RTSH> = 62;\n"
    "<LALT> = 64;\n"
    "<SPCE> = 65;\n"
    "<RCTL> = 105;\n"
    "<RALT> = 108;\n"
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
    "key <BKSP> { [ BackSpace ] };\n"
    "key <TAB> { [ Tab ] };\n"
    "key <AD01> { type=\"TWO_LEVEL\", [ q, Q ] };\n"
    "key <AD02> { type=\"TWO_LEVEL\", [ w, W ] };\n"
    "key <AD03> { type=\"TWO_LEVEL\", [ e, E ] };\n"
    "key <AD04> { type=\"TWO_LEVEL\", [ r, R ] };\n"
    "key <AD05> { type=\"TWO_LEVEL\", [ t, T ] };\n"
    "key <AD06> { type=\"TWO_LEVEL\", [ y, Y ] };\n"
    "key <AD07> { type=\"TWO_LEVEL\", [ u, U ] };\n"
    "key <AD08> { type=\"TWO_LEVEL\", [ i, I ] };\n"
    "key <AD09> { type=\"TWO_LEVEL\", [ o, O ] };\n"
    "key <AD10> { type=\"TWO_LEVEL\", [ p, P ] };\n"
    "key <RTRN> { [ Return ] };\n"
    "key <LCTL> { [ Control_L ] };\n"
    "key <AC01> { type=\"TWO_LEVEL\", [ a, A ] };\n"
    "key <AC02> { type=\"TWO_LEVEL\", [ s, S ] };\n"
    "key <AC03> { type=\"TWO_LEVEL\", [ d, D ] };\n"
    "key <AC04> { type=\"TWO_LEVEL\", [ f, F ] };\n"
    "key <AC05> { type=\"TWO_LEVEL\", [ g, G ] };\n"
    "key <AC06> { type=\"TWO_LEVEL\", [ h, H ] };\n"
    "key <AC07> { type=\"TWO_LEVEL\", [ j, J ] };\n"
    "key <AC08> { type=\"TWO_LEVEL\", [ k, K ] };\n"
    "key <AC09> { type=\"TWO_LEVEL\", [ l, L ] };\n"
    "key <LFSH> { [ Shift_L ] };\n"
    "key <AB01> { type=\"TWO_LEVEL\", [ z, Z ] };\n"
    "key <AB02> { type=\"TWO_LEVEL\", [ x, X ] };\n"
    "key <AB03> { type=\"TWO_LEVEL\", [ c, C ] };\n"
    "key <AB04> { type=\"TWO_LEVEL\", [ v, V ] };\n"
    "key <AB05> { type=\"TWO_LEVEL\", [ b, B ] };\n"
    "key <AB06> { type=\"TWO_LEVEL\", [ n, N ] };\n"
    "key <AB07> { type=\"TWO_LEVEL\", [ m, M ] };\n"
    "key <RTSH> { [ Shift_R ] };\n"
    "key <LALT> { [ Alt_L ] };\n"
    "key <SPCE> { [ space ] };\n"
    "key <RCTL> { [ Control_R ] };\n"
    "key <RALT> { [ Alt_R ] };\n"
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
