// card.lua: Card, everything that looks like a card (playing cards, jokers, consumeables, vouchers, boosters,
// the cards of the collection and of the menus). Card = Moveable:extend().
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below). The scratch table self.ARGS does not exist: use locals
// (self.ARGS.send_to_shader is a local {a, b} pair built in Card_draw).
#ifndef CARD_HEADER_H
#define CARD_HEADER_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- Card
// Tables of a card (same keys as the Lua, read with tget/tnum/tstr...):
//   ability  self.ability: rebuilt by every set_ability. name, effect, set, mult, h_mult, h_x_mult, h_dollars,
//            p_dollars, t_mult, t_chips, x_mult, h_size, d_size, extra (nil, a number or a table: a deep copy of
//            center.config.extra), extra_value, type, order, forced_selection, perma_bonus, bonus, consumeable (the
//            center's config table itself, not a copy), hands_played_at_create, eternal, perishable, perish_tally,
//            rental, couponed, perma_debuff, queue_negative_removal, wheel_flipped, money, mod_num (in consumeable),
//            blueprint_compat, blueprint_compat_ui, blueprint_compat_check, and each joker's own counters
//            (invis_rounds, to_do_poker_hand, caino_xmult, yorick_discards, burnt_hand, loyalty_remaining,
//            steel_tally, stone_tally, nine_tally, driver_tally...).
//   base     self.base: rebuilt by every set_base. name, suit, value, nominal, suit_nominal, suit_nominal_original,
//            face_nominal, id, colour (P(float *) of G.C.SUITS[suit], or nil), times_played, original_value.
//   edition  self.edition: NULL as nil, else {type=, holo/foil/polychrome/negative = true, mult/chips/x_mult}.
//   config   self.config is the Node's config table (moveable.node.config): card (a G.P_CARDS entry or {}),
//            center (a G.P_CENTERS entry), card_key, center_key, h_popup, h_popup_config.
//   params   the constructor's params or {}: playing_card, viewed_back, bypass_discovery_center,
//            bypass_discovery_ui, bypass_lock, discover, bypass_back.
// Children, by name in moveable.node.children (O(obj); absent as nil; use Card_child):
//   shadow (Moveable), center, back (Sprite: always there after init), front (Sprite, only with a non-empty
//   config.card), floating_sprite (Sprite, centers with soul_pos), particles (Particles), alert, use_button,
//   sell_button, price, buy_button, buy_and_use_button, focused_ui, h_popup, top_disp, bot_disp (UIBox).
// Inherited members the Lua writes on a card: self.CT (init: &VT), self.zoom, self.mouse_damping (1.5 for a
// Booster), self.shadow_height, self.juice, self.pinch, self.parent (the area), self.layered_parallax
// (set_card_area copies the area's; remove_from_area sets {0, 0}), self.REMOVED: all in Moveable / Node.
//
// in dyn: (NOT members; tget(self->moveable.node.object.dyn, "..."))
//   getting_sliced (bool), shattered (bool), destroyed (bool), removed (bool, lower case: Card_remove sets it;
//   REMOVED is the Node's), opening (bool), shop_voucher (bool), vampired (bool), lucky_trigger (true or nil),
//   ability_UIBox_table (table), highligted (the Lua's typo read by Card:save: always nil), and whatever else
//   another file hangs on a card.
struct Card {
    Moveable moveable;
    Table *params;         // self.params; never NULL
    Table *ability;        // self.ability; never NULL after init
    Table *base;           // self.base; NULL only before the first set_base (set_ability runs first in init)
    Table *edition;        // self.edition; NULL as nil
    const char *seal;      // self.seal: 'Gold', 'Red', 'Blue', 'Purple'; NULL as nil
    CardArea *area;        // self.area; NULL as nil
    const char *facing;         // 'front' or 'back'
    const char *sprite_facing;  // 'front' or 'back': the side drawn (follows facing when the flip pinch closes)
    const char *flipping;       // NULL, 'f2b' or 'b2f' (never reset to nil once set)
    bool highlighted;
    bool debuff;           // self.debuff (never nil in practice: see Card_set_debuff)
    bool added_to_deck;    // nil and false are both false (the Lua only tests truthiness)
    bool pinned;           // self.pinned
    bool greyed;           // self.greyed: drawn with the 'played' shader, no shadow
    bool vortex;           // self.vortex: the splash screen's cards
    bool no_shadow;        // self.no_shadow
    bool no_ui;            // self.no_ui: no hover popup
    bool bypass_discovery_center, bypass_discovery_ui, bypass_lock; // copies of params, forced true in G.jokers / G.consumeables
    int rank;              // self.rank: 1-based position in its area (the area sets it); 0 as nil
    int playing_card;      // self.playing_card: 1-based index in G.playing_cards; 0 as nil (only truthiness and < are used)
    int sort_id;           // creation order: G.sort_id at init
    const char *back;      // 'viewed_back' or 'selected_back': the key of G.GAME whose Back gives the back sprite
    const char *label;     // self.label
    const char *sticker;       // self.sticker: a stake sticker name (key of G.shared_stickers); NULL as nil
    const char *sticker_run;   // self.sticker_run: the same for this run's stake, or 'NONE'; NULL as nil (not computed yet)
    num base_cost;         // center.cost or 1 (0 before set_ability)
    num extra_cost;        // inflation + the edition's surcharge
    num cost;              // buy price, dollars
    num sell_cost;         // sell price, dollars
    Val sell_cost_label;   // N(sell_cost), or S("?") while facing == 'back' (the UI reads it by name: Card_field)
    num unique_val;        // 1 - ID/1603301: the tie-breaker of get_nominal (needs double)
    struct { float mx, my, dx, dy, amt; } tilt_var; // the point the card tilts towards (pixels) and how much; never nil
    float ambient_tilt;    // 0.2 by default; never nil
    float hover_tilt;      // set to 1 by every draw (Hologram multiplies it while drawing)
    float dissolve;        // 0 = whole .. 1 = gone; 0 as nil (the 'ease' events of explode/shatter/start_* drive it)
    Table *dissolve_colours;   // array of P(float *) colours (burn_colour_1, _2 and the particles' colours); NULL as nil
    struct { float r, x, y; } discard_pos; // random scatter in the discard pile
    float click_timeout;   // 0.3
    Colour back_overlay;   // self.back_overlay: the storage of the darkened deck-back colour that Card_draw fills
    Table *eligible_strength_jokers;    // The Wheel of Fortune: array of O(Card), refilled every update; NULL as nil
    Table *eligible_editionless_jokers; // Ectoplasm / Hex: the same
    Table *send_to_shader; // self.ARGS.send_to_shader: {tilt, G.TIMERS.REAL}, made and refilled by a 'shadow' pass of
                           // Card_draw and read by its 'card' pass; NULL as nil. A member, not a dyn field: it is read every frame.
    // not the Lua's: what update, move and draw read every frame of self.children, self.ability, self.config.center,
    // kept while those tables' stamps (core.h, tstamp) are these. Read through Card_kids(), Card_ab(), Card_cen() at
    // every use (two or three compares), never across a call that may write the table.
    struct CardKids { // self.children.<name>: the object, NULL as nil (Card_child's values)
        Table *t; uint64_t stamp;
        void *center, *front, *back, *focused_ui, *particles, *price, *buy_button, *buy_and_use_button, *use_button,
             *floating_sprite, *h_popup, *alert;
    } kids;
    struct CardAb { // self.ability.<key>
        Table *t; uint64_t stamp;
        const char *name, *set, *effect; // tstr: NULL when not a string
        Table *consumeable;              // ttab: NULL when not a table
        bool has_consumeable, eternal, rental, perishable, perma_debuff; // truthy
    } ab;
    struct CardCen { // self.config.center and its keys
        Table *config, *center; uint64_t config_stamp, stamp;
        bool discovered, unlocked, demo, alerted, soul_pos; // truthy
    } cen;
};
extern Class Card_class;
void Card_kids_fill(Card *self);
void Card_ab_fill(Card *self);
void Card_cen_fill(Card *self);
static inline const struct CardKids *Card_kids(Card *self) {
    Table *t = self->moveable.node.children;
    if (self->kids.t != t || self->kids.stamp != tstamp(t)) Card_kids_fill(self);
    return &self->kids;
}
static inline const struct CardAb *Card_ab(Card *self) {
    if (self->ab.t != self->ability || self->ab.stamp != tstamp(self->ability)) Card_ab_fill(self);
    return &self->ab;
}
static inline const struct CardCen *Card_cen(Card *self) {
    Table *config = self->moveable.node.config;
    if (self->cen.config != config || self->cen.config_stamp != tstamp(config) || self->cen.stamp != tstamp(self->cen.center)) Card_cen_fill(self);
    return &self->cen;
}

// self.children.<name>, or NULL
#define Card_child(self, name) tobj((self)->moveable.node.children, name) // (a macro: a literal name is interned once per call site, core.h)
// card[key] for the UI's ref_table = card, ref_value = key bindings ('sell_cost_label', 'cost') and other reads by
// name: the members a UI definition refers to, else the dyn table.
static inline Val Card_field(Card *self, const char *key) {
    if (streq(key, "sell_cost_label")) return self->sell_cost_label;
    if (streq(key, "cost")) return N(self->cost);
    if (streq(key, "sell_cost")) return N(self->sell_cost);
    return tget(self->moveable.node.object.dyn, key);
}

// card.lua:5 Card(X, Y, W, H, card, center, params) is NEW(Card) + this. card: a G.P_CARDS entry, NULL as nil ({}).
// center: a G.P_CENTERS entry (never NULL). params: NULL as nil ({}).
void Card_init(Card *self, float X, float Y, float W, float H, Table *card, Table *center, Table *params);
// card.lua:79
void Card_update_alert(Card *self);
// card.lua:97 card: NULL as nil ({}). initial: nil as false.
void Card_set_base(Card *self, Table *card, bool initial);
// card.lua:147 _center, _front: NULL as nil (that part is skipped). note: the Lua shares the card's states.hover /
// click / drag tables with the front, center and back sprites.
void Card_set_sprites(Card *self, Table *_center, Table *_front);
// card.lua:223 initial, delay_sprites: nil as false
void Card_set_ability(Card *self, Table *center, bool initial, bool delay_sprites);
// card.lua:369
void Card_set_cost(Card *self);
// card.lua:387 edition: {holo/foil/polychrome/negative = true} or NULL as nil (clears it and returns). immediate,
// silent: nil as false.
void Card_set_edition(Card *self, Table *edition, bool immediate, bool silent);
// card.lua:464 _seal: NULL as nil. silent, immediate: nil as false.
void Card_set_seal(Card *self, const char *_seal, bool silent, bool immediate);
// card.lua:501 returns NULL as nil. bypass_debuff: nil as false.
const char *Card_get_seal(Card *self, bool bypass_debuff);
// card.lua:506
void Card_set_eternal(Card *self, bool _eternal);
// card.lua:513 (_perishable is ignored by the Lua)
void Card_set_perishable(Card *self, bool _perishable);
// card.lua:521
void Card_set_rental(Card *self, bool _rental);
// card.lua:526 the one call with no argument (card.lua:2283) passes false: it always takes the perishable branch,
// which does not read should_debuff
void Card_set_debuff(Card *self, bool should_debuff);
// card.lua:540
void Card_remove_UI(Card *self);
// card.lua:547 new_suit: 'Diamonds', 'Spades', 'Clubs', 'Hearts'
void Card_change_suit(Card *self, const char *new_suit);
// card.lua:564 from_debuff: nil as false
void Card_add_to_deck(Card *self, bool from_debuff);
// card.lua:645 from_debuff: nil as false
void Card_remove_from_deck(Card *self, bool from_debuff);
// card.lua:702 returns generate_card_ui's table. hidden: nil as false.
Table *Card_generate_UIBox_unlock_table(Card *self, bool hidden);
// card.lua:708 returns generate_card_ui's table
Table *Card_generate_UIBox_ability_table(Card *self);
// card.lua:950 mod: 'suit', or NULL as nil
num Card_get_nominal(Card *self, const char *mod);
// card.lua:957 base.id; 0 as nil (a card with no rank); a Stone Card gives a new random negative number each call
num Card_get_id(Card *self);
// card.lua:964 true, or false as nil. from_boss: nil as false.
bool Card_is_face(Card *self, bool from_boss);
// card.lua:972 base.original_value; NULL as nil
const char *Card_get_original_rank(Card *self);
// card.lua:976
num Card_get_chip_bonus(Card *self);
// card.lua:984
num Card_get_chip_mult(Card *self);
// card.lua:999 context is not used: NULL
num Card_get_chip_x_mult(Card *self, Table *context);
// card.lua:1006
num Card_get_chip_h_mult(Card *self);
// card.lua:1011
num Card_get_chip_h_x_mult(Card *self);
// card.lua:1016 {card = O(self), x_mult_mod=, mult_mod=, chip_mod=}; NULL as nil (debuffed or no edition)
Table *Card_get_edition(Card *self);
// card.lua:1033 never NULL ({} when debuffed): {h_dollars=, card=, effect=}. context is not used: NULL.
Table *Card_get_end_of_round_effect(Card *self, Table *context);
// card.lua:1068
num Card_get_p_dollars(Card *self);
// card.lua:1091 area is not used by the Lua. copier: the card that uses it in its place; NULL as nil.
void Card_use_consumeable(Card *self, CardArea *area, Card *copier);
// card.lua:1523 any_state, skip_check: nil as false
bool Card_can_use_consumeable(Card *self, bool any_state, bool skip_check);
// card.lua:1581 true, or false as nil
bool Card_check_use(Card *self);
// card.lua:1590
void Card_sell_card(Card *self);
// card.lua:1640 context is not used: NULL
bool Card_can_sell_card(Card *self, Table *context);
// card.lua:1655 the dollars as N(), or NIL (the caller tests truthiness, and 0 is true)
Val Card_calculate_dollar_bonus(Card *self);
// card.lua:1681
void Card_open(Card *self);
// card.lua:1813
void Card_redeem(Card *self);
// card.lua:1880 center: a voucher's center, or NULL as nil (then self's ability is used). self is NULL for
// Card.apply_to_run(nil, center) (back.lua:179, 236, game.lua:2095).
void Card_apply_to_run(Card *self, Table *center);
// card.lua:1973 dissolve_colours: array of P(float *), NULL as nil ({G.C.WHITE}). explode_time_fac < 0: nil (1).
void Card_explode(Card *self, Table *dissolve_colours, num explode_time_fac);
// card.lua:2079
void Card_shatter(Card *self);
// card.lua:2130 dissolve_colours: array of P(float *), NULL as nil (the default five). silent, no_juice: nil as
// false. dissolve_time_fac < 0: nil (1).
void Card_start_dissolve(Card *self, Table *dissolve_colours, bool silent, num dissolve_time_fac, bool no_juice);
// card.lua:2183 dissolve_colours: array of P(float *), NULL as nil (by the card's set). silent: nil as false.
// timefac < 0: nil (1).
void Card_start_materialize(Card *self, Table *dissolve_colours, bool silent, num timefac);
// card.lua:2242 context: {repetition=} or {discard=}. Returns {message=, repetitions=, card=} or NULL as nil.
Table *Card_calculate_seal(Card *self, Table *context);
// card.lua:2271
void Card_calculate_rental(Card *self);
// card.lua:2278
void Card_calculate_perishable(Card *self);
// card.lua:2291 context: the Lua's context table (it writes blueprint and blueprint_card into it). Returns the
// effect table as T(), B(true) (destroying_card: Sixth Sense), or NIL. This function is lines 2291-2334 (debuff,
// Planet, Blueprint, Brainstorm), then, only for ability.set == 'Joker': Card_calculate_joker_a; if it returns
// false Card_calculate_joker_b; if it returns false Card_calculate_joker_c. For any other set it returns NIL.
Val Card_calculate_joker(Card *self, Table *context);
// card.lua:2335-2873 the context branches open_booster, buying_card, selling_self, selling_card, reroll_shop,
// ending_shop, skip_blind, skipping_booster, playing_card_added, first_hand_drawn, setting_blind,
// destroying_card, cards_destroyed, remove_playing_cards, using_consumeable, debuffed_hand, pre_discard, discard.
// Returns true when the condition of one of those branches held (with its 'and not self.getting_sliced' /
// 'and not context.blueprint' part): *ret is then what the Lua returns (NIL when the branch falls out without a
// return). false: none held, *ret untouched.
bool Card_calculate_joker_a(Card *self, Table *context, Val *ret);
// card.lua:2874-3408 the context branches end_of_round, individual, repetition, other_joker: as _a
bool Card_calculate_joker_b(Card *self, Table *context, Val *ret);
// card.lua:3409-4063 the final else (context.cardarea == G.jokers: before, after, joker_main...): what the Lua
// returns, NIL when it falls out
Val Card_calculate_joker_c(Card *self, Table *context);
// card.lua:4064 false as nil. bypass_debuff, flush_calc: nil as false.
bool Card_is_suit(Card *self, const char *suit, bool bypass_debuff, bool flush_calc);
// card.lua:4091 area is never NULL
void Card_set_card_area(Card *self, CardArea *area);
// card.lua:4097
void Card_remove_from_area(Card *self);
// card.lua:4103
void Card_align(Card *self);
// card.lua:4113
void Card_flip(Card *self);
// card.lua:4126 (Class.update)
void Card_update(Card *self, float dt);
// card.lua:4256 (Class.hard_set_T) each of X, Y, W, H: NAN as nil (keeps self.T's value); Card:hard_set_T() with
// no arguments (cardarea.lua:567, UI_definitions.lua:3272) is four NANs
void Card_hard_set_T(Card *self, float X, float Y, float W, float H);
// card.lua:4267 (Class.move)
void Card_move(Card *self, float dt);
// card.lua:4275 the arguments for the popup's Moveable_set_alignment: {major = O(), parent = O(), xy_bond=,
// r_bond=, wh_bond=, offset = {x=, y=}, type=}
Table *Card_align_h_popup(Card *self);
// card.lua:4306 (Class.hover)
void Card_hover(Card *self);
// card.lua:4329 (Class.stop_hover)
void Card_stop_hover(Card *self);
// card.lua:4333 (Class.juice_up) scale < 0: nil (0.11, else scale*0.4). rot_amount: N(), or NIL (a random sign
// times 0.16).
void Card_juice_up(Card *self, float scale, Val rot_amount);
// card.lua:4340 layer: 'shadow', 'card' or 'both'; NULL as nil ('both'). Class.draw calls it with NULL.
void Card_draw(Card *self, const char *layer);
// card.lua:4576 (Class.release) dragged: a Node, never NULL
void Card_release(Card *self, void *dragged);
// card.lua:4582
void Card_highlight(Card *self, bool is_higlighted);
// card.lua:4610 (Class.click)
void Card_click(Card *self);
// card.lua:4625 the table that goes into the save file (the Lua leaves it in the global cardTable: not kept)
Table *Card_save(Card *self);
// card.lua:4659 other_card is not used: NULL
void Card_load(Card *self, Table *cardTable, Card *other_card);
// card.lua:4727 (Class.remove)
void Card_remove(Card *self);
#endif
