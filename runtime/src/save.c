// The files the game writes: settings, profiles and the run in progress, for boot.lua's
// love.filesystem, and the deflate of love.data, which the game puts them through.
//   save.open(dir) -> usable     the directory the files live in: mc0:/BALATRO on the EE, any directory on the host
//   save.read(path) -> string | nil      save.write(path, data) -> ok      save.remove(path)      save.mkdir(path)
//   save.deflate(data, level) -> string  save.inflate(data) -> string      raw deflate, LÖVE's "deflate" format
// A file on the card is 12 bytes ("BLT1", the length, its Adler-32) and then the data: a file cut short by a
// power off reads as no file at all. The game itself cannot take one (it finds save.jkr, fails to inflate it and
// indexes nil, button_callbacks.lua:205).
// ponytail: a write goes straight over the old file, so that power cut loses the file. A temporary file and a
// rename if it hurts.
// ponytail: no icon.sys, so the console's browser shows the directory as corrupted data; not done yet.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "lauxlib.h"
#include "lua.h"

#ifdef _EE
#include <loadfile.h>
#endif

static char base[200];

static const char *full(lua_State *L, int i) {
    static char path[300];
    snprintf(path, sizeof path, "%s/%s", base, luaL_checkstring(L, i));
    return path;
}

static int l_open(lua_State *L) {
    snprintf(base, sizeof base, "%s", luaL_checkstring(L, 1));
#ifdef _EE
    // pad.c has loaded rom0:XSIO2MAN; with these two, mc0: is a device like any other
    SifLoadModule("rom0:XMCMAN", 0, NULL);
    SifLoadModule("rom0:XMCSERV", 0, NULL);
#endif
    mkdir(base, 0777); // it may exist
    char probe[300];
    snprintf(probe, sizeof probe, "%s/probe", base);
    FILE *f = fopen(probe, "wb");
    if (f) fclose(f), remove(probe);
    lua_pushboolean(L, f != NULL);
    return 1;
}

static void header(unsigned char h[12], const char *data, size_t len) {
    unsigned long sum = adler32(adler32(0, NULL, 0), (const Bytef *)data, len);
    memcpy(h, "BLT1", 4);
    for (int i = 0; i < 4; i++) h[4 + i] = len >> (8 * i), h[8 + i] = sum >> (8 * i);
}

static int l_read(lua_State *L) {
    FILE *f = fopen(full(L, 1), "rb");
    if (!f) return 0;
    unsigned char h[12], want[12];
    size_t len = fread(h, 1, 12, f) == 12 && !memcmp(h, "BLT1", 4) ? h[4] | h[5] << 8 | h[6] << 16 | (size_t)h[7] << 24 : 0;
    char *data = len <= 4 << 20 ? malloc(len + 1) : NULL; // (4 MB: more than the card holds)
    int ok = data && fread(data, 1, len, f) == len;
    fclose(f);
    if (ok) header(want, data, len), ok = !memcmp(h, want, 12);
    if (ok) lua_pushlstring(L, data, len);
    free(data);
    return ok;
}

static int l_write(lua_State *L) {
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    unsigned char h[12];
    header(h, data, len);
    FILE *f = fopen(full(L, 1), "wb");
    int ok = f && fwrite(h, 1, 12, f) == 12 && fwrite(data, 1, len, f) == len;
    if (f && fclose(f)) ok = 0;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_remove(lua_State *L) {
    const char *path = full(L, 1);
    int ok = remove(path) == 0;
#ifdef _EE
    // The console's own file module removes the file and then, falling through, makes a directory of that name
    // (seen in PCSX2: the name could never be written again). Removing that directory is the known way around.
    rmdir(path);
#endif
    lua_pushboolean(L, ok);
    return 1;
}

static int l_mkdir(lua_State *L) {
    mkdir(full(L, 1), 0777);
    return 0;
}

// one pass of zlib over the string at 1, the output grown as it fills
static int flate(lua_State *L, z_stream *z, int (*step)(z_stream *, int), int (*end)(z_stream *)) {
    size_t len;
    const char *in = luaL_checklstring(L, 1, &len);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    z->next_in = (Bytef *)in, z->avail_in = len;
    int r;
    do {
        z->next_out = (Bytef *)luaL_prepbuffer(&b), z->avail_out = LUAL_BUFFERSIZE;
        r = step(z, Z_FINISH);
        luaL_addsize(&b, LUAL_BUFFERSIZE - z->avail_out);
    } while (r == Z_OK || (r == Z_BUF_ERROR && z->avail_out == 0));
    end(z);
    if (r != Z_STREAM_END) return luaL_error(L, "save: not deflate data (zlib %d)", r);
    luaL_pushresult(&b);
    return 1;
}
static int l_deflate(lua_State *L) {
    z_stream z = {0};
    if (deflateInit2(&z, luaL_optint(L, 2, Z_DEFAULT_COMPRESSION), Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return luaL_error(L, "save: no memory to deflate");
    return flate(L, &z, deflate, deflateEnd);
}
static int l_inflate(lua_State *L) {
    z_stream z = {0};
    if (inflateInit2(&z, -15) != Z_OK) return luaL_error(L, "save: no memory to inflate");
    return flate(L, &z, inflate, inflateEnd);
}

int luaopen_save(lua_State *L) {
    static const luaL_Reg save[] = {{"open", l_open},       {"read", l_read},       {"write", l_write}, {"remove", l_remove},
                                    {"mkdir", l_mkdir},     {"deflate", l_deflate}, {"inflate", l_inflate}, {NULL, NULL}};
    luaL_register(L, "save", save);
    return 1;
}
