// functions/state_events.lua: the steps of a round (start, play, discard, scoring, end, win), the moves of cards
// between the areas, the round's payout and the tutorial.
// The file defines no class: no structs here. Everything it touches is G.GAME / card.ability / contexts (tables)
// or objects of other headers.
//
// G.FUNCS of this file: the UI refers to two of them by string (config.button, button_listen):
// 'play_cards_from_highlighted' and 'discard_cards_from_highlighted'. state_events.c registers those two with
// funcs_register; discard takes a second argument, so it is registered through a static wrapper
// (UIElement *e) -> FUNCS_discard_cards_from_highlighted(e, false). The others are only called directly, with the
// arguments below; they are not registered.
#ifndef STATE_EVENTS_H
#define STATE_EVENTS_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- file-level state
// The Lua's GLOBALS mult and hand_chips (state_events.lua:615-616, never declared local): the running score of
// FUNCS_evaluate_play. Only state_events.lua reads them, but the event queued at :1031 reads them when it RUNS (not
// captured values), so they are real globals, not locals of evaluate_play. Both start at 0 (nil in the Lua until the
// first hand is scored; nothing reads them before).
extern num mult;
extern num hand_chips;

// ---------------------------------------------------------------- global functions
// state_events.lua:1. The run is won: unlocks, profile progress, the win overlay (queued), endless mode.
void win_game(void);
// state_events.lua:87. Queues the event that ends the round (game over or ROUND_EVAL).
void end_round(void);
// state_events.lua:290. Queues the event that resets the round's counters and sets the blind.
void new_round(void);

// ---------------------------------------------------------------- G.FUNCS
// state_events.lua:355. e: the number of cards to draw (blind.lua:389 passes 1), NIL as nil: the default
// min(#G.deck.cards, G.hand.config.card_limit - #G.hand.cards). Returns true only when it set GAME_OVER (no hand
// size and no cards: game.lua:3222 tests it); false as the Lua's nil otherwise.
bool FUNCS_draw_from_deck_to_hand(Val e);
// state_events.lua:379. e unused: NULL from blind.lua:482. hook: false as nil (the button), true from The Hook
// (blind.lua:482). In the pre_discard context set the key 'hook' only when hook is true (nil in the Lua otherwise).
void FUNCS_discard_cards_from_highlighted(UIElement *e, bool hook);
// state_events.lua:450. e unused (the play button).
void FUNCS_play_cards_from_highlighted(UIElement *e);
// state_events.lua:540. _cards: array of O(Card), never NULL. Returns text, the hand's key in G.GAME.hands ('NULL'
// if no hand matched). Out-pointers (each may be NULL when the caller does not want it), in the Lua's return order:
// loc_disp_text (localized display name), poker_hands (evaluate_poker_hand's table), scoring_hand (array of O(Card):
// the SAME table as poker_hands[text][1], which evaluate_play then modifies in place; a new empty table if no hand
// matched), disp_text (not localized: text, or 'Royal Flush').
const char *FUNCS_get_poker_hand_info(Table *_cards, const char **loc_disp_text, Table **poker_hands, Table **scoring_hand,
                                      const char **disp_text);
// state_events.lua:571. e unused: always called with no argument (NULL). Scores G.play.cards.
void FUNCS_evaluate_play(UIElement *e);
// state_events.lua:1088. e unused: NULL.
void FUNCS_draw_from_play_to_discard(UIElement *e);
// state_events.lua:1099. cards: array of O(Card), never NULL. (No caller in 1.0.1o.)
void FUNCS_draw_from_play_to_hand(Table *cards);
// state_events.lua:1108. e unused: NULL.
void FUNCS_draw_from_discard_to_deck(UIElement *e);
// state_events.lua:1121. e unused: NULL.
void FUNCS_draw_from_hand_to_deck(UIElement *e);
// state_events.lua:1128. e unused: NULL.
void FUNCS_draw_from_hand_to_discard(UIElement *e);
// state_events.lua:1135. The rows of the round's payout (add_round_eval_row).
void FUNCS_evaluate_round(void);
// state_events.lua:1210. Called every frame by Game_update while the tutorial is not complete (game.lua:2457).
void FUNCS_tutorial_controller(void);
// state_events.lua:1281. _part: 'small_blind', 'big_blind', 'first_hand', 'first_hand_2', 'first_hand_3',
// 'first_hand_4', 'second_hand', 'shop_1'; never NULL.
void FUNCS_tutorial_part(const char *_part);
#endif
