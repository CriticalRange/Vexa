/* Packaged as libvexaloader.so in jniLibs so the app can execve() it from
 * nativeLibraryDir (the only app-writable location with exec permission).
 * FEX's guest execve fallback targets this binary via VEXA_REEXEC. It resolves
 * libvexa.so from the directory of /proc/self/exe (the nativeLibraryDir) and
 * forwards into VexaLoaderMain. */

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef int (*VexaLoaderMainFn)(int argc, char **argv, char **envp);

int main(int argc, char **argv, char **envp) {
    char self[1024];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n <= 0) {
        fprintf(stderr, "vexaloader: cannot resolve /proc/self/exe\n");
        return 127;
    }
    self[n] = '\0';
    char *slash = strrchr(self, '/');
    if (!slash) {
        fprintf(stderr, "vexaloader: bad exe path %s\n", self);
        return 127;
    }
    slash[1] = '\0';

    char lib[1100];
    // libvexa.so needs libc++_shared.so, which the default linker namespace
    // cannot find outside the app process; preload it from the same dir first.
    snprintf(lib, sizeof(lib), "%slibc++_shared.so", self);
    if (!dlopen(lib, RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "vexaloader: dlopen %s failed: %s\n", lib, dlerror());
        return 127;
    }
    snprintf(lib, sizeof(lib), "%slibvexa.so", self);
    void *h = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
    if (!h) {
        fprintf(stderr, "vexaloader: dlopen %s failed: %s\n", lib, dlerror());
        return 127;
    }
    VexaLoaderMainFn fn = (VexaLoaderMainFn) dlsym(h, "VexaLoaderMain");
    if (!fn) {
        fprintf(stderr, "vexaloader: VexaLoaderMain missing\n");
        return 127;
    }
    return fn(argc, argv, envp);
}
