/* Offline local leaderboard backend for the flat Steam UserStats API.
 * Leaderboard finds, score downloads and uploads all succeed immediately:
 * boards live in a per-app local file seeded with fake entries so games see
 * a populated table, and the player's own uploads persist across runs.
 * No Steam service is contacted. */

#define LB_MAX_BOARDS 16
#define LB_MAX_ENTRIES 64
#define LB_FRIENDS 8
#define LB_FRIEND_BASE 76561198000000101ULL
#define LB_SEEDED 12

typedef struct { uint64_t id; int32_t score; } lb_row;
typedef struct {
  char name[64];
  uint64_t handle;
  int loaded;
  int sort;    /* 1 asc, 2 desc */
  int display; /* 1 numeric */
  lb_row rows[LB_MAX_ENTRIES];
  int count;
} lb_board;
static lb_board lb_boards[LB_MAX_BOARDS];
static int lb_board_count;

typedef struct {
  uint64_t handle;
  int32_t callback;
  uint8_t payload[40];
  int payload_size;
} lb_call;
static pthread_mutex_t lb_mutex = PTHREAD_MUTEX_INITIALIZER;
static lb_call lb_calls[32];
static uint64_t lb_next_call = 0x600000000000ULL;

typedef struct { uint64_t handle; int count; lb_row rows[LB_MAX_ENTRIES]; } lb_snapshot;
static lb_snapshot lb_snaps[8];
static uint64_t lb_next_snap = 1;

static int lb_is_fake_friend(uint64_t id) {
  return id >= LB_FRIEND_BASE && id < LB_FRIEND_BASE + LB_FRIENDS;
}

static uint64_t lb_friend_at(int index) {
  return LB_FRIEND_BASE + (uint64_t)index;
}

static int lb_safe_name(const char *name, char out[96]) {
  int j = 0;
  if (!name || !*name) return 0;
  for (const char *p = name; *p && j < 95; ++p) {
    char c = *p;
    out[j++] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ? c : '_';
  }
  out[j] = 0;
  return 1;
}

static int lb_path(const char *name, char path[PATH_MAX]) {
  char safe[96];
  if (!lb_safe_name(name, safe)) return 0;
  const char *root = getenv("VEXA_STEAM_STORAGE_DIR");
  int n;
  if (root && *root) {
    n = snprintf(path, PATH_MAX, "%s/leaderboards/%s.lbd", root, safe);
  } else {
    const char *home = getenv("HOME");
    if (!home || !*home) return 0;
    n = snprintf(path, PATH_MAX, "%s/.local/share/vexa/steam/%lld/leaderboards/%s.lbd",
                 home, (long long)appid(), safe);
  }
  return n > 0 && n < PATH_MAX;
}

static uint64_t lb_hash(const char *name) {
  uint64_t h = 1469598103934665603ULL;
  for (const char *p = name; p && *p; ++p) { h ^= (uint8_t)*p; h *= 1099511628211ULL; }
  return h ? h : 1;
}

static void lb_seed(lb_board *b) {
  uint64_t h = lb_hash(b->name);
  int32_t top = 20000 + (int32_t)(h % 60000);
  b->count = LB_SEEDED;
  for (int i = 0; i < LB_SEEDED; ++i) {
    b->rows[i].id = i < LB_FRIENDS ? lb_friend_at(i) : 76561199000000000ULL + (uint64_t)i;
    b->rows[i].score = top - (int32_t)((top * (int64_t)i) / (LB_SEEDED + 2)) - i * 137;
    if (b->rows[i].score < 1) b->rows[i].score = 1 + i;
  }
}

static void lb_sort(lb_board *b) {
  for (int i = 1; i < b->count; ++i) {
    lb_row row = b->rows[i];
    int j = i - 1;
    int better;
    while (j >= 0) {
      better = b->sort == 1 ? b->rows[j].score > row.score : b->rows[j].score < row.score;
      if (!better) break;
      b->rows[j + 1] = b->rows[j];
      --j;
    }
    b->rows[j + 1] = row;
  }
}

static void lb_save(lb_board *b) {
  char path[PATH_MAX], temporary[PATH_MAX];
  if (!lb_path(b->name, path)) return;
  for (char *p = path + 1; *p; ++p) {
    if (*p != '/') continue;
    *p = 0;
    mkdir(path, 0700);
    *p = '/';
  }
  int n = snprintf(temporary, sizeof(temporary), "%s.vexa-XXXXXX", path);
  if (n < 0 || n >= (int)sizeof(temporary)) return;
  int fd = mkstemp(temporary);
  if (fd < 0) return;
  char line[64];
  int ok = 1;
  for (int i = 0; i < b->count && ok; ++i) {
    n = snprintf(line, sizeof(line), "%llu %d\n",
                 (unsigned long long)b->rows[i].id, (int)b->rows[i].score);
    size_t offset = 0;
    while (offset < (size_t)n) {
      ssize_t written = write(fd, line + offset, (size_t)n - offset);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) { ok = 0; break; }
      offset += (size_t)written;
    }
  }
  if (fsync(fd) != 0) ok = 0;
  if (close(fd) != 0) ok = 0;
  if (ok) ok = rename(temporary, path) == 0;
  if (!ok) unlink(temporary);
}

static void lb_load(lb_board *b) {
  if (b->loaded) return;
  b->loaded = 1;
  char path[PATH_MAX];
  if (lb_path(b->name, path)) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
      char data[16384];
      ssize_t total = read(fd, data, sizeof(data) - 1);
      close(fd);
      if (total > 0) {
        data[total] = 0;
        char *cursor = data;
        while (b->count < LB_MAX_ENTRIES) {
          unsigned long long id;
          long score;
          int used = 0;
          if (sscanf(cursor, "%llu %ld%n", &id, &score, &used) != 2 || !used) break;
          b->rows[b->count].id = (uint64_t)id;
          b->rows[b->count].score = (int32_t)score;
          ++b->count;
          cursor += used;
        }
      }
    }
  }
  if (!b->count) {
    lb_seed(b);
    lb_save(b);
  }
  lb_sort(b);
}

static lb_board *lb_board_for(uint64_t handle) {
  for (int i = 0; i < lb_board_count; ++i)
    if (lb_boards[i].handle == handle) return &lb_boards[i];
  return 0;
}

static lb_board *lb_open(const char *name, int sort, int display) {
  pthread_mutex_lock(&lb_mutex);
  uint64_t handle = lb_hash(name);
  lb_board *b = lb_board_for(handle);
  if (!b && lb_board_count < LB_MAX_BOARDS) {
    b = &lb_boards[lb_board_count++];
    memset(b, 0, sizeof(*b));
    snprintf(b->name, sizeof(b->name), "%s", name && *name ? name : "leaderboard");
    b->handle = handle;
    b->sort = sort == 1 || sort == 2 ? sort : 2;
    b->display = display >= 1 && display <= 3 ? display : 1;
  }
  if (b) lb_load(b);
  pthread_mutex_unlock(&lb_mutex);
  return b;
}

static uint64_t lb_register(int callback, const void *payload, int size) {
  pthread_mutex_lock(&lb_mutex);
  unsigned slot = 0;
  while (slot < 32 && lb_calls[slot].handle) ++slot;
  if (slot == 32 || size > (int)sizeof(lb_calls[0].payload)) {
    pthread_mutex_unlock(&lb_mutex);
    return 0;
  }
  uint64_t handle = lb_next_call++;
  lb_calls[slot].handle = handle;
  lb_calls[slot].callback = callback;
  lb_calls[slot].payload_size = size;
  memcpy(lb_calls[slot].payload, payload, (size_t)size);
  pthread_mutex_unlock(&lb_mutex);
  return handle;
}

/* LeaderboardFindResult_t / LeaderboardScoresDownloaded_t / ... use natural
 * x86_64 alignment, matching the SDK structs the game allocates. */
static uint64_t lb_find_call(const char *name, int create, int sort, int display) {
  lb_board *b = lb_open(name, sort, display);
  struct { uint64_t lb; uint8_t found; } result = { b ? b->handle : 0, (uint8_t)(b || create) };
  trace("FindLeaderboard completed");
  return lb_register(1104, &result, (int)sizeof(result));
}

static const char *lb_name(uint64_t handle) {
  lb_board *b = lb_board_for(handle);
  return b ? b->name : "";
}

static int64_t lb_entry_count(uint64_t handle) {
  lb_board *b = lb_board_for(handle);
  return b ? b->count : 0;
}

static int64_t lb_sort_method(uint64_t handle) {
  lb_board *b = lb_board_for(handle);
  return b ? b->sort : 2;
}

static int64_t lb_display_type(uint64_t handle) {
  lb_board *b = lb_board_for(handle);
  return b ? b->display : 1;
}

static uint64_t lb_download_call(uint64_t handle, int request, int start, int end,
                                 const uint64_t *users, int user_count) {
  lb_board *b = lb_board_for(handle);
  uint64_t snap_handle = 0;
  int count = 0;
  pthread_mutex_lock(&lb_mutex);
  if (b) {
    lb_load(b);
    lb_snapshot *snap = &lb_snaps[lb_next_snap % 8];
    snap_handle = lb_next_snap++;
    snap->handle = snap_handle;
    snap->count = 0;
    if (request == 2) {
      for (int i = 0; i < b->count && snap->count < LB_MAX_ENTRIES; ++i)
        if (lb_is_fake_friend(b->rows[i].id) || b->rows[i].id == steam_id())
          snap->rows[snap->count++] = b->rows[i];
    } else if (request == 3) {
      for (int i = 0; i < b->count && snap->count < LB_MAX_ENTRIES; ++i)
        for (int u = 0; u < user_count; ++u)
          if (b->rows[i].id == users[u]) { snap->rows[snap->count++] = b->rows[i]; break; }
    } else if (request == 1) {
      int rank = -1;
      for (int i = 0; i < b->count; ++i)
        if (b->rows[i].id == steam_id()) { rank = i; break; }
      if (rank < 0) rank = b->count / 2;
      int lo = rank + start, hi = rank + end;
      if (lo < 0) lo = 0;
      if (hi >= b->count) hi = b->count - 1;
      for (int i = lo; i <= hi && snap->count < LB_MAX_ENTRIES; ++i)
        snap->rows[snap->count++] = b->rows[i];
    } else {
      int lo = start > 0 ? start - 1 : 0;
      int hi = end >= start ? end : b->count;
      for (int i = lo; i < hi && i < b->count && snap->count < LB_MAX_ENTRIES; ++i)
        snap->rows[snap->count++] = b->rows[i];
    }
    count = snap->count;
  }
  pthread_mutex_unlock(&lb_mutex);
  struct { uint64_t lb; uint64_t entries; int32_t count; } result = { handle, snap_handle, count };
  trace("DownloadLeaderboardEntries completed");
  return lb_register(1105, &result, (int)sizeof(result));
}

static int lb_get_entry(uint64_t entries, int index, void *out, int32_t *details, int max_details) {
  typedef struct { uint64_t steam; int32_t rank; int32_t score; int32_t cdetails; uint64_t ugc; } lb_entry;
  lb_snapshot *snap = 0;
  pthread_mutex_lock(&lb_mutex);
  for (unsigned i = 0; i < 8; ++i)
    if (lb_snaps[i].handle == entries) { snap = &lb_snaps[i]; break; }
  if (!snap || index < 0 || index >= snap->count) {
    pthread_mutex_unlock(&lb_mutex);
    return 0;
  }
  lb_row row = snap->rows[index];
  pthread_mutex_unlock(&lb_mutex);
  if (out) {
    lb_entry *e = (lb_entry*)out;
    e->steam = row.id;
    e->rank = index + 1;
    e->score = row.score;
    e->cdetails = 0;
    e->ugc = 0;
  }
  if (details && max_details > 0) details[0] = 0;
  return 1;
}

static uint64_t lb_upload_call(uint64_t handle, int method, int32_t score) {
  lb_board *b = lb_board_for(handle);
  int changed = 0;
  int rank_prev = -1, rank_new = -1;
  if (b) {
    pthread_mutex_lock(&lb_mutex);
    lb_load(b);
    int idx = -1;
    for (int i = 0; i < b->count; ++i) {
      if (b->rows[i].id == steam_id()) { idx = i; rank_prev = i + 1; break; }
    }
    int better = b->sort == 1 ? score < (idx >= 0 ? b->rows[idx].score : INT32_MAX)
                              : score > (idx >= 0 ? b->rows[idx].score : INT32_MIN);
    if (method == 2 || idx < 0 || better) {
      if (idx < 0 && b->count < LB_MAX_ENTRIES) {
        idx = b->count++;
        b->rows[idx].id = steam_id();
      }
      if (idx >= 0) {
        b->rows[idx].score = score;
        changed = 1;
        lb_sort(b);
        lb_save(b);
      }
    }
    for (int i = 0; i < b->count; ++i)
      if (b->rows[i].id == steam_id()) { rank_new = i + 1; break; }
    pthread_mutex_unlock(&lb_mutex);
  }
  struct { uint8_t ok; uint64_t lb; int32_t score; uint8_t changed; int32_t rank_new; int32_t rank_prev; } result =
      { (uint8_t)(b != 0), handle, score, (uint8_t)changed, rank_new, rank_prev };
  trace("UploadLeaderboardScore completed");
  return lb_register(1106, &result, (int)sizeof(result));
}

static uint64_t lb_ugc_call(uint64_t handle) {
  struct { int32_t result; uint64_t lb; } result = { 1, handle };
  return lb_register(1111, &result, (int)sizeof(result));
}

static uint64_t lb_players_call(void) {
  struct { uint8_t ok; int32_t players; } result = { 1, 5 };
  return lb_register(1107, &result, (int)sizeof(result));
}

static int64_t lb_friend_count(int flags) {
  return (flags & 4) || flags == -1 || flags == 0xFFFF ? LB_FRIENDS : 0;
}

static uint64_t lb_friend_index(int index, int flags) {
  if (!lb_friend_count(flags) || index < 0 || index >= LB_FRIENDS) return 0;
  return lb_friend_at(index);
}

static int64_t lb_relationship(uint64_t id) {
  return lb_is_fake_friend(id) ? 3 : 0;
}

static const char *lb_persona(uint64_t id) {
  static __thread char name[40];
  if (id == steam_id()) return persona();
  if (lb_is_fake_friend(id))
    snprintf(name, sizeof(name), "Player %d", (int)(id - LB_FRIEND_BASE) + 1);
  else
    snprintf(name, sizeof(name), "Player %llu", (unsigned long long)(id % 100000));
  return name;
}

static int apicall_completed(uint64_t handle, bool *failed) {
  pthread_mutex_lock(&lb_mutex);
  for (unsigned i = 0; i < 32; ++i) {
    if (handle && lb_calls[i].handle == handle) {
      if (failed) *failed = false;
      pthread_mutex_unlock(&lb_mutex);
      return 1;
    }
  }
  pthread_mutex_unlock(&lb_mutex);
  return storage_completed(handle, failed);
}

static int apicall_result(uint64_t handle, void *data, int size, int callback, bool *failed) {
  pthread_mutex_lock(&lb_mutex);
  for (unsigned i = 0; i < 32; ++i) {
    if (handle && lb_calls[i].handle == handle) {
      int ok = data && size >= lb_calls[i].payload_size &&
               callback == lb_calls[i].callback;
      if (ok) {
        memcpy(data, lb_calls[i].payload, (size_t)lb_calls[i].payload_size);
        lb_calls[i].handle = 0;
      }
      if (failed) *failed = !ok;
      pthread_mutex_unlock(&lb_mutex);
      return ok;
    }
  }
  pthread_mutex_unlock(&lb_mutex);
  return storage_result(handle, data, size, callback, failed);
}
