// love-ps2: runs the game's own Lua (LÖVE 11 API) on PUC Lua 5.1. Builds for the EE and for the host (tests).
// usage (host): balatro [game_dir] [frames] [mode]   (the modes are boot.lua's)
//   menu (default): the game left alone        run: the bench plays hands through the game's functions
//   pad: a bot plays with pad presses only     play: no bench, the measured dt; frames 0 = until the game quits
//   saves: the bot, over two boots: it plays until the game saves the run, then continues it from the card
// On the EE: -DFRAMES=n and -DBENCH_RUN, -DBENCH_PAD, -DBENCH_SAVES or -DPLAY for the mode (XF in build.sh's
// environment).
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "boot_lua.h" // xxd -i of boot.lua (Makefile)
#include "pack.h"

// -DDISC (EE): the game's files are the pack of the disc (tools/disc.py), not host:game. On the host, a pack is
// tried with PACK=<file> in the environment and "pak:" as the game directory.
#ifdef DISC
#define GAME_DIR "pak:"
#define PACK "cdrom0:\\GAME.PAK;1"
#endif
#ifndef GAME_DIR
#ifdef _EE
#define GAME_DIR "host:game"
#else
#define GAME_DIR "build/game"
#endif
#endif
#ifndef FRAMES
#define FRAMES 1200
#endif
#if defined(BENCH_RUN)
#define MODE "run"
#elif defined(BENCH_PAD)
#define MODE "pad"
#elif defined(BENCH_SAVES)
#define MODE "saves"
#elif defined(PLAY)
#define MODE "play"
#else
#define MODE "menu"
#endif

static int l_clock(lua_State *L) { // seconds, as two parts so a float lua_Number keeps sub-millisecond precision
    clock_t c = clock();
    lua_pushnumber(L, (lua_Number)(c / CLOCKS_PER_SEC));
    lua_pushnumber(L, (lua_Number)(c % CLOCKS_PER_SEC) / CLOCKS_PER_SEC);
    return 2;
}

// sys.read(path [, bytes]) -> a game file's contents (or its first bytes), nil if there is none
static int l_read(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    long size = pack_size(path);
    if (size < 0) return 0;
    if (lua_isnumber(L, 2) && lua_tointeger(L, 2) < size) size = lua_tointeger(L, 2);
    char *data = malloc(size + 1);
    int ok = data && pack_read(path, 0, data, size);
    if (ok) lua_pushlstring(L, data, size);
    free(data);
    return ok;
}

static int l_mem(lua_State *L) { // bytes malloc has handed out (the Lua heap and everything else)
#ifdef __GLIBC__
    lua_pushnumber(L, (lua_Number)mallinfo2().uordblks);
#else
    lua_pushnumber(L, (lua_Number)(unsigned)mallinfo().uordblks);
#endif
    return 1;
}

static double allocated; // bytes Lua ever asked for: its growth per frame is the garbage the game makes
static void *alloc(void *ud, void *p, size_t osize, size_t nsize) {
    (void)ud;
    if (nsize == 0) {
        free(p);
        return NULL;
    }
    if (nsize > osize) allocated += nsize - osize;
    return realloc(p, nsize);
}
static int l_allocated(lua_State *L) { // KB
    lua_pushnumber(L, (lua_Number)(allocated / 1024));
    return 1;
}

// LuaJIT's bit module, the three functions the game calls: 32-bit, results signed
static uint32_t tobit(lua_State *L, int i) { return (uint32_t)(int64_t)luaL_checknumber(L, i); }
static int bit_bxor(lua_State *L) { lua_pushnumber(L, (int32_t)(tobit(L, 1) ^ tobit(L, 2))); return 1; }
static int bit_lshift(lua_State *L) { lua_pushnumber(L, (int32_t)(tobit(L, 1) << (tobit(L, 2) & 31))); return 1; }
static int bit_rshift(lua_State *L) { lua_pushnumber(L, (int32_t)(tobit(L, 1) >> (tobit(L, 2) & 31))); return 1; }

// math.random and math.randomseed as LuaJIT 2.0 has them (lib_math.c: a Tausworthe generator of period 2^223
// seeded from the bits of a double). The game seeds with fractions (pseudoseed, misc_functions.lua:298), which
// PUC Lua's srand((int)seed) would all turn into the same sequence; and a seeded run has to deal the cards the PC
// game deals. boot.lua checks the first numbers against the game's own lua51.dll.
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
static void rng_seed(double d) {
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
static int l_randomseed(lua_State *L) { rng_seed(luaL_checknumber(L, 1)); return 0; }
static int l_random(lua_State *L) {
    union { double d; uint64_t u; } u;
    u.u = rng_step();
    double d = u.d - 1.0;
    int n = lua_gettop(L);
    if (n == 1) {
        d = floor(d * luaL_checknumber(L, 1)) + 1.0;
    } else if (n > 1) {
        double lo = luaL_checknumber(L, 1);
        d = floor(d * (luaL_checknumber(L, 2) - lo + 1.0)) + lo;
    }
    lua_pushnumber(L, d);
    return 1;
}

#ifndef SHOTS
#define SHOTS "" // -DSHOTS='"900,1600"': frames the EE holds on screen for a screenshot
#endif

int luaopen_engine(lua_State *L); // engine.c
int luaopen_gfx(lua_State *L);    // gfx.c
int luaopen_pad(lua_State *L);    // pad.c
int luaopen_snd(lua_State *L);    // snd.c
int luaopen_save(lua_State *L);   // save.c

static int traceback(lua_State *L) {
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

#ifdef PROF
// -DPROF (EE): a sampling profiler. sys.prof(true) starts a 1 kHz timer alarm that counts the interrupted pc in
// 16-byte buckets of the text; sys.prof(false) writes them to prof.bin beside the ELF, for tools/prof.py.
// boot.lua samples the frames from 1300 on.
#include <timer_alarm.h>
extern char _ftext[], _etext[];
static unsigned short *hist;
static volatile int prof_gate = 1; // sys.prof_gate(false): samples are not counted (boot.lua: one section of the frame)
static int l_prof_gate(lua_State *L) {
    prof_gate = lua_toboolean(L, 1);
    return 0;
}
static u64 prof_tick(s32 id, u64 scheduled, u64 actual, void *arg, void *pc) {
    unsigned off = (char *)pc - _ftext;
    if (prof_gate) hist[off < (unsigned)(_etext - _ftext) ? off >> 4 : 0]++; // bucket 0 (crt0) also takes what is outside the text
    return USec2TimerBusClock(1000); // from the scheduled time
}
static int l_prof(lua_State *L) {
    static s32 alarm;
    size_t n = ((_etext - _ftext) >> 4) + 1;
    if (lua_toboolean(L, 1)) {
        hist = calloc(n, sizeof *hist);
        alarm = SetTimerAlarm(USec2TimerBusClock(1000), prof_tick, NULL);
    } else {
        ReleaseTimerAlarm(alarm);
        FILE *f = fopen("prof.bin", "wb");
        if (f) fwrite(hist, sizeof *hist, n, f), fclose(f);
    }
    return 0;
}
#endif

int main(int argc, char **argv) {
    static const luaL_Reg sys[] = {{"clock", l_clock}, {"mem", l_mem}, {"allocated", l_allocated}, {"read", l_read}, {NULL, NULL}};
    static const luaL_Reg bit[] = {{"bxor", bit_bxor}, {"lshift", bit_lshift}, {"rshift", bit_rshift}, {NULL, NULL}};
    // A line at a time: on the console the loader's udptty sends stdout over the network (tools/udptty.py), and
    // newlib kept whole buffers back, so a freeze took its last lines with it.
    setvbuf(stdout, NULL, _IOLBF, 0);
    lua_State *L = lua_newstate(alloc, NULL);
    if (!L) {
        printf("BOOT ERROR no memory\n");
        return 1;
    }
    luaL_openlibs(L);
    luaL_register(L, "sys", sys);
#ifdef _EE
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "ee"); // sys.ee: on the console (or its emulator), not the host's tests
#endif
#ifdef DRESS // -DDRESS=<frame> [-DBLIND=<chips>]: boot.lua's test of the card shaders, as the host's DRESS= and BLIND=
    lua_pushinteger(L, DRESS);
    lua_setfield(L, -2, "dress");
#endif
#ifdef BLIND
    lua_pushinteger(L, BLIND);
    lua_setfield(L, -2, "blind");
#endif
#ifdef SECTIONS
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "sections"); // -DSECTIONS: SECT lines (boot.lua)
#endif
#ifdef OSD
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "osd"); // -DOSD: the frame's numbers on screen, for the console, where nothing reads printf
#endif
#ifdef PACK
    const char *pack = PACK;
#else
    const char *pack = getenv("PACK");
#endif
    if (pack && !pack_mount(pack)) {
        printf("BOOT ERROR no pack %s\n", pack);
        return 1;
    }
    luaL_register(L, "bit", bit);
    rng_seed(0);
    lua_getglobal(L, "math");
    lua_pushcfunction(L, l_random);
    lua_setfield(L, -2, "random");
    lua_pushcfunction(L, l_randomseed);
    lua_setfield(L, -2, "randomseed");
#ifdef PROF
    lua_getglobal(L, "sys");
    lua_pushcfunction(L, l_prof);
    lua_setfield(L, -2, "prof");
    lua_pushcfunction(L, l_prof_gate);
    lua_setfield(L, -2, "prof_gate");
#ifdef PROF_SECTION
    lua_pushinteger(L, PROF_SECTION);
    lua_setfield(L, -2, "prof_section"); // -DPROF_SECTION=n: only that section of the frame is sampled (boot.lua)
#endif
#endif
    luaopen_gfx(L);
    luaopen_pad(L);
    luaopen_snd(L);
    luaopen_save(L); // after the pad: its modules are the memory card's too
#ifndef NO_NATIVE // -DNO_NATIVE: the game's Lua engine functions as they are, to compare
    luaopen_engine(L);
#endif
    lua_settop(L, 0);
    lua_pushcfunction(L, traceback);
    if (luaL_loadbuffer(L, (const char *)boot_lua, boot_lua_len, "@boot.lua")) {
        printf("BOOT ERROR %s\n", lua_tostring(L, -1));
        return 1;
    }
    lua_pushstring(L, argc > 1 ? argv[1] : GAME_DIR);
    lua_pushinteger(L, argc > 2 ? atoi(argv[2]) : FRAMES);
    lua_pushinteger(L, (int)sizeof(lua_Number));
    lua_pushstring(L, argc > 3 ? argv[3] : MODE);
    lua_pushstring(L, SHOTS);
    if (lua_pcall(L, 5, 0, 1)) {
        printf("ERROR %s\n", lua_tostring(L, -1));
        return 1;
    }
    printf("DONE\n");
    return 0;
}
