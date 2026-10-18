/* Minimal libudev backend for Android. Object creation must succeed even when
   there are almost no devices: SDL treats a NULL monitor/enumerator as
   initialization failure and can tear down its already initialized video
   subsystem. The backend reports exactly one input device - the virtual
   Xbox 360 pad that libvexa_evdev.so serves on /dev/input/event0 - so engines
   that enumerate controllers through udev (Unity, Godot, SDL built against
   udev) find it automatically. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

struct udev { int unused; };
struct udev_list_entry {
  struct udev_list_entry* next;
  const char* name;
  const char* value;
};
struct udev_device {
  const char* syspath;
  const char* sysname;
  const char* devnode;
  const char* subsystem;
  dev_t devnum;
  struct udev_list_entry* properties;
  struct udev_list_entry* tags;
};
struct udev_enumerate {
  unsigned refs;
  struct udev* owner;
  char subsystems[8][32];
  unsigned nsubsystems;
  struct udev_list_entry results;
};
struct udev_monitor { unsigned refs; struct udev* owner; int fd; };
struct udev_queue { int unused; };
struct udev_hwdb { int unused; };

static struct udev s_udev;
static struct udev_queue s_queue;
/* Lets the asset stager reject the host distribution's real netlink client. */
const char vexa_udev_backend[] = "VEXA gamepad udev backend";

#define PAD_SYSPATH "/sys/devices/vexa/input/event0"

static struct udev_list_entry s_pad_tags[] = {
  { NULL, "seat", NULL },
  { NULL, "uaccess", NULL },
};
static struct udev_list_entry s_pad_props[] = {
  { NULL, "DEVNAME", "/dev/input/event0" },
  { NULL, "DEVPATH", "/devices/vexa/input/event0" },
  { NULL, "SUBSYSTEM", "input" },
  { NULL, "ID_INPUT", "1" },
  { NULL, "ID_INPUT_JOYSTICK", "1" },
  { NULL, "ID_BUS", "usb" },
  { NULL, "ID_VENDOR_ID", "045e" },
  { NULL, "ID_MODEL_ID", "028e" },
  { NULL, "ID_PATH", "platform-vexa-input0" },
  { NULL, "MAJOR", "13" },
  { NULL, "MINOR", "64" },
};
static struct udev_device s_pad_device = {
  PAD_SYSPATH,
  "event0",
  "/dev/input/event0",
  "input",
  0, /* filled in the constructor */
  NULL,
  NULL,
};
static struct udev_list_entry s_pad_result = { NULL, PAD_SYSPATH, NULL };

__attribute__((constructor)) static void vexa_udev_init(void) {
  size_t i;
  for (i = 0; i + 1 < sizeof(s_pad_props) / sizeof(s_pad_props[0]); ++i) {
    s_pad_props[i].next = &s_pad_props[i + 1];
  }
  for (i = 0; i + 1 < sizeof(s_pad_tags) / sizeof(s_pad_tags[0]); ++i) {
    s_pad_tags[i].next = &s_pad_tags[i + 1];
  }
  s_pad_device.devnum = makedev(13, 64);
  s_pad_device.properties = s_pad_props;
  s_pad_device.tags = s_pad_tags;
}

struct udev* udev_new(void) { return &s_udev; }
struct udev* udev_ref(struct udev* u) { return u; }
struct udev* udev_unref(struct udev* u) { (void)u; return NULL; }
void udev_set_log_fn(struct udev* u, void* fn) { (void)u; (void)fn; }
int udev_get_log_priority(struct udev* u) { (void)u; return 0; }
void udev_set_log_priority(struct udev* u, int p) { (void)u; (void)p; }
void* udev_get_userdata(struct udev* u) { (void)u; return NULL; }
void udev_set_userdata(struct udev* u, void* d) { (void)u; (void)d; }
const char* udev_get_sys_path(struct udev* u) { (void)u; return "/sys"; }
const char* udev_get_dev_path(struct udev* u) { (void)u; return "/dev"; }
const char* udev_get_run_path(struct udev* u) { (void)u; return "/run"; }

/* devices: the virtual pad, or nothing */
struct udev_device* udev_device_ref(struct udev_device* d) { return d; }
struct udev_device* udev_device_unref(struct udev_device* d) { (void)d; return NULL; }
struct udev* udev_device_get_udev(struct udev_device* d) { (void)d; return &s_udev; }
struct udev_device* udev_device_new_from_syspath(struct udev* u, const char* p) {
  (void)u;
  if (p && strcmp(p, PAD_SYSPATH) == 0) return &s_pad_device;
  return NULL;
}
struct udev_device* udev_device_new_from_devnum(struct udev* u, char t, dev_t n) {
  (void)u;
  if (t == 'c' && n == makedev(13, 64)) return &s_pad_device;
  return NULL;
}
struct udev_device* udev_device_new_from_subsystem_sysname(struct udev* u, const char* s, const char* n) {
  (void)u;
  if (s && n && strcmp(s, "input") == 0 && strcmp(n, "event0") == 0) return &s_pad_device;
  return NULL;
}
struct udev_device* udev_device_new_from_device_id(struct udev* u, const char* i) {
  (void)u;
  /* SDL resolves joysticks as "c13:64". */
  if (i && strcmp(i, "c13:64") == 0) return &s_pad_device;
  return NULL;
}
struct udev_device* udev_device_new_from_environment(struct udev* u) { (void)u; return NULL; }
struct udev_device* udev_device_get_parent(struct udev_device* d) { (void)d; return NULL; }
struct udev_device* udev_device_get_parent_with_subsystem_devtype(struct udev_device* d, const char* s, const char* t) { (void)d; (void)s; (void)t; return NULL; }
const char* udev_device_get_devpath(struct udev_device* d) {
  return d == &s_pad_device ? "/devices/vexa/input/event0" : NULL;
}
const char* udev_device_get_subsystem(struct udev_device* d) { return d == &s_pad_device ? "input" : NULL; }
const char* udev_device_get_devtype(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_syspath(struct udev_device* d) { return d == &s_pad_device ? PAD_SYSPATH : NULL; }
const char* udev_device_get_sysname(struct udev_device* d) { return d == &s_pad_device ? "event0" : NULL; }
const char* udev_device_get_sysnum(struct udev_device* d) { return d == &s_pad_device ? "0" : NULL; }
const char* udev_device_get_devnode(struct udev_device* d) { return d == &s_pad_device ? "/dev/input/event0" : NULL; }
int udev_device_get_is_initialized(struct udev_device* d) { return d == &s_pad_device ? 1 : 0; }
struct udev_list_entry* udev_device_get_devlinks_list_entry(struct udev_device* d) { (void)d; return NULL; }
struct udev_list_entry* udev_device_get_properties_list_entry(struct udev_device* d) {
  return d == &s_pad_device ? s_pad_device.properties : NULL;
}
struct udev_list_entry* udev_device_get_tags_list_entry(struct udev_device* d) {
  return d == &s_pad_device ? s_pad_device.tags : NULL;
}
struct udev_list_entry* udev_device_get_sysattr_list_entry(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_property_value(struct udev_device* d, const char* k) {
  struct udev_list_entry* e;
  if (d != &s_pad_device || !k) return NULL;
  for (e = s_pad_device.properties; e; e = e->next) {
    if (strcmp(e->name, k) == 0) return e->value;
  }
  return NULL;
}
const char* udev_device_get_driver(struct udev_device* d) { (void)d; return NULL; }
dev_t udev_device_get_devnum(struct udev_device* d) { return d == &s_pad_device ? s_pad_device.devnum : 0; }
const char* udev_device_get_action(struct udev_device* d) { (void)d; return NULL; }
unsigned long long udev_device_get_seqnum(struct udev_device* d) { (void)d; return 0; }
unsigned long long udev_device_get_usec_since_initialized(struct udev_device* d) { (void)d; return 0; }
const char* udev_device_get_sysattr_value(struct udev_device* d, const char* a) { (void)d; (void)a; return NULL; }
int udev_device_set_sysattr_value(struct udev_device* d, const char* a, const char* v) { (void)d; (void)a; (void)v; return -1; }
int udev_device_has_tag(struct udev_device* d, const char* t) {
  struct udev_list_entry* e;
  if (d != &s_pad_device || !t) return 0;
  for (e = s_pad_device.tags; e; e = e->next) {
    if (strcmp(e->name, t) == 0) return 1;
  }
  return 0;
}
int udev_device_has_current_tag(struct udev_device* d, const char* t) { return udev_device_has_tag(d, t); }
struct udev_list_entry* udev_device_get_current_tags_list_entry(struct udev_device* d) {
  return udev_device_get_tags_list_entry(d);
}

/* list entries */
struct udev_list_entry* udev_list_entry_get_next(struct udev_list_entry* e) { return e ? e->next : NULL; }
struct udev_list_entry* udev_list_entry_get_by_name(struct udev_list_entry* e, const char* n) {
  for (; e; e = e->next) {
    if (e->name && n && strcmp(e->name, n) == 0) return e;
  }
  return NULL;
}
const char* udev_list_entry_get_name(struct udev_list_entry* e) { return e ? e->name : NULL; }
const char* udev_list_entry_get_value(struct udev_list_entry* e) { return e ? e->value : NULL; }

/* enumerate: the pad shows up for input/udev-wide scans */
struct udev_enumerate* udev_enumerate_new(struct udev* u) {
  if (!u) { errno = EINVAL; return NULL; }
  struct udev_enumerate* e = calloc(1, sizeof(*e));
  if (e) { e->refs = 1; e->owner = udev_ref(u); }
  return e;
}
struct udev_enumerate* udev_enumerate_ref(struct udev_enumerate* e) {
  if (e) ++e->refs;
  return e;
}
struct udev_enumerate* udev_enumerate_unref(struct udev_enumerate* e) {
  if (e && --e->refs == 0) { udev_unref(e->owner); free(e); }
  return NULL;
}
struct udev* udev_enumerate_get_udev(struct udev_enumerate* e) { return e ? e->owner : NULL; }
int udev_enumerate_add_match_subsystem(struct udev_enumerate* e, const char* s) {
  if (e && s && e->nsubsystems < 8) {
    snprintf(e->subsystems[e->nsubsystems++], 32, "%s", s);
  }
  return 0;
}
int udev_enumerate_add_nomatch_subsystem(struct udev_enumerate* e, const char* s) { (void)e; (void)s; return 0; }
int udev_enumerate_add_match_sysattr(struct udev_enumerate* e, const char* a, const char* v) { (void)e; (void)a; (void)v; return 0; }
int udev_enumerate_add_nomatch_sysattr(struct udev_enumerate* e, const char* a, const char* v) { (void)e; (void)a; (void)v; return 0; }
int udev_enumerate_add_match_property(struct udev_enumerate* e, const char* p, const char* v) { (void)e; (void)p; (void)v; return 0; }
int udev_enumerate_add_match_sysname(struct udev_enumerate* e, const char* n) { (void)e; (void)n; return 0; }
int udev_enumerate_add_match_tag(struct udev_enumerate* e, const char* t) { (void)e; (void)t; return 0; }
int udev_enumerate_add_match_parent(struct udev_enumerate* e, struct udev_device* d) { (void)e; (void)d; return 0; }
int udev_enumerate_add_match_is_initialized(struct udev_enumerate* e) { (void)e; return 0; }
int udev_enumerate_add_syspath(struct udev_enumerate* e, const char* p) { (void)e; (void)p; return 0; }
int udev_enumerate_scan_devices(struct udev_enumerate* e) {
  unsigned i;
  if (!e) return -EINVAL;
  e->results.next = NULL;
  if (e->nsubsystems == 0) {
    e->results.next = &s_pad_result;
    return 0;
  }
  for (i = 0; i < e->nsubsystems; ++i) {
    if (strcmp(e->subsystems[i], "input") == 0 || strcmp(e->subsystems[i], "joystick") == 0) {
      e->results.next = &s_pad_result;
      return 0;
    }
  }
  return 0;
}
int udev_enumerate_scan_subsystems(struct udev_enumerate* e) { return udev_enumerate_scan_devices(e); }
struct udev_list_entry* udev_enumerate_get_list_entry(struct udev_enumerate* e) {
  return e ? e->results.next : NULL;
}

/* A quiet, nonblocking pollable descriptor, with no kernel netlink dependency.
   Invalid fds break select(); readable EOF would spin SDL's hotplug loop. */
struct udev_monitor* udev_monitor_new_from_netlink(struct udev* u, const char* n) {
  if (!u || (n && strcmp(n, "udev") && strcmp(n, "kernel"))) { errno = EINVAL; return NULL; }
  struct udev_monitor* m = calloc(1, sizeof(*m));
  if (!m) return NULL;
  m->fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (m->fd < 0) { int error = errno; free(m); errno = error; return NULL; }
  m->refs = 1;
  m->owner = udev_ref(u);
  return m;
}
struct udev_monitor* udev_monitor_new_from_socket(struct udev* u, const char* p) {
  (void)p;
  return udev_monitor_new_from_netlink(u, NULL);
}
struct udev_monitor* udev_monitor_ref(struct udev_monitor* m) {
  if (m) ++m->refs;
  return m;
}
struct udev_monitor* udev_monitor_unref(struct udev_monitor* m) {
  if (m && --m->refs == 0) { close(m->fd); udev_unref(m->owner); free(m); }
  return NULL;
}
struct udev* udev_monitor_get_udev(struct udev_monitor* m) { return m ? m->owner : NULL; }
int udev_monitor_enable_receiving(struct udev_monitor* m) { return m ? 0 : -EINVAL; }
int udev_monitor_set_receive_buffer_size(struct udev_monitor* m, int s) { (void)m; (void)s; return 0; }
int udev_monitor_get_fd(struct udev_monitor* m) { return m ? m->fd : -EINVAL; }
struct udev_device* udev_monitor_receive_device(struct udev_monitor* m) {
  errno = m ? EAGAIN : EINVAL;
  return NULL;
}
int udev_monitor_filter_add_match_subsystem_devtype(struct udev_monitor* m, const char* s, const char* t) { (void)m; (void)s; (void)t; return 0; }
int udev_monitor_filter_add_match_tag(struct udev_monitor* m, const char* t) { (void)m; (void)t; return 0; }
int udev_monitor_filter_update(struct udev_monitor* m) { (void)m; return 0; }
int udev_monitor_filter_remove(struct udev_monitor* m) { (void)m; return 0; }

/* queue: always idle/empty */
struct udev_queue* udev_queue_new(struct udev* u) { (void)u; return &s_queue; }
struct udev_queue* udev_queue_ref(struct udev_queue* q) { return q; }
struct udev_queue* udev_queue_unref(struct udev_queue* q) { (void)q; return NULL; }
struct udev* udev_queue_get_udev(struct udev_queue* q) { (void)q; return &s_udev; }
int udev_queue_get_udev_is_active(struct udev_queue* q) { (void)q; return 0; }
int udev_queue_get_queue_is_empty(struct udev_queue* q) { (void)q; return 1; }
int udev_queue_get_seqnum_is_finished(struct udev_queue* q, unsigned long long s) { (void)q; (void)s; return 1; }
int udev_queue_get_seqnum_sequence_is_finished(struct udev_queue* q, unsigned long long a, unsigned long long b) { (void)q; (void)a; (void)b; return 1; }
unsigned long long udev_queue_get_kernel_seqnum(struct udev_queue* q) { (void)q; return 0; }
unsigned long long udev_queue_get_udev_seqnum(struct udev_queue* q) { (void)q; return 0; }
struct udev_list_entry* udev_queue_get_queued_list_entry(struct udev_queue* q) { (void)q; return NULL; }

/* hwdb / util */
struct udev_hwdb* udev_hwdb_new(struct udev* u) { (void)u; return NULL; }
struct udev_hwdb* udev_hwdb_ref(struct udev_hwdb* h) { return h; }
struct udev_hwdb* udev_hwdb_unref(struct udev_hwdb* h) { (void)h; return NULL; }
struct udev_list_entry* udev_hwdb_get_properties_list_entry(struct udev_hwdb* h, const char* m, unsigned f) { (void)h; (void)m; (void)f; return NULL; }
int udev_util_encode_string(const char* s, char* d, size_t l) { (void)s; (void)d; (void)l; return -1; }
