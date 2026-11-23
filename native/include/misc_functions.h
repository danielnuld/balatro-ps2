// functions/misc_functions.lua: the global helpers everything else calls: the seeded random functions, poker hand
// evaluation, colours, localization, number formatting, sound requests, profile statistics, saving the run.
// Implemented in src/misc_functions.c. The file defines no class: plain functions over Tables, objects and floats.
//
// Shapes shared with the other files (read before calling):
// - A colour is a float * to 4 floats (node.h). A colour kept in a Table (config.colour, vars.colours[i], a
//   blind's boss_colour, G.ARGS.LOC_COLOURS[k]) is P(float *). The functions that make a colour (HEX, mix_colours,
//   lighten, darken, adjust_alpha) return 4 new floats that are never freed, like a table: where the Lua's result
//   only goes to love.graphics.setColor in drawing code, pass a local Colour as the last argument and nothing is
//   allocated.
// - "array of O(Card)" is a Table whose 1..n are O(Card *): hands, G.jokers.cards as the rules see it, results.
// - Every random function works on num (double) and calls love.h's math_randomseed / math_random*: the same
//   arithmetic, in the same order, as the Lua. Do not use float anywhere near them.
// - The fields of G this file creates on first use or assigns: G.PREV_GARB, G.check, G.LOADING, G.DRAW_HASH,
//   G.E_SWITCH_POINT, G.SPLASH_VOL, G.PITCH_MOD, G.PROGRESS, G.DISCOVER_TALLIES, G.action, G.culled_table,
//   G.FILE_HANDLER, G.SAVED_GAME, G.playing_card, G.C.STAKES, and the scratch tables G.ARGS.play_sound, G.ARGS.push,
//   G.ARGS.score_intensity, G.ARGS.ambient_sounds, G.ARGS.save_run, G.ARGS.LOC_COLOURS, G.ARGS.bt. They are fields
//   of Game: members if game.h declares them, else in G's dyn table (docs/conventions.md). None is declared here.
// - misc_functions.lua:1892-2022 (PLAY_SOUND, STOP_AUDIO, SET_SFX, MODULATE, RESTART_MUSIC, AMBIENT, RESET_STATES)
//   is the main thread's copy of the sound manager for G.F_SOUND_THREAD = false, which is never taken: those seven
//   names are declared in io.h and defined in src/sound_manager.c ONLY. misc_functions.c must not define them, and
//   this header does not declare them again.
#ifndef MISC_FUNCTIONS_H
#define MISC_FUNCTIONS_H
#include "types.h"
#include "node.h"

// misc_functions.lua:5 screenmode: 'Windowed', 'Fullscreen', 'Borderless'; NULL as nil (G.SETTINGS.WINDOW.screenmode
// or 'Windowed'). display: 1-based; 0 as nil (G.SETTINGS.WINDOW.selcted_display [sic, always nil] or 1). Rebuilds
// G.SETTINGS.WINDOW.display_names and .DISPLAYS[i]; returns the 1-based index of the current resolution option.
int GET_DISPLAYINFO(const char *screenmode, int display);
// misc_functions.lua:63 label: NULL as nil (only with reset). type: 'update' or 'draw'. reset: nil as false. With
// G.F_ENABLE_PERF_OVERLAY false (always) it only does G.PREV_GARB = G.PREV_GARB or 0.
void timer_checkpoint(const char *label, const char *type, bool reset);
// misc_functions.lua:105 the loading bar, drawn and presented at once. _label is not used. _next: the text after
// "LOADING: " (only with G.F_VERBOSE and not _RELEASE_MODE). progress: 0..1; 0 as nil (0: no bar is filled).
void boot_timer(const char *_label, const char *_next, float progress);
// misc_functions.lua:129 t NULL as nil: returns a new table. Else every key of t is removed and t is returned.
Table *EMPTY(Table *t);
// misc_functions.lua:137 per*(max - min) + min as N(), or NIL when per or max is nil/false. min: NIL as nil (0).
// No caller in the game.
Val interp(Val per, Val max, Val min);
// misc_functions.lua:144 t: a table of O(node) (an array, named children, or both; never NULL). Empties the array
// part from the end, calling remove_all on each node's children and then its remove (CALL(v, remove)); then the
// same for the values under the other keys, which stay in t. A false or nil value in the array part is skipped.
void remove_all(Table *t);
// misc_functions.lua:161 the distance between two points, game units. note: the Lua takes two transforms and a
// third argument mid (add half the difference of w and h); the one caller (controller.lua:342) passes the cursors'
// {x, y} and no mid, so the points are Vec2 and mid does not exist.
float Vector_Dist(Vec2 trans1, Vec2 trans2);
// misc_functions.lua:167 no caller in the game
float Vector_Len(Vec2 trans1);
// misc_functions.lua:171 trans1 - trans2. No caller in the game.
Vec2 Vector_Sub(Vec2 trans1, Vec2 trans2);
// misc_functions.lua:175 the key under which t holds val (veq), the last one in iteration order; NIL as nil
Val get_index(Table *t, Val val);
// misc_functions.lua:185 the number of keys of any kind
int table_length(Table *t);
// misc_functions.lua:191 a NEW array with t's values in iteration order; t itself is not changed (the callers,
// game.lua:2610, button_callbacks.lua:2423, UI_definitions.lua:490, :3235, discard the result: keep them so)
Table *remove_nils(Table *t);
// misc_functions.lua:199 swaps t[i] and t[j] (keys of any type: tgetv/tsetv). t NULL, or i or j nil/false: nothing.
void SWAP(Table *t, Val i, Val j);
// misc_functions.lua:206 list: an array, shuffled in place (Fisher-Yates from the end with math.random(i)). If
// list[1] has a sort_id (an O(Card)) the array is first sorted by sort_id (tsort: Lua's own algorithm). seed: N() to
// call math.randomseed first, NIL as nil (the generator is left as it is).
void pseudoshuffle(Table *list, Val seed);
// misc_functions.lua:219 a new run's seed: 8 characters of random_string, seeded with G.CONTROLLER.cursor_hover
// (T.x, T.y, time); on stake >= 8 it looks for a seed whose first legendary joker has no gold sticker yet.
const char *generate_starting_seed(void);
// misc_functions.lua:248 the key of the first legendary joker the run seeded _key would give
const char *get_first_legendary(const char *_key);
// misc_functions.lua:253 returns _t[key] and, through key (NULL: not wanted), the key chosen (N() for an array,
// S() for a map). seed: N() to call math.randomseed first, NIL as nil. The pairs {k, v} of _t are sorted by
// v.sort_id when the first one collected is an O(Card), else by k ascending (numbers by value, strings by bytes),
// and math.random(#keys) picks one. An empty _t is an ERROR (the Lua indexes nil).
Val pseudorandom_element(Table *_t, Val seed, Val *key);
// misc_functions.lua:270 length characters of 1-9, A-N, P-Z. seed: N() to call math.randomseed first, NIL as nil.
const char *random_string(int length, Val seed);
// misc_functions.lua:279 [0, 1). From the last byte to the first, 1-based i:
// h = lmod((1.1239285023/h)*byte*M_PI + M_PI*i, 1), starting at 1; an empty string gives 1. str is never NULL.
num pseudohash(const char *str);
// misc_functions.lua:298 key 'seed': math.random(). predict_seed NULL as nil: advances G.GAME.pseudorandom[key]
// (made from pseudohash(key .. G.GAME.pseudorandom.seed) the first time) and returns
// (it + G.GAME.pseudorandom.hashed_seed)/2. predict_seed given: the same from that seed, touching no state.
// The step is fabs(strtod(fmt("%.13f", lmod(2.134453429141 + x*1.72431234, 1)))): the text round trip is needed.
num pseudoseed(const char *key, const char *predict_seed);
// misc_functions.lua:315 seed: S(key), which is first turned into pseudoseed(key), or N(seed). Then
// math.randomseed(seed) and, with min and max both given (N()), math.random(min, max) (an integer); with either
// NIL, math.random() in [0, 1). pseudorandom('lucky_mult') is pseudorandom(S("lucky_mult"), NIL, NIL).
num pseudorandom(Val seed, Val min, Val max);
// misc_functions.lua:322 debugging text of a table. indent: 0 as nil.
const char *tprint(Table *tbl, int indent);
// misc_functions.lua:351 e1.order < e2.order, for tables. Has the shape of core.h's Less (ctx is not used): tsort(t,
// sortingFunction, NULL). No caller in the game.
bool sortingFunction(Val e1, Val e2, void *ctx);
// misc_functions.lua:355 hex: 'RRGGBB' or 'RRGGBBAA'. A new colour (4 floats, 0..1; alpha 1 when not given).
float *HEX(const char *hex);
// misc_functions.lua:362 blind: a key of G.P_BLINDS, or 'Small', 'Big', 'Boss' (this ante's choice; G.C.BLACK once
// it is defeated or skipped); NULL as nil (''). Returns G.C.BLACK, the blind's boss_colour, or a NEW mixed colour
// for bl_small / bl_big: never NULL.
float *get_blind_main_colour(const char *blind);
// misc_functions.lua:376 hand: array of O(Card). Returns a new table: each of the 12 hand names ("Flush Five",
// "Flush House", "Five of a Kind", "Straight Flush", "Four of a Kind", "Full House", "Flush", "Straight",
// "Three of a Kind", "Two Pair", "Pair", "High Card") -> an array of arrays of O(Card) (empty when the hand is not
// there), and top -> the first non-empty one in that order (absent for an empty hand).
Table *evaluate_poker_hand(Table *hand);
// misc_functions.lua:522 {} or {array of the O(Card) of the first suit that has enough}
Table *get_flush(Table *hand);
// misc_functions.lua:548 {} or {array of the O(Card) of the straight}
Table *get_straight(Table *hand);
// misc_functions.lua:592 the groups of exactly num_ cards of the same id, highest id first. num_ is the Lua's num.
Table *get_X_same(num num_, Table *hand);
// misc_functions.lua:613 {{O(card of the highest nominal)}}, or {} for an empty hand
Table *get_highest(Table *hand);
// misc_functions.lua:623 G.DRAW_HASH = EMPTY(G.DRAW_HASH)
void reset_drawhash(void);
// misc_functions.lua:645 the Lua's incremental garbage collection. time_budget: seconds, 0 as nil (3e-4).
// memory_ceiling: MB, 0 as nil (300). There is no collector (docs/conventions.md, Memory): nothing to do, by design.
void nuGC(num time_budget, num memory_ceiling, bool disable_otherwise);
// misc_functions.lua:669 obj: a Node of any class (no cast needed), appended to G.DRAW_HASH; NULL as nil: nothing.
void add_to_drawhash(void *obj);
// misc_functions.lua:675 C1*proportionC1 + C2*(1 - proportionC1), each of the 4 channels. out is not in the Lua:
// NULL returns a new colour (never freed: for a colour that is kept); else the 4 floats are written there and out
// is returned (drawing code: ui.lua:788, :827).
float *mix_colours(const float *C1, const float *C2, float proportionC1, float *out);
// misc_functions.lua:684 min(_chips, max(G.GAME.dollars, 0)) with G.GAME.modifiers.chips_dollar_cap, else _chips
num mod_chips(num _chips);
// misc_functions.lua:691
num mod_mult(num _mult);
// misc_functions.lua:695 sound_code: a file of resources/sounds without '.ogg'; NULL as nil: nothing. per: pitch,
// < 0 as nil (the sound manager's default 1). vol: volume, < 0 as nil (1); 0 is a real value (silent). Fills
// G.ARGS.play_sound and gives it to io.h's sound_manager_request (the Lua's channel push).
// play_sound('tarot2', 0.76, 0.4) is play_sound("tarot2", 0.76f, 0.4f); play_sound('button') is
// play_sound("button", -1, -1).
void play_sound(const char *sound_code, float per, float vol);
// misc_functions.lua:719 once a frame (game.lua:2459): the music track, G.PITCH_MOD, G.SPLASH_VOL, the ambient
// volumes in G.SETTINGS.ambient_control; sends the 'modulate' request (sound_manager_request). dt: real seconds.
void modulate_sound(float dt);
// misc_functions.lua:782 how many cards of area.cards have base.suit == suit. No caller in the game.
int count_of_suit(CardArea *area, const char *suit);
// misc_functions.lua:792 pushes the transform that puts the origin at the top left corner of moveable's VT, one
// unit = one game unit times scale; the caller pops. moveable: a Moveable of any class (no cast needed). scale: never
// nil. rotate: radians added to VT.r; 0 as nil (the Lua then still calls rotate(VT.r + 0): the same picture).
// offset: NULL as nil. The fifth argument of sprite.lua:184 is ignored by the Lua: dropped.
void prep_draw(void *moveable, float scale, float rotate, const Vec2 *offset);
// misc_functions.lua:805 the bobbing arrow of a chosen UI element: 3 points (x, y, x, y, x, y) written to out, for
// love_graphics_polygon("fill", out, 3). vert: nil as false.
void get_chosen_triangle_from_rect(float x, float y, float w, float h, bool vert, float out[6]);
// misc_functions.lua:824 _T += delta
void point_translate(Vec2 *_T, Vec2 delta);
// misc_functions.lua:829 rotates _T around the origin: c = cos(angle + pi/2), s = sin(angle + pi/2);
// x' = -y*c + x*s, y' = y*s + x*c
void point_rotate(Vec2 *_T, float angle);
// misc_functions.lua:835 rgb towards white by percent, alpha kept. no_tab: NULL as nil/false: a new colour (never
// freed); else the Lua's no_tab = true (the 4 values go straight to setColor): written to no_tab's 4 floats,
// which is returned: love_graphics_setColorv(lighten(c, 0.2f, tmp)).
float *lighten(const float *colour, float percent, float *no_tab);
// misc_functions.lua:851 rgb towards black by percent, alpha kept. no_tab: as lighten.
float *darken(const float *colour, float percent, float *no_tab);
// misc_functions.lua:867 the colour with another alpha. no_tab: as lighten.
float *adjust_alpha(const float *colour, float new_alpha, float *no_tab);
// misc_functions.lua:883 the "no space" message over area, with card and the area's cards shaking; neither NULL
void alert_no_space(Card *card, CardArea *area);
// misc_functions.lua:903 a new array of O(Card): the cards of G.jokers and G.consumeables whose ability.name is
// name, without the debuffed ones unless non_debuff (nil as false). Never NULL: {} when there is none or no
// G.jokers yet. next(find_joker(x)) is !tempty(find_joker(x, false)).
Table *find_joker(const char *name, bool non_debuff);
// misc_functions.lua:919 the base chips of an ante's blind, by G.GAME.modifiers.scaling (nil, 1, 2, 3). ante < 1:
// 100. note: for any other scaling the Lua returns nil (the caller's arithmetic then fails): ERROR.
num get_blind_amount(num ante);
// misc_functions.lua:956 _num is the Lua's num: N() -> "1,234" / "12.5" / "1.234e12" from G.E_SWITCH_POINT (1e11)
// on; S() -> that string itself; NIL or false -> "". Never NULL.
const char *number_format(Val _num);
// misc_functions.lua:967 the text scale for a score. scale: never nil at any call. amt: anything but N() gives
// 0.7*scale.
float score_number_scale(float scale, Val amt);
// misc_functions.lua:979 docs/conventions.md: copy_table is tcopy (deep for tables, objects by reference)
static inline Table *copy_table(Table *O) { return tcopy(O); }
// misc_functions.lua:994 the HTTP manager's score upload: only with G.F_HTTP_SCORES (never): does nothing
void send_score(num _score);
// misc_functions.lua:1006 the same for the name
void send_name(void);
// misc_functions.lua:1017 score: a key of G.GAME.round_scores / the profile's high_scores ('hand', 'most_money',
// 'furthest_ante', 'furthest_round', 'win_streak', 'current_streak', 'boss_streak'). amt: every caller passes a
// number (the Lua returns at once for anything else).
void check_and_set_high_score(const char *score, num amt);
// misc_functions.lua:1034
void set_joker_usage(void);
// misc_functions.lua:1047
void set_joker_win(void);
// misc_functions.lua:1060 _center: a G.P_CENTERS entry. index true: N(the highest stake won with it), N(0) for
// none. index false as nil: S(G.sticker_map[that stake]), or NIL (none won, or no record).
Val get_joker_win_sticker(Table *_center, bool index);
// misc_functions.lua:1073
void set_joker_loss(void);
// misc_functions.lua:1085
void set_deck_usage(void);
// misc_functions.lua:1097
void set_deck_win(void);
// misc_functions.lua:1112
void set_challenge_unlock(void);
// misc_functions.lua:1136 _deck_key: a Back's center key: the highest stake won with it (0: none), and _w_low is
// not written. NULL as nil: the highest stake won with any deck, and through _w_low (NULL: not wanted) the Lua's
// second result: the lowest of the running maxima once every Back has a win, else 0.
num get_deck_win_stake(const char *_deck_key, num *_w_low);
// misc_functions.lua:1162 the sticker name of the highest stake won with that Back center; NULL as nil
const char *get_deck_win_sticker(Table *_center);
// misc_functions.lua:1173
void set_deck_loss(void);
// misc_functions.lua:1184
void set_consumeable_usage(Card *card);
// misc_functions.lua:1232
void set_voucher_usage(Card *card);
// misc_functions.lua:1243 hand: the poker hand's name ('Full House'); the key is the name without spaces
void set_hand_usage(const char *hand);
// misc_functions.lua:1259
void set_profile_progress(void);
// misc_functions.lua:1295
void set_discover_tallies(void);
// misc_functions.lua:1404
void stop_use(void);
// misc_functions.lua:1409
void dec_stop_use(int _depth);
// misc_functions.lua:1427 stat: a key of the profile's career_stats ('c_wins', 'c_cards_sold'...). mod: every
// caller passes a number.
void inc_career_stat(const char *stat, num mod);
// misc_functions.lua:1434 a copy of t, deep, where every object (V_OBJ) is the string "\"MANUAL_REPLACE\"" (with
// the quotes)
Table *recursive_table_cull(Table *t);
// misc_functions.lua:1446 action: {type = 'use_card', card = sort_id} (button_callbacks.lua:2172); never NULL
void save_with_action(Table *action);
// misc_functions.lua:1452 builds G.ARGS.save_run (= G.culled_table) and sets G.FILE_HANDLER.run / .update_queued;
// the file is written by Game_update. note: the Lua finds the card areas with pairs(G): here every CardArea field
// of G (members and dyn), under the field's Lua name ("hand", "deck", "jokers"...).
void save_run(void);
// misc_functions.lua:1487
void remove_save(void);
// misc_functions.lua:1493 _c: a colour name of the localization's {C:name} ('red', 'mult', 'attention'...); NULL
// as nil. _default: NULL as nil. Returns the G.C colour of that name, else _default, else G.C.UI.TEXT_DARK: never
// NULL, and not a copy (the G.C colours animate in place).
float *loc_colour(const char *_c, float *_default);
// misc_functions.lua:1521 parses G.localization once it is loaded: adds misc.v_dictionary_parsed,
// misc.v_text_parsed, tutorial_parsed, quips_parsed, and text_parsed / name_parsed / unlock_parsed to each entry of
// descriptions
void init_localization(void);
// misc_functions.lua:1580 cards: array of O(Card), or {B(true)} (card.lua:2481, :2600); passed on as context.cards
void playing_card_joker_effects(Table *cards);
// misc_functions.lua:1586
void convert_save_to_meta(void);
// misc_functions.lua:1625 control: {s = suit letter, r = rank letter, e = center key or nil, d = edition name or
// nil, g = seal or nil}
void card_from_control(Table *control);
// misc_functions.lua:1634 a localization line ("{C:mult}+#1#{} Mult") as an array of parts {strings = {S(text) or
// {S("1")} for a #1# variable, ...}, control = {C = S("mult"), ...}}. line is never NULL; "" returns NULL as nil.
Table *loc_parse_string(const char *line);

// misc_functions.lua:1666 utf8 = {pattern = '[%z\1-\127\194-\244][\128-\191]*'}: a character is a lead byte
// (1..127 or 194..244) and the continuation bytes (128..191) after it; any other byte is skipped.
// misc_functions.lua:1667 utf8.map(s, f, no_subs): f(i, c, b, ctx) for the i-th character (from 1): c its bytes as
// a string, b the 1-based position of its first byte. no_subs true (nil as false): c is NULL and len is given
// instead (the Lua passes the byte count in c's place); len is the byte count in both cases.
typedef void (*Utf8Fn)(int i, const char *c, int len, int b, void *ctx);
void utf8_map(const char *s, Utf8Fn f, void *ctx, bool no_subs);
// misc_functions.lua:1684 utf8.chars(s, no_subs) is a coroutine iterator: here as core.h's TIter.
// `for _, c in utf8.chars(v) do` is `for (Utf8Iter it = utf8_chars(v, false); utf8_next(&it);) { it.c ... }`
typedef struct Utf8Iter {
    const char *s; // the string walked (never NULL)
    int at;        // 0-based offset of the next byte to look at
    int i;         // the character's number, from 1 (the Lua's first loop variable)
    const char *c; // the character as a string (interned); NULL with no_subs
    int len;       // its byte count (with no_subs this is the Lua's second loop variable)
    int b;         // the 1-based position of its first byte
    bool no_subs;
} Utf8Iter;
Utf8Iter utf8_chars(const char *s, bool no_subs);
bool utf8_next(Utf8Iter *it);

// misc_functions.lua:1689 args: S(key) (or any non-table key: tgetv), or T({type=, key=, set=, vars=, nodes=,
// node=, shadow=, default_col=}). misc_cat: NULL as nil.
// - args not a table: G.localization.misc[misc_cat][args] if misc_cat names a category, else misc.dictionary[args];
//   S("ERROR") when absent. The value is S() for most keys and T() (an array of lines) for the 'ml_*' keys, the
//   categories 'poker_hand_descriptions', 'collabs'...: the caller knows which.
// - type 'raw_descriptions': T(array of S(line)), {} when the key is unknown.
// - type 'variable': S(text), or T(array of S()) for a multi-line entry; S("ERROR") when unknown.
// - type 'name_text': S(name) (T() for a name of several lines), S("ERROR") when the set or the key is unknown.
// - type 'name' or 'text': T(the first line's array of UI nodes), NIL when unknown or empty.
// - type 'other', 'descriptions', 'unlocks', 'tutorial', 'quips': appends one array of UI nodes per line to
//   args.nodes and returns NIL.
// vars: the array of values put in place of #n# (numbers are written as tostring does), plus vars.colours = array of
// P(float *) for {V:n}. node (types 'name', 'name_text' without set/key): O() of a Node whose config.center gives
// them. default_col: P(float *). The UI nodes are the Lua's tables ({n = G.UIT.T, config = {...}}) as ui.h keeps them.
// args NIL is the Lua's error: ERROR.
Val localize(Val args, const char *misc_cat);
// localize('k_no_space_ex') / localize(key, 'poker_hands') where the value is known to be a string: the string, or
// NULL if it is a table. Only a shorthand for vstr(localize(S(key), misc_cat)).
static inline const char *localize_s(const char *key, const char *misc_cat) {
    Val v = localize(S(key), misc_cat);
    return v.t == V_STR ? v.s : NULL;
}
// misc_functions.lua:1816 a new Sprite of the 'chips' atlas. _stake: 1..8; 0 as nil (1). _scale: <= 0 as nil (1).
// note: for stake 8 the Lua replaces the instance's draw (dissolve + voucher shaders, :1822): in C that sprite's
// cls is a Class private to misc_functions.c whose super is Sprite_class (is_a(s, &Sprite_class) holds) and whose
// draw is that function.
Sprite *get_stake_sprite(int _stake, float _scale);
// misc_functions.lua:1834 _front: a G.P_CARDS entry (never NULL). Returns its atlas (a collab's for a face card
// with G.SETTINGS.CUSTOM_DECK, else G.ASSET_ATLAS[_front.atlas] or 'cards_1' / 'cards_2') and through pos (never
// NULL) the {x=, y=} table of the sprite in it.
Atlas *get_front_spriteinfo(Table *_front, Table **pos);
// misc_functions.lua:1847 the colour of a stake, 1..8 (G.C.STAKES, made on first use); NULL as nil out of range
float *get_stake_col(int _stake);
// misc_functions.lua:1861 the 1-based index in G.CHALLENGES of the challenge with that id, or 0. _id never NULL.
int get_challenge_int_from_id(const char *_id);
// misc_functions.lua:1868 a new table: {dollars = 4, hand_size = 8, discards = 3, hands = 4, reroll_cost = 5,
// joker_slots = 5, ante_scaling = 1, consumable_slots = 2, no_faces = false, erratic_suits_and_ranks = false}
Table *get_starting_params(void);
// misc_functions.lua:1883 _challenge: a G.CHALLENGES entry, NULL as nil. _type: 'custom' or 'modifiers'. The value
// of the rule with that id, or NIL.
Val get_challenge_rule(Table *_challenge, const char *_type, const char *_id);
// misc_functions.lua:1892-2022 PLAY_SOUND, STOP_AUDIO, SET_SFX, MODULATE, RESTART_MUSIC, AMBIENT, RESET_STATES: see
// the note at the top (io.h).
#endif
