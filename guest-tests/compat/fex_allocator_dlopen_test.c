// Check dynamic loading of the actual candidate runtime with rpmalloc linked.
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  void* handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return 1;
  }
  puts("PASS rpmalloc-enabled VEXA runtime dlopen");
  // Real app keeps the runtime loaded; avoid testing unsupported teardown here.
  return 0;
}
