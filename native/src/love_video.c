// The screen before the game that asks for the picture's mode (love_gs.c's table: 480i, 480i soft, 480p, 720p,
// 720p 4:3). It is the
// platform's, not the game's: drawn with love.graphics' own calls and the game's font, read from the pad as it is.
//  - Always shown in 480i, which every cable shows. Up and down move, cross accepts; after 5 seconds with no
//    button the mode it opened on is taken, so a player who never touches it waits 5 seconds.
//  - It opens on the last mode accepted, kept in the saves as video.txt (the mode's name). No card, no file: 480i.
//  - Another mode than 480i is set and then asked about, in that mode, for 10 seconds: cross keeps it; any other
//    button, or none (a screen that cannot show the mode shows nothing), goes back to 480i. A mode taken by the
//    5 seconds running out is asked about the same way, and if nobody answers the game starts in 480i with the
//    remembered mode left as it was (another screen today, the same one tomorrow).
//  - Only a mode the player accepted with the pad is remembered.
// -DVIDEO='"720p"': that mode with no screen (PCSX2's screenshots). The host has no modes (VIDEO=720p there:
// love_gfx.c).
// NOT IN THE FIRST RELEASE: the screen is only built with -DVIDEO_PICK. On the console the kernel's SetGsCrt for
// 480p or 720p changes the screen's mode and never returns when the game is started through Neutrino with no
// forced mode (five test runs, gs_screen.c and out/setgscrt-notes.md), and with a forced mode (-gsm) the loader
// already shows 480i as 480p and ignores other requests. Until that is understood the game runs in 480i and the
// loader's own option is the way to 480p.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOVE_INTERNAL
#include "love.h"

#ifdef _EE
#define CROSS (1 << 14)
#define UP (1 << 4)
#define DOWN (1 << 6)
#define BLANKS 60 // a second of gs_show
static const char saved[] = "video.txt";

static void centred(const char *s, float y, float size) {
    Font *f = love_graphics_setNewFont(NULL, size);
    love_graphics_print(s, floorf((LOVE_SCREEN_W - Font_getWidth(f, s)) / 2), y, 0, 1, 1);
}
static int set(int mode) { // 0: the mode gave no picture timing and 480i is back (gs_mode)
    int w, h;
    int ok = gs_mode(mode, &w, &h);
    love_gfx_scale(w, h);
    return ok;
}
// Buttons pressed since the last call (held ones only once)
static unsigned pressed(void) {
    static unsigned before = 0xFFFF; // (what is held when the screen opens is not a press)
    unsigned now = love_pad_peek(), fresh = now & ~before;
    before = now;
    return fresh;
}
// The list, opened on `at`. The mode accepted; *by_hand: with the pad, not by the 5 seconds.
static int list(int at, int count, int *by_hand) {
    for (int idle = 0;; idle++) {
        unsigned b = pressed();
        if (b) idle = 0, *by_hand = 1;
        if (b & UP) at = (at + count - 1) % count;
        if (b & DOWN) at = (at + 1) % count;
        if (b & CROSS) return at;
        if (!*by_hand && idle >= 5 * BLANKS) return at;
        love_graphics_clear(0.15f, 0.19f, 0.21f, 1);
        love_graphics_setColor(1, 1, 1, 1);
        centred("VIDEO", 96, 32);
        for (int i = 0; i < count; i++) {
            float y = 150 + i * 30;
            if (i == at) {
                love_graphics_setColor(0.99f, 0.37f, 0.33f, 1);
                love_graphics_rectangle("fill", LOVE_SCREEN_W / 2 - 80, y - 5, 160, 26);
            }
            love_graphics_setColor(1, 1, 1, i == at ? 1 : 0.6f);
            centred(gs_mode_name(i), y, 16);
        }
        char hint[64];
        if (*by_hand) snprintf(hint, sizeof hint, "UP / DOWN: CHOOSE    X: ACCEPT");
        else snprintf(hint, sizeof hint, "UP / DOWN: CHOOSE    X: ACCEPT    (%d)", 5 - idle / BLANKS);
        love_graphics_setColor(1, 1, 1, 0.8f);
        centred(hint, 324, 16);
        centred("480p AND 720p NEED A COMPONENT CABLE", 352, 16);
        centred("720p FILLS A WIDE SCREEN: SET THE SCREEN TO 4:3", 380, 16);
        gs_show();
    }
}
// In the mode just set: whether cross was pressed within 10 seconds. *answered: a button was, whichever.
static int keep(int mode, int *answered) {
    for (int t = 0; t < 10 * BLANKS; t++) {
        unsigned b = pressed();
        if (b) return *answered = 1, (b & CROSS) != 0;
        char line[64];
        love_graphics_clear(0.15f, 0.19f, 0.21f, 1);
        love_graphics_setColor(1, 1, 1, 1);
        centred(gs_mode_name(mode), 120, 32);
        centred("KEEP THIS MODE?", 200, 16);
        snprintf(line, sizeof line, "X: KEEP    ANY OTHER BUTTON: BACK    (%d)", 10 - t / BLANKS);
        love_graphics_setColor(1, 1, 1, 0.8f);
        centred(line, 250, 16);
        gs_show();
    }
    return 0;
}
#endif

void love_video_pick(void) {
#ifdef _EE
    int count = 0, mode = 0, by_hand = 0;
    while (gs_mode_name(count)) count++;
#ifdef VIDEO
    for (int i = 0; i < count; i++)
        if (!strcmp(gs_mode_name(i), VIDEO)) set(i);
    return;
#endif
#ifndef VIDEO_PICK
    return;
#endif
    if (gs_forced()) return; // (the picture is the loader's: asking for a mode would only lose it)
    char *was = love_filesystem_read(saved, NULL);
    int remembered = 0;
    for (int i = 0; was && i < count; i++)
        if (!strncmp(was, gs_mode_name(i), strlen(gs_mode_name(i)))) remembered = i;
    free(was);
    for (int at = remembered;;) {
        int answered = 0;
        mode = list(at, count, &by_hand);
        if (!mode) break;
        if (!set(mode)) {
            mode = 0, at = 0, by_hand = 1;
            continue;
        }
        printf("VIDEO asking whether to keep %s\n", gs_mode_name(mode));
        int kept = keep(mode, &answered);
        printf("VIDEO %s\n", kept ? "kept" : answered ? "refused" : "no answer in 10 seconds");
        if (kept) {
            by_hand = 1; // (cross here is the player's word, also after the 5 seconds)
            break;
        }
        set(mode = 0);
        if (!answered && !by_hand) break; // nobody there: 480i for this run, the remembered mode stays
        at = 0, by_hand = 1;              // back to the list, which waits now
    }
    if (by_hand && mode != remembered) love_filesystem_write(saved, gs_mode_name(mode), strlen(gs_mode_name(mode)));
    love_graphics_setColor(1, 1, 1, 1);
    love_graphics_setNewFont(NULL, 12); // (as love_gfx_init left it)
#endif
}
