// The DualShock 2 for boot.lua's love.joystick. pad.read() -> the 16 buttons as libpad's bits
// (1 = pressed here) and the two sticks, each axis -1..1. The host has no pad: zeros (tests script it, boot.lua).
#include "lauxlib.h"
#include "lua.h"

#ifdef _EE
#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>

static char pad_buf[256] __attribute__((aligned(64)));
static int open, analog;

// rom0:XSIO2MAN + XPADMAN with libpadx: the plain pair hangs under Neutrino
static void pad_init(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:XSIO2MAN", 0, NULL);
    SifLoadModule("rom0:XPADMAN", 0, NULL);
    padInit(0);
    open = padPortOpen(0, 0, pad_buf) != 0;
    for (int tries = 0; open && tries < 500000; tries++) {
        int s = padGetState(0, 0);
        if (s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1 || s == PAD_STATE_DISCONN) break;
    }
}

static lua_Number axis(unsigned char v) { // 0..255, 128 at rest
    return v >= 128 ? (v - 128) / (lua_Number)127 : (v - 128) / (lua_Number)128;
}
#endif

static int l_read(lua_State *L) {
    unsigned buttons = 0;
    lua_Number a[4] = {0, 0, 0, 0};
#ifdef _EE
    struct padButtonStatus st;
    int s = open ? padGetState(0, 0) : PAD_STATE_DISCONN;
    if (s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1) {
        if (!analog) { // DualShock mode, locked: the sticks report
            padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
            analog = 1;
        } else if (padRead(0, 0, &st) != 0) {
            buttons = 0xFFFF ^ st.btns; // libpad: 0 = pressed
            if (st.mode >> 4 == 7) {
                a[0] = axis(st.ljoy_h), a[1] = axis(st.ljoy_v);
                a[2] = axis(st.rjoy_h), a[3] = axis(st.rjoy_v);
            }
        }
    } else if (s == PAD_STATE_DISCONN) {
        analog = 0; // a pad plugged in later is set up again
    }
#endif
    lua_pushinteger(L, buttons);
    for (int i = 0; i < 4; i++) lua_pushnumber(L, a[i]);
    return 5;
}

int luaopen_pad(lua_State *L) {
    static const luaL_Reg pad[] = {{"read", l_read}, {NULL, NULL}};
#ifdef _EE
    pad_init();
#endif
    luaL_register(L, "pad", pad);
    return 1;
}
