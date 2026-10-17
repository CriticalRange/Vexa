/* UnityPlayer (Hollow Knight) calls memcpy@GLIBC_2.14. The guest libc
 * exports that version as an IFUNC. On this runtime the resolved slot is
 * NULL, so the first call jumps to address 0. Provide a plain function at
 * that symbol version and forward to the non-IFUNC memcpy@GLIBC_2.2.5.
 */
#include <stddef.h>

__asm__(".symver memcpy_old, memcpy@GLIBC_2.2.5");
void* memcpy_old(void* dst, const void* src, size_t n);

__asm__(".symver memcpy_214, memcpy@GLIBC_2.14");
void* memcpy_214(void* dst, const void* src, size_t n) {
    return memcpy_old(dst, src, n);
}
