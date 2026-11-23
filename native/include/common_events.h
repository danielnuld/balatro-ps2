// functions/common_events.lua: the helpers the whole game calls: the eased HUD counters (chips, dollars, hands,
// discards, ante, round), the background colours, moving cards between areas, the scoring pop-ups, the round's payout
// rows, unlocks / achievements / discoveries, the pools and the creation of cards, the per-round joker targets, the
// boss choice, and the text of a card's tooltip (generate_card_ui). Implemented in src/common_events*.c.
//
// The file defines no class: no structs here. It has no file-level locals either: no extern state.
// (evaluate_poker_hand, get_flush, get_straight, get_X_same, get_highest, find_joker, pseudorandom..., localize,
// attention_text, play_sound, mix_colours, check_and_set_high_score, inc_career_stat, set_discover_tallies,
// get_deck_win_stake, EMPTY, copy_table are functions/misc_functions.lua: misc_functions.h, not here.)
//
// Conventions used by every prototype below:
//   - A "center" / "prototype" (an entry of G.P_CENTERS, G.P_TAGS, G.P_BLINDS, G.P_CARDS) is a Table *.
//   - A colour is the Lua's {r, g, b, a}: a float * to 4 floats (node.h Colour); inside a Table it is P(float *).
//   - A float argument documented "< 0: nil" takes any negative value as the Lua's nil and the callee applies the
//     default. Pass the Lua's own number otherwise, 0 included (0 is NOT the default).
//   - bool arguments are the Lua's truthiness (nil and false are both false), except where an int says otherwise.
//   - The closures of the Lua (the local _mod of ease_dollars / ease_discard / ease_hands_played, the event funcs)
//     are static functions of the .c, not declared here.
//
// The G.ARGS scratch tables of this file:
//   G.ARGS.TEMP_POOL (get_current_pool): private to common_events.c (a static Table *), see get_current_pool.
//   G.ARGS.set_alerts_alertables (set_alerts): private to common_events.c (built once, the first time).
//   G.ARGS.eased_cursor_pos {x, y, sx, sy} (update_canvas_juice): shared with Game:draw (game.lua:2937, which also
//     creates it if missing): it is where game.h puts it, not here.
#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- screen and HUD counters
// common_events.lua:1. Places the six run areas (STAGE RUN) or G.title_top (STAGE MAIN_MENU) and hard-sets their VT.
void set_screen_positions(void);
// common_events.lua:41. mod: the value G.GAME.chips is eased TO (not a delta); nil never passed (0 as the default).
// Queues an immediate event that queues the ease (floored) and juices 'chip_UI_count'.
void ease_chips(num mod);
// common_events.lua:68. mod: dollars to add (negative: to take); the Lua's nil is 0. instant: false as nil (the
// change is queued as an immediate event); true: applied now.
void ease_dollars(num mod, bool instant);
// common_events.lua:111. mod: discards to add (clamped to -discards_left); never nil (the Lua's math.max would
// fail). instant, silent: false as nil. silent: no 'chips2' sound.
void ease_discard(num mod, bool instant, bool silent);
// common_events.lua:152. mod: hands to add; the Lua's nil is 0. instant: false as nil.
void ease_hands_played(num mod, bool instant);
// common_events.lua:191. mod: antes to add; the Lua's nil is 0. Always queued.
void ease_ante(num mod);
// common_events.lua:224. mod: rounds to add; the Lua's nil is 0. Always queued.
void ease_round(num mod);

// ---------------------------------------------------------------- eases
// common_events.lua:258 ease_value(ref_table, ref_value, mod, floored, timer_type, not_blockable, delay, ease_type).
// Queues one 'ease' event (not blocking) that takes ref_table[ref_value] from its value now to that value + mod.
// ref_table + ref_value name a number: in C that is one of event.h's forms, so there are three functions:
//   ease_value    a Table and a string key (G->GAME, an object's dyn: ease_value(obj->dyn, "dissolve", ...))
//   ease_value_f  the address of a float: a struct member (&card->moveable.node.T->scale) or a colour channel
//                 (the Lua's colour[4] is &colour[3])
//   ease_value_n  the address of a num struct member
// The common arguments:
//   mod            the amount to add; the Lua's nil is 0.
//   floored        false as nil; true: the stored value is math.floor'ed.
//   timer_type     a key of G.TIMERS ('REAL', 'TOTAL', 'UPTIME'); NULL as nil (the Event's own default).
//   not_blockable  int, NOT bool, because the Lua computes blockable = (not_blockable == false): -1 for the Lua's
//                  nil, 1 for true (both: the event is NOT blockable), 0 for false (the event IS blockable; no
//                  caller passes it). A Lua `nil` here must be written -1, never 0.
//   delay          seconds (num: an event is due when time + delay <= timer, exactly as the Lua's doubles;
//                  write the literal as the Lua does, 0.2, never 0.2f); < 0: nil (0.3).
//   ease_type      'quad', 'elastic'; NULL as nil. The Lua passes it to Event as `type`, a key Event does not
//                  read, so it has no effect (event.h): keep the argument, do not set EventCfg.ease from it.
// The pointer given to _f / _n must stay valid while the event is queued (event.h, "Lifetime").
void ease_value(Table *ref_table, const char *ref_value, num mod, bool floored, const char *timer_type, int not_blockable,
                num delay, const char *ease_type);
void ease_value_f(float *ref, num mod, bool floored, const char *timer_type, int not_blockable, num delay,
                  const char *ease_type);
void ease_value_n(num *ref, num mod, bool floored, const char *timer_type, int not_blockable, num delay,
                  const char *ease_type);
// common_events.lua:276. args (never NULL): {new_colour = P(float *), special_colour = P(float *) or nil,
// tertiary_colour = P(float *) or nil, contrast = number or nil}. Eases G.C.BACKGROUND's L, D, C (rgb only) and
// contrast over 0.6 s. The Lua walks pairs(G.C.BACKGROUND): the order of the queued eases is the order of that
// table's keys (data.h: the order of the source, globals.lua).
void ease_background_colour(Table *args);
// common_events.lua:303. Eases the 4 channels of old_colour (changed in place) to new_colour on the REAL timer.
// delay: seconds; < 0: nil (ease_value's 0.3).
void ease_colour(float *old_colour, const float *new_colour, num delay);
// common_events.lua:311. state: a G.STATES value (STATES_SHOP...; int so that a number kept in a table, such as
// G.GAME.PACK_INTERRUPT, passes as is). blind_override: a blind's name ('Small Blind', 'The Hook'...), NULL as nil
// (G.GAME.blind's name is used); '' is 'Small Blind'.
void ease_background_colour_blind(int state, const char *blind_override);

// ---------------------------------------------------------------- queue helpers and cards on the move
// common_events.lua:362. Queues an empty 'after' event of `time` seconds. time < 0: nil (1). queue: NULL as nil
// ('base').
void delay(num time, const char *queue);
// common_events.lua:372. joker: a key of G.P_CENTERS (never NULL). edition: 'foil', 'holo', 'polychrome',
// 'negative'; NULL as nil (none). silent, eternal: false as nil. Returns the new card (never NULL).
Card *add_joker(const char *joker, const char *edition, bool silent, bool eternal);
// common_events.lua:386. Queues a 'before' event that moves one card from an area to another.
// from: NULL as nil (the card is not removed from anywhere). to: never NULL. percent: 0..100, the sound's pitch;
// < 0: nil (50). dir: 'up', 'down'; NULL as nil. sort: false as nil (to:sort() after). card: the card to move;
// NULL as nil (to:draw_card_from(from, stay_flipped, discarded_only): the top one). delay: seconds; < 0: nil
// (0.1). mute: false as nil. stay_flipped: false as nil (only used when card is NULL; with a card the blind
// decides). vol: < 0: nil (1). discarded_only: false as nil.
void draw_card(CardArea *from, CardArea *to, float percent, const char *dir, bool sort, Card *card, num delay, bool mute,
               bool stay_flipped, float vol, bool discarded_only);
// common_events.lua:426. card: never NULL. percent: 0..1; < 0: nil (0.5). dir: 'up' (highlight), 'down'
// (unhighlight); NULL as nil (highlight).
void highlight_card(Card *card, float percent, const char *dir);
// common_events.lua:445. text: never NULL. silent: false as nil. delay: seconds; < 0: nil (0.6); 0: the event is
// 'immediate' instead of 'before'.
void play_area_status_text(const char *text, bool silent, num delay);
// common_events.lua:464. card: what is juiced, NULL as nil (nothing): a Card, an AnimatedSprite (blind.lua:555) or
// a Tag (tag.lua:197: not a Moveable; juice it with its own Tag_juice_up after is_a(card, &Tag_class), the others
// with CALL(card, juice_up, 0.8f, N(0.5))). hand: a key of G.GAME.hands ('Flush'...; never NULL). instant: false
// as nil (the HUD animation is queued). amount: levels to add, may be negative; NIL as nil (1).
void level_up_hand(void *card, const char *hand, bool instant, Val amount);
// common_events.lua:495. config (never NULL): {immediate, nopulse, delay (nil: 0.8), sound, volume, pitch, modded}.
// vals (never NULL): {chips, mult, handname, chip_total, level, StatusText}; chips, mult and level are a number OR
// a string ('...', '+', '?', '', '+1'): read them as Val.
void update_hand_text(Table *config, Table *vals);
// common_events.lua:580. context: NULL as nil ({}); context.cardarea is O(CardArea). Returns a new table (never
// NULL) with the keys the card produced: chips, mult, x_mult, p_dollars, h_mult (numbers), jokers, edition, seals
// (tables).
Table *eval_card(Card *card, Table *context);

// ---------------------------------------------------------------- menu UI
// common_events.lua:658. Does nothing unless G.REFRESH_ALERTS. Creates / removes the '!' boxes of the collection
// (G[alert_uibox_name], G.collection_alert).
void set_alerts(void);
// common_events.lua:753. Creates G.MAIN_MENU_UI (and, queued, G.PROFILE_BUTTON).
void set_main_menu_UI(void);

// ---------------------------------------------------------------- scoring pop-ups and the payout
// common_events.lua:779. card: never NULL (extra.focus, O(Card), replaces it). eval_type: 'debuff', 'chips',
// 'mult', 'x_mult', 'h_x_mult', 'h_mult', 'dollars', 'swap', 'extra', 'jokers'. amt: the Lua's nil is 0 ('debuff',
// 'extra' and 'jokers' set it to 1 themselves; the others always get a number). percent: the sound's pitch; < 0:
// nil, which is 0.9 + 0.2*math.random(): the random call is made ONLY then. dir: 'down'; NULL as nil. extra: NULL
// as nil: {focus, colour = P(float *), message, delay, instant, no_juice, edition, mult_mod, Xmult_mod, chip_mod,
// swap, playing_cards_created}; required (never NULL) for 'extra' and 'jokers'.
void card_eval_status_text(Card *card, const char *eval_type, num amt, float percent, const char *dir, Table *extra);
// common_events.lua:927. config: NULL as nil ({}): {name ('blind1', 'hands', 'discards', 'interest', 'joker<i>',
// 'tag<i>', 'bottom'), dollars (nil: 1), disp, bonus, pitch, saved, card = O(Card), condition, pos, tag}.
void add_round_eval_row(Table *config);
// common_events.lua:1097. mod: slots to add (may be negative; never nil).
void change_shop_size(num mod);
// common_events.lua:1120. Queues card:juice_up(0.7). card: never NULL.
void juice_card(Card *card);
// common_events.lua:1127. Every frame (game.lua:2461): the screen shake of G.ROOM and the rumble. dt: seconds.
void update_canvas_juice(float dt);
// common_events.lua:1156. Queues (REAL timer, not blocking, not blockable) a juice of the card that repeats every
// 0.8 s while eval_func(card) is truthy. eval_func: a FN(); called with one argument, O(card). first: false as nil
// (it has no effect: the Lua tests `not first or first`). delay: seconds; < 0: nil (0.1).
void juice_card_until(Card *card, Val eval_func, bool first, num delay);

// ---------------------------------------------------------------- unlocks, achievements, discoveries
// common_events.lua:1163. args (never NULL; an empty table returns at once): {type = 'win', 'hand', 'money',
// 'career_stat', 'ante_up', 'chip_score', 'modify_deck', 'discover_amount', 'have_edition'..., and per type:
// statname, ante, handname, disp_text, scoring_hand, shattered, hand, level, chips, cards (arrays of O(Card)),
// amount, tarot_count, planet_count}.
void check_for_unlock(Table *args);
// common_events.lua:1628. card: a center (never NULL). Does nothing unless card.unlocked == false.
void unlock_card(Table *card);
// common_events.lua:1642. Builds G.ACHIEVEMENTS (once) and marks the ones in G.SETTINGS.ACHIEVEMENTS_EARNED. The
// Steam part is not translated.
void fetch_achievements(void);
// common_events.lua:1711. achievement_name: a key of G.ACHIEVEMENTS ('ante_up'...; never NULL). Queued on
// 'achievement'. The Steam part is not translated.
void unlock_achievement(const char *achievement_name);
// common_events.lua:1772. _achievement: an achievement's name or a center's key (never NULL). _type: the center's
// set ('Joker', 'Back', 'Voucher'...); NULL as nil ('achievement').
void notify_alert(const char *_achievement, const char *_type);
// common_events.lua:1840. Steam only: G.STEAM is nil, so the body is a comment and it returns at once. It exists
// because button_callbacks.lua:2537 calls it.
void inc_steam_stat(const char *stat_name);
// common_events.lua:1849. Reads <profile>/unlock_notify.jkr (one key per line), queues an overlay for each, removes
// the file.
void unlock_notify(void);
// common_events.lua:1859. key: a key of G.P_CENTERS (an unknown one is ignored; never NULL).
void create_unlock_overlay(const char *key);
// common_events.lua:1879. card: a center, a tag's or a blind's prototype; NULL as nil ({}: nothing lasting happens,
// but set_discover_tallies and the queued save still run unless the run is seeded or a challenge).
void discover_card(Table *card);

// ---------------------------------------------------------------- pools and the creation of cards
// common_events.lua:1895. _name: a center's name ('Red Deck'); NULL as nil. Returns the first center of
// G.P_CENTERS (in its iteration order) with that name, NULL as nil if none.
Table *get_deck_from_name(const char *_name);
// common_events.lua:1901. _from_tag: false as nil; true: the seed key is 'Voucher_fromtag'. Returns a key of
// G.P_CENTERS.
const char *get_next_voucher_key(bool _from_tag);
// common_events.lua:1914. append: added to the seed key; NULL as nil (''). Returns a key of G.P_TAGS
// (G.FORCE_TAG, a debug value nothing sets, is not translated).
const char *get_next_tag_key(const char *append);
// common_events.lua:1927. card_init: {front = a G.P_CARDS entry, center = a center}; NULL as nil ({}); a given table
// is WRITTEN (front and center are filled in when missing, front with a random call). area: NULL as nil (the card
// is placed at G.hand's position and not emplaced anywhere). skip_materialize, silent: false as nil. colours: array
// of P(float *) for start_materialize; NULL as nil. Returns the new card, already in G.playing_cards.
Card *create_playing_card(Table *card_init, CardArea *area, bool skip_materialize, bool silent, Table *colours);
// common_events.lua:1944. _key: the seed key; NULL as nil ('pack_generic'). _type: a booster's kind; NULL as nil
// (any). Returns a center of the set Booster (NULL as nil if the poll matched none).
// note: the first call of a run uses math.random(1, 2), not the seeded generator, as the Lua does.
Table *get_pack(const char *_key, const char *_type);
// common_events.lua:1963. Two returns: the pool, and through _pool_key (never NULL) the seed key.
// _type: 'Joker', 'Tarot', 'Planet', 'Spectral', 'Tarot_Planet', 'Voucher', 'Tag', 'Enhanced', 'Demo'... (a key of
// G.P_CENTER_POOLS; never NULL). _rarity: a number 0..1, used only for 'Joker'; NIL as nil: a pseudorandom call
// is made (0 is a value: no call). _legendary: false as nil. _append: NULL as nil ('').
// The pool is an array of S(key) and S('UNAVAILABLE'), the SAME table on every call (the Lua's G.ARGS.TEMP_POOL,
// emptied at the start): use it before the next call, do not keep it.
Table *get_current_pool(const char *_type, Val _rarity, bool _legendary, const char *_append, const char **_pool_key);
// common_events.lua:2055. _key: the seed key; NULL as nil ('edition_generic'). _mod: the rate's multiplier; < 0:
// nil (1). _no_neg, _guaranteed: false as nil. Returns a new {negative = true} / {polychrome = true} /
// {holo = true} / {foil = true}, or NULL as nil (no edition).
Table *poll_edition(const char *_key, num _mod, bool _no_neg, bool _guaranteed);
// common_events.lua:2082. _type: as get_current_pool's, or 'Base' (never NULL). area: NULL as nil (G.jokers); the
// card is created at the area's position but NOT emplaced. legendary: false as nil. _rarity: as get_current_pool's
// (NIL as nil). skip_materialize, soulable: false as nil. forced_key: a key of G.P_CENTERS; NULL as nil.
// key_append: NULL as nil (''). Returns the new card (never NULL).
Card *create_card(const char *_type, CardArea *area, bool legendary, Val _rarity, bool skip_materialize, bool soulable,
                  const char *forced_key, const char *key_append);
// common_events.lua:2156. other: the card copied (never NULL). new_card: the card to overwrite; NULL as nil (a new
// one is created at other's position). card_scale: the new card's size factor; < 0: nil (1). playing_card: the
// number for params.playing_card; NIL as nil. strip_edition: false as nil. Returns new_card or the new card.
Card *copy_card(Card *other, Card *new_card, float card_scale, Val playing_card, bool strip_edition);

// ---------------------------------------------------------------- tutorial
// common_events.lua:2183. args (never NULL; WRITTEN: args.highlight): {text_key, align, step, attach, pos, button,
// highlight (array of O(node), or a FN() returning one), loc_vars, hard_set, content (FN()), content_config,
// button_listen, no_button, snap_to (FN())}. Returns step + 1 (step is args.step or 1).
num tutorial_info(Table *args);

// ---------------------------------------------------------------- per-round values
// common_events.lua:2263. skip_increment: false as nil (the cost increase grows by 1).
void calculate_reroll_cost(bool skip_increment);
// common_events.lua:2271. G.GAME.current_round.idol_card {rank, suit, id}.
void reset_idol_card(void);
// common_events.lua:2288. G.GAME.current_round.mail_card {rank, id}.
void reset_mail_rank(void);
// common_events.lua:2303. G.GAME.current_round.ancient_card.suit.
void reset_ancient_card(void);
// common_events.lua:2312. G.GAME.current_round.castle_card.suit.
void reset_castle_card(void);
// common_events.lua:2326. G.GAME.round_resets.blind_states; a new boss once the last one is 'Defeated'.
void reset_blinds(void);
// common_events.lua:2338. Returns a key of G.P_BLINDS ('bl_hook'...), and counts it in G.GAME.bosses_used.
// (G.FORCE_BOSS, a debug value nothing sets, is not translated.)
// note: three loops of the Lua use pairs() on hash tables (G.P_BLINDS, G.GAME.banned_keys, G.GAME.bosses_used):
// pseudorandom_element sorts its candidates itself, so the result does not depend on that order.
const char *get_new_boss(void);

// ---------------------------------------------------------------- tooltips
// common_events.lua:2385. _c: a center, or a card's config when it has none (never NULL). card: NULL as nil.
// Returns a colour: one of G.C's, a new one (mix_colours) or the constant {0, 1, 1, 1}; never NULL. Not to be
// written through.
float *get_type_colour(Table *_c, Card *card);
// common_events.lua:2397. Builds the description of a card for its hover box. Returns full_UI_table, or the new one
// made when it is NULL: {main = array of lines, info = array of arrays of lines (each with a .name string),
// type = {}, name = array of nodes OR true, badges = array of strings, card_type = string}.
//   _c             a center, a tag's prototype, or {key =, set =, config =, vars =} for an info box (never NULL).
//   full_UI_table  NULL as nil: the first pass. A table: _c is appended to its info (the recursive calls).
//   specific_vars  the loc_vars of Card:generate_UIBox_ability_table (an array of values with keys such as
//                  playing_card, value, suit, colour, nominal_chips, bonus_chips, debuffed, pinned, sticker,
//                  no_name, not_hidden, perish_tally); NULL as nil.
//   card_type      'Locked', 'Undiscovered', 'Default', 'Enhanced', 'Booster', 'Joker', 'Tag'...; NULL as nil
//                  (_c.set).
//   badges         array of strings ('foil', 'gold_seal', 'eternal'...), kept in the result as its badges; NULL
//                  as nil ({}).
//   hide_desc      false as nil.
//   main_start     a line (array of UI node definitions) put before the description; NULL as nil.
//   main_end       the same, after it; NULL as nil.
Table *generate_card_ui(Table *_c, Table *full_UI_table, Table *specific_vars, const char *card_type, Table *badges,
                        bool hide_desc, Table *main_start, Table *main_end);
#endif
