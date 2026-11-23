// main.lua:1-388: the top-level chunk (main), love.run and LÖVE's callbacks.
#include <malloc.h>
#include <time.h>
#include "all.h"
#include "needs/main.h"

// globals.lua:522 `G = Game()`: game.h gives the creation to main() below.
// note: types.h only declares `extern Game *G`; the definition is here, with the code that creates it.
Game *G;

#include <time.h>
num os_time(void) { return getenv("TIME") ? atof(getenv("TIME")) : (num)time(NULL); }

// main.lua:39-41, the locals of love.run the frame closure captures
static double dt = 0;
static double dt_smooth = 1. / 100;
static double run_time = 0;
static double spent_update, spent_draw; // seconds in love_update and love_draw since the last PERF line

#if defined(PROF) && defined(_EE)
// -DPROF -DPROF_FROM=<frame> -DPROF_TO=<frame> (EE): a sampling profiler, as the Lua runtime's. A 1 kHz timer alarm
// counts the interrupted pc in 16-byte buckets of the text between those frames, then writes them to prof.bin
// beside the ELF, for the repo's tools/prof.py.
#include <kernel.h>
#include <timer_alarm.h>
extern char _ftext[], _etext[];
static unsigned short *hist;
static u64 prof_tick(s32 id, u64 scheduled, u64 actual, void *arg, void *pc) {
    unsigned off = (char *)pc - _ftext;
    hist[off < (unsigned)(_etext - _ftext) ? off >> 4 : 0]++; // bucket 0 (crt0) also takes what is outside the text
    return USec2TimerBusClock(1000); // from the scheduled time
}
static void prof_frame(int frame) {
    static s32 alarm;
    size_t n = ((_etext - _ftext) >> 4) + 1;
    if (frame == PROF_FROM) {
        hist = (calloc)(n, sizeof *hist); // libc's: not the collector's
        alarm = SetTimerAlarm(USec2TimerBusClock(1000), prof_tick, NULL);
    } else if (frame == PROF_TO) {
        ReleaseTimerAlarm(alarm);
        FILE *f = fopen("host:prof.bin", "wb");
        if (f) fwrite(hist, sizeof *hist, n, f), fclose(f);
        printf("PROF written\n");
    }
}
#else
static void prof_frame(int frame) {}
#endif

// main.lua:44-83, the function love.run returns
bool love_run_frame(int *exit_code) {
    gc_frame(); // the collector's turn, here where the stack is shallow (Lua's ran inside its allocations)
    run_time = love_timer_getTime();
    // note: the platform has no "quit" event: love_frame_begin() is false once love_event_quit() was called. It is
    // called every frame (it counts them), outside the Lua's `G and G.CONTROLLER` test.
    bool quit = !love_frame_begin();
    // Process events.
    if (G && G->CONTROLLER) {
        love_event_pump();
        // main.lua:51-55
        if (quit) {
            if (!love_quit()) {
                if (exit_code) *exit_code = 0; // a or 0: love_event_quit() takes no exit status
                return true;
            }
        }
        // main.lua:56-66: 'touchpressed' and 'mousepressed' (the last press is kept and given to
        // love_mousepressed(_a, _b, _c, touched) after the loop) never come from the pad: love_event_poll has
        // only the gamepad's events.
        LoveEvent e;
        while (love_event_poll(&e)) {
            // main.lua:61 love.handlers[name](a,b,c,d,e,f)
            switch (e.type) {
            case LOVE_GAMEPADPRESSED: love_gamepadpressed(e.joystick, e.button); break;
            case LOVE_GAMEPADRELEASED: love_gamepadreleased(e.joystick, e.button); break;
            case LOVE_JOYSTICKAXIS: love_joystickaxis(e.joystick, e.axis, e.value); break;
            }
        }
    }

    // Update dt, as we'll be passing it to update
    dt = love_timer_step();
    dt_smooth = lmin(0.8 * dt_smooth + 0.2 * dt, 0.1);
    // Call update and draw
    double t0 = love_timer_getTime();
    love_update(dt_smooth);
    double t1 = love_timer_getTime();
    spent_update += t1 - t0;

    if (love_graphics_isActive()) {
        love_draw();
        spent_draw += love_timer_getTime() - t1;
        love_present(); // love.graphics.present(), and the sound's share of the frame
    }

    run_time = lmin(love_timer_getTime() - run_time, 0.1);
    if (!G->FPS_CAP) G->FPS_CAP = 500; // G.FPS_CAP or 500 (0 as nil)
    if (run_time < 1. / G->FPS_CAP) love_timer_sleep((float)(1. / G->FPS_CAP - run_time));
    return false;
}

// main.lua:33
int love_run(void) {
    love_load(); // love.arg.parseGameArguments(arg): love.load takes no arguments
    printf("BOOT loaded\n");

    // We don't want the first frame's dt to include time taken by love.load.
    love_timer_step();

    dt = 0;
    dt_smooth = 1. / 100;
    run_time = 0;

    // Main loop time.
    int exit_code = 0;
    void hook_blinds(int), hook_shop(int), hook_jokers(int), hook_saves(int), hook_ui(int);
    const char *hook = getenv("HOOK");
    // note: host tests: FRAMES=<n> in the environment ends the run after n frames (the oracle's third argument).
    int frames = getenv("FRAMES") ? atoi(getenv("FRAMES")) : 0;
    int trace = getenv("TRACE") ? atoi(getenv("TRACE")) : 0; // every n frames, the oracle's TRACE line (boot.lua's trace())
#ifdef TEST_TRACE // the PS2 build has no environment
    trace = TEST_TRACE;
#endif
    while (!love_run_frame(&exit_code)) {
        if (hook) { // HOOK=<name>: a test hook per frame (src/hook_<name>.c), the oracle's HOOK=<file.lua>
            if (streq(hook, "blinds")) hook_blinds(love_frame());
            if (streq(hook, "shop")) hook_shop(love_frame());
            if (streq(hook, "jokers")) hook_jokers(love_frame());
            if (streq(hook, "saves")) hook_saves(love_frame());
            if (streq(hook, "ui")) hook_ui(love_frame());
        }
        if (trace && love_frame() % trace == 0) {
            double s = 0;
            int n = tlen(G->MOVEABLES);
            for (int j = 1; j <= n; j++) {
                Moveable *m = tgeti(G->MOVEABLES, j).obj;
                Transform *T = m->node.T, *VT = &m->VT;
                s += j * ((double)T->x + 2 * (double)T->y + 3 * (double)T->w + 5 * (double)T->h + 7 * (double)T->r + 11 * (double)VT->x + 13 * (double)VT->y + 17 * (double)VT->w + 19 * (double)VT->h +
                          23 * (double)VT->r + 29 * (double)VT->scale + 31 * (double)m->velocity.x + 37 * (double)m->velocity.y + 41 * (double)m->velocity.r + 43 * (double)m->velocity.scale);
            }
            for (int j = 1; s != s && j <= n; j++) { // a NaN: which moveable has it
                Moveable *m = tgeti(G->MOVEABLES, j).obj;
                Transform *T = m->node.T, *VT = &m->VT;
                float f[15] = {T->x, T->y, T->w, T->h, T->r, VT->x, VT->y, VT->w, VT->h, VT->r, VT->scale, m->velocity.x, m->velocity.y, m->velocity.r, m->velocity.scale};
                double all = 0; // (each as a double: the PS2's FPU takes a NaN for a number)
                for (int k = 0; k < 15; k++) all += (double)f[k];
                if (all != all) {
                    printf("NAN moveable %d %s T %g %g %g %g %g VT %g %g %g %g %g %g vel %g %g %g %g\n", j, m->node.object.cls->name, T->x, T->y, T->w, T->h, T->r, VT->x, VT->y,
                           VT->w, VT->h, VT->r, VT->scale, m->velocity.x, m->velocity.y, m->velocity.r, m->velocity.scale);
                    break;
                }
            }
            printf("TRACE %d state %d moveables %d sum %.17g chips %s\n", love_frame(), G->STATE, n, s,
                   G->GAME ? tostring(tget(G->GAME, "chips")) : "nil");
        }
        if (love_frame() % 60 == 0) { // as the Lua runtime's PERF line: the time of 60 frames and the heap
            static clock_t last;
            clock_t now = clock();
            // gc: the collector's arena in use and free, the bytes it ever handed out, its collections, the longest
            // slice of the last collection and of all, a slice's work units and the most one took, the final step
            // (the one that is not cut) of the last collection, the longest, and its units. malloc: libc's heap (the platform's textures, sounds and file buffers; on the PS2 the arena too)
            const GcStats *gc = gc_stats();
            printf("PERF frame %d  %.2f ms a frame  update %.2f ms  draw %.2f ms  malloc %d KB  state %d  gc used %d KB free %d KB allocated %llu KB collections %u last %.2f ms worst %.2f ms slice %ld units worst %ld final %.2f ms worst %.2f ms %ld units\n",
                   love_frame(), (now - last) * 1000.0 / CLOCKS_PER_SEC / 60, spent_update * 1000 / 60, spent_draw * 1000 / 60, (int)(mallinfo().uordblks / 1024), G->STATE, (int)(gc->used >> 10),
                   (int)((gc->heap - gc->used) >> 10), gc->allocated >> 10, gc->collections, gc->last_ms, gc->worst_ms, gc->slice, gc->worst_units, gc->final_ms, gc->worst_final_ms, gc->final_units);
            last = now;
            spent_update = spent_draw = 0;
        }
        prof_frame(love_frame());
#ifdef TEST_QUIT // -DTEST_QUIT=<frame> (EE): love.event.quit() at that frame, to see the saves still queued get written
        if (love_frame() == TEST_QUIT) love_event_quit();
#endif
#ifdef TEST_NOTUT // -DTEST_NOTUT (EE): the tutorial counts as done, as NOTUT=1 of the host's saves hook: Play opens the run setup (Continue). -DTEST_NOTUT=0 takes that back (the settings are saved on the card)
        if (love_frame() == 2) tset(G->SETTINGS, "tutorial_complete", TEST_NOTUT ? B(true) : NIL);
#else
        if (love_frame() == 2 && getenv("NOTUT")) tset(G->SETTINGS, "tutorial_complete", B(true)); // host: the same, NOTUT=1 (test hook only)
#endif
        if (frames && love_frame() >= frames) break;
    }
    return exit_code;
}

// main.lua:86
void love_load(void) {
    Game_start_up(G);
    // main.lua:88-114 Steam integration: only on 'OS X' and 'Windows' (love_system_getOS() is "ps4"); not ported.

    // Set the mouse to invisible immediately, this visibility is handled in the G.CONTROLLER
    love_mouse_setVisible(false);
}

// main.lua:120
bool love_quit(void) {
    if (G->SOUND_MANAGER) sound_manager_request(TAB("type", S("stop")));
    // main.lua:123 G.STEAM:shutdown(): no Steam.
    return false;
}

// main.lua:126
void love_update(num dt) {
    // Perf monitoring checkpoint
    timer_checkpoint(NULL, "update", true);
    Game_update(G, dt);
}

// main.lua:132
void love_draw(void) {
    // Perf monitoring checkpoint
    timer_checkpoint(NULL, "draw", true);
    Game_draw(G);
}

// main.lua:138
void love_keypressed(const char *key) {
    if (!RELEASE_MODE && truthy(tget(G->keybind_mapping, key)))
        love_gamepadpressed(G->CONTROLLER->keyboard_controller, tstr(G->keybind_mapping, key));
    else {
        Controller_set_HID_flags(G->CONTROLLER, "mouse", NULL);
        Controller_key_press(G->CONTROLLER, key);
    }
}

// main.lua:146
void love_keyreleased(const char *key) {
    if (!RELEASE_MODE && truthy(tget(G->keybind_mapping, key)))
        love_gamepadreleased(G->CONTROLLER->keyboard_controller, tstr(G->keybind_mapping, key));
    else {
        Controller_set_HID_flags(G->CONTROLLER, "mouse", NULL);
        Controller_key_release(G->CONTROLLER, key);
    }
}

// main.lua:154
void love_gamepadpressed(Joystick *joystick, const char *button) {
    Val mapped = tget(G->button_mapping, button);
    if (truthy(mapped)) button = vstr(mapped);
    Controller_set_gamepad(G->CONTROLLER, joystick);
    Controller_set_HID_flags(G->CONTROLLER, "button", button);
    Controller_button_press(G->CONTROLLER, button);
}

// main.lua:161
void love_gamepadreleased(Joystick *joystick, const char *button) {
    Val mapped = tget(G->button_mapping, button);
    if (truthy(mapped)) button = vstr(mapped);
    Controller_set_gamepad(G->CONTROLLER, joystick);
    Controller_set_HID_flags(G->CONTROLLER, "button", button);
    Controller_button_release(G->CONTROLLER, button);
}

// main.lua:168
void love_mousepressed(float x, float y, int button, bool touch) {
    Controller_set_HID_flags(G->CONTROLLER, touch ? "touch" : "mouse", NULL);
    if (button == 1) {
        Controller_queue_L_cursor_press(G->CONTROLLER, N(x), N(y));
    }
    if (button == 2) {
        Controller_queue_R_cursor_press(G->CONTROLLER, N(x), N(y));
    }
}

// main.lua:179
void love_mousereleased(float x, float y, int button) {
    if (button == 1) Controller_L_cursor_release(G->CONTROLLER, N(x), N(y));
}

// main.lua:183
void love_mousemoved(float x, float y, float dx, float dy, bool istouch) {
    Table *dyn = G->CONTROLLER->object.dyn; // last_touch_time is not a member of Controller
    Val last = tget(dyn, "last_touch_time");
    tsetn(dyn, "last_touch_time", truthy(last) ? last.n : -1);
    if (love_touch_getTouches() > 0) { // next(love.touch.getTouches()) ~= nil
        tsetn(dyn, "last_touch_time", G->TIMERS.UPTIME);
    }
    Controller_set_HID_flags(G->CONTROLLER, tnum(dyn, "last_touch_time") > G->TIMERS.UPTIME - 0.2 ? "touch" : "mouse", NULL);
}

// main.lua:191
void love_joystickaxis(Joystick *joystick, int axis, float value) {
    if (fabs(value) > 0.2 && Joystick_isGamepad(joystick)) {
        Controller_set_gamepad(G->CONTROLLER, joystick);
        Controller_set_HID_flags(G->CONTROLLER, "axis", NULL);
    }
}

// main.lua:285, the local function draw() of love.errhand
static void errhand_draw(const char *p, Font *font) {
    float pos = love_window_toPixels(70);
    love_graphics_push();
    love_graphics_clear(G->C.BLACK[0], G->C.BLACK[1], G->C.BLACK[2], G->C.BLACK[3]);
    love_graphics_setColor(1., 1., 1., 1.);
    love_graphics_printf(p, font, pos, pos); // note: the wrap limit (getWidth() - pos) is dropped: love.h's printf does not wrap
    love_graphics_pop();
    love_graphics_present();
}

// main.lua:198
void love_errhand(const char *msg) {
    if (G->F_NO_ERROR_HAND) return;
    msg = tostring(S(msg));

    // main.lua:202-248 the crash report sent over HTTP (G.F_CRASH_REPORTS, the HTTP thread): not ported.

    // main.lua:250 love.window, love.graphics and love.event always exist.

    if (!love_graphics_isCreated() || !love_window_isOpen()) {
        bool status = love_window_setMode(800, 600); // pcall: it cannot fail
        if (!status) {
            return;
        }
    }

    // Reset state.
    love_mouse_setVisible(true);
    love_mouse_setGrabbed(false);
    love_mouse_setRelativeMode(false);
    {
        // Stop all joystick vibrations.
        int n = 0;
        Joystick **joysticks = love_joystick_getJoysticks(&n);
        for (int i = 0; i < n; i++) Joystick_setVibration(joysticks[i], 0, 0);
    }
    love_audio_stop();
    love_graphics_reset();
    Font *font = love_graphics_setNewFont("resources/fonts/m6x11plus.ttf", 20);

    love_graphics_clear(G->C.BLACK[0], G->C.BLACK[1], G->C.BLACK[2], G->C.BLACK[3]);
    love_graphics_origin();

    // note: debug.traceback() has no equivalent in C: without _RELEASE_MODE the message ends after the blank line.
    // note: the three texts are literals of the Lua: LITERALS.errhand of data/game.dat (data.h): 1 the heading (it
    // ends with the newline), 2 and 3 the paragraphs with crash reports on and off.
    Table *x = lits("errhand");
    const char *p = fmt("%s%s\n\n%s", lit_at(x, 1), msg,
        !RELEASE_MODE ? "" : tbool(G->SETTINGS, "crashreports") ? lit_at(x, 2) : lit_at(x, 3));

    while (true) {
        love_event_pump();

        // main.lua:299-313: "quit" is love_frame_begin() returning false (it also counts this frame); the pad
        // sends no "keypressed" (escape) nor "touchpressed" (the quit message box), so its events are dropped.
        if (!love_frame_begin()) return;
        LoveEvent e;
        while (love_event_poll(&e)) {}

        errhand_draw(p, font);

        love_timer_sleep(0.1f);
    }
}

// main.lua:324
void love_resize(float w, float h) {
    if (w / h < 1) { // Dont allow the screen to be too square, since pop in occurs above and below screen
        h = w / 1;
    }

    // When the window is resized, this code resizes the Canvas, then places the 'room' or gamearea into the middle without streching it
    if (w / h < G->window_prev.orig_ratio) {
        G->TILESCALE = G->window_prev.orig_scale * w / G->window_prev.w;
    } else {
        G->TILESCALE = G->window_prev.orig_scale * h / G->window_prev.h;
    }

    if (G->ROOM) {
        G->ROOM->T->w = G->TILE_W;
        G->ROOM->T->h = G->TILE_H;
        G->ROOM_ATTACH->node.T->w = G->TILE_W;
        G->ROOM_ATTACH->node.T->h = G->TILE_H;

        if (w / h < G->window_prev.orig_ratio) {
            G->ROOM->T->x = G->ROOM_PADDING_W;
            G->ROOM->T->y = (h / (G->TILESIZE * G->TILESCALE) - (G->ROOM->T->h + G->ROOM_PADDING_H)) / 2 + G->ROOM_PADDING_H / 2;
        } else {
            G->ROOM->T->y = G->ROOM_PADDING_H;
            G->ROOM->T->x = (w / (G->TILESIZE * G->TILESCALE) - (G->ROOM->T->w + G->ROOM_PADDING_W)) / 2 + G->ROOM_PADDING_W / 2;
        }

        G->ROOM_ORIG.x = G->ROOM->T->x;
        G->ROOM_ORIG.y = G->ROOM->T->y;
        G->ROOM_ORIG.r = G->ROOM->T->r;

        if (G->buttons) UIBox_recalculate(G->buttons);
        if (G->HUD) UIBox_recalculate(G->HUD);
    }

    G->WINDOWTRANS.x = 0, G->WINDOWTRANS.y = 0;
    G->WINDOWTRANS.w = G->TILE_W + 2 * G->ROOM_PADDING_W;
    G->WINDOWTRANS.h = G->TILE_H + 2 * G->ROOM_PADDING_H;
    G->WINDOWTRANS.real_window_w = w;
    G->WINDOWTRANS.real_window_h = h;

    G->CANV_SCALE = 1;

    if (streq(love_system_getOS(), "Windows") && false) { // implement later if needed (dead in the Lua too)
        int render_w, render_h, unscaled_w, unscaled_h;
        love_window_getDesktopDimensions(&render_w, &render_h);
        love_window_getFullscreenModes(&unscaled_w, &unscaled_h); // [1].width, [1].height

        double DPI_scale = floor((0.5 * unscaled_w / render_w + 0.5 * unscaled_h / render_h) * 500 + 0.5) / 500;

        if (DPI_scale > 1.1) {
            G->CANV_SCALE = 1.5;

            G->AA_CANVAS = love_graphics_newCanvas(G->WINDOWTRANS.real_window_w * G->CANV_SCALE, G->WINDOWTRANS.real_window_h * G->CANV_SCALE);
            Image_setFilter(G->AA_CANVAS, "linear", "linear");
        } else {
            G->AA_CANVAS = NULL;
        }
    }

    G->CANVAS = love_graphics_newCanvas(w * G->CANV_SCALE, h * G->CANV_SCALE);
    Image_setFilter(G->CANVAS, "linear", "linear");
}

// main.lua:1-31, the top-level chunk, and what boot.lua did around it (game.h's rules: love_init, the classes,
// G = Game(), the seed, love.run).
// (not inlined: its frame, and every frame under it, lies below the address main gives the collector as the
// bottom of the stack)
#ifdef DISC // the data files (data/*.dat) are in the disc's pack with the game's files (tools/disc_native.py)
static unsigned char *read_packed(const char *path, size_t *size) { return love_game_read(path, size); }
#endif
__attribute__((noinline)) static int game_main(void) {
    love_init(NULL);
#ifdef DISC
    data_read_file = read_packed;
#endif
    // main.lua:1 jit.off() on ARM Macs: nothing. main.lua:2-29, the requires: the classes are made ready.
    classes_init();
    funcs_register_all(); // the requires of main.lua define G.FUNCS before any UI is built
    printf("BOOT data\n");
    game_dat();
    printf("BOOT game\n");
    G = NEW(Game); // globals.lua:522 G = Game()
    Game_init(G);
    printf("BOOT game made\n");
    // main.lua:29 require "challenges": the file's one statement, G.CHALLENGES = {...} (challenges.h: the key
    // CHALLENGES of game.dat). The table itself, not a copy: it is the game's only one.
    G->CHALLENGES = ttab(game_dat(), "CHALLENGES");
    if (!G->CHALLENGES) ERROR("data/game.dat has no CHALLENGES");
    math_randomseed(G->SEED); // main.lua:31
    return love_run();
}

int main(int argc, char **argv) {
    void *stack_bottom;
    gc_init(&stack_bottom); // before anything is allocated
    exit(game_main()); // (not a return: the EE's start-up code would leave without the atexit calls, and the saves still waiting for the card are written by one, love_fs.c)
}
