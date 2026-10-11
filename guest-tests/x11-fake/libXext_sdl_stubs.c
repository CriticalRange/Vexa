/* SHM and Xdbe are absent. SDL falls back to the core X path. */
const char vexa_sdl_stub[] = "VEXA";

void *XShapeCombineMask(void) { return 0; }

void *XShmAttach(void) { return 0; }

void *XShmCreateImage(void) { return 0; }

void *XShmDetach(void) { return 0; }

void *XShmPutImage(void) { return 0; }

void *XShmQueryExtension(void) { return 0; }

void *XdbeAllocateBackBufferName(void) { return 0; }

void *XdbeBeginIdiom(void) { return 0; }

void *XdbeDeallocateBackBufferName(void) { return 0; }

void *XdbeEndIdiom(void) { return 0; }

void *XdbeQueryExtension(void) { return 0; }

void *XdbeSwapBuffers(void) { return 0; }
