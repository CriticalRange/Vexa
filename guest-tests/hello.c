/* Freestanding x86_64 Linux program. No libc. */
static long sys3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

void _start(void) {
    static const char out[] = "vexa-hello: ok\n";
    static const char err[] = "vexa-hello: stderr ok\n";
    sys3(1, 1, (long)out, (long)(sizeof(out) - 1));
    sys3(1, 2, (long)err, (long)(sizeof(err) - 1));
    sys3(60, 0, 0, 0);
    __builtin_unreachable();
}
