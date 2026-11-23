// Sound for boot.lua's love.audio: a software mixer on the EE, mono at 22050 Hz, into audsrv's PCM
// stream. Every sound is SPU ADPCM (tools/sounds.py: 16-byte blocks of 28 samples), decoded as it plays with its
// own volume and pitch, which is what the game asks of every source. Short sounds stay whole in memory; long ones
// (the five music layers, the ambiences) are streamed from their files into a ring the main thread tops up once a
// frame (snd.pump), so they cost 32 KB each instead of 2 MB.
// ponytail: not the SPU2's own voices (design D9): audsrv gives its ADPCM voices no pitch per play, and 2 MB of
// SPU RAM would not hold the effects and the ambiences. If the mixer's EE time matters (docs/perf.md), the effects
// can move to SPU voices with an IOP module of our own. Mono: stereo doubles the decoding for five music layers.
// On the host the same mixer runs inside snd.pump, a frame's worth of samples at a time, and writes them to the
// file named by SND_RAW in the environment (raw signed 16-bit), so tests can listen.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

#include "pack.h"

#define RATE 22050
#define VOICES 24
#define RING 8192                // blocks a stream keeps ahead: 10.4 s at pitch 1 (128 KB), so the disc is asked seldom
#define STREAM_OVER (256 * 1024) // bytes: a longer sound is streamed whatever kind the game asks for

typedef struct Sample {
    uint32_t blocks;      // its length
    uint8_t *data;        // all its blocks, or NULL: a stream
    char *path;           // a stream's file; its block `loaded` is the next to read
    uint8_t *ring;        // RING blocks; read and write count blocks since the start and never wrap
    volatile uint32_t read, write;
    uint32_t loaded;
} Sample;

typedef struct Voice {
    Sample *s;
    volatile int active;
    unsigned gen;         // bumped at every start: a stale handle of this slot is told apart
    uint32_t block;       // the next block to decode
    int16_t buf[28];
    int n;                // samples of buf consumed
    int h1, h2;           // the decoder's history
    int prev, cur;        // the samples around the playing position
    uint32_t frac, step;  // 16.16: the position between them, the advance per output sample (the pitch)
    int volume;           // 256 = 1
} Voice;
static Voice voices[VOICES];

#ifdef _EE
#include <audsrv.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
extern void *_gp;
static int lock = -1; // a semaphore: the mixer thread against the main thread changing voices
#define LOCK() WaitSema(lock)
#define UNLOCK() SignalSema(lock)
// The streams' rings are topped up by a thread of their own (filler): from a disc each top-up is a seek and a read
// the main thread would wait for, 60 ms a frame with the game's five music layers (docs/perf.md). disc guards a
// stream's ring and position between that thread and the main one starting or closing the stream.
static int disc = -1, wake = -1;
#define DISC_LOCK() WaitSema(disc)
#define DISC_UNLOCK() SignalSema(disc)
#else
#define LOCK()
#define UNLOCK()
#define DISC_LOCK()
#define DISC_UNLOCK()
static FILE *raw;
static double owed; // samples the host mixer still has to produce
#endif

// SPU ADPCM (psx-spx "SPU ADPCM Samples"): a block's 28 samples
static void decode(Voice *v, const uint8_t *b) {
    static const int f[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    int shift = b[0] & 15, fi = b[0] >> 4 > 4 ? 0 : b[0] >> 4;
    for (int i = 0; i < 28; i++) {
        int nib = b[2 + i / 2] >> (i & 1 ? 4 : 0) & 15;
        int s = ((int)((unsigned)nib << 28) >> 16 >> shift) + ((v->h1 * f[fi][0] + v->h2 * f[fi][1] + 32) >> 6);
        s = s < -32768 ? -32768 : s > 32767 ? 32767 : s;
        v->buf[i] = s;
        v->h2 = v->h1;
        v->h1 = s;
    }
}

// the voice's next sample; 0 at the end of the sound. A stream whose ring ran dry gives silence and waits.
static int next(Voice *v, int *out) {
    Sample *s = v->s;
    if (v->n == 28) {
        if (v->block >= s->blocks) return 0;
        if (s->data) {
            decode(v, s->data + 16 * v->block);
        } else {
            if (s->read == s->write) {
                *out = 0;
                return 1;
            }
            decode(v, s->ring + 16 * (s->read % RING));
            s->read++;
        }
        v->block++;
        v->n = 0;
    }
    *out = v->buf[v->n++];
    return 1;
}

static void mix(int16_t *out, int n) {
    static int acc[512];
    memset(acc, 0, n * sizeof *acc);
    for (Voice *v = voices; v < voices + VOICES; v++) {
        if (!v->active) continue;
        for (int i = 0; i < n; i++) {
            v->frac += v->step;
            while (v->frac >= 65536 && v->active) {
                v->frac -= 65536;
                v->prev = v->cur;
                if (!next(v, &v->cur)) v->active = 0;
            }
            if (!v->active) break;
            int f = v->frac >> 4; // 12 bits: the product fits in 32
            acc[i] += ((v->prev * (4096 - f) + v->cur * f) >> 12) * v->volume >> 8;
        }
    }
    for (int i = 0; i < n; i++) out[i] = acc[i] < -32768 ? -32768 : acc[i] > 32767 ? 32767 : acc[i];
}

static void fill(Sample *s, int starting);

#ifdef _EE
static char filler_stack[16 * 1024] __attribute__((aligned(16)));
static void filler(void *arg) {
    for (;;) {
        WaitSema(wake); // snd.pump, once a frame
        DISC_LOCK();
        for (Voice *v = voices; v < voices + VOICES; v++)
            if (v->active && !v->s->data) fill(v->s, 0);
        DISC_UNLOCK();
    }
}
extern unsigned char audsrv_irx[], audsrv_irx_end[]; // irx.S
static char mixer_stack[16 * 1024] __attribute__((aligned(16)));
static void mixer(void *arg) {
    static int16_t chunk[512] __attribute__((aligned(64)));
    for (;;) {
        audsrv_wait_audio(sizeof chunk);
        LOCK();
        mix(chunk, 512);
        UNLOCK();
        audsrv_play_audio((const char *)chunk, sizeof chunk);
    }
}
static void init(void) {
    int ret;
    SifInitRpc(0);
    sbv_patch_enable_lmb(); // for loading a module from memory
    if (SifLoadModule("rom0:LIBSD", 0, NULL) < 0 ||
        SifExecModuleBuffer(audsrv_irx, audsrv_irx_end - audsrv_irx, 0, NULL, &ret) < 0 || audsrv_init() != 0) {
        printf("snd: audsrv not started, no sound\n");
        return;
    }
    struct audsrv_fmt_t fmt = {RATE, 16, 1};
    audsrv_set_format(&fmt);
    audsrv_set_volume(MAX_VOLUME);
    ee_sema_t sema = {.init_count = 1, .max_count = 1};
    lock = CreateSema(&sema);
    disc = CreateSema(&sema);
    sema.init_count = 0;
    wake = CreateSema(&sema);
    // between the mixer and the main thread: it sleeps in the disc's requests
    ee_thread_t fill_thread = {.func = filler, .stack = filler_stack, .stack_size = sizeof filler_stack,
                               .gp_reg = &_gp, .initial_priority = 40};
    StartThread(CreateThread(&fill_thread), NULL);
    // above the main thread, so the stream is fed whatever a frame takes; it sleeps in audsrv_wait_audio
    ee_thread_t thread = {.func = mixer, .stack = mixer_stack, .stack_size = sizeof mixer_stack,
                          .gp_reg = &_gp, .initial_priority = 30};
    int id = CreateThread(&thread);
    ChangeThreadPriority(GetThreadId(), 64);
    StartThread(id, NULL);
}
#endif

// snd.open(path, stream) -> sample, shared. shared: the sound is whole in memory, one sample serves any number
// of voices and is never closed; else it is a stream, for one voice at a time, to snd.close when done with.
static int l_open(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    uint32_t head[2]; // the rate (always RATE), the blocks
    if (!pack_read(path, 0, head, 8)) return luaL_error(L, "no sound %s: run tools/sounds.py", path);
    Sample *s = calloc(1, sizeof *s);
    s->blocks = head[1];
    int stream = lua_toboolean(L, 2) || s->blocks * 16 > STREAM_OVER;
    if (stream) {
        s->path = strdup(path);
        s->ring = malloc(RING * 16);
        if (!s->path || !s->ring) return luaL_error(L, "sound %s: no memory to stream it", path);
    } else {
        s->data = malloc(s->blocks * 16);
        if (!s->data || !pack_read(path, 8, s->data, s->blocks * 16)) return luaL_error(L, "sound %s: short", path);
    }
    lua_pushlightuserdata(L, s);
    lua_pushboolean(L, !stream);
    return 2;
}

static Voice *voice(lua_State *L) { // the live voice of a handle, or NULL: it ended, or its slot plays another
    unsigned h = (unsigned)luaL_checknumber(L, 1);
    Voice *v = &voices[h % VOICES];
    return v->active && v->gen == h / VOICES ? v : NULL;
}

static void tune(lua_State *L, Voice *v, int arg) { // volume, pitch
    v->volume = (int)(lua_tonumber(L, arg) * 256);
    v->step = (uint32_t)(lua_tonumber(L, arg + 1) * 65536);
}

// A stream's ring, topped up from its file (the main thread only). No file stays open for it: the disc allows
// few at once and the game leaves streams around. Each top-up is one request, made when a quarter of the ring is
// free (every 2.6 s at pitch 1) unless the sound is starting, so the disc is not asked every frame.
static void fill(Sample *s, int starting) {
    while (s->loaded < s->blocks && RING - (s->write - s->read) >= (starting ? 1 : RING / 4)) {
        uint32_t at = s->write % RING, n = RING - (s->write - s->read);
        if (n > RING - at) n = RING - at;
        if (n > s->blocks - s->loaded) n = s->blocks - s->loaded;
        if (!pack_read(s->path, 8 + 16 * (long)s->loaded, s->ring + 16 * at, 16 * (long)n)) {
            s->blocks = s->loaded; // a short file: the sound ends here
            return;
        }
        s->loaded += n;
        s->write += n; // after the data is in place: the mixer reads up to here
    }
}

// snd.start(sample, volume, pitch) -> a voice handle, or nil when every voice is busy
static int l_start(lua_State *L) {
    Sample *s = lua_touserdata(L, 1);
    luaL_argcheck(L, s, 1, "a sample");
    luaL_checknumber(L, 2), luaL_checknumber(L, 3); // before the lock is taken
    for (Voice *v = voices; v < voices + VOICES; v++) {
        if (v->active) continue;
        if (!s->data) DISC_LOCK();
        LOCK();
        v->s = s;
        v->block = 0, v->n = 28, v->h1 = v->h2 = v->prev = v->cur = 0, v->frac = 0;
        tune(L, v, 2);
        if (!s->data) { // a stream starts over
            s->loaded = 0, s->read = s->write = 0;
            fill(s, 1);
        }
        v->gen++;
        v->active = 1;
        UNLOCK();
        if (!s->data) DISC_UNLOCK();
        lua_pushnumber(L, v->gen * VOICES + (v - voices));
        return 1;
    }
    return 0;
}

static int l_set(lua_State *L) { // snd.set(voice, volume, pitch)
    Voice *v = voice(L);
    luaL_checknumber(L, 2), luaL_checknumber(L, 3);
    if (v) tune(L, v, 2);
    return 0;
}

static int l_stop(lua_State *L) {
    Voice *v = voice(L);
    LOCK();
    if (v) v->active = 0;
    UNLOCK();
    return 0;
}

static int l_playing(lua_State *L) {
    lua_pushboolean(L, voice(L) != NULL);
    return 1;
}

static int l_close(lua_State *L) { // a stream no voice plays any more
    Sample *s = lua_touserdata(L, 1);
    if (!s || s->data) return 0;
    DISC_LOCK();
    LOCK();
    for (Voice *v = voices; v < voices + VOICES; v++)
        if (v->s == s) v->active = 0;
    UNLOCK();
    free(s->path);
    free(s->ring);
    free(s);
    DISC_UNLOCK();
    return 0;
}

// snd.pump(dt) -> the voices playing. Once a frame: the streams' rings topped up; on the host, dt of sound mixed.
static int l_pump(lua_State *L) {
    int playing = 0;
    for (Voice *v = voices; v < voices + VOICES; v++) {
        if (!v->active) continue;
        playing++;
#ifndef _EE
        if (!v->s->data) fill(v->s, 0);
#endif
    }
#ifdef _EE
    SignalSema(wake);
#else
    static int16_t chunk[512];
    for (owed += luaL_checknumber(L, 1) * RATE; owed >= 1;) {
        int n = owed > 512 ? 512 : (int)owed;
        mix(chunk, n);
        if (raw) fwrite(chunk, 2, n, raw);
        owed -= n;
        for (Voice *v = voices; v < voices + VOICES; v++) // a long frame: the rings again
            if (v->active && !v->s->data) fill(v->s, 0);
    }
#endif
    lua_pushinteger(L, playing);
    return 1;
}

int luaopen_snd(lua_State *L) {
    static const luaL_Reg snd[] = {{"open", l_open}, {"close", l_close}, {"start", l_start}, {"set", l_set},
                                   {"stop", l_stop}, {"playing", l_playing}, {"pump", l_pump}, {NULL, NULL}};
#ifdef _EE
    init();
#else
    if (getenv("SND_RAW")) raw = fopen(getenv("SND_RAW"), "wb");
#endif
    luaL_register(L, "snd", snd);
    return 1;
}
