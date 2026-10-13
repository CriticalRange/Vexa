#include <libudev.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

int main(void) {
  struct udev *u = udev_new();
  assert(u);
  assert(!udev_monitor_new_from_netlink(u, "invalid"));
  assert(errno == EINVAL);
  assert(!udev_enumerate_new(NULL));
  for (int i = 0; i < 1000; ++i) {
    struct udev_enumerate *e = udev_enumerate_new(u);
    assert(e && udev_enumerate_get_udev(e) == u);
    assert(udev_enumerate_ref(e) == e);
    assert(!udev_enumerate_unref(e));
    assert(udev_enumerate_add_match_subsystem(e, "input") == 0);
    assert(udev_enumerate_scan_devices(e) == 0);
    assert(!udev_enumerate_get_list_entry(e));
    assert(!udev_enumerate_unref(e));

    struct udev_monitor *m = udev_monitor_new_from_netlink(u, "udev");
    assert(m && udev_monitor_get_udev(m) == u);
    assert(udev_monitor_ref(m) == m);
    assert(!udev_monitor_unref(m));
    assert(udev_monitor_filter_add_match_subsystem_devtype(m, "input", NULL) == 0);
    assert(udev_monitor_enable_receiving(m) == 0);
    int fd = udev_monitor_get_fd(m);
    assert(fd >= 0);
    assert(fcntl(fd, F_GETFL) & O_NONBLOCK);
    assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
    struct pollfd p = {fd, POLLIN, 0};
    assert(poll(&p, 1, 0) == 0 && p.revents == 0);
    assert(!udev_monitor_receive_device(m) && errno == EAGAIN);
    assert(!udev_monitor_unref(m));
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
  }
  /* Real descriptor exhaustion must fail cleanly and allow recovery. */
  struct rlimit previous, limited;
  assert(getrlimit(RLIMIT_NOFILE, &previous) == 0);
  limited = previous;
  limited.rlim_cur = 64;
  assert(setrlimit(RLIMIT_NOFILE, &limited) == 0);
  int fds[64], count = 0, fd;
  while ((fd = open("/dev/null", O_RDONLY)) >= 0) fds[count++] = fd;
  assert(errno == EMFILE);
  assert(!udev_monitor_new_from_netlink(u, "udev") && errno == EMFILE);
  while (count) close(fds[--count]);
  assert(setrlimit(RLIMIT_NOFILE, &previous) == 0);
  struct udev_monitor *m = udev_monitor_new_from_netlink(u, "kernel");
  assert(m);
  udev_monitor_unref(m);
  udev_unref(u);
  puts("PASS: empty enumeration, monitor lifetime, quiet fd, exhaustion recovery");
}
