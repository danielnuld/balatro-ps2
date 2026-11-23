// The DualShock 2 as LÖVE's one gamepad: love.joystick and the gamepad events of love.event. love_pad_read gives
// the 16 buttons as libpad's bits (1 = pressed here) and the two sticks, each axis -1..1; love_event_pump queues
// what changed since the last one as LÖVE's events. The host has no pad: zeros (tests script it, below).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOVE_INTERNAL
#include "love.h"

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

// The buttons at every vertical blank, kept until the game asks: a frame of a run is several blanks long (far
// more on the console than in PCSX2), and a tap that began and ended between two frames was never seen, so the
// pad seemed dead wherever frames are long. (padGetState and padRead only read the buffer padman fills.)
static volatile unsigned tapped;
static int watching;
static int pad_blank(int cause) {
    struct padButtonStatus st;
    int s = padGetState(0, 0);
    if ((s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1) && padRead(0, 0, &st) != 0) tapped |= 0xFFFF ^ st.btns;
    ExitHandler();
    return 0;
}

static float axis(unsigned char v) { // 0..255, 128 at rest
    return v >= 128 ? (v - 128) / 127.0f : (v - 128) / 128.0f;
}
#endif

// The buttons now, with no handler started (love_video.c's screen, before the game loads)
unsigned love_pad_peek(void) {
#ifdef _EE
    struct padButtonStatus st;
    int s = open ? padGetState(0, 0) : PAD_STATE_DISCONN;
    if ((s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1) && padRead(0, 0, &st) != 0) return 0xFFFF ^ st.btns;
#endif
    return 0;
}
unsigned love_pad_read(float a[4]) {
    unsigned buttons = 0;
    a[0] = a[1] = a[2] = a[3] = 0;
#ifdef _EE
    struct padButtonStatus st;
    int s = open ? padGetState(0, 0) : PAD_STATE_DISCONN;
    if (s == PAD_STATE_STABLE || s == PAD_STATE_FINDCTP1) {
        if (!analog) { // DualShock mode, locked: the sticks report
            padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
            analog = 1;
        } else if (padRead(0, 0, &st) != 0) {
            if (!watching) { // from the first frame on, not while the game loads (docs/perf.md: a hang then)
                watching = 1;
                DI();
                AddIntcHandler(INTC_VBLANK_S, pad_blank, 0);
                EnableIntc(INTC_VBLANK_S);
                EI();
            }
            DI();
            unsigned since = tapped;
            tapped = 0;
            EI();
            buttons = (0xFFFF ^ st.btns) | since; // libpad: 0 = pressed. A tap already over is down for this frame
            if (st.mode >> 4 == 7) {
                a[0] = axis(st.ljoy_h), a[1] = axis(st.ljoy_v);
                a[2] = axis(st.rjoy_h), a[3] = axis(st.rjoy_v);
            }
        }
    } else if (s == PAD_STATE_DISCONN) {
        analog = 0; // a pad plugged in later is set up again
    }
#endif
    return buttons;
}

// ---------------------------------------------------------------- LÖVE's gamepad and its events
// The names are SDL's, as LÖVE gives them; in libpad's bit order. SELECT is "guide": with getOS() == 'ps4' the
// game puts there what the PC build has on "back" (G.F_GUIDE). L2 and R2 are LÖVE's trigger axes.
static const char *const BUTTONS[16] = {"guide", "leftstick", "rightstick", "start", "dpup", "dpright", "dpdown", "dpleft",
                                        "triggerleft", "triggerright", "leftshoulder", "rightshoulder", "y", "b", "a", "x"};
static const char *const AXES[6] = {"leftx", "lefty", "rightx", "righty", "triggerleft", "triggerright"};
#define TRIGGERS 8 // the first of the two buttons that are axes: their place in BUTTONS, and 4 less in AXES
struct Joystick { int held[16]; float axes[6]; };
static Joystick gamepad, *gamepads[1] = {&gamepad};
static LoveEvent events[64]; // (a pump queues 17 at most)
static int nevents, polled;
// Scripted presses, for tests: PAD="1500:a 1560:dpdown" in the environment (host) presses those buttons at those
// frames; a bot calls love_joystick_press. A press is held for two frames (the d-pad repeats after 0.3 s).
static int pressed_until[16];
static struct { int at, button; } scripted[512]; // PAD=<frame>:<button> ... (host): the presses of a test script
static int nscripted;

static int button_index(const char *name) {
    for (int n = 0; n < 16; n++)
        if (!strcmp(BUTTONS[n], name)) return n;
    return -1;
}
void love_joystick_press(const char *button) {
    int n = button_index(button);
    if (n < 0) love_error("pad: no button '%s'", button);
    pressed_until[n] = love_frame() + 2;
}

void love_pad_init(void) {
    char name[32];
    int at, used;
#ifdef _EE
    pad_init();
    const char *s = NULL;
#ifdef TEST_DT // a scripted run in PCSX2: host:pad.txt beside the ELF is the PAD= of the host (a disc has no host:)
    static char script[8192];
#ifndef PAD_FILE // -DPAD_FILE=pad_mine.txt: a script of one's own beside the shared one
#define PAD_FILE pad.txt
#endif
#define PAD_STR(x) #x
#define PAD_NAME(x) "host:" PAD_STR(x)
    FILE *f = fopen(PAD_NAME(PAD_FILE), "r");
    if (f) script[fread(script, 1, sizeof script - 1, f)] = 0, fclose(f), s = script;
#endif
#else
    const char *s = getenv("PAD");
#endif
    while (s && nscripted < 512) { // "<frame>:<button> ..." (by hand: sscanf's %[ read nothing on the PS2)
        char *end;
        at = (int)strtol(s, &end, 10);
        if (end == s || *end != ':') break;
        for (used = 0, s = end + 1; *s >= 'a' && *s <= 'z' && used < 31; s++) name[used++] = *s;
        name[used] = 0;
        if (button_index(name) < 0) love_error("pad: no button '%s' (PAD)", name);
        scripted[nscripted].at = at, scripted[nscripted++].button = button_index(name);
    }
    if (nscripted) printf("PAD script: %d presses\n", nscripted);
}

Joystick **love_joystick_getJoysticks(int *n) {
    if (n) *n = 1;
    return gamepads;
}
float Joystick_getGamepadAxis(const Joystick *self, const char *axis) {
    for (int n = 0; n < 6; n++)
        if (!strcmp(AXES[n], axis)) return self->axes[n];
    love_error("pad: no axis '%s'", axis);
}
bool Joystick_isGamepadDown(const Joystick *self, const char *button) {
    int n = button_index(button);
    if (n < 0) love_error("pad: no button '%s'", button);
    return n >= TRIGGERS && n < TRIGGERS + 2 ? self->axes[n - 4] > 0 : self->held[n];
}

static void queue(LoveEventType type, const char *button, int axis, float value) {
    if (nevents < 64) events[nevents++] = (LoveEvent){type, &gamepad, button, axis, value}; // ponytail: or it is lost; nobody polls
}
void love_event_pump(void) {
    int frame = love_frame();
    if (polled == nevents) polled = nevents = 0;
    for (int i = 0; i < nscripted; i++)
        if (scripted[i].at == frame) pressed_until[scripted[i].button] = frame + 2;
#ifdef PADLOG
    for (int i = 0; i < nscripted; i++)
        if (scripted[i].at == frame) printf("PADLOG frame %d press %s\n", frame, BUTTONS[scripted[i].button]);
#endif
    float moved = 0;
    unsigned bits = love_pad_read(gamepad.axes);
    for (int n = 0; n < 4; n++)
        if (fabsf(gamepad.axes[n]) > moved) moved = fabsf(gamepad.axes[n]);
    if (moved > 0.2f) queue(LOVE_JOYSTICKAXIS, NULL, 1, moved);
    for (int n = 0; n < 16; n++) {
        int down = (bits >> n & 1) || pressed_until[n] > frame;
        if (n >= TRIGGERS && n < TRIGGERS + 2) {
            gamepad.axes[n - 4] = down;
            if (down) queue(LOVE_JOYSTICKAXIS, NULL, 5, 1);
        } else if (down != gamepad.held[n]) {
            gamepad.held[n] = down;
            queue(down ? LOVE_GAMEPADPRESSED : LOVE_GAMEPADRELEASED, BUTTONS[n], 0, 0);
#ifdef PADLOG
            printf("PADLOG frame %d queue %s %d bits %x\n", frame, BUTTONS[n], down, bits);
#endif
        }
    }
}
bool love_event_poll(LoveEvent *e) {
    if (polled == nevents) return false;
    *e = events[polled++];
    return true;
}
