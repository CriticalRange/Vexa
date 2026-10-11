/* Pulse is probed first. A failed context sends SDL on to ALSA. */
const char vexa_sdl_stub[] = "VEXA";

void *pa_channel_map_init_auto(void) { return 0; }

void *pa_context_connect(void) { return (void *)(unsigned long)-1; }

void *pa_context_disconnect(void) { return 0; }

void *pa_context_get_sink_info_by_index(void) { return 0; }

void *pa_context_get_sink_info_list(void) { return 0; }

void *pa_context_get_source_info_by_index(void) { return 0; }

void *pa_context_get_source_info_list(void) { return 0; }

void *pa_context_get_state(void) { return (void *)5; }

void *pa_context_new(void) { return 0; }

void *pa_context_set_subscribe_callback(void) { return 0; }

void *pa_context_subscribe(void) { return 0; }

void *pa_context_unref(void) { return 0; }

void *pa_get_library_version(void) { return 0; }

void *pa_mainloop_free(void) { return 0; }

void *pa_mainloop_get_api(void) { return 0; }

void *pa_mainloop_iterate(void) { return 0; }

void *pa_mainloop_new(void) { return 0; }

void *pa_mainloop_quit(void) { return 0; }

void *pa_mainloop_run(void) { return 0; }

void *pa_operation_get_state(void) { return (void *)2; }

void *pa_operation_unref(void) { return 0; }

void *pa_stream_begin_write(void) { return 0; }

void *pa_stream_cancel_write(void) { return 0; }

void *pa_stream_connect_playback(void) { return 0; }

void *pa_stream_connect_record(void) { return 0; }

void *pa_stream_disconnect(void) { return 0; }

void *pa_stream_drop(void) { return 0; }

void *pa_stream_get_state(void) { return 0; }

void *pa_stream_new(void) { return 0; }

void *pa_stream_peek(void) { return 0; }

void *pa_stream_readable_size(void) { return 0; }

void *pa_stream_unref(void) { return 0; }

void *pa_stream_writable_size(void) { return 0; }

void *pa_stream_write(void) { return 0; }
