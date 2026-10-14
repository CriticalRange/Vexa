/* Opt-in compatibility for Megabonk's verified UnityPlayer build. Its screen
   manager enumerates 71 fixed sizes <= the desktop, omitting unusual native
   sizes. Replace the final 122880x69120 entry (outside our supported surfaces)
   with the published native size; retain every ordinary PC mode and index.
   No executable code or file on disk is modified. */
#if defined(__x86_64__)
#include <link.h>
#include <sys/mman.h>

struct vexa_native_mode_range { uintptr_t base, table; int readable; };
static int vexa_native_mode_segment(struct dl_phdr_info* info, size_t size, void* opaque) {
  (void)size;
  struct vexa_native_mode_range* range = opaque;
  if ((uintptr_t)info->dlpi_addr != range->base) return 0;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)* p = &info->dlpi_phdr[i];
    const uintptr_t start = range->base + p->p_vaddr;
    if (p->p_type == PT_LOAD && (p->p_flags & PF_R) && !(p->p_flags & PF_W) &&
        range->table >= start && range->table + 71 * 8 <= start + p->p_memsz)
      range->readable = 1;
  }
  return 1;
}

static void vexa_unity_native_mode(void* caller) {
  const char* enabled = getenv("VEXA_UNITY_NATIVE_MODE");
  if (!enabled || strcmp(enabled, "1") || !g_published) return;
  Dl_info module;
  if (!dladdr(caller, &module) || !module.dli_fname) return;
  const char* name = strrchr(module.dli_fname, '/');
  if (strcmp(name ? name + 1 : module.dli_fname, "UnityPlayer.so")) return;
  struct vexa_native_mode_range range = {(uintptr_t)module.dli_fbase,
                                        (uintptr_t)module.dli_fbase + 0x301930, 0};
  dl_iterate_phdr(vexa_native_mode_segment, &range);
  if (!range.readable) return;
  uint32_t (*modes)[2] = (void*)range.table;
  const uint32_t width = g_screen.width, height = g_screen.height;
  if (width < 2 || height < 2 || width > 8192 || height > 8192) return;
  for (int i = 0; i < 71; ++i)
    if (modes[i][0] == width && modes[i][1] == height) return;
  uint64_t signature = UINT64_C(14695981039346656037);
  for (int i = 0; i < 71 * 8; ++i)
    signature = (signature ^ ((const unsigned char*)modes)[i]) * UINT64_C(1099511628211);
  if (signature != UINT64_C(0x1d88f0ab5e2257a9)) {
    fprintf(stderr, "[VEXA][UNITY] native mode skipped: unmatched mode table\n");
    return;
  }
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) return;
  void* page = (void*)((uintptr_t)&modes[70] & ~((uintptr_t)page_size - 1));
  if (mprotect(page, page_size, PROT_READ | PROT_WRITE)) return;
  modes[70][0] = width;
  modes[70][1] = height;
  if (mprotect(page, page_size, PROT_READ))
    fprintf(stderr, "[VEXA][UNITY] native mode page protection restore failed: %d\n", errno);
  fprintf(stderr, "[VEXA][UNITY] added native mode %ux%u to verified table\n", width, height);
}
#else
static void vexa_unity_native_mode(void* caller) { (void)caller; }
#endif
