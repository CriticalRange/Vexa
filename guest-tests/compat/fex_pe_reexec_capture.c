#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  const char* target = "/data/local/tmp/vexa2610-rpmalloc/probe.exe";
  if (argc != 3 || strcmp(argv[1], target) || strcmp(argv[2], "arg-one") || getenv("FEX_EXECVEFD")) {
    fprintf(stderr, "FAIL PE reexec argc=%d fd=%s\n", argc, getenv("FEX_EXECVEFD") ? "set" : "unset");
    return 98;
  }
  puts("PASS PE reexec pathname, argument and no FEX_EXECVEFD");
  return 0;
}
