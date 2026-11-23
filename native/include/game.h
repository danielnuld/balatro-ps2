// game.lua, globals.lua, main.lua: the Game class (its one instance is the global G), every field of G, the
// constants Game:set_globals() gives it, and LÖVE's callbacks. Implemented in src/game*.c, src/globals.c, src/main.c.
//
// RULES OF THIS HEADER (docs/conventions.md applies; these are the points that are specific to G):
// - G.<name> is G-><name>: every field any file of the game reads or writes on G is a member of struct Game below,
//   with the exceptions listed in the "in dyn:" block after the struct. G.FUNCS.x and G.UIDEF.x are C functions
//   (FUNCS_x, UIDEF_x), G.UIT.x is the enum UIT_x of ui.h, G.STATES.x / G.STAGES.x are the enums STATES_x / STAGES_x.
// - A member that is a pointer is NULL where the Lua has nil. A bool member is false where the Lua has nil OR false
//   (no reader of those tells them apart). An int / float / num member documented "0 as nil" is 0 where the Lua has
//   nil; a Val member is NIL where the Lua has nil (used where 0 is a value the Lua tells from nil).
// - What the Lua keeps in plain tables stays a Table * with the Lua's keys: G.GAME.dollars is tnum(G->GAME, "dollars"),
//   G.SETTINGS.GRAPHICS.crt is tnum(ttab(G->SETTINGS, "GRAPHICS"), "crt"), G.P_CENTERS.j_joker is
//   ttab(G->P_CENTERS, "j_joker"), G.PROFILES[G.SETTINGS.profile] is ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")).
// - Colours: G.C.RED is G->C.RED, a Colour (float[4], r g b a), which is a float * wherever a colour is passed or
//   stored (in a table: P(G->C.RED)). G.C.RED[1] of the Lua is G->C.RED[0]. Lookups by a name computed at run time
//   have an accessor: G.C.SUITS[suit] is C_SUITS_get(suit), and so on (after the struct).
// - G.TIMERS[name] is *TIMERS_get(name); G.I[name] is I_get(name); G.ASSET_ATLAS[name] is ASSET_ATLAS_get(name);
//   G.ANIMATION_ATLAS[name] is ANIMATION_ATLAS_get(name); G.SHADERS[name] is SHADERS_get(name); G[name] for the
//   name of a card area is *Game_cardarea(G, name).
// - ref_table / ref_value pairs that point INTO A STRUCT OF G (the Lua's {ref_table = G.TIMERS, ref_value = 'REAL'}):
//     * in an Event config (event.h): .ref_num = &G->TIMERS.REAL (a num), .ref_float = &G->C.BACKGROUND.contrast,
//       a colour channel .ref_float = &G->C.BACKGROUND.L[0];
//     * in a Sprite draw step's send list (Sprite_define_draw_steps; game.lua:1303, 1396, 1412, 1548, 1739, 2282):
//       the entry keeps name and val as the Lua has them, and INSTEAD OF ref_table + ref_value it has
//       ref_num = P(&G->TIMERS.REAL) for a timer (a num), ref_float = P(&a float member) or ref_colour = P(G->C.BLUE) for a colour. An entry whose
//       ref_table is a real table (G.ARGS.spin, G.SANDBOX, the splash_args of main_menu) keeps ref_table = T(t) and
//       ref_value = S(key). Sprite_draw_shader's custom send reads, in this order: val, ref_float, ref_colour,
//       ref_table[ref_value].
//     * ref_table = G itself (UI_definitions.lua:6113, button_callbacks.lua:1863): see "in dyn:".
// - The top level of main.lua (:1-31) and globals.lua:522 (`G = Game()`) are main() of src/main.c: the platform's
//   love_init, the classes' init, G = NEW(Game) + Game_init(G), math_randomseed(G->SEED), then love_run().
#ifndef GAME_H
#define GAME_H
#include "types.h"
#include "node.h"

typedef struct Joystick Joystick; // LÖVE's joystick object (love.h); used only through pointers

// conf.lua:1-2, the two globals the game's files test (`not _RELEASE_MODE`, `_DEMO`)
#ifndef RELEASE_MODE
#define RELEASE_MODE true // _RELEASE_MODE
#endif
#ifndef DEMO_MODE
#define DEMO_MODE false // _DEMO
#endif

// ---------------------------------------------------------------- enums (globals.lua:284, :306)
// G.STATES.SHOP is STATES_SHOP. The values are the Lua's (they are saved in save.jkr and sent to the sound code).
// G.STATE is an int: it also takes -1 (Game_delete_run) and a number read from a save.
typedef enum {
    STATES_SELECTING_HAND = 1,
    STATES_HAND_PLAYED = 2,
    STATES_DRAW_TO_HAND = 3,
    STATES_GAME_OVER = 4,
    STATES_SHOP = 5,
    STATES_PLAY_TAROT = 6,
    STATES_BLIND_SELECT = 7,
    STATES_ROUND_EVAL = 8,
    STATES_TAROT_PACK = 9,
    STATES_PLANET_PACK = 10,
    STATES_MENU = 11,
    STATES_TUTORIAL = 12,
    STATES_SPLASH = 13, // the sound code depends on this value
    STATES_SANDBOX = 14,
    STATES_SPECTRAL_PACK = 15,
    STATES_DEMO_CTA = 16,
    STATES_STANDARD_PACK = 17,
    STATES_BUFFOON_PACK = 18,
    STATES_NEW_ROUND = 19
} GameState;
// G.STAGES.RUN is STAGES_RUN. G.STAGE is an int, 1..3: the index of G.STAGE_OBJECTS.
typedef enum { STAGES_MAIN_MENU = 1, STAGES_RUN = 2, STAGES_SANDBOX = 3 } GameStage;
// (G.UIT is the enum UIT_T ... of ui.h; G.UIT.padding is there too.)

// ---------------------------------------------------------------- Game (game.lua:2, globals.lua:7)
struct Game {
    Object object; // Game = Object:extend(). G->object.dyn holds the fields listed under "in dyn:" below

    // ================================================================ globals.lua: Game:set_globals()
    const char *VERSION; // '1.0.1o-FULL' (the Lua global VERSION is only this member's initial value)

    // ---- feature flags (globals.lua:13-120; the values are those of love_system_getOS())
    bool F_QUIT_BUTTON;            // the main menu has a Quit button
    bool F_SKIP_TUTORIAL;          // skip the tutorial on a fresh save
    bool F_BASIC_CREDITS;          // no reference to itch.io in the credits
    bool F_EXTERNAL_LINKS;         // links that open a browser are shown
    bool F_ENABLE_PERF_OVERLAY;    // timer_checkpoint records (debug)
    bool F_NO_SAVING;              // no run is saved
    bool F_MUTE;                   // every sound is muted
    bool F_SOUND_THREAD;           // the sound manager is used (always the path of the port: io.h)
    bool F_VIDEO_SETTINGS;         // the video settings tab exists
    bool F_CTA;                    // the demo's call to action
    bool F_VERBOSE;                // debug text
    bool F_HTTP_SCORES;            // HTTP high scores (not ported)
    float F_RUMBLE;                // rumble strength; 0 as nil (no rumble)
    bool F_CRASH_REPORTS;          // crash reports (not ported)
    bool F_NO_ERROR_HAND;          // love_errhand returns at once
    bool F_SWAP_AB_PIPS;           // the A and B button pips are swapped
    bool F_SWAP_AB_BUTTONS;        // the A and B buttons are swapped (button_mapping)
    bool F_SWAP_XY_BUTTONS;        // the X and Y buttons are swapped (button_mapping)
    bool F_NO_ACHIEVEMENTS;        // achievements are disabled
    Val F_DISP_USERNAME;           // NIL, B(true) (a name is awaited) or S(name)
    bool F_ENGLISH_ONLY;           // no language selection (nil and false are the same)
    bool F_GUIDE;                  // 'guide' replaces the back / select button
    bool F_JAN_CTA;                // the January demo's call to action
    bool F_HIDE_BG;                // the game's objects are not drawn under an overlay menu
    bool F_TROPHIES;               // 'trophy' wording for achievements
    bool F_PS4_PLAYSTATION_GLYPHS; // PS4 glyphs in place of PS5 ones
    bool F_LOCAL_CLIPBOARD;        // G.CLIPBOARD in place of the system's clipboard
    float F_SAVE_TIMER;            // seconds between two flushes of G.FILE_HANDLER (30, or 5)
    bool F_MOBILE_UI;              // bigger description text
    bool F_HIDE_BETA_LANGS;        // beta languages are not offered
    bool F_DISCORD;                // the Discord button (set only on Windows / OS X)
    bool F_STREAMER_EVENT;         // never set by the game (misc_functions.lua:995)
    bool F_DAILIES;                // never set by the game (UI_definitions.lua:5266)

    // ---- time (globals.lua:125-137)
    num SEED; // os_time() at start: math_randomseed(G->SEED) (main.lua:31)
    // G.TIMERS, seconds. G.TIMERS[name] is *TIMERS_get(name).
    // note: num, as the Lua's: they accumulate dt, and an event is due when `time + delay <= timer` (event.lua),
    // which as floats came a frame late whenever the timer crossed a power of two (and after hours the steps were
    // coarse). Code that only draws or animates takes one into a float local first ((float)G->TIMERS.REAL) and
    // computes in float: the PS2 has no double FPU.
    struct {
        num TOTAL;       // game time: advances by dt*SPEEDFACTOR, stopped while paused
        num REAL;        // real time since the stage started (reset by the splash screen and the main menu)
        num REAL_SHADER; // REAL, or 300 with SETTINGS.reduced_motion
        num UPTIME;      // real time since the start, never reset
        num BACKGROUND;  // the background shader's spin time
    } TIMERS;
    // not the Lua's: what per-frame code reads of G.SETTINGS and of SETTINGS.GRAPHICS, kept while SETTINGS is this
    // table and the two stamps (core.h, tstamp) are these. Read through SETTINGS_snap(), never directly.
    struct SettingsSnap {
        Table *t, *GRAPHICS;
        uint64_t stamp, gstamp;
        bool paused, reduced_motion, shadows_on; // SETTINGS.paused, .reduced_motion (truthy), GRAPHICS.shadows == 'On'
    } settings_snap;
    struct { int DRAW, MOVE; } FRAMES; // frame counters of Game_draw / Game_update (Node.FRAME is compared with them)
    // G.exp_times: this frame's smoothing factors of Moveable_move_* (Game_update sets them)
    struct { float xy, scale, r, max_vel; } exp_times;

    // ---- settings and profiles (globals.lua:141-264)
    Table *SETTINGS; // G.SETTINGS: the Lua's keys (COMP, DEMO, ACHIEVEMENTS_EARNED, language, GAMESPEED, paused,
                     // SOUND, WINDOW, CUSTOM_DECK, GRAPHICS, profile, tutorial_complete, QUEUED_CHANGE...); never NULL
    Table *COLLABS;  // {pos = {Jack, Queen, King -> {x, y}}, options = {suit -> array of collab names}}
    Table *METRICS;  // {cards = {used, bought, appeared}, decks = {chosen, win, lose}, bosses = {faced, win, lose}}
    Table *PROFILES; // array [1..3] of profile tables (Game_load_profile fills the defaults); never NULL

    // ---- render scale (globals.lua:269-279)
    float TILESIZE;         // 20: pixels of a game unit at TILESCALE 1
    float TILESCALE;        // 3.65 at start; love_resize sets it from the window
    float TILE_W;           // 20: the room's width in game units
    float TILE_H;           // 11.5: the room's height in game units
    float DRAW_HASH_BUFF;   // 2
    float CARD_W;           // 2.4*35/41 game units
    float CARD_H;           // 2.4*47/41 game units
    float HIGHLIGHT_H;      // 0.2*CARD_H: how far a highlighted card rises
    float COLLISION_BUFFER; // 0.05
    float PITCH_MOD;        // 1: the music's pitch, eased by modulate_sound

    // ---- game state (globals.lua:284-317)
    Table *STAGE_OBJECTS; // array [1..3] (a GameStage) of arrays of O(node): G.STAGE_OBJECTS[G.STAGE] is ttabi(G->STAGE_OBJECTS, G->STAGE)
    int STAGE;            // a GameStage
    int STATE;            // a GameState; -1 after Game_delete_run
    int TAROT_INTERRUPT;  // the GameState a consumeable was used in, while its use is shown; 0 as nil
    bool STATE_COMPLETE;  // the current state's entry code has run

    // ---- instances (globals.lua:322-338)
    // G.ARGS: the scratch tables the Lua shares by name, never NULL. G.ARGS.x is tget(G->ARGS, "x") for the keys
    // that live across calls or are read by another function: spin {amount, real, eased}, eased_cursor_pos {x, y,
    // sx, sy}, save_progress {UDA, SETTINGS, PROFILE}, save_settings, save_metrics, save_run, score_intensity,
    // play_sound, push, ambient_sounds, chip_flames, mult_flames, flame_handler, LOC_COLOURS, TEMP_POOL,
    // voucher_tag, set_alerts_alertables, HIGH_SCORE_RESPONSE, screenmode_vals, bt, FUNC_TRACKER,
    // hand_chip_total_UI_set. A key that is only one function's scratch (focus_list, focusables, focus_cursor_pos,
    // focus_vec, gamepad_patterns, event_manager_update) is a local of that function: its own header says so.
    Table *ARGS;
    // G.I: every live instance by kind, arrays of O(obj) in creation order, never NULL. G.I[name] is I_get(name).
    struct { Table *NODE, *MOVEABLE, *SPRITE, *UIBOX, *POPUP, *CARD, *CARDAREA, *ALERT; } I;
    Table *ANIMATION_ATLAS; // name -> P(Atlas *): "blind_chips", "shop_sign". Read with ANIMATION_ATLAS_get(name)
    Table *ASSET_ATLAS;     // name -> P(Atlas *): "cards_1", "centers", "Joker", "Tarot", "Planet" (the same Atlas as
                            // "Tarot"), "ui_1", "balatro", "collab_AU_1"... Read with ASSET_ATLAS_get(name)
    Table *MOVEABLES;       // array of O(moveable): everything Game_update moves and updates
    Table *ANIMATIONS;      // array of O(animated sprite): everything Game_update animates (holes removed each frame)
    Table *DRAW_HASH;       // array of O(node): what was drawn this frame, in order (reset_drawhash, add_to_drawhash)

    // ---- constants (globals.lua:343-348)
    float MIN_CLICK_DIST; // 0.9 game units
    num MIN_HOVER_TIME;   // 0.1 s (an event's delay: num)
    bool DEBUG;           // false: the debug code is not translated
    float ANIMATION_FPS;  // 10
    float VIBRATION;      // rumble asked for this frame; added to by events, consumed by update_canvas_juice
    num CHALLENGE_WINS;   // 5

    // ---- colours (globals.lua:353-472). G.C.X is G->C.X; a member is a float * when used.
    struct {
        Colour MULT, CHIPS, MONEY, XMULT, FILTER, BLUE, RED, GREEN, PALE_GREEN, ORANGE, IMPORTANT, GOLD, YELLOW, CLEAR, WHITE,
            PURPLE, BLACK, L_BLACK, GREY, CHANCE, JOKER_GREY, VOUCHER, BOOSTER;
        Colour EDITION;      // animated every frame (Game_update)
        Colour DARK_EDITION; // animated every frame (Game_update)
        Colour ETERNAL, PERISHABLE, RENTAL;
        struct { Colour MAIN, DARK, BOSS_MAIN, BOSS_DARK, BOSS_PALE; } DYN_UI; // eased in place (ease_colour)
        struct { Colour Hearts, Diamonds, Spades, Clubs; } SO_1; // suit colours, normal. G.C["SO_"..n][suit] is C_SO_get(n, suit)
        struct { Colour Hearts, Diamonds, Spades, Clubs; } SO_2; // suit colours, high contrast
        // G.C.SUITS: each member is THE SAME colour as one of SO_1 / SO_2, as in the Lua (game.lua:43,
        // button_callbacks.lua:1758): G->C.SUITS.Hearts = G->C.SO_1.Hearts. Before Game_start_up they point at
        // SUITS_init (globals.lua:402-407). G.C.SUITS[suit] is C_SUITS_get(suit).
        struct { float *Hearts, *Diamonds, *Spades, *Clubs; } SUITS;
        struct { Colour Hearts, Diamonds, Spades, Clubs; } SUITS_init; // the four literals of globals.lua:403-406
        struct {
            Colour TEXT_LIGHT, TEXT_DARK, TEXT_INACTIVE, BACKGROUND_LIGHT, BACKGROUND_WHITE, BACKGROUND_DARK, BACKGROUND_INACTIVE,
                OUTLINE_LIGHT, OUTLINE_LIGHT_TRANS, OUTLINE_DARK, TRANSPARENT_LIGHT, TRANSPARENT_DARK, HOVER;
        } UI; // (G.C.UI.RED, UI_definitions.lua:6311, does not exist in the Lua either: it is nil, NULL)
        struct { Colour Default, Enhanced, Joker, Tarot, Planet, Spectral, Voucher; } SET;                     // G.C.SET[set] is C_SET_get(set)
        struct { Colour Default, Enhanced, Joker, Tarot, Planet, Spectral, Voucher, Edition; } SECONDARY_SET; // C_SECONDARY_SET_get(set)
        Colour RARITY[5];      // G.C.RARITY[1..4] is G->C.RARITY[1..4] (index 0 unused)
        struct { Colour Small, Big, Boss, won; } BLIND; // G.C.BLIND[name] is C_BLIND_get(name)
        Colour HAND_LEVELS[8]; // G.C.HAND_LEVELS[0..7] is G->C.HAND_LEVELS[0..7]; [0] is a copy of RED (the Lua's is RED itself; nothing changes either)
        // G.C.BACKGROUND. pairs() over it visits L, D, C, contrast in this order (ease_background_colour)
        struct { Colour L, D, C; float contrast; } BACKGROUND;
        Colour UI_CHIPS;      // copy of BLUE, eased in place (back.lua:137, game.lua:2046)
        Colour UI_MULT;       // copy of RED, eased in place
        float *STAKES[9];     // G.C.STAKES[1..8]: get_stake_col fills it on first use with WHITE, RED, GREEN, BLACK, BLUE, PURPLE, ORANGE, GOLD; [1] NULL: G.C.STAKES is nil
        Colour UI_CHIPLICK;   // FUNCS_flame_handler: {1, 1, 1, 1} on first use (all zero: the Lua's nil), then channels 1..3 every frame
        Colour UI_MULTLICK;   // the same for mult
    } C;

    // ---- lists (globals.lua:487-519)
    Table *handlist;        // array of the 12 poker hand names, best first
    Table *button_mapping;  // {a =, b =, y =, x =}: a button name -> the name it is swapped with; empty without the F_SWAP flags
    Table *keybind_mapping; // {{a = 'dpleft', ...}}: an array holding one table, as the Lua has it (so G.keybind_mapping[key] is always nil)

    // ================================================================ game.lua: Game:start_up()
    Table *SOUND_MANAGER; // non-NULL once the sound manager is started (an empty table: there is no thread or
                          // channel). G.SOUND_MANAGER.channel:push(t) is sound_manager_request(t) (io.h)
    Table *SAVE_MANAGER;  // the same for saves: G.SAVE_MANAGER.channel:push(t) is save_manager_request(t) (io.h)
    Table *HTTP_MANAGER;  // an empty table; G.HTTP_MANAGER.in_channel:pop() is always nil (not ported)
    Table *SHADERS;       // shader name ("dissolve", "CRT", "splash"...) -> P(Shader *). Read with SHADERS_get(name)
    Controller *CONTROLLER;  // the input handler
    EventManager *E_MANAGER; // the event queues
    num SPEEDFACTOR;      // game speed of this frame: SETTINGS.GAMESPEED in a run (1 otherwise) plus the scoring acceleration
    Sprite *shared_debuff;              // the debuff cross drawn over cards (atlas "centers")
    Sprite *shared_soul;                // the soul stone
    Sprite *shared_undiscovered_joker;  // the "?" over an undiscovered joker
    Sprite *shared_undiscovered_tarot;  // the "?" over an undiscovered consumeable
    Sprite *shared_sticker_eternal, *shared_sticker_perishable, *shared_sticker_rental;
    Table *shared_stickers; // stake sticker name ('White', 'Red', 'Green', 'Black', 'Blue', 'Purple', 'Orange', 'Gold') -> O(Sprite *)
    Table *shared_seals;    // seal name ('Gold', 'Purple', 'Red', 'Blue') -> O(Sprite *)
    Table *sticker_map;     // array [1..8] of the stake sticker names, by stake
    bool STAGE_OBJECT_INTERRUPT; // true: the nodes created now are not added to G.STAGE_OBJECTS (Node_init)
    Sprite *CURSOR;         // the visible cursor (0.3 x 0.3, atlas 'gamepad_ui')

    // ================================================================ game.lua: Game:init_item_prototypes()
    // The prototype tables: key -> prototype table, the Lua's fields plus key (set here). Never NULL after it.
    Table *P_SEALS;   // 'Gold', 'Red', 'Blue', 'Purple'
    Table *P_TAGS;    // 'tag_uncommon'...
    Table *P_STAKES;  // 'stake_white'...
    Table *P_BLINDS;  // 'bl_small', 'bl_big', the bosses
    Table *P_CARDS;   // 'H_2' ... 'S_A' (and what the Lua reads as G.P_CARDS.empty: nil)
    Table *P_CENTERS; // jokers 'j_', consumeables 'c_', vouchers 'v_', backs 'b_', enhancements 'm_', editions 'e_', boosters 'p_', soul, undiscovered_joker, undiscovered_tarot
    Table *P_CENTER_POOLS;       // set name ('Joker', 'Tarot', 'Booster', 'Tag', 'Seal', 'Stake', 'Demo'...) -> sorted array of the same prototype tables
    Table *P_JOKER_RARITY_POOLS; // array [1..4] of arrays of joker prototypes
    Table *P_LOCKED;             // array of the locked j_ / b_ / v_ prototypes, by order
    // the placeholder prototypes of locked and undiscovered things
    Table *tag_undiscovered, *b_undiscovered, *j_locked, *v_locked, *c_locked, *j_undiscovered, *t_undiscovered, *p_undiscovered,
        *s_undiscovered, *v_undiscovered, *booster_undiscovered;

    // ================================================================ game.lua: Game:set_language()
    Table *LANGUAGES;    // language key ('en-us', 'de'... 'all1', 'all2') -> {font = a FONTS entry, label, key, button, warning, beta, omit}; NULL until the first call
    Table *FONTS;        // array [1..9] of {file, render_scale, TEXT_HEIGHT_SCALE, TEXT_OFFSET = {x, y}, FONTSCALE, squish, DESCSCALE, FONT = P(Font *)}; read FONT with font_FONT(t)
    Table *LANG;         // the LANGUAGES entry of SETTINGS.language (G.LANG.font is ttab(G->LANG, "font"))
    Table *localization; // what localization/<language> holds: {descriptions, misc...}

    // ================================================================ game.lua: Game:set_render_settings()
    Table *animation_atli; // array of {name, path, px, py, frames}
    Table *asset_atli;     // array of {name, path, px, py}
    Table *asset_images;   // array of {name, path, px, py}

    // ================================================================ game.lua: Game:init_window(), main.lua: love.resize()
    float ROOM_PADDING_H; // 0.7 game units
    float ROOM_PADDING_W; // 1 game unit
    // G.WINDOWTRANS: the window in game units (w, h) and in pixels (real_window_*: 0 until love_resize)
    struct { float x, y, w, h, real_window_w, real_window_h; } WINDOWTRANS;
    // G.window_prev: the window the scale was designed for, in pixels
    struct { float orig_scale, w, h, orig_ratio; } window_prev;
    struct { float x, y, r; } ROOM_ORIG; // G.ROOM.T's x, y, r as love_resize left them: update_canvas_juice shakes around it
    float CANV_SCALE;   // 1
    Canvas *CANVAS;     // what Game_draw draws to
    Canvas *AA_CANVAS;  // NULL always (main.lua:370 is dead code)
    num FPS_CAP;        // frames per second at most; 0 as nil (love_run makes it 500)

    // ================================================================ game.lua: Game:prep_stage()
    Table *GAME;          // G.GAME: the run, what Game_init_game_object returns and saves restore (blind = O(Blind *),
                          // selected_back = O(Back *), tags = array of O(Tag *)...); NULL before the first prep_stage
    Node *ROOM;           // the room every object is placed in. G.ROOM.jiggle is in its dyn: tnum(G->ROOM->object.dyn, "jiggle")
    Moveable *ROOM_ATTACH; // a moveable of the room's size, the major of the screen-attached UI

    // ================================================================ game.lua: the stages (splash, menu, run)
    Table *SANDBOX;       // the sandbox stage's state (debug); NULL
    Sprite *SPLASH_BACK;  // the background (shader 'splash' / 'background'); NULL
    Sprite *SPLASH_FRONT; // the white flash of the splash screen; NULL
    Sprite *SPLASH_LOGO;  // the title logo; NULL (its dissolve, dissolve_colours are in its dyn)
    float vortex_time;    // G.TIMERS.REAL when the splash vortex started; 0 as nil
    CardArea *title_top;  // the title screen's card area; NULL
    UIBox *MAIN_MENU_UI;  // the main menu's buttons; NULL
    UIBox *PROFILE_BUTTON; // the profile button of the main menu; NULL
    UIBox *GAME_OVER_UI;  // never created (only removed in Game_delete_run); NULL
    UIBox *online_leaderboard; // never created (only compared in Game_draw); NULL
    UIBox *collection_alert;   // the alert on the main menu's collection button; NULL
    bool REFRESH_ALERTS;  // set_alerts must rebuild the alerts
    bool debug_splash_size_toggle, debug_background_toggle, debug_UI_toggle, debug_tooltip_toggle; // debug keys; false
    UIBox *debug_tools;   // the debug panel; NULL
    Table *debug_tool_config; // its state; NULL

    // ---- the run's card areas (Game_start_run, UIDEF_shop, the create_UIBox_*_pack, the collection)
    // Game_cardarea(G, name) gives the address of each by name; G_CARDAREA_NAMES lists them.
    CardArea *consumeables; // the consumeable slots; NULL outside a run (as every member of this group)
    CardArea *jokers;       // the joker slots
    CardArea *discard;      // the discard pile
    CardArea *deck;         // the draw pile
    CardArea *hand;         // the hand
    CardArea *play;         // the cards being played
    CardArea *shop_jokers;  // the shop's cards
    CardArea *shop_vouchers; // the shop's voucher
    CardArea *shop_booster; // the shop's booster packs
    CardArea *pack_cards;   // the cards of an opened booster pack
    CardArea *shop_demo;    // never created by the game (only compared); NULL
    Val your_collection;    // the collection screens: T(array of O(CardArea *)) or, for two of them
                            // (UI_definitions.lua:3785, :4144), O(CardArea *); NIL before the first one
    Table *view_deck;       // never assigned by the game (cardarea.lua:411 only reads it); NULL
    Table *playing_cards;   // array of O(Card *): every playing card of the run's deck
    int playing_card;       // the highest Card.playing_card given so far; 0 as nil ((G.playing_card and G.playing_card + 1) or 1 is +1)
    int sort_id;            // the last Card.sort_id given; 0 as nil
    int ID;                 // the next Node.ID; 0 as nil (Node_init: G.ID = G.ID or 1)
    int tagid;              // the next Tag.ID; 0 as nil
    Table *load_shop_jokers, *load_shop_vouchers, *load_shop_booster; // a saved shop area waiting for the shop to exist (CardArea_load's argument); NULL

    // ---- the run's UI
    UIBox *HUD;             // the left panel; NULL outside a run
    UIBox *HUD_blind;       // the blind's box inside it
    Table *HUD_tags;        // array of O(UIBox *), one per tag held; NULL
    Table *hand_text_area;  // {chips, mult, ante, round, chip_total, handname, hand_level, game_chips, blind_chips, blind_spacer -> O(UIElement *)}
    UIBox *buttons;         // Play / Discard; NULL
    UIBox *deck_preview;    // the deck's summary shown while the deck is hovered; NULL
    UIBox *shop;            // NULL
    UIBox *SHOP_SIGN;       // NULL
    UIBox *blind_select;    // NULL
    Table *blind_select_opts; // {small, big, boss -> O(UIBox *)}; NULL
    UIBox *blind_prompt_box;  // NULL
    UIBox *booster_pack;    // NULL
    UIBox *round_eval;      // NULL (its alignment.offset.py is in its dyn)
    Particles *booster_pack_sparkles, *booster_pack_stars, *booster_pack_meteors; // NULL
    UIBox *boss_warning_text; // "this hand will not score"; NULL
    bool boss_throw_hand;   // the highlighted hand is debuffed by the boss (CardArea_parse_highlighted)
    // G.OVERLAY_MENU; NULL. FUNCS_overlay_menu sets it to `true` while it builds the box (button_callbacks.lua:1345)
    // and code run from the UIBox's init tests its truthiness: that value is OVERLAY_MENU_TRUE, which is non-NULL
    // and must never be dereferenced (the Lua would fail on it too).
    UIBox *OVERLAY_MENU;
    UIBox *OVERLAY_TUTORIAL; // NULL (Jimbo, content, highlights, step, step_complete, button_listen... are in its dyn)
    UIBox *achievement_notification; // NULL
    UIBox *screenwipe;      // the transition between stages; NULL (colours, children.particles are its own)
    Card *screenwipecard;   // the card shown in it; NULL
    float screenwipe_amt;   // eased towards 0.4 while a wipe shows (Game_draw); valid when screenwipe_amt_set
    bool screenwipe_amt_set; // false: G.screenwipe_amt is nil (the first Game_draw makes it 1)
    Card_Character *DT_jimbo; // the debug tools' Jimbo; NULL
    Card *sticker_card;     // the deck card of the run setup that shows the stake sticker; NULL
    Sprite *shared_shadow;  // scratch of Card_draw: the sprite whose shadow is being drawn
    const float *BRUTE_OVERLAY; // never set by the game (sprite.lua:133 reads it); NULL

    // ---- flow flags
    bool under_overlay;        // nodes created now are under the tutorial overlay (Node_init copies it)
    bool ALERT_ON_SCREEN;      // an alert was drawn this frame (Game_draw)
    bool RESET_BLIND_STATES;   // the localized blind states must be rebuilt (Game_update)
    const char *prev_small_state, *prev_large_state, *prev_boss_state; // G.GAME.round_resets.blind_states as of the last rebuild; NULL
    bool RESET_JIGGLES;        // jokers stop their attention jiggle
    bool VIEWING_DECK;         // the deck view overlay is open
    bool TAROT_INTERRUPT_PULSE; // the hand text is being pulsed by a consumeable
    bool NO_MOD_CURSOR_STACK;  // FUNCS_overlay_menu does not push a cursor layer
    bool jimboed;              // a Card_Character is on screen
    bool from_boss_tag;        // the boss reroll in progress is the Boss Tag's (free)
    bool normal_music_speed;   // the game over screen keeps the music's pitch
    const char *exception_queue; // the event queue FUNCS_wipe_on keeps ('other'); NULL
    const char *orbital_hand;  // the poker hand the Orbital Tag being created levels up; NULL
    const char *FORCE_TAG;     // debug: the key get_next_tag_key returns; NULL
    const char *FORCE_BOSS;    // debug: the key get_new_boss returns; NULL
    Val last_materialized;     // N(G.TIMERS.REAL) of the last materialize sound; NIL
    Table *card_area_focus_reset; // {area = O(CardArea *), rank = n}: written by state_events.lua:388, never read; NULL
    Table *action;             // the pending action save_run stores as ACTION; NULL
    Table *culled_table;       // the last table save_run built
    Table *SAVED_GAME;         // the unpacked save.jkr of the profile, when it has been read; NULL
    Table *FILE_HANDLER;       // {progress, settings, metrics, run, update_queued, force, last_sent_stage, last_sent_time, last_sent_pause}; NULL until the first save request
    Table *CHALLENGES;         // challenges.lua: array of challenge tables
    Table *DISCOVER_TALLIES;   // set_discover_tallies; NULL until then and after a profile change
    Table *PROGRESS;           // set_profile_progress; NULL until then and after a profile change
    Table *ACHIEVEMENTS;       // fetch_achievements; NULL until then
    Table *BADGE_COL;          // get_badge_colour's cache: key -> P(float *); NULL until then
    Table *collab_credits;     // UI_definitions.lua:4804; NULL until then
    Table *LOADING;            // boot_timer's state; NULL
    Table *check;              // timer_checkpoint's state (debug); NULL
    num PREV_GARB;             // timer_checkpoint (debug)
    Table *STEAM;              // NULL: no Steam
    Val prof;                  // the debug profiler; NIL
    const char *DEBUG_VALUE;   // debug text; NULL
    int MAJORS, MINORS;        // zeroed every Game_update (unused counters)
    bool fbf;                  // debug frame-by-frame mode; false
    bool new_frame;            // fbf: run one frame
    bool recording_mode;       // debug; false
    num ACC;                   // the scoring acceleration, 0..16 (accumulates dt: num)
    int ACC_state;             // G.STATE of the last Game_update; 0 as nil
    num real_dt;               // this frame's dt as love_update gave it, seconds (num: the timers add it up)
    int REFRESH_FRAME_MAJOR_CACHE; // > 0: Moveable_get_major does not use its frame cache (UIBox_recalculate nests); 0 as nil
    float DEADZONE;            // 0.2: the right stick's dead zone (Controller sets it)
    float CURR_VIBRATION;      // the rumble being applied, 0..1 (update_canvas_juice)
    float JIGGLE_VIBRATION;    // the room's jiggle as rumble
    Val SPLASH_VOL;            // N(the splash's volume factor); NIL until the first modulate_sound
    num E_SWITCH_POINT;        // from this number on, scores are shown in e-notation; 0 as nil (1e11 on first use)
    const char *CLIPBOARD;     // the local clipboard (F_LOCAL_CLIPBOARD); NULL

    // ---- menus
    int focused_profile;       // the profile shown in the profile menu, 1..3; 0 as nil
    bool CHECK_PROFILE_DATA;   // the focused profile has a profile.jkr (nil and false are the same)
    int viewed_stake;          // the stake shown in the run setup, 1..8; 0 as nil
    const char *viewed_collab; // the collab shown in the credits; NULL
    num forced_stake;          // the stake of the run being restarted; 0 as nil
    const char *forced_seed;   // the seed of the seeded run being restarted; NULL
    Table *challenge_tab;      // the challenge of the run being restarted; NULL
    int CHALLENGE_PAGE_SIZE;   // 10

    // ---- debug video tools (functions/test_functions.lua: not translated; the game's code only reads these)
    void *video;               // NULL
    Table *video_control;      // NULL
    const char *video_soundtrack; // NULL
    float video_volume, video_volume_real;
    Val video_organ;           // N(organ volume) or NIL (button_callbacks.lua:3010 sets 0.6; misc_functions.lua:747 reads it)
    bool screenglitch;
    UIBox *STRESS;             // NULL
};
// in dyn: (fields of G that are NOT members; they are in G->object.dyn)
//   setup_seed       string: the seed typed in the run setup. Bound by the UI with ref_table = G, ref_value =
//   run_setup_seed   bool: the "seeded run" toggle.          'setup_seed' / 'run_setup_seed', and an object's
//                    fields are bound through its dyn table: pass ref_table = O(G). Read them with G_setup_seed()
//                    and G_run_setup_seed(), write them with tsets / tsetb on G->object.dyn (nil: tset NIL).
//   your_collection_alert, your_collection_jokers_alert, your_collection_tarots_alert, your_collection_planets_alert,
//   your_collection_spectrals_alert, your_collection_vouchers_alert, your_collection_editions_alert,
//   your_collection_blinds_alert, your_collection_tags_alert, your_collection_seals_alert
//                    O(UIBox *): G[v.alert_uibox_name] of set_alerts (common_events.lua:692-732).
//   load_<name>      G['load_'..k] of Game_start_run (game.lua:2313) for a k other than shop_jokers, shop_vouchers,
//                    shop_booster (those three are members): never read.

#define OVERLAY_MENU_TRUE ((UIBox *)1) // G.OVERLAY_MENU = true (see the member)

// ---------------------------------------------------------------- accessors of G (tiny, inline)
// G.SETTINGS.paused, .reduced_motion and .GRAPHICS.shadows == 'On' as they are now: SETTINGS_snap()->paused...
// (the tables are read again only after something was written to them: Game.settings_snap)
void SETTINGS_snap_fill(void);
static inline const struct SettingsSnap *SETTINGS_snap(void) {
    struct SettingsSnap *s = &G->settings_snap;
    if (s->t != G->SETTINGS || s->stamp != tstamp(s->t) || s->gstamp != tstamp(s->GRAPHICS)) SETTINGS_snap_fill();
    return s;
}
// G.TIMERS[name]: 'TOTAL', 'REAL', 'REAL_SHADER', 'UPTIME', 'BACKGROUND'. NULL for another name (the Lua's nil).
static inline num *TIMERS_get(const char *name) {
    if (streq(name, "TOTAL")) return &G->TIMERS.TOTAL;
    if (streq(name, "REAL")) return &G->TIMERS.REAL;
    if (streq(name, "REAL_SHADER")) return &G->TIMERS.REAL_SHADER;
    if (streq(name, "UPTIME")) return &G->TIMERS.UPTIME;
    if (streq(name, "BACKGROUND")) return &G->TIMERS.BACKGROUND;
    return NULL;
}
// G.I[name]: 'NODE', 'MOVEABLE', 'SPRITE', 'UIBOX', 'POPUP', 'CARD', 'CARDAREA', 'ALERT'. NULL for another name.
static inline Table *I_get(const char *name) {
    if (streq(name, "NODE")) return G->I.NODE;
    if (streq(name, "MOVEABLE")) return G->I.MOVEABLE;
    if (streq(name, "SPRITE")) return G->I.SPRITE;
    if (streq(name, "UIBOX")) return G->I.UIBOX;
    if (streq(name, "POPUP")) return G->I.POPUP;
    if (streq(name, "CARD")) return G->I.CARD;
    if (streq(name, "CARDAREA")) return G->I.CARDAREA;
    if (streq(name, "ALERT")) return G->I.ALERT;
    return NULL;
}
// G.ASSET_ATLAS[name], G.ANIMATION_ATLAS[name], G.SHADERS[name]: NULL as nil (no such name, or name NULL)
static inline Atlas *ASSET_ATLAS_get(const char *name) { return name ? (Atlas *)tget(G->ASSET_ATLAS, name).p : NULL; }
static inline Atlas *ANIMATION_ATLAS_get(const char *name) { return name ? (Atlas *)tget(G->ANIMATION_ATLAS, name).p : NULL; }
// (a macro: a literal name is interned once per call site, core.h; name is written once more inside tget's test)
#define SHADERS_get(name) ({ const char *name_ = (name); name_ ? (Shader *)tget(G->SHADERS, name).p : (Shader *)NULL; })
// font.FONT of an entry of G.FONTS (G.LANG.font.FONT is font_FONT(ttab(G->LANG, "font"))); NULL if it has none
static inline Font *font_FONT(Table *font) { return (Font *)tget(font, "FONT").p; }
// G.C.SUITS[suit]: 'Hearts', 'Diamonds', 'Spades', 'Clubs'. NULL as nil.
static inline float *C_SUITS_get(const char *suit) {
    if (streq(suit, "Hearts")) return G->C.SUITS.Hearts;
    if (streq(suit, "Diamonds")) return G->C.SUITS.Diamonds;
    if (streq(suit, "Spades")) return G->C.SUITS.Spades;
    if (streq(suit, "Clubs")) return G->C.SUITS.Clubs;
    return NULL;
}
// G.C["SO_"..n][suit], n 1 (normal) or 2 (high contrast). NULL as nil.
static inline float *C_SO_get(int n, const char *suit) {
    if (streq(suit, "Hearts")) return n == 2 ? G->C.SO_2.Hearts : G->C.SO_1.Hearts;
    if (streq(suit, "Diamonds")) return n == 2 ? G->C.SO_2.Diamonds : G->C.SO_1.Diamonds;
    if (streq(suit, "Spades")) return n == 2 ? G->C.SO_2.Spades : G->C.SO_1.Spades;
    if (streq(suit, "Clubs")) return n == 2 ? G->C.SO_2.Clubs : G->C.SO_1.Clubs;
    return NULL;
}
// G.C.SET[set]: NULL as nil.
static inline float *C_SET_get(const char *set) {
    if (streq(set, "Default")) return G->C.SET.Default;
    if (streq(set, "Enhanced")) return G->C.SET.Enhanced;
    if (streq(set, "Joker")) return G->C.SET.Joker;
    if (streq(set, "Tarot")) return G->C.SET.Tarot;
    if (streq(set, "Planet")) return G->C.SET.Planet;
    if (streq(set, "Spectral")) return G->C.SET.Spectral;
    if (streq(set, "Voucher")) return G->C.SET.Voucher;
    return NULL;
}
// G.C.SECONDARY_SET[set]: NULL as nil.
static inline float *C_SECONDARY_SET_get(const char *set) {
    if (streq(set, "Default")) return G->C.SECONDARY_SET.Default;
    if (streq(set, "Enhanced")) return G->C.SECONDARY_SET.Enhanced;
    if (streq(set, "Joker")) return G->C.SECONDARY_SET.Joker;
    if (streq(set, "Tarot")) return G->C.SECONDARY_SET.Tarot;
    if (streq(set, "Planet")) return G->C.SECONDARY_SET.Planet;
    if (streq(set, "Spectral")) return G->C.SECONDARY_SET.Spectral;
    if (streq(set, "Voucher")) return G->C.SECONDARY_SET.Voucher;
    if (streq(set, "Edition")) return G->C.SECONDARY_SET.Edition;
    return NULL;
}
// G.C.BLIND[name]: 'Small', 'Big', 'Boss', 'won'. NULL as nil.
static inline float *C_BLIND_get(const char *name) {
    if (streq(name, "Small")) return G->C.BLIND.Small;
    if (streq(name, "Big")) return G->C.BLIND.Big;
    if (streq(name, "Boss")) return G->C.BLIND.Boss;
    if (streq(name, "won")) return G->C.BLIND.won;
    return NULL;
}
// The members of G that hold a CardArea, in the order the port visits them where the Lua scans G with pairs()
// (Game_delete_run game.lua:1174, save_run misc_functions.lua). note: the Lua's order is its hash order; this one
// is the order of creation. G.your_collection is one more when it holds a CardArea (is_a of its V_OBJ).
#define G_CARDAREA_COUNT 11
static const char *const G_CARDAREA_NAMES[G_CARDAREA_COUNT] __attribute__((unused)) = {
    "consumeables", "jokers", "discard", "deck", "hand", "play", "shop_jokers", "shop_vouchers", "shop_booster", "pack_cards", "shop_demo"};
// &G[name] for the name of a card area member (the names above and 'title_top'): *Game_cardarea(G, "hand") is
// G.hand, and it can be assigned. NULL when G has no card area member of that name (G[name] is then nil for a
// read; game.lua:2313 handles the write).
static inline CardArea **Game_cardarea(Game *self, const char *name) {
    if (streq(name, "consumeables")) return &self->consumeables;
    if (streq(name, "jokers")) return &self->jokers;
    if (streq(name, "discard")) return &self->discard;
    if (streq(name, "deck")) return &self->deck;
    if (streq(name, "hand")) return &self->hand;
    if (streq(name, "play")) return &self->play;
    if (streq(name, "shop_jokers")) return &self->shop_jokers;
    if (streq(name, "shop_vouchers")) return &self->shop_vouchers;
    if (streq(name, "shop_booster")) return &self->shop_booster;
    if (streq(name, "pack_cards")) return &self->pack_cards;
    if (streq(name, "shop_demo")) return &self->shop_demo;
    if (streq(name, "title_top")) return &self->title_top;
    return NULL;
}
// G.setup_seed and G.run_setup_seed (in dyn: see the block after the struct)
static inline const char *G_setup_seed(void) { return tstr(G->object.dyn, "setup_seed"); }  // NULL as nil
static inline bool G_run_setup_seed(void) { return tbool(G->object.dyn, "run_setup_seed"); } // false as nil

// os.time(): libc's time(NULL); on the host TIME=<n> in the environment fixes it (a deterministic run, as the
// oracle's TRACE does with 1700000000). Defined in src/main.c.
num os_time(void);
extern Class Game_class;
// (the instance: extern Game *G, in types.h. No other file-level state: the locals of love.run are statics of main.c.)

// ================================================================ game.lua
// game.lua:5 Game:init(). `G = Game()` (globals.lua:522) is G = NEW(Game); Game_init(G): it sets G = self first.
void Game_init(Game *self);
// game.lua:11 Game:start_up(). Once, from love_load.
void Game_start_up(Game *self);
// game.lua:216 Game:init_item_prototypes(). At start, and again after a profile or language change.
void Game_init_item_prototypes(Game *self);
// game.lua:845 Game:load_profile(_profile). _profile: 1..3; one that G.PROFILES does not have (0 as nil too) is 1.
void Game_load_profile(Game *self, int _profile);
// game.lua:927 Game:set_language()
void Game_set_language(Game *self);
// game.lua:998 Game:set_render_settings()
void Game_set_render_settings(Game *self);
// game.lua:1122 Game:init_window(reset). reset is not used by the Lua (its one caller passes nothing: false).
void Game_init_window(Game *self, bool reset);
// game.lua:1142 Game:delete_run()
void Game_delete_run(Game *self);
// game.lua:1193 Game:save_progress()
void Game_save_progress(Game *self);
// game.lua:1217 Game:save_notify(card). card: a prototype table (a G.P_CENTERS entry: its key is read); never NULL.
void Game_save_notify(Game *self, Table *card);
// game.lua:1225 Game:save_settings()
void Game_save_settings(Game *self);
// game.lua:1232 Game:save_metrics()
void Game_save_metrics(Game *self);
// game.lua:1239 Game:prep_stage(new_stage, new_state, new_game_obj). new_stage: a GameStage, 0 as nil
// (STAGES_MAIN_MENU). new_state: a GameState or the number of a save, 0 as nil (STATES_MENU). new_game_obj: false as nil.
void Game_prep_stage(Game *self, int new_stage, int new_state, bool new_game_obj);
// game.lua:1270 Game:sandbox(). Debug stage. The G.FUNCS it defines at :1315-1331 (col1change, col2change,
// edition_change, pulseme, spawn_joker, rem_joker, do_time, cb) are statics of its file, registered by name.
void Game_sandbox(Game *self);
// game.lua:1314 create_UIBox_sandbox_controls(), a global the Lua defines inside Game:sandbox. Returns the UI
// definition table (the ROOT node).
Table *create_UIBox_sandbox_controls(void);
// game.lua:1373 Game:splash_screen()
void Game_splash_screen(Game *self);
// game.lua:1447 make_splash_card(args), a global the Lua defines inside the splash screen's event. args: {scale =
// (default 1.5), card_pos = {x, y}, no_vortex =}, NULL as nil ({}). Returns the card; the second return value
// card_pos (the one given or the random one) through *card_pos if it is not NULL.
Card *make_splash_card(Table *args, Vec2 *card_pos);
// game.lua:1517 Game:main_menu(change_context). change_context: 'splash' (from the splash screen), 'game' (from a
// run), NULL as nil (anything else).
void Game_main_menu(Game *self, const char *change_context);
// game.lua:1716 Game:demo_cta(). Demo builds only.
void Game_demo_cta(Game *self);
// game.lua:1862 Game:init_game_object(). Returns a new G.GAME table.
Table *Game_init_game_object(Game *self);
// game_5.c: t's entries (string keys) in the order Lua 5.1's pairs walks a table that got them in t's order, from one
// constructor (true) or key by key from {} (false). For G.GAME.hands, whose pairs order the rules depend on.
Table *lua51_pairs_order(Table *t, bool constructor);
// game.lua:2018 Game:start_run(args). args: {savetext = the unpacked save table, stake = n, seed = string,
// challenge = a G.CHALLENGES entry}, every key optional; NULL as nil ({}).
void Game_start_run(Game *self, Table *args);
// game.lua:2449 Game:update(dt). dt: seconds, the smoothed frame time love_run computes.
void Game_update(Game *self, num dt);
// game.lua:2732 Game:draw()
void Game_draw(Game *self);
// game.lua:3006 Game:state_col(_state). Debug overlay. Returns r; g, b and a (always 1) through the pointers.
float Game_state_col(Game *self, num _state, float *g, float *b, float *a);
// game.lua:3010 Game:update_selecting_hand(dt). dt: seconds, 0 while paused (as for every update_* below).
void Game_update_selecting_hand(Game *self, float dt);
// game.lua:3072 Game:update_shop(dt)
void Game_update_shop(Game *self, float dt);
// game.lua:3183 Game:update_play_tarot(dt)
void Game_update_play_tarot(Game *self, float dt);
// game.lua:3187 Game:update_hand_played(dt)
void Game_update_hand_played(Game *self, float dt);
// game.lua:3208 Game:update_draw_to_hand(dt)
void Game_update_draw_to_hand(Game *self, float dt);
// game.lua:3248 Game:update_new_round(dt)
void Game_update_new_round(Game *self, float dt);
// game.lua:3258 Game:update_blind_select(dt)
void Game_update_blind_select(Game *self, float dt);
// game.lua:3302 Game:update_round_eval(dt)
void Game_update_round_eval(Game *self, float dt);
// game.lua:3341 Game:update_arcana_pack(dt)
void Game_update_arcana_pack(Game *self, float dt);
// game.lua:3392 Game:update_spectral_pack(dt)
void Game_update_spectral_pack(Game *self, float dt);
// game.lua:3443 Game:update_standard_pack(dt)
void Game_update_standard_pack(Game *self, float dt);
// game.lua:3492 Game:update_buffoon_pack(dt)
void Game_update_buffoon_pack(Game *self, float dt);
// game.lua:3528 Game:update_celestial_pack(dt)
void Game_update_celestial_pack(Game *self, float dt);
// game.lua:3581 Game:update_game_over(dt)
void Game_update_game_over(Game *self, float dt);
// game.lua:3628 Game:update_menu(dt). Empty in the Lua.
void Game_update_menu(Game *self, float dt);

// ================================================================ globals.lua
// globals.lua:7 Game:set_globals(). Called by Game_init. Gives every member of the "globals.lua" part of the struct
// its value (G.FUNCS = {} and the enums have nothing to set).
void Game_set_globals(Game *self);

// ================================================================ main.lua
// main.lua:33 love.run(). love_load(), a first love_timer_step(), then love_run_frame() until it returns true.
// Returns the exit code.
int love_run(void);
// main.lua:44-83, the function love.run returns: one frame (events, dt smoothing, love_update, love_draw and the
// present). Returns true when the game quits, with the Lua's return value (a or 0) in *exit_code if it is not NULL.
bool love_run_frame(int *exit_code);
// main.lua:86 love.load()
void love_load(void);
// main.lua:120 love.quit(). Returns false always (the Lua returns nothing): the quit goes ahead.
bool love_quit(void);
// main.lua:126 love.update(dt). dt: seconds.
void love_update(num dt);
// main.lua:132 love.draw()
void love_draw(void);
// main.lua:138 love.keypressed(key). key: LÖVE's key name.
void love_keypressed(const char *key);
// main.lua:146 love.keyreleased(key)
void love_keyreleased(const char *key);
// main.lua:154 love.gamepadpressed(joystick, button). button: LÖVE's gamepad button name ("a", "dpup"...).
void love_gamepadpressed(Joystick *joystick, const char *button);
// main.lua:161 love.gamepadreleased(joystick, button)
void love_gamepadreleased(Joystick *joystick, const char *button);
// main.lua:168 love.mousepressed(x, y, button, touch). x, y: pixels. button: 1 left, 2 right. touch: false as nil.
void love_mousepressed(float x, float y, int button, bool touch);
// main.lua:179 love.mousereleased(x, y, button)
void love_mousereleased(float x, float y, int button);
// main.lua:183 love.mousemoved(x, y, dx, dy, istouch). The Lua uses none of its arguments.
void love_mousemoved(float x, float y, float dx, float dy, bool istouch);
// main.lua:191 love.joystickaxis(joystick, axis, value). value: -1..1.
void love_joystickaxis(Joystick *joystick, int axis, float value);
// main.lua:198 love.errhand(msg). Returns at once with G.F_NO_ERROR_HAND; else shows msg until the game quits.
void love_errhand(const char *msg);
// main.lua:324 love.resize(w, h). w, h: the window in pixels. Also called by Game_prep_stage with the screen's size.
void love_resize(float w, float h);
#endif
