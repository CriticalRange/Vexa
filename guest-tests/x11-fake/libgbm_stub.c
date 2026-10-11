/* KMS/GBM is not the Android surface. A null device makes SDL skip it. */
const char vexa_sdl_stub[] = "VEXA";

void *gbm_bo_create(void) { return 0; }

void *gbm_bo_destroy(void) { return 0; }

void *gbm_bo_get_handle(void) { return 0; }

void *gbm_bo_get_height(void) { return 0; }

void *gbm_bo_get_stride(void) { return 0; }

void *gbm_bo_get_user_data(void) { return 0; }

void *gbm_bo_get_width(void) { return 0; }

void *gbm_bo_set_user_data(void) { return 0; }

void *gbm_bo_write(void) { return 0; }

void *gbm_create_device(void) { return 0; }

void *gbm_device_destroy(void) { return 0; }

void *gbm_device_is_format_supported(void) { return 0; }

void *gbm_surface_create(void) { return 0; }

void *gbm_surface_destroy(void) { return 0; }

void *gbm_surface_lock_front_buffer(void) { return 0; }

void *gbm_surface_release_buffer(void) { return 0; }
