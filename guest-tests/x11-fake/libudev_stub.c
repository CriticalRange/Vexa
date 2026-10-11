/* Guest libudev.so.1 stub. There is no udev on Android: every API returns an
   empty/benign result so callers (Rewired, SDL hotplug) enumerate zero devices
   instead of failing to load. Input reaches guests through fake X11 anyway. */
#include <stddef.h>
#include <sys/types.h>

struct udev { int unused; };
struct udev_device { int unused; };
struct udev_enumerate { int unused; };
struct udev_monitor { int unused; };
struct udev_queue { int unused; };
struct udev_list_entry { int unused; };
struct udev_hwdb { int unused; };

static struct udev s_udev;
static struct udev_queue s_queue;

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

/* devices: never any, every query is empty */
struct udev_device* udev_device_ref(struct udev_device* d) { return d; }
struct udev_device* udev_device_unref(struct udev_device* d) { (void)d; return NULL; }
struct udev* udev_device_get_udev(struct udev_device* d) { (void)d; return &s_udev; }
struct udev_device* udev_device_new_from_syspath(struct udev* u, const char* p) { (void)u; (void)p; return NULL; }
struct udev_device* udev_device_new_from_devnum(struct udev* u, char t, dev_t n) { (void)u; (void)t; (void)n; return NULL; }
struct udev_device* udev_device_new_from_subsystem_sysname(struct udev* u, const char* s, const char* n) { (void)u; (void)s; (void)n; return NULL; }
struct udev_device* udev_device_new_from_device_id(struct udev* u, const char* i) { (void)u; (void)i; return NULL; }
struct udev_device* udev_device_new_from_environment(struct udev* u) { (void)u; return NULL; }
struct udev_device* udev_device_get_parent(struct udev_device* d) { (void)d; return NULL; }
struct udev_device* udev_device_get_parent_with_subsystem_devtype(struct udev_device* d, const char* s, const char* t) { (void)d; (void)s; (void)t; return NULL; }
const char* udev_device_get_devpath(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_subsystem(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_devtype(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_syspath(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_sysname(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_sysnum(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_devnode(struct udev_device* d) { (void)d; return NULL; }
int udev_device_get_is_initialized(struct udev_device* d) { (void)d; return 0; }
struct udev_list_entry* udev_device_get_devlinks_list_entry(struct udev_device* d) { (void)d; return NULL; }
struct udev_list_entry* udev_device_get_properties_list_entry(struct udev_device* d) { (void)d; return NULL; }
struct udev_list_entry* udev_device_get_tags_list_entry(struct udev_device* d) { (void)d; return NULL; }
struct udev_list_entry* udev_device_get_sysattr_list_entry(struct udev_device* d) { (void)d; return NULL; }
const char* udev_device_get_property_value(struct udev_device* d, const char* k) { (void)d; (void)k; return NULL; }
const char* udev_device_get_driver(struct udev_device* d) { (void)d; return NULL; }
dev_t udev_device_get_devnum(struct udev_device* d) { (void)d; return 0; }
const char* udev_device_get_action(struct udev_device* d) { (void)d; return NULL; }
unsigned long long udev_device_get_seqnum(struct udev_device* d) { (void)d; return 0; }
unsigned long long udev_device_get_usec_since_initialized(struct udev_device* d) { (void)d; return 0; }
const char* udev_device_get_sysattr_value(struct udev_device* d, const char* a) { (void)d; (void)a; return NULL; }
int udev_device_set_sysattr_value(struct udev_device* d, const char* a, const char* v) { (void)d; (void)a; (void)v; return -1; }
int udev_device_has_tag(struct udev_device* d, const char* t) { (void)d; (void)t; return 0; }
int udev_device_has_current_tag(struct udev_device* d, const char* t) { (void)d; (void)t; return 0; }
struct udev_list_entry* udev_device_get_current_tags_list_entry(struct udev_device* d) { (void)d; return NULL; }

/* list entries: always empty */
struct udev_list_entry* udev_list_entry_get_next(struct udev_list_entry* e) { (void)e; return NULL; }
struct udev_list_entry* udev_list_entry_get_by_name(struct udev_list_entry* e, const char* n) { (void)e; (void)n; return NULL; }
const char* udev_list_entry_get_name(struct udev_list_entry* e) { (void)e; return NULL; }
const char* udev_list_entry_get_value(struct udev_list_entry* e) { (void)e; return NULL; }

/* enumerate: empty result set */
struct udev_enumerate* udev_enumerate_new(struct udev* u) { (void)u; return NULL; }
struct udev_enumerate* udev_enumerate_ref(struct udev_enumerate* e) { return e; }
struct udev_enumerate* udev_enumerate_unref(struct udev_enumerate* e) { (void)e; return NULL; }
struct udev* udev_enumerate_get_udev(struct udev_enumerate* e) { (void)e; return &s_udev; }
int udev_enumerate_add_match_subsystem(struct udev_enumerate* e, const char* s) { (void)e; (void)s; return 0; }
int udev_enumerate_add_nomatch_subsystem(struct udev_enumerate* e, const char* s) { (void)e; (void)s; return 0; }
int udev_enumerate_add_match_sysattr(struct udev_enumerate* e, const char* a, const char* v) { (void)e; (void)a; (void)v; return 0; }
int udev_enumerate_add_nomatch_sysattr(struct udev_enumerate* e, const char* a, const char* v) { (void)e; (void)a; (void)v; return 0; }
int udev_enumerate_add_match_property(struct udev_enumerate* e, const char* p, const char* v) { (void)e; (void)p; (void)v; return 0; }
int udev_enumerate_add_match_sysname(struct udev_enumerate* e, const char* n) { (void)e; (void)n; return 0; }
int udev_enumerate_add_match_tag(struct udev_enumerate* e, const char* t) { (void)e; (void)t; return 0; }
int udev_enumerate_add_match_parent(struct udev_enumerate* e, struct udev_device* d) { (void)e; (void)d; return 0; }
int udev_enumerate_add_match_is_initialized(struct udev_enumerate* e) { (void)e; return 0; }
int udev_enumerate_add_syspath(struct udev_enumerate* e, const char* p) { (void)e; (void)p; return 0; }
int udev_enumerate_scan_devices(struct udev_enumerate* e) { (void)e; return 0; }
int udev_enumerate_scan_subsystems(struct udev_enumerate* e) { (void)e; return 0; }
struct udev_list_entry* udev_enumerate_get_list_entry(struct udev_enumerate* e) { (void)e; return NULL; }

/* monitor: no kernel netlink, fd stays invalid so poll() never fires */
struct udev_monitor* udev_monitor_new_from_netlink(struct udev* u, const char* n) { (void)u; (void)n; return NULL; }
struct udev_monitor* udev_monitor_new_from_socket(struct udev* u, const char* p) { (void)u; (void)p; return NULL; }
struct udev_monitor* udev_monitor_ref(struct udev_monitor* m) { return m; }
struct udev_monitor* udev_monitor_unref(struct udev_monitor* m) { (void)m; return NULL; }
struct udev* udev_monitor_get_udev(struct udev_monitor* m) { (void)m; return &s_udev; }
int udev_monitor_enable_receiving(struct udev_monitor* m) { (void)m; return -1; }
int udev_monitor_set_receive_buffer_size(struct udev_monitor* m, int s) { (void)m; (void)s; return 0; }
int udev_monitor_get_fd(struct udev_monitor* m) { (void)m; return -1; }
struct udev_device* udev_monitor_receive_device(struct udev_monitor* m) { (void)m; return NULL; }
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
