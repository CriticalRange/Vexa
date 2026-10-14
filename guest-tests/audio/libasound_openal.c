/* Guest libasound.so.2 for Godot's ALSA driver.
   The loader dlsyms the whole asoundlib surface; playback calls feed OpenAL.
   libopenal.so.1 is the FEX guest thunk, which plays on host OpenSL. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>

#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO16 0x1103
#define AL_SOURCE_STATE 0x1010
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_BUFFERS_QUEUED 0x1015
#define AL_BUFFERS_PROCESSED 0x1016
#define AL_VENDOR 0xB001
#define AL_VERSION 0xB002
#define AL_RENDERER 0xB003
#define NBUF 8

typedef void* (*alc_open_fn)(const char*);
typedef void* (*alc_create_fn)(void*, const int*);
typedef char (*alc_make_fn)(void*);
typedef void* (*alc_current_fn)(void);
typedef const char* (*alc_string_fn)(void*, int);
typedef void (*al_gen_fn)(int, unsigned*);
typedef void (*al_bufdata_fn)(unsigned, int, const void*, int, int);
typedef void (*al_queue_fn)(unsigned, int, const unsigned*);
typedef void (*al_unqueue_fn)(unsigned, int, unsigned*);
typedef void (*al_play_fn)(unsigned);
typedef void (*al_stop_fn)(unsigned);
typedef void (*al_pause_fn)(unsigned);
typedef void (*al_geti_fn)(unsigned, int, int*);
typedef const char* (*al_string_fn)(int);
typedef int (*al_error_fn)(void);

static alc_open_fn p_alcOpenDevice;
static alc_create_fn p_alcCreateContext;
static alc_make_fn p_alcMakeContextCurrent;
static alc_current_fn p_alcGetCurrentContext;
static alc_string_fn p_alcGetString;
static al_gen_fn p_alGenSources;
static al_gen_fn p_alGenBuffers;
static al_bufdata_fn p_alBufferData;
static al_queue_fn p_alSourceQueueBuffers;
static al_unqueue_fn p_alSourceUnqueueBuffers;
static al_play_fn p_alSourcePlay;
static al_stop_fn p_alSourceStop;
static al_pause_fn p_alSourcePause;
static al_geti_fn p_alGetSourcei;
static al_string_fn p_alGetString;
static al_error_fn p_alGetError;

static void* g_al;
static void* g_device;
static void* g_context;
static unsigned g_source;
static unsigned g_bufs[NBUF];
static unsigned g_free[NBUF];
static unsigned g_buf_frames[NBUF];
static int g_nfree;
static int g_ready;
static int g_open;
static int g_writes;
static unsigned g_rate = 44100;
static unsigned g_channels = 2;
static unsigned long g_period = 1024;
static unsigned long g_buffer = 2048;
static unsigned long g_queued_frames;
static unsigned long g_written_frames;
static unsigned long g_start_ms;
static unsigned long g_last_reclaim_ms;
static int g_cookie;
static int g_nonblock;
static int g_restarts;

static unsigned long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000ul + (unsigned long)ts.tv_nsec / 1000000ul;
}

/* Estimated frames still queued: tracked queue minus what realtime playback
   has consumed since the last OpenAL query. Avoids hammering the host
   device lock on every guest poll. */
static unsigned long est_queued(void) {
  unsigned long now = now_ms();
  unsigned long played = 0;
  if (now > g_last_reclaim_ms) {
    played = (now - g_last_reclaim_ms) * g_rate / 1000ul;
    if (played > g_queued_frames) {
      played = g_queued_frames;
    }
  }
  return g_queued_frames - played;
}

static int al_sym(void** dst, const char* name) {
  *dst = dlsym(g_al, name);
  if (*dst) {
    return 1;
  }
  fprintf(stderr, "[VEXA][ALSA] missing %s\n", name);
  return 0;
}

static int al_init(void) {
  if (g_ready) {
    return 1;
  }
  g_al = dlopen("libopenal.so.1", RTLD_NOW);
  if (!g_al) {
    fprintf(stderr, "[VEXA][ALSA] dlopen libopenal.so.1: %s\n", dlerror());
    return 0;
  }
  if (!al_sym((void**)&p_alcOpenDevice, "alcOpenDevice") ||
      !al_sym((void**)&p_alcCreateContext, "alcCreateContext") ||
      !al_sym((void**)&p_alcMakeContextCurrent, "alcMakeContextCurrent") ||
      !al_sym((void**)&p_alcGetCurrentContext, "alcGetCurrentContext") ||
      !al_sym((void**)&p_alcGetString, "alcGetString") ||
      !al_sym((void**)&p_alGenSources, "alGenSources") ||
      !al_sym((void**)&p_alGenBuffers, "alGenBuffers") ||
      !al_sym((void**)&p_alBufferData, "alBufferData") ||
      !al_sym((void**)&p_alSourceQueueBuffers, "alSourceQueueBuffers") ||
      !al_sym((void**)&p_alSourceUnqueueBuffers, "alSourceUnqueueBuffers") ||
      !al_sym((void**)&p_alSourcePlay, "alSourcePlay") ||
      !al_sym((void**)&p_alSourceStop, "alSourceStop") ||
      !al_sym((void**)&p_alSourcePause, "alSourcePause") ||
      !al_sym((void**)&p_alGetSourcei, "alGetSourcei") ||
      !al_sym((void**)&p_alGetString, "alGetString") ||
      !al_sym((void**)&p_alGetError, "alGetError")) {
    return 0;
  }
  g_device = p_alcOpenDevice(NULL);
  if (!g_device) {
    fprintf(stderr, "[VEXA][ALSA] alcOpenDevice failed\n");
    return 0;
  }
  g_context = p_alcCreateContext(g_device, NULL);
  if (!g_context || !p_alcMakeContextCurrent(g_context)) {
    fprintf(stderr, "[VEXA][ALSA] alcCreateContext failed\n");
    return 0;
  }
  p_alGenSources(1, &g_source);
  p_alGenBuffers(NBUF, g_bufs);
  g_nfree = NBUF;
  for (int i = 0; i < NBUF; i++) {
    g_free[i] = g_bufs[i];
  }
  fprintf(stderr, "[VEXA][ALSA] openal %s / %s / %s\n",
          p_alGetString(AL_VENDOR) ? p_alGetString(AL_VENDOR) : "?",
          p_alGetString(AL_RENDERER) ? p_alGetString(AL_RENDERER) : "?",
          p_alGetString(AL_VERSION) ? p_alGetString(AL_VERSION) : "?");
  g_ready = 1;
  return 1;
}

static void al_current(void) {
  if (g_context && p_alcGetCurrentContext() != g_context) {
    p_alcMakeContextCurrent(g_context);
  }
}

static void al_reclaim(void) {
  int processed = 0;
  unsigned long now = now_ms();
  if (g_nfree > 0 && now - g_last_reclaim_ms < 20) {
    return;
  }
  g_last_reclaim_ms = now;
  al_current();
  p_alGetSourcei(g_source, AL_BUFFERS_PROCESSED, &processed);
  /* Underrun recovery must live here, not only in writei: once the pool is
     exhausted writei returns EAGAIN before reaching the restart code, so a
     stopped source would starve the queue forever. */
  if (processed == 0 && g_queued_frames > 0) {
    int state = 0;
    p_alGetSourcei(g_source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
      if ((g_restarts++ & 7) == 0) {
        fprintf(stderr, "[VEXA][ALSA] restart src n=%d state=%x qf=%lu\n",
                g_restarts, state, g_queued_frames);
      }
      p_alSourcePlay(g_source);
    }
  }
  while (processed > 0 && g_nfree < NBUF) {
    unsigned id = 0;
    p_alSourceUnqueueBuffers(g_source, 1, &id);
    for (int i = 0; i < NBUF; ++i) {
      if (g_bufs[i] == id) {
        if (g_queued_frames >= g_buf_frames[i]) {
          g_queued_frames -= g_buf_frames[i];
        } else {
          g_queued_frames = 0;
        }
        g_buf_frames[i] = 0;
        break;
      }
    }
    g_free[g_nfree++] = id;
    processed--;
  }
}

int snd_pcm_open(void** pcm, const char* name, int stream, int mode) {
  if (!pcm) {
    return -EINVAL;
  }
  /* SND_PCM_STREAM_PLAYBACK */
  if (stream != 0) {
    return -ENODEV;
  }
  if (g_open) {
    return -EBUSY;
  }
  if (!al_init()) {
    return -ENODEV;
  }
  g_open = 1;
  g_writes = 0;
  /* SND_PCM_NONBLOCK */
  g_nonblock = mode & 1;
  /* Opt-in pacing for clients that do not retry EAGAIN: it
     retains every PCM block while the host drains its queue. */
  const char* blocking = getenv("VEXA_ALSA_BLOCKING");
  if (blocking && strcmp(blocking, "1") == 0) g_nonblock = 0;
  *pcm = &g_cookie;
  fprintf(stderr, "[VEXA][ALSA] open %s tid=%ld\n", name ? name : "(null)",
          (long)syscall(SYS_gettid));
  return 0;
}

int snd_pcm_close(void* pcm) {
  (void)pcm;
  g_open = 0;
  fprintf(stderr, "[VEXA][ALSA] close\n");
  return 0;
}

size_t snd_pcm_hw_params_sizeof(void) { return 256; }
size_t snd_pcm_sw_params_sizeof(void) { return 256; }

int snd_pcm_hw_params_malloc(void** ptr) {
  if (!ptr) return -EINVAL;
  *ptr = calloc(1, 256);
  return *ptr ? 0 : -ENOMEM;
}
int snd_pcm_hw_params_free(void* ptr) {
  free(ptr);
  return 0;
}
int snd_pcm_sw_params_malloc(void** ptr) { return snd_pcm_hw_params_malloc(ptr); }
int snd_pcm_sw_params_free(void* ptr) { return snd_pcm_hw_params_free(ptr); }

int snd_pcm_hw_params_any(void* pcm, void* params) {
  (void)pcm;
  if (params) memset(params, 0, 256);
  return 0;
}
int snd_pcm_hw_params_set_access(void* pcm, void* params, int access) {
  (void)pcm;
  (void)params;
  /* SND_PCM_ACCESS_RW_INTERLEAVED */
  return access == 3 ? 0 : -EINVAL;
}
int snd_pcm_hw_params_set_format(void* pcm, void* params, int format) {
  (void)pcm;
  (void)params;
  /* SND_PCM_FORMAT_S16_LE */
  return format == 2 ? 0 : -EINVAL;
}
int snd_pcm_hw_params_set_channels(void* pcm, void* params, unsigned int channels) {
  (void)pcm;
  (void)params;
  if (channels != 1 && channels != 2) return -EINVAL;
  g_channels = channels;
  return 0;
}
int snd_pcm_hw_params_set_rate(void* pcm, void* params, unsigned int rate, int dir) {
  (void)pcm;
  (void)params;
  (void)dir;
  if (rate < 8000) rate = 44100;
  g_rate = rate;
  return 0;
}
int snd_pcm_hw_params_set_rate_near(void* pcm, void* params, unsigned int* rate, int* dir) {
  (void)pcm;
  (void)params;
  (void)dir;
  if (!rate) return -EINVAL;
  if (*rate < 8000) *rate = 44100;
  g_rate = *rate;
  return 0;
}
int snd_pcm_hw_params_set_buffer_size_near(void* pcm, void* params, unsigned long* val) {
  (void)pcm;
  (void)params;
  if (!val) return -EINVAL;
  if (*val < 64) *val = 1024;
  g_buffer = *val;
  return 0;
}
int snd_pcm_hw_params_set_period_size_near(void* pcm, void* params, unsigned long* val, int* dir) {
  (void)pcm;
  (void)params;
  (void)dir;
  if (!val) return -EINVAL;
  if (*val < 64) *val = 512;
  g_period = *val;
  return 0;
}
int snd_pcm_hw_params_set_periods_near(void* pcm, void* params, unsigned int* val, int* dir) {
  (void)pcm;
  (void)params;
  (void)dir;
  if (!val) return -EINVAL;
  if (*val < 2) *val = 2;
  return 0;
}
int snd_pcm_hw_params(void* pcm, void* params) {
  (void)pcm;
  (void)params;
  fprintf(stderr, "[VEXA][ALSA] hw %u Hz %u ch period %lu buffer %lu\n", g_rate, g_channels, g_period, g_buffer);
  return 0;
}
int snd_pcm_sw_params_current(void* pcm, void* params) {
  (void)pcm;
  if (params) memset(params, 0, 256);
  return 0;
}
int snd_pcm_sw_params_set_avail_min(void* pcm, void* params, unsigned long val) {
  (void)pcm;
  (void)params;
  (void)val;
  return 0;
}
int snd_pcm_sw_params_set_start_threshold(void* pcm, void* params, unsigned long val) {
  (void)pcm;
  (void)params;
  (void)val;
  return 0;
}
int snd_pcm_sw_params(void* pcm, void* params) {
  (void)pcm;
  (void)params;
  return 0;
}

long snd_pcm_writei(void* pcm, const void* buffer, unsigned long frames) {
  int queued = 0;
  int state = 0;
  unsigned id;
  int bytes;
  if (!pcm || !g_ready || !g_open) return -ENODEV;
  if (!buffer || frames == 0) return 0;
  al_current();
  /* Blocking-mode writes wait for room instead of bouncing EAGAIN; the guest
     retry loop burns a full core and starves the OpenSL mixer otherwise. */
  const unsigned long wait_started = now_ms();
  for (; g_nfree == 0;) {
    al_reclaim();
    if (g_nfree > 0) {
      break;
    }
    if (g_nonblock) {
      static int eagain;
      if ((eagain++ & 15) == 0) {
        fprintf(stderr, "[VEXA][ALSA] EAGAIN n=%d qf=%lu\n", eagain, g_queued_frames);
      }
      return -EAGAIN;
    }
    /* Blocking mode must not turn ordinary host mixer latency into EAGAIN.
       Only fail if playback makes no progress for a full second. */
    if (now_ms() - wait_started >= 1000) return -EPIPE;
    struct timespec ts = { 0, 2000000L };
    nanosleep(&ts, NULL);
  }
  id = g_free[--g_nfree];
  bytes = (int)frames * (int)g_channels * (int)sizeof(int16_t);
  p_alBufferData(id, g_channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, buffer, bytes, (int)g_rate);
  for (int i = 0; i < NBUF; ++i) {
    if (g_bufs[i] == id) {
      g_buf_frames[i] = (unsigned)frames;
      break;
    }
  }
  g_queued_frames += frames;
  g_written_frames += frames;
  if (!g_start_ms) {
    g_start_ms = now_ms();
  }
  p_alSourceQueueBuffers(g_source, 1, &id);
  p_alGetSourcei(g_source, AL_SOURCE_STATE, &state);
  if (state != AL_PLAYING) {
    static int restarts;
    if ((restarts++ & 7) == 0) {
      fprintf(stderr, "[VEXA][ALSA] restart src n=%d state=%x qf=%lu\n",
              restarts, state, g_queued_frames);
    }
    p_alSourcePlay(g_source);
  }
  p_alGetSourcei(g_source, AL_BUFFERS_QUEUED, &queued);
  if ((g_writes++ & 63) == 0) {
    unsigned long elapsed = now_ms() - g_start_ms;
    unsigned long expected = elapsed * g_rate / 1000ul;
    fprintf(stderr, "[VEXA][ALSA] write n=%d %lu f queued %d qf=%lu feed=%lu need=%lu err %d tid=%ld\n",
            g_writes, frames, queued, g_queued_frames, g_written_frames, expected,
            p_alGetError ? p_alGetError() : 0, (long)syscall(SYS_gettid));
  }
  return (long)frames;
}

int snd_pcm_recover(void* pcm, int err, int silent) {
  (void)pcm;
  (void)silent;
  if (err == -EPIPE || err == -ESTRPIPE) return 0;
  return err;
}

/* SND_PCM_STATE_*: PREPARED=2, RUNNING=3, PAUSED=6, DISCONNECTED=-3 */
static int al_state(void) {
  int state = 0;
  if (!g_ready) {
    return 0;
  }
  al_current();
  p_alGetSourcei(g_source, AL_SOURCE_STATE, &state);
  return state;
}

long snd_pcm_avail_update(void* pcm) {
  (void)pcm;
  if (!g_open) {
    return -ENODEV;
  }
  al_reclaim();
  const long pool_capacity = (long)NBUF * (long)g_period;
  const long capacity = (long)g_buffer < pool_capacity ? (long)g_buffer : pool_capacity;
  long room = capacity - (long)est_queued();
  /* A buffer cannot be reused until OpenAL marks it processed. Time-based
     estimates alone can advertise space while every buffer is still owned. */
  const long writable = (long)g_nfree * (long)g_period;
  if (room > writable) room = writable;
  return room < 0 ? 0 : room;
}

long snd_pcm_avail(void* pcm) { return snd_pcm_avail_update(pcm); }

int snd_pcm_avail_delay(void* pcm, long* availp, long* delayp) {
  const long avail = snd_pcm_avail_update(pcm);
  if (avail < 0) {
    return (int)avail;
  }
  if (availp) *availp = avail;
  if (delayp) *delayp = (long)est_queued();
  return 0;
}

int snd_pcm_delay(void* pcm, long* delayp) {
  (void)pcm;
  if (delayp) {
    *delayp = (long)est_queued();
  }
  return 0;
}

int snd_pcm_state(void* pcm) {
  (void)pcm;
  if (!g_open) {
    return -3;
  }
  switch (al_state()) {
    case AL_PLAYING: return 3;
    case AL_PAUSED: return 6;
    default: return 2;
  }
}

int snd_pcm_wait(void* pcm, int timeout) {
  (void)pcm;
  const int capped = timeout < 0 ? 20 : (timeout > 20 ? 20 : timeout);
  if (capped > 0) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)capped * 1000000L;
    nanosleep(&ts, NULL);
  }
  al_reclaim();
  return 1;
}

int snd_pcm_prepare(void* pcm) {
  (void)pcm;
  if (!g_ready || !g_open) {
    return -ENODEV;
  }
  fprintf(stderr, "[VEXA][ALSA] prepare wf=%lu\n", g_written_frames);
  al_current();
  p_alSourceStop(g_source);
  int queued = 0;
  p_alGetSourcei(g_source, AL_BUFFERS_QUEUED, &queued);
  while (queued-- > 0) {
    unsigned id = 0;
    p_alSourceUnqueueBuffers(g_source, 1, &id);
    if (id) {
      g_free[g_nfree < NBUF ? g_nfree++ : NBUF - 1] = id;
    }
  }
  for (int i = 0; i < NBUF; ++i) {
    g_buf_frames[i] = 0;
  }
  g_queued_frames = 0;
  g_last_reclaim_ms = now_ms();
  return 0;
}

int snd_pcm_start(void* pcm) {
  (void)pcm;
  if (!g_ready) {
    return -ENODEV;
  }
  fprintf(stderr, "[VEXA][ALSA] start\n");
  al_current();
  p_alSourcePlay(g_source);
  return 0;
}

int snd_pcm_pause(void* pcm, int enable) {
  (void)pcm;
  if (!g_ready) {
    return -ENODEV;
  }
  fprintf(stderr, "[VEXA][ALSA] pause %d\n", enable);
  al_current();
  if (enable) {
    p_alSourcePause(g_source);
  } else {
    p_alSourcePlay(g_source);
  }
  return 0;
}

int snd_pcm_drop(void* pcm) { return snd_pcm_prepare(pcm); }

int snd_pcm_drain(void* pcm) {
  (void)pcm;
  fprintf(stderr, "[VEXA][ALSA] drain\n");
  return 0;
}

int snd_pcm_nonblock(void* pcm, int nonblock) {
  (void)pcm;
  g_nonblock = nonblock;
  const char* blocking = getenv("VEXA_ALSA_BLOCKING");
  if (blocking && strcmp(blocking, "1") == 0) g_nonblock = 0;
  fprintf(stderr, "[VEXA][ALSA] nonblock requested=%d effective=%d\n", nonblock, g_nonblock);
  return 0;
}

long snd_pcm_bytes_to_frames(void* pcm, long bytes) {
  (void)pcm;
  const int bpf = (int)g_channels * 2;
  return bpf > 0 ? bytes / bpf : 0;
}

long snd_pcm_frames_to_bytes(void* pcm, long frames) {
  (void)pcm;
  return frames * (long)g_channels * 2;
}

const char* snd_strerror(int err) {
  static char buf[64];
  if (err < 0) err = -err;
  snprintf(buf, sizeof(buf), "vexa-alsa %d", err);
  return buf;
}

const char* snd_asoundlib_version(void) { return "1.2.0-vexa"; }

int snd_device_name_hint(int card, const char* iface, void*** hints) {
  void** list;
  (void)card;
  (void)iface;
  if (!hints) return -EINVAL;
  list = calloc(1, sizeof(void*));
  if (!list) return -ENOMEM;
  *hints = list;
  return 0;
}
char* snd_device_name_get_hint(const void* hint, const char* id) {
  (void)hint;
  (void)id;
  return NULL;
}
int snd_device_name_free_hint(void** hints) {
  free(hints);
  return 0;
}
