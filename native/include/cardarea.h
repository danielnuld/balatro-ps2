// cardarea.lua: CardArea, a place that holds cards and lays them out (the deck, the hand, the played cards, the
// discard pile, the jokers, the consumeables, the shop's rows, the booster's cards, the rows of the collection and
// of the menus). CardArea = Moveable:extend().
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below).
//
// How the Lua's shapes are kept:
// - self.cards and self.highlighted are Tables, arrays of O(Card *), 1-based exactly as the Lua: the rules index
//   them (G.hand.cards[i]), take #, insert, remove, sort (tsort: Lua 5.1's own unstable algorithm, the order of
//   equal cards matters) and shuffle them (pseudoshuffle), and other files hold the same table (G.play.cards is
//   passed as `cards` to the scoring). cards[1] is CardArea_card(area, 1); #cards is CardArea_n(area).
// - self.config is the Node's config table (moveable.node.config), with the Lua's keys, because the UI binds to it
//   by name (ref_table = self.config, ref_value = 'card_count' / 'card_limit', cardarea.lua:285) and CardArea:save
//   stores the table itself. Keys:
//     type              'deck' (default), 'hand', 'play', 'discard', 'joker', 'consumeable', 'shop', 'voucher',
//                       'title', 'title_2'
//     card_limit        number, default 52        highlighted_limit  number, default 5 (from the constructor's
//     temp_limit        number: max(#cards, card_limit), every update        key highlight_limit: another name)
//     card_count        number: #cards, every update
//     sort              'desc' (default), 'asc', 'suit desc', 'suit asc', 'order'
//     lr_padding        number, default 0.1 (never read)
//     real_card_limit   number or nil: change_size's running total, which may go below 0
//     last_poll_size    number or nil: update's memory for G.GAME.modifiers.minus_hand_size_per_X_dollar
//     and whatever the constructor's caller passed: card_w, highlight_limit, deck_height, thin_draw, draw_layers
//     (array of layer names), spread, view_deck, collection, bg_colour, negative_info...
// - Children, by name in moveable.node.children (O(UIBox *); absent as nil): area_uibox (the background and the
//   count), view_deck and peek_deck (G.deck only). CardArea_init replaces children by a new empty table.
// - The scratch table self.ARGS does not exist: self.ARGS.invisible_area_types is a constant test in
//   CardArea_draw; self.ARGS.draw_layers is the member draw_layers (it is computed once and kept).
// - `self == G.hand` and the like are pointer comparisons with the Game's members.
// - The accidental global `loading` (cardarea.lua:642, set around the Card constructor in CardArea:load) is read by
//   nothing: it does not exist in C.
#ifndef CARDAREA_H
#define CARDAREA_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- CardArea
// Inherited members the Lua writes on an area: states (init: drag.can, hover.can, click.can = false; update sets
// collide.can, hover.can, click.can = true on G.deck), T (move slides G.hand's and G.play's y), shadow_parrallax and
// layered_parallax (read by the cards), parent / container (set by the UI that holds the area).
//
// in dyn: (NOT members; tget(self->moveable.node.object.dyn, "..."))
//   nothing of cardarea.lua. Whatever another file hangs on an area lives there.
struct CardArea {
    Moveable moveable;
    Table *cards;        // self.cards: array of O(Card *), 1-based, in layout order (index 1 is the top of a deck and
                         // the left of a row); NULL as nil, which it is only after CardArea_remove
    Table *highlighted;  // self.highlighted: array of O(Card *), 1-based, in the order they were highlighted; never NULL
    float card_w;        // self.card_w: the width a card takes in the layout, game units (config.card_w or G.CARD_W)
    float shuffle_amt;   // self.shuffle_amt: 0 always (the deck's shuffle animation reads it; nothing sets it)
    Table *draw_layers;  // self.ARGS.draw_layers: array of layer names ('shadow', 'card') passed to Card_draw;
                         // NULL until the first CardArea_draw, which sets it to config.draw_layers or {'shadow', 'card'}
                         // and keeps it (it does not follow a later change of config, as in the Lua)
    // not the Lua's: config.type as last read, valid while config is this table with this stamp (core.h, tstamp).
    // Read with CardArea_type().
    struct { Table *config; uint64_t stamp; const char *type; } type_snap;
};
extern Class CardArea_class;

// self.config.type ('deck', 'hand', 'joker'...; NULL when it is not a string)
void CardArea_type_fill(CardArea *self);
static inline const char *CardArea_type(CardArea *self) {
    Table *config = self->moveable.node.config;
    if (self->type_snap.config != config || self->type_snap.stamp != tstamp(config)) CardArea_type_fill(self);
    return self->type_snap.type;
}
// #self.cards (0 for the nil of a removed area)
static inline int CardArea_n(CardArea *self) { return tlen(self->cards); }
// self.cards[i], 1-based; NULL as nil
static inline Card *CardArea_card(CardArea *self, int i) { return (Card *)tobji(self->cards, i); }
// #self.highlighted and self.highlighted[i], 1-based; NULL as nil
static inline int CardArea_n_highlighted(CardArea *self) { return tlen(self->highlighted); }
static inline Card *CardArea_highlighted(CardArea *self, int i) { return (Card *)tobji(self->highlighted, i); }
// self.children.<name> ('area_uibox', 'view_deck', 'peek_deck'), or NULL
#define CardArea_child(self, name) tobj((self)->moveable.node.children, name) // (a macro: a literal name is interned once per call site, core.h)

// cardarea.lua:5 CardArea(X, Y, W, H, config) is NEW(CardArea) + this. X, Y, W, H: game units. config: never NULL
// (the Lua indexes it); it BECOMES self.config (not a copy) and gets the defaults listed above written into it.
// Adds self to G.I.CARDAREA when the class is exactly CardArea.
void CardArea_init(CardArea *self, float X, float Y, float W, float H, Table *config);
// cardarea.lua:32 location: 'front' or NULL as nil (appended; a 'deck' area always inserts at 1).
// stay_flipped: false as nil. Flips the card face up unless stay_flipped or the area is a deck / discard.
void CardArea_emplace(CardArea *self, Card *card, const char *location, bool stay_flipped);
// cardarea.lua:66 card NULL as nil: the last card of a 'deck' / 'discard' area, else the first (among the cards with
// ability.discarded when discarded_only; false as nil). Returns the card (the argument even if it was not in the
// area), or NULL as nil: no card to take, or self.cards is nil.
Card *CardArea_remove_card(CardArea *self, Card *card, bool discarded_only);
// cardarea.lua:94 delta: cards, signed. Queues the event that changes config.real_card_limit / card_limit (and
// draws cards into G.hand); nothing when delta == 0.
void CardArea_change_size(CardArea *self, num delta);
// cardarea.lua:113 card is not used (may be NULL). Depends on config.type and on G.CONTROLLER.HID.controller.
bool CardArea_can_highlight(CardArea *self, Card *card);
// cardarea.lua:131 silent: false as nil (true: no sound).
void CardArea_add_to_highlighted(CardArea *self, Card *card, bool silent);
// cardarea.lua:162 Evaluates self.highlighted as a poker hand and shows it in the HUD; sets G.boss_throw_hand.
void CardArea_parse_highlighted(CardArea *self);
// cardarea.lua:187 force: false as nil (true: also a card with ability.forced_selection in G.hand). card NULL as
// nil is a Lua error unless the early return is taken (the Lua calls card:highlight(false)): ERROR.
void CardArea_remove_from_highlighted(CardArea *self, Card *card, bool force);
// cardarea.lua:201 Keeps the forced_selection cards of G.hand.
void CardArea_unhighlight_all(CardArea *self);
// cardarea.lua:214 Sets each card's rank (1-based position) and what can be dragged / collided.
void CardArea_set_ranks(CardArea *self);
// cardarea.lua:229 (Class.move) dt: seconds.
void CardArea_move(CardArea *self, float dt);
// cardarea.lua:243 (Class.update) dt: seconds (not used).
void CardArea_update(CardArea *self, float dt);
// cardarea.lua:270 (Class.draw) Builds children.area_uibox / view_deck / peek_deck on first need, then draws the
// cards once per layer of draw_layers.
void CardArea_draw(CardArea *self);
// cardarea.lua:410 Sets every card's T from the area's (per config.type), sorts self.cards by x (by ability.order
// for several vouchers) and renumbers the ranks.
void CardArea_align_cards(CardArea *self);
// cardarea.lua:554 (Class.hard_set_T) each of X, Y, W, H: NAN as nil (keeps self.T's value); CardArea:hard_set_T()
// with no arguments (game.lua:2384, state_events.lua:345, button_callbacks.lua:2919) is four NANs.
void CardArea_hard_set_T(CardArea *self, float X, float Y, float W, float H);
// cardarea.lua:565 card:hard_set_T() (four NANs: card.h) and card:calculate_parrallax() for every card, in pairs()
// order (the array's).
void CardArea_hard_set_cards(CardArea *self);
// cardarea.lua:572 _seed: the pseudoseed key; NULL as nil: 'shuffle'.
void CardArea_shuffle(CardArea *self, const char *_seed);
// cardarea.lua:577 method: one of config.sort's values, stored in config.sort; NULL as nil: config.sort as it is.
void CardArea_sort(CardArea *self, const char *method);
// cardarea.lua:592 Moves one card from `area` to self if there is room (always for G.deck and G.hand). area: any
// object; the Lua tests area:is(CardArea) (is_a) and does nothing otherwise. stay_flipped is NOT used: the Lua
// shadows it with the blind's answer (and G.GAME.modifiers.flipped_cards); pass false as nil. discarded_only: false
// as nil. Returns true when a card moved, false as nil.
bool CardArea_draw_card_from(CardArea *self, CardArea *area, bool stay_flipped, bool discarded_only);
// cardarea.lua:613 (Class.click) G.deck: G.FUNCS.deck_info().
void CardArea_click(CardArea *self);
// cardarea.lua:619 Returns {cards = {each card:save()}, config = self.config (the table itself)}, or NULL as nil
// when self.cards is nil.
Table *CardArea_save(CardArea *self);
// cardarea.lua:632 cardAreaTable: what save returned (never NULL). Replaces cards, children and config
// (self.config becomes cardAreaTable.config itself); highlighted is NOT cleared, as in the Lua.
void CardArea_load(CardArea *self, Table *cardAreaTable);
// cardarea.lua:657 (Class.remove) Removes its cards and children, sets cards to NULL, leaves G.I.CARDAREA.
void CardArea_remove(CardArea *self);
#endif
