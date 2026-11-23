// The rest of the platform: boot (love_init) and the frame, love.timer, love.thread's channels and Lua's
// math.random as the game's own LuaJIT has it. The stubs of love.system, love.window and love.mouse are in love.h.
#include <stdarg.h>
#include <time.h>

#define LOVE_INTERNAL
#include "love.h"

// -DDISC (EE): the game's files are the pack of the disc (tools/disc.py), not host:game. -DPLAY (EE): the saves
// go to the memory card. -DSHOTS='"900,1600"' (EE): frames held on screen for a screenshot.
#ifndef SHOTS
#define SHOTS ""
#endif
// -DFRAME_US=<us> (EE): a frame of the game lasts at least that long, and -DFRAME_VARY=<us> more on one frame of three:
// a console's pace in PCSX2, where the game fits in a blank (18000, 25000, 33400, 41700: 1.1, 1.5, 2, 2.5 blanks).
#if defined(_EE) && defined(FRAME_US)
#ifndef FRAME_VARY
#define FRAME_VARY 0
#endif
#endif

void love_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("ERROR ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
    exit(1);
}

// ---------------------------------------------------------------- love.timer
// dt is the measured one when LoveConf.dt is 0 (the game played); benches and tests step a fixed one, so a run
// does the same on every machine. getTime is the real clock.
static int frame, quit;
static double fixed_dt, dt; // double, as LOVE's: the game's timers add it up (game.h)
static double last_step = -1;
double love_timer_getTime(void) { return (double)clock() / CLOCKS_PER_SEC; }
double love_timer_step(void) {
    dt = fixed_dt;
    if (fixed_dt <= 0) {
        double t = love_timer_getTime();
        dt = t - (last_step < 0 ? t : last_step), last_step = t;
    }
    return dt;
}
double love_timer_getDelta(void) { return dt; }
int love_timer_getFPS(void) { float d = (float)dt; return (int)floorf(1 / (d > 1 / 60.0f ? d : 1 / 60.0f) + 0.5f); }

// ---------------------------------------------------------------- boot and the frame
void love_init(const LoveConf *conf) {
    LoveConf c = {0};
    // A line at a time: on the console the loader's udptty sends stdout over the network (tools/udptty.py), and
    // newlib kept whole buffers back, so a freeze took its last lines with it.
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (conf) {
        c = *conf;
    } else {
#ifdef _EE
#ifdef DISC
        c.game = "pak:", c.pack = "cdrom0:\\GAME.PAK;1";
#endif
#ifdef PLAY
        c.save_dir = "mc0:/BALATROC"; // its own directory: the Lua runtime's saves (mc0:/BALATRO) stay as they are
#endif
#ifdef TEST_DT // a scripted run in PCSX2 (host:pad.txt): the host tests' fixed step
        c.dt = TEST_DT;
#endif
#else
        c.game = getenv("GAME_DIR"), c.pack = getenv("PACK"), c.save_dir = getenv("SAVE_DIR"), c.soft = getenv("SHOT") != NULL;
        c.dt = getenv("DT") ? atof(getenv("DT")) : 1 / 60.0;
#endif
    }
#ifdef _EE
    if (!c.game) c.game = "host:game";
#else
    if (!c.game) c.game = "build/game";
#endif
    // (BOOT lines: on the console a hang while starting says where)
    printf("BOOT start, game files in %s\n", c.game);
    if (c.pack && !pack_mount(c.pack)) love_error("no pack %s", c.pack);
    fixed_dt = dt = c.dt;
    math_randomseed(0);
#ifdef VIDEO_PROBE // (EE) gs_screen.c: after which stage the console stops answering a request for 480p
    void gs_probe(const char *where);
#else
#define gs_probe(where)
#endif
    gs_probe("at the start");
    printf("BOOT pad\n");
    love_pad_init();
    gs_probe("after the pad");
    printf("BOOT sound\n");
    love_snd_init();
    gs_probe("after the sound");
    printf("BOOT files\n");
    love_fs_init(c.game, c.save_dir); // after the pad: its modules are the memory card's too
    gs_probe("after the files");
    printf("BOOT screen\n");
    love_gfx_init(LOVE_SCREEN_W, LOVE_SCREEN_H, c.soft);
#ifdef VIDEO_PROBE
    gs_probe("after the screen");
    gs_mode(0, NULL, NULL); // (the picture as it was)
#endif
    love_video_pick(); // (EE) the picture's mode, asked for on a screen of its own
    printf("BOOT platform ready\n");
}
int love_frame(void) { return frame; }
bool love_frame_begin(void) {
    frame++;
    return !quit;
}
void love_event_quit(void) { quit = 1; }
// SHOT=<frame>,<frame>... in the environment (host): those frames of the screen as shot-<frame>.ppm.
// On the EE the frames come from -DSHOTS: each stays on screen for three seconds after a SHOT line, for
// tools/pcsx2.sh to press the emulator's screenshot key.
static int shot_due(void) {
#ifdef _EE
    const char *s = SHOTS;
#else
    const char *s = getenv("SHOT");
#endif
    while (s && *s) {
        char *end;
        long n = strtol(s, &end, 10);
        if (end != s && n == frame) return 1;
        s = end == s ? s + 1 : end;
    }
    return 0;
}
void love_present(void) {
#if defined(_EE) && defined(FRAME_US)
    static clock_t last;
    clock_t least = (clock_t)(((FRAME_US) + (frame % 3 ? 0 : (FRAME_VARY))) * (CLOCKS_PER_SEC / 1e6));
    while (clock() - last < least) {}
    last = clock();
#endif
    love_graphics_present();
    love_audio_pump(dt);
    if (!shot_due()) return;
#ifdef _EE
    printf("SHOT %d\n", frame);
    for (double t = love_timer_getTime(); love_timer_getTime() - t < 3;) {}
#else
    char name[40];
    snprintf(name, sizeof name, "shot-%d.ppm", frame);
    if (!love_graphics_shot(name)) love_error("gfx: cannot write the screenshot");
#endif
}

// ---------------------------------------------------------------- love.thread
// A channel somebody reads keeps what is pushed; the others drop it (there is no network: the http thread does
// not run). A save is packed inside the request that asks for it (save_manager.c: the packing reads the game's
// tables); the deflate and the card's write are left to the card's thread (love_fs.c).
struct Channel {
    Channel *next;
    char *name;
    ChannelFn serve;
    Val *queue;
    int first, last, room; // the values waiting are queue[first .. last - 1]
    bool queued, copies;
};
static Channel *channels;
Channel *love_thread_getChannel(const char *name) {
    Channel *c;
    for (c = channels; c && strcmp(c->name, name); c = c->next) {}
    if (c) return c;
    // The collector's memory, as the queue below: the values waiting hold tables and strings, and libc's memory
    // is not scanned for them. `channels` is the root.
    c = gc_alloc(sizeof *c);
    if (!(c->name = strdup(name))) love_error("no memory for channel %s", name);
    c->queued = !strcmp(name, "sound_request") || !strcmp(name, "load_channel") || !strcmp(name, "save_request");
    c->copies = strcmp(name, "save_request") != 0; // its tables are used before the push returns
    c->next = channels, channels = c;
    return c;
}
void love_thread_serve(const char *name, ChannelFn serve) { love_thread_getChannel(name)->serve = serve; }
Val Channel_pop(Channel *self) {
    if (self->first == self->last) return NIL;
    Val v = self->queue[self->first++];
    if (self->first == self->last) self->first = self->last = 0;
    return v;
}
Val Channel_demand(Channel *self) { return Channel_pop(self); }
void Channel_push(Channel *self, Val v) {
    if (!self->queued) return;
    if (self->last == self->room) {
        self->room = self->room ? self->room * 2 : 16;
        self->queue = gc_realloc(self->queue, self->room * sizeof *self->queue);
    }
    // a pushed table is copied, as between LÖVE's threads: the game reuses its request tables
    self->queue[self->last++] = self->copies && v.t == V_TAB ? T(tcopy(v.tab)) : v;
    if (self->serve) self->serve(self);
}

// ---------------------------------------------------------------- math.random
// math.random and math.randomseed as LuaJIT 2.0 has them (lib_math.c: a Tausworthe generator of period 2^223
// seeded from the bits of a double). The game seeds with fractions (pseudoseed, misc_functions.lua:298), and a
// seeded run has to deal the cards the PC game deals. test/love_test.c checks the first numbers against the
// ones the game's own lua51.dll gives.
static uint64_t rng[4];
static uint64_t rng_step(void) {
    static const int K[4] = {63, 58, 55, 47}, Q[4] = {31, 19, 24, 21}, S[4] = {18, 28, 7, 8};
    uint64_t r = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t z = rng[i];
        z = (((z << Q[i]) ^ z) >> (K[i] - S[i])) ^ ((z & (~(uint64_t)0 << (64 - K[i]))) << S[i]);
        r ^= z;
        rng[i] = z;
    }
    return (r & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL; // a double in [1, 2)
}
void math_randomseed(double d) {
    uint32_t r = 0x11090601; // 64 - K[i]
    for (int i = 0; i < 4; i++) {
        union { double d; uint64_t u; } u;
        uint32_t m = 1u << (r & 255);
        r >>= 8;
        u.d = d = d * 3.14159265358979323846 + 2.7182818284590452354;
        if (u.u < m) u.u += m;
        rng[i] = u.u;
    }
    for (int i = 0; i < 10; i++) rng_step();
}
double math_random(void) {
    union { double d; uint64_t u; } u;
    u.u = rng_step();
    return u.d - 1.0;
}
double math_random_to(double m) { return floor(math_random() * m) + 1.0; }
double math_random_range(double lo, double hi) { return floor(math_random() * (hi - lo + 1.0)) + lo; }
