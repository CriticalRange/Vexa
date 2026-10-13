/* Test-only link wrapper: reproduce the old NULL monitor failure without
   altering production code or requiring device/descriptor exhaustion in SDL. */
#include <errno.h>
#include <stdlib.h>
int __real_eventfd(unsigned int initial, int flags);
int __wrap_eventfd(unsigned int initial, int flags) {
  if (getenv("VEXA_TEST_FAIL_MONITOR")) { errno = EMFILE; return -1; }
  return __real_eventfd(initial, flags);
}
