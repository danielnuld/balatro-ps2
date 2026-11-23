// globals.lua:1-522: Game:set_globals()
// (globals.lua:1-2 VERSION is G->VERSION's initial value; :522 `G = Game()` is main() of src/main.c: game.h)
#include "all.h"
#include <time.h> // note: os.time() (globals.lua:125) is libc's time(NULL); love.h has no os_time

// A table the data file must have (the members it fills are "never NULL").
static Table *need_tab(Table *t, const char *key) {
    Table *v = ttab(t, key);
    if (!v) ERROR("data/game.dat: globals has no table '%s'", key);
    return v;
}

// A colour of the data file ({r, g, b, a}) into a Colour of G: the 4 numbers.
static void colour(float *dst, Table *t, const char *key) {
    Table *c = need_tab(t, key);
    for (int i = 0; i < 4; i++) dst[i] = (float)tnumi(c, i + 1);
}
static void colouri(float *dst, Table *t, int i) {
    Table *c = ttabi(t, i);
    if (!c) ERROR("data/game.dat: globals has no colour [%d]", i);
    for (int k = 0; k < 4; k++) dst[k] = (float)tnumi(c, k + 1);
}

// globals.lua:7 Game:set_globals()
void Game_set_globals(Game *self) {
    // The literals this function assigns: section "globals" of data/game.dat (data.h).
    // note: the "globals" tables of the shared copy (data.h game_dat) become G's (not copied).
    Table *data = game_dat();
    Table *g = need_tab(data, "globals");

    self->VERSION = tstr(g, "VERSION"); // :8

    // :13-42 feature flags
    // note: the data file holds the flags AFTER the OS blocks of :45-120, for the OS of the dump ('ps4', which is
    // what love_system_getOS() answers), not the defaults of :13-42. The blocks below then assign the same values
    // again. If love_system_getOS() ever answers something else, the dump must be made with that DUMP_OS.
#define FLAG(X) self->X = tbool(g, #X)
    FLAG(F_QUIT_BUTTON);
    FLAG(F_SKIP_TUTORIAL);
    FLAG(F_BASIC_CREDITS);
    FLAG(F_EXTERNAL_LINKS);
    FLAG(F_ENABLE_PERF_OVERLAY);
    FLAG(F_NO_SAVING);
    FLAG(F_MUTE);
    FLAG(F_SOUND_THREAD);
    FLAG(F_VIDEO_SETTINGS);
    FLAG(F_CTA);
    FLAG(F_VERBOSE);
    FLAG(F_HTTP_SCORES);
    self->F_RUMBLE = (float)tnum(g, "F_RUMBLE"); // absent: 0 as nil
    FLAG(F_CRASH_REPORTS);
    FLAG(F_NO_ERROR_HAND);
    FLAG(F_SWAP_AB_PIPS);
    FLAG(F_SWAP_AB_BUTTONS);
    FLAG(F_SWAP_XY_BUTTONS);
    FLAG(F_NO_ACHIEVEMENTS);
    self->F_DISP_USERNAME = tget(g, "F_DISP_USERNAME"); // absent: NIL
    FLAG(F_ENGLISH_ONLY);                               // absent: nil, false
    FLAG(F_GUIDE);
    FLAG(F_JAN_CTA);
    FLAG(F_HIDE_BG);
    FLAG(F_TROPHIES);
    FLAG(F_PS4_PLAYSTATION_GLYPHS);
    FLAG(F_LOCAL_CLIPBOARD);
    self->F_SAVE_TIMER = (float)tnum(g, "F_SAVE_TIMER");
    FLAG(F_MOBILE_UI);
    FLAG(F_HIDE_BETA_LANGS);
    FLAG(F_DISCORD); // set only by the Windows / OS X blocks
#undef FLAG

    // :44 a commented-out loadstring
    if (streq(love_system_getOS(), "Windows")) { // :45
        self->F_DISCORD = true;
        self->F_SAVE_TIMER = 5;
        self->F_ENGLISH_ONLY = false;
        self->F_CRASH_REPORTS = false;
    }

    if (streq(love_system_getOS(), "OS X")) { // :52
        self->F_SAVE_TIMER = 5;
        self->F_DISCORD = true;
        self->F_ENGLISH_ONLY = false;
        self->F_CRASH_REPORTS = false;
    }

    if (streq(love_system_getOS(), "Nintendo Switch")) { // :59
        self->F_HIDE_BETA_LANGS = true;
        self->F_BASIC_CREDITS = true;
        self->F_NO_ERROR_HAND = true;
        self->F_QUIT_BUTTON = false;
        self->F_SKIP_TUTORIAL = false;
        self->F_ENABLE_PERF_OVERLAY = false;
        self->F_NO_SAVING = false;
        self->F_MUTE = false;
        self->F_SOUND_THREAD = true;
        self->F_SWAP_AB_PIPS = true;
        self->F_SWAP_AB_BUTTONS = false;
        self->F_SWAP_XY_BUTTONS = true;
        self->F_VIDEO_SETTINGS = false;
        self->F_RUMBLE = 0.7f;
        self->F_CTA = false;
        self->F_VERBOSE = false;
        self->F_NO_ACHIEVEMENTS = true;
        self->F_ENGLISH_ONLY = false; // nil

        self->F_EXTERNAL_LINKS = false;
        self->F_HIDE_BG = true;
    }

    if (streq(love_system_getOS(), "ps4") || streq(love_system_getOS(), "ps5")) { // :83
        self->F_HIDE_BETA_LANGS = true;
        self->F_NO_ERROR_HAND = true;
        self->F_QUIT_BUTTON = false;
        self->F_SKIP_TUTORIAL = false;
        self->F_ENABLE_PERF_OVERLAY = false;
        self->F_NO_SAVING = false;
        self->F_MUTE = false;
        self->F_SOUND_THREAD = true;
        self->F_VIDEO_SETTINGS = false;
        self->F_RUMBLE = 0.5f;
        self->F_CTA = false;
        self->F_VERBOSE = false;

        self->F_GUIDE = true;
        self->F_PS4_PLAYSTATION_GLYPHS = false;

        self->F_EXTERNAL_LINKS = false;
        self->F_HIDE_BG = true;
    }

    if (streq(love_system_getOS(), "xbox")) { // :105
        self->F_HIDE_BETA_LANGS = true;
        self->F_NO_ERROR_HAND = true;
        self->F_DISP_USERNAME = B(true);
        self->F_SKIP_TUTORIAL = false;
        self->F_ENABLE_PERF_OVERLAY = false;
        self->F_NO_SAVING = false;
        self->F_MUTE = false;
        self->F_SOUND_THREAD = true;
        self->F_VIDEO_SETTINGS = false;
        self->F_RUMBLE = 1.0f;
        self->F_CTA = false;
        self->F_VERBOSE = false;
        self->F_EXTERNAL_LINKS = false;
        self->F_HIDE_BG = true;
    }

    // :125-137 time
    self->SEED = os_time();
    Table *TIMERS = need_tab(g, "TIMERS");
    self->TIMERS.TOTAL = tnum(TIMERS, "TOTAL");
    self->TIMERS.REAL = tnum(TIMERS, "REAL");
    self->TIMERS.REAL_SHADER = tnum(TIMERS, "REAL_SHADER");
    self->TIMERS.UPTIME = tnum(TIMERS, "UPTIME");
    self->TIMERS.BACKGROUND = tnum(TIMERS, "BACKGROUND");
    Table *FRAMES = need_tab(g, "FRAMES");
    self->FRAMES.DRAW = (int)tnum(FRAMES, "DRAW");
    self->FRAMES.MOVE = (int)tnum(FRAMES, "MOVE");
    Table *exp_times = need_tab(g, "exp_times");
    self->exp_times.xy = (float)tnum(exp_times, "xy");
    self->exp_times.scale = (float)tnum(exp_times, "scale");
    self->exp_times.r = (float)tnum(exp_times, "r");

    // :141-195 settings
    self->SETTINGS = need_tab(g, "SETTINGS");
    tset(self->SETTINGS, "rumble", self->F_RUMBLE != 0 ? N(self->F_RUMBLE) : NIL); // :160 rumble = self.F_RUMBLE

    self->COLLABS = need_tab(g, "COLLABS"); // :197
    self->METRICS = need_tab(g, "METRICS"); // :239

    self->PROFILES = need_tab(g, "PROFILES"); // :260

    // :269-279 render scale
    self->TILESIZE = (float)tnum(g, "TILESIZE");
    self->TILESCALE = (float)tnum(g, "TILESCALE");
    self->TILE_W = (float)tnum(g, "TILE_W");
    self->TILE_H = (float)tnum(g, "TILE_H");
    self->DRAW_HASH_BUFF = (float)tnum(g, "DRAW_HASH_BUFF");
    self->CARD_W = (float)tnum(g, "CARD_W");
    self->CARD_H = (float)tnum(g, "CARD_H");
    self->HIGHLIGHT_H = (float)(0.2 * tnum(g, "CARD_H")); // :276 0.2*self.CARD_H, in the Lua's doubles
    self->COLLISION_BUFFER = (float)tnum(g, "COLLISION_BUFFER");

    self->PITCH_MOD = (float)tnum(g, "PITCH_MOD");

    // :284-310 self.STATES, self.STAGES: the enums STATES_x / STAGES_x of game.h
    self->STAGE_OBJECTS = need_tab(g, "STAGE_OBJECTS"); // :311
    self->STAGE = STAGES_MAIN_MENU;                     // :314
    self->STATE = STATES_SPLASH;                        // :315
    self->TAROT_INTERRUPT = 0;                          // :316 nil
    self->STATE_COMPLETE = tbool(g, "STATE_COMPLETE");  // :317

    // :322-338 instances
    self->ARGS = need_tab(g, "ARGS");
    // :323 self.FUNCS = {}: the registry of button_callbacks.h (funcs_register)
    Table *I = need_tab(g, "I");
    self->I.NODE = need_tab(I, "NODE");
    self->I.MOVEABLE = need_tab(I, "MOVEABLE");
    self->I.SPRITE = need_tab(I, "SPRITE");
    self->I.UIBOX = need_tab(I, "UIBOX");
    self->I.POPUP = need_tab(I, "POPUP");
    self->I.CARD = need_tab(I, "CARD");
    self->I.CARDAREA = need_tab(I, "CARDAREA");
    self->I.ALERT = need_tab(I, "ALERT");
    self->ANIMATION_ATLAS = need_tab(g, "ANIMATION_ATLAS");
    self->ASSET_ATLAS = need_tab(g, "ASSET_ATLAS");
    self->MOVEABLES = need_tab(g, "MOVEABLES");
    self->ANIMATIONS = need_tab(g, "ANIMATIONS");
    self->DRAW_HASH = need_tab(g, "DRAW_HASH");

    // :343-348 constants
    self->MIN_CLICK_DIST = (float)tnum(g, "MIN_CLICK_DIST");
    self->MIN_HOVER_TIME = tnum(g, "MIN_HOVER_TIME");
    self->DEBUG = tbool(g, "DEBUG");
    self->ANIMATION_FPS = (float)tnum(g, "ANIMATION_FPS");
    self->VIBRATION = (float)tnum(g, "VIBRATION");
    self->CHALLENGE_WINS = tnum(g, "CHALLENGE_WINS");

    // :353-469 colours
    Table *C = need_tab(g, "C");
#define COL(X) colour(self->C.X, C, #X)
#define SUB(GROUP, X) colour(self->C.GROUP.X, sub, #X)
    COL(MULT);
    COL(CHIPS);
    COL(MONEY);
    COL(XMULT);
    COL(FILTER);
    COL(BLUE);
    COL(RED);
    COL(GREEN);
    COL(PALE_GREEN);
    COL(ORANGE);
    COL(IMPORTANT);
    COL(GOLD);
    COL(YELLOW);
    COL(CLEAR);
    COL(WHITE);
    COL(PURPLE);
    COL(BLACK);
    COL(L_BLACK);
    COL(GREY);
    COL(CHANCE);
    COL(JOKER_GREY);
    COL(VOUCHER);
    COL(BOOSTER);
    COL(EDITION);
    COL(DARK_EDITION);
    COL(ETERNAL);
    COL(PERISHABLE);
    COL(RENTAL);
    Table *sub = need_tab(C, "DYN_UI"); // :382
    SUB(DYN_UI, MAIN);
    SUB(DYN_UI, DARK);
    SUB(DYN_UI, BOSS_MAIN);
    SUB(DYN_UI, BOSS_DARK);
    SUB(DYN_UI, BOSS_PALE);
    sub = need_tab(C, "SO_1"); // :390
    SUB(SO_1, Hearts);
    SUB(SO_1, Diamonds);
    SUB(SO_1, Spades);
    SUB(SO_1, Clubs);
    sub = need_tab(C, "SO_2"); // :396
    SUB(SO_2, Hearts);
    SUB(SO_2, Diamonds);
    SUB(SO_2, Spades);
    SUB(SO_2, Clubs);
    sub = need_tab(C, "SUITS"); // :402 the four literals are SUITS_init; SUITS points at them (game.h)
    SUB(SUITS_init, Hearts);
    SUB(SUITS_init, Diamonds);
    SUB(SUITS_init, Spades);
    SUB(SUITS_init, Clubs);
    self->C.SUITS.Hearts = self->C.SUITS_init.Hearts;
    self->C.SUITS.Diamonds = self->C.SUITS_init.Diamonds;
    self->C.SUITS.Spades = self->C.SUITS_init.Spades;
    self->C.SUITS.Clubs = self->C.SUITS_init.Clubs;
    sub = need_tab(C, "UI"); // :408
    SUB(UI, TEXT_LIGHT);
    SUB(UI, TEXT_DARK);
    SUB(UI, TEXT_INACTIVE);
    SUB(UI, BACKGROUND_LIGHT);
    SUB(UI, BACKGROUND_WHITE);
    SUB(UI, BACKGROUND_DARK);
    SUB(UI, BACKGROUND_INACTIVE);
    SUB(UI, OUTLINE_LIGHT);
    SUB(UI, OUTLINE_LIGHT_TRANS);
    SUB(UI, OUTLINE_DARK);
    SUB(UI, TRANSPARENT_LIGHT);
    SUB(UI, TRANSPARENT_DARK);
    SUB(UI, HOVER);
    sub = need_tab(C, "SET"); // :423
    SUB(SET, Default);
    SUB(SET, Enhanced);
    SUB(SET, Joker);
    SUB(SET, Tarot);
    SUB(SET, Planet);
    SUB(SET, Spectral);
    SUB(SET, Voucher);
    sub = need_tab(C, "SECONDARY_SET"); // :432
    SUB(SECONDARY_SET, Default);
    SUB(SECONDARY_SET, Enhanced);
    SUB(SECONDARY_SET, Joker);
    SUB(SECONDARY_SET, Tarot);
    SUB(SECONDARY_SET, Planet);
    SUB(SECONDARY_SET, Spectral);
    SUB(SECONDARY_SET, Voucher);
    SUB(SECONDARY_SET, Edition);
    sub = need_tab(C, "RARITY"); // :442
    for (int i = 1; i <= 4; i++) colouri(self->C.RARITY[i], sub, i);
    sub = need_tab(C, "BLIND"); // :448
    SUB(BLIND, Small);
    SUB(BLIND, Big);
    SUB(BLIND, Boss);
    SUB(BLIND, won);
    sub = need_tab(C, "HAND_LEVELS"); // :454
    for (int i = 1; i <= 7; i++) colouri(self->C.HAND_LEVELS[i], sub, i);
    sub = need_tab(C, "BACKGROUND"); // :463
    SUB(BACKGROUND, L);
    SUB(BACKGROUND, D);
    SUB(BACKGROUND, C);
    self->C.BACKGROUND.contrast = (float)tnum(sub, "contrast");
#undef COL
#undef SUB
    // :470 G.C.HAND_LEVELS[0] = G.C.RED. note: a copy, as game.h says (the Lua's is RED itself; nothing changes either)
    memcpy(self->C.HAND_LEVELS[0], self->C.RED, sizeof(Colour));
    memcpy(self->C.UI_CHIPS, self->C.BLUE, sizeof(Colour)); // :471 copy_table(G.C.BLUE)
    memcpy(self->C.UI_MULT, self->C.RED, sizeof(Colour));   // :472 copy_table(G.C.RED)

    // :476 self.UIT: the enum UIT_x of ui.h
    self->handlist = need_tab(g, "handlist"); // :487
    // :501
    self->button_mapping = tnew();
    tset(self->button_mapping, "a", self->F_SWAP_AB_BUTTONS ? S("b") : NIL);
    tset(self->button_mapping, "b", self->F_SWAP_AB_BUTTONS ? S("a") : NIL);
    tset(self->button_mapping, "y", self->F_SWAP_XY_BUTTONS ? S("x") : NIL);
    tset(self->button_mapping, "x", self->F_SWAP_XY_BUTTONS ? S("y") : NIL);
    self->keybind_mapping = need_tab(g, "keybind_mapping"); // :507
}

// game.h SETTINGS_snap: reads the three values again
void SETTINGS_snap_fill(void) {
    struct SettingsSnap *s = &G->settings_snap;
    s->t = G->SETTINGS;
    s->GRAPHICS = ttab(s->t, "GRAPHICS");
    s->paused = tbool(s->t, "paused");
    s->reduced_motion = tbool(s->t, "reduced_motion");
    s->shadows_on = streq(tstr(s->GRAPHICS, "shadows"), "On");
    s->stamp = tstamp(s->t), s->gstamp = tstamp(s->GRAPHICS); // (last: the reads above write nothing)
}
