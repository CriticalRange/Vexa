/* Offline local backing for the flat Steam RemoteStorage write API.
 * A completed write owns a typed result; invalid handles complete as failures.
 * No Steam cloud service is contacted. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

typedef struct { uint64_t handle; int32_t result; } storage_call;
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;
static storage_call storage_calls[64];
static uint64_t storage_next = 1;

static int storage_path(const char *name, char path[PATH_MAX]) {
  if (!name || !*name || name[0] == '/') return 0;
  const char *part = name;
  for (const char *p = name;; ++p) {
    if (*p == '/' || !*p) {
      size_t n = (size_t)(p - part);
      if (!n || (n == 1 && part[0] == '.') ||
          (n == 2 && part[0] == '.' && part[1] == '.')) return 0;
      if (!*p) break;
      part = p + 1;
    }
  }
  const char *root = getenv("VEXA_STEAM_STORAGE_DIR");
  int n;
  if (root && *root) {
    n = snprintf(path, PATH_MAX, "%s/%s", root, name);
  } else {
    const char *home = getenv("HOME");
    if (!home || !*home) return 0;
    n = snprintf(path, PATH_MAX, "%s/.local/share/vexa/steam/%lld/remote/%s",
                 home, (long long)appid(), name);
  }
  return n > 0 && n < PATH_MAX;
}

static bool storage_write(const char *name, const void *data, uint32_t size) {
  char path[PATH_MAX], temporary[PATH_MAX];
  if ((!data && size) || size > INT_MAX || !storage_path(name, path)) return false;
  pthread_mutex_lock(&storage_mutex);
  bool ok = false;
  for (char *p = path + 1; *p; ++p) {
    if (*p != '/') continue;
    *p = 0;
    int rc = mkdir(path, 0700);
    *p = '/';
    if (rc && errno != EEXIST) goto done;
  }
  int n = snprintf(temporary, sizeof(temporary), "%s.vexa-XXXXXX", path);
  if (n < 0 || n >= (int)sizeof(temporary)) goto done;
  int fd = mkstemp(temporary);
  if (fd < 0) goto done;
  size_t offset = 0;
  while (offset < size) {
    ssize_t written = write(fd, (const char*)data + offset, size - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) break;
    offset += (size_t)written;
  }
  ok = offset == size && fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
  if (ok) ok = rename(temporary, path) == 0;
  if (!ok) unlink(temporary);
done:
  pthread_mutex_unlock(&storage_mutex);
  return ok;
}

static int32_t storage_size(const char *name) {
  char path[PATH_MAX];
  struct stat st;
  if (!storage_path(name, path) || stat(path, &st) || !S_ISREG(st.st_mode) ||
      st.st_size > INT_MAX) return 0;
  return (int32_t)st.st_size;
}

static int32_t storage_read(const char *name, void *data, int32_t size) {
  char path[PATH_MAX];
  if (size < 0 || (!data && size) || !storage_path(name, path)) return 0;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return 0;
  int32_t offset = 0;
  while (offset < size) {
    ssize_t n = read(fd, (char*)data + offset, (size_t)(size - offset));
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    offset += (int32_t)n;
  }
  close(fd);
  return offset;
}

static uint64_t storage_write_async(const char *name, const void *data, uint32_t size) {
  /* Reserve the handle before writing so exhaustion cannot modify a save. */
  pthread_mutex_lock(&storage_mutex);
  unsigned slot = 0;
  while (slot < 64 && storage_calls[slot].handle) ++slot;
  if (slot == 64) { pthread_mutex_unlock(&storage_mutex); return 0; }
  uint64_t handle = storage_next++;
  if (!handle) handle = storage_next++;
  storage_calls[slot] = (storage_call){handle, 0};
  pthread_mutex_unlock(&storage_mutex);
  bool ok = storage_write(name, data, size);
  pthread_mutex_lock(&storage_mutex);
  storage_calls[slot].result = ok ? 1 : 2; /* k_EResultOK / k_EResultFail */
  pthread_mutex_unlock(&storage_mutex);
  trace(ok ? "local FileWriteAsync completed" : "local FileWriteAsync failed");
  return handle;
}

static bool storage_completed(uint64_t handle, bool *failed) {
  pthread_mutex_lock(&storage_mutex);
  for (unsigned i = 0; i < 64; ++i) {
    if (handle && storage_calls[i].handle == handle) {
      bool complete = storage_calls[i].result != 0;
      if (failed) *failed = false; /* operation failure is in the typed result */
      pthread_mutex_unlock(&storage_mutex);
      return complete;
    }
  }
  pthread_mutex_unlock(&storage_mutex);
  if (failed) *failed = true;
  return true;
}

static bool storage_result(uint64_t handle, void *data, int size, int callback, bool *failed) {
  pthread_mutex_lock(&storage_mutex);
  for (unsigned i = 0; i < 64; ++i) {
    if (handle && storage_calls[i].handle == handle && storage_calls[i].result &&
        data && size >= 4 && callback == 1331) {
      memcpy(data, &storage_calls[i].result, 4);
      storage_calls[i].handle = 0;
      if (failed) *failed = false;
      pthread_mutex_unlock(&storage_mutex);
      return true;
    }
  }
  pthread_mutex_unlock(&storage_mutex);
  if (failed) *failed = true;
  return false;
}
