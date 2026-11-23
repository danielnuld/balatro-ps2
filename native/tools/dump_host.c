// The Lua 5.1 that runs dump_data.lua: the repo's vendor/lua-5.1.5 with lua51_keyorder.patch applied (every hash
// node remembers when its key was first set), plus keyseq(t), which gives that order to the script. The stock
// interpreter cannot tell the order in which a table constructor defined its keys; the port's tables iterate in it.
// Built and run by dump_data.sh only; not part of the game.
#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"

// keyseq(t): {key = sequence number} for the keys in t's hash part
static int keyseq(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const Table *t = lua_topointer(L, 1);
    lua_newtable(L);
    for (int i = 0; i < sizenode(t); i++) {
        Node *n = gnode(t, i);
        if (ttisnil(gval(n))) continue;
        luaL_checkstack(L, 2, "keyseq");
        L->top->value = gkey(n)->value, L->top->tt = gkey(n)->tt; // push the key as it is
        L->top++;
        lua_pushnumber(L, n->seq);
        lua_rawset(L, -3);
    }
    return 1;
}

// f64(n): the 8 bytes of the double, little endian (the host is: x86)
static int f64(lua_State *L) {
    double d = luaL_checknumber(L, 1);
    char b[8];
    memcpy(b, &d, 8);
    lua_pushlstring(L, b, 8);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) return fprintf(stderr, "usage: %s script.lua [args]\n", argv[0]), 2;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "keyseq", keyseq);
    lua_register(L, "f64", f64);
    lua_newtable(L);
    for (int i = 1; i < argc; i++) lua_pushstring(L, argv[i]), lua_rawseti(L, -2, i - 1); // arg[0] is the script
    lua_setglobal(L, "arg");
    if (luaL_dofile(L, argv[1])) return fprintf(stderr, "%s\n", lua_tostring(L, -1)), 1;
    return 0;
}
