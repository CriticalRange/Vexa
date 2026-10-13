#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int64_t appid(void) { return 1794680; }
static void trace(const char *name) { (void)name; }
#include "remote_storage.c"

int main(void) {
  char directory[] = "/data/local/tmp/vexa-storage-test-XXXXXX";
  assert(mkdtemp(directory));
  assert(!setenv("VEXA_STEAM_STORAGE_DIR", directory, 1));
  const char original[] = "save\0with\0binary\xff";
  uint64_t call = storage_write_async("slot.sav", original, sizeof(original));
  assert(call);
  bool failed = true;
  assert(storage_completed(call, &failed) && !failed);
  int32_t result = 0;
  assert(!storage_result(call, &result, 4, 9999, &failed) && failed);
  assert(storage_result(call, &result, 4, 1331, &failed) && !failed && result == 1);
  assert(storage_completed(call, &failed) && failed); /* consumed handle */
  assert(storage_size("slot.sav") == sizeof(original));
  char copy[sizeof(original)];
  assert(storage_read("slot.sav", copy, sizeof(copy)) == sizeof(copy));
  assert(!memcmp(copy, original, sizeof(copy)));
  assert(storage_completed(0, &failed) && failed);
  assert(storage_completed(UINT64_MAX, &failed) && failed);
  call = storage_write_async("slot.sav", NULL, 5);
  assert(call && storage_result(call, &result, 4, 1331, &failed));
  assert(result == 2 && !failed);
  assert(storage_read("slot.sav", copy, sizeof(copy)) == sizeof(copy));
  assert(!memcmp(copy, original, sizeof(copy))); /* failed write keeps save */
  char path[PATH_MAX];
  assert(!storage_path("../escape.sav", path));
  assert(!storage_path("nested/../../escape.sav", path));
  assert(!storage_path("/absolute.sav", path));
  assert(!storage_path("", path));
  assert(storage_write("nested/slot.sav", original, sizeof(original)));
  assert(storage_read("nested/slot.sav", copy, sizeof(copy)) == sizeof(copy));
  assert(!memcmp(copy, original, sizeof(copy)));
  snprintf(path, sizeof(path), "%s/nested/slot.sav", directory); assert(!unlink(path));
  snprintf(path, sizeof(path), "%s/nested", directory); assert(!rmdir(path));
  snprintf(path, sizeof(path), "%s/slot.sav", directory); assert(!unlink(path));
  assert(!rmdir(directory));
  puts("STORAGE_TEST_OK: binary roundtrip, typed completion, failures, save preservation, paths");
}
