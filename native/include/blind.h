// blind.lua: Blind, the blind being played: its chip (an AnimatedSprite), the score to beat, the reward, and the boss
// blinds' effects on the hand, the cards and the jokers. Blind = Moveable:extend(). One instance per run:
// G.GAME.blind (game.lua:2392, Blind(0, 0, 2, 1)), which is saved and loaded with the run.
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below).
//
// How the Lua's shapes are kept:
// - self.config is the Node's config table (moveable.node.config): blind (the G.P_BLINDS entry being played, or {}
//   when there is none: `next(G.GAME.blind.config.blind)` is !tempty; nil before the first set_blind),
//   h_popup_config ({align = "tm", offset = {x = 0, y = -0.1}, parent = O(self)}).
// - Children, by name in moveable.node.children: animatedSprite (AnimatedSprite, always there after init:
//   Blind_sprite) and particles (Particles, made by Blind_defeat, never removed by this file). Blind_draw and
//   Blind_align walk the whole table with pairs().
// - THE STATES ARE SHARED. blind.lua:18 makes the sprite's `states` the very table of the blind
//   (self.children.animatedSprite.states = self.states), so `self.children.animatedSprite.states.visible = false`
//   (blind.lua:19) hides the BLIND, and the two never differ. In C a Node holds its States by value, so: wherever
//   the Lua names the sprite's states (blind.lua:19, 20, 430, 670, 734, 740, 743), read and write the blind's
//   (self->moveable.node.states), and call Blind_sync_states(self) after writing them and in Blind_move, so that
//   engine code looking at the sprite's own copy sees the same. Only the blind is in the draw hash (Blind_draw adds
//   self, not the sprite), so the controller writes the blind's states, never the sprite's.
// - The members tilt_var, ambient_tilt, hover_tilt, dissolve and dissolve_colours have the names and types of
//   Card's (card.h): Sprite_draw_shader reads them from its role.draw_major, which Blind_draw sets to the blind.
// - The UI binds to a blind by name (ref_table = G.GAME.blind, ref_value = 'chip_text' / 'loc_name',
//   UI_definitions.lua:1218, 1240): Blind_field. It binds to loc_debuff_lines as a table and an index (:1225).
// - Blind_init adds the blind to G.I.CARD (blind.lua:28): that list holds Cards, the Blind and Card_Characters;
//   test with is_a before treating an entry as a Card.
#ifndef BLIND_H
#define BLIND_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- Blind
// Inherited members the Lua writes on a blind: zoom (true), shadow_height (0, then Blind_align animates it), states
// (collide.can and drag.can true; visible false at init: see above; button_callbacks.lua:127 shows it), T.
//
// in dyn: (NOT members; tget(self->moveable.node.object.dyn, "..."))
//   nothing of blind.lua. Whatever another file hangs on a blind lives there.
struct Blind {
    Moveable moveable;
    const char *name;        // self.name: the blind's name ('Small Blind', 'The Hook'...), '' when no blind is set;
                             // NULL as nil before the first set_blind / load
    num dollars;             // self.dollars: the reward (0 with no blind, or when G.GAME.modifiers.no_blind_reward says so)
    num sound_pings;         // self.sound_pings: blind.dollars + 2, the pings of Blind_defeat; 0 as nil (then 3 is used)
    Table *debuff;           // self.debuff: the entry's debuff table ({suit=, is_face=, hand=, value=, nominal=,
                             // h_size_ge=, h_size_le=}) or a new {}; NULL as nil (before set_blind, or loaded as nil)
    Table *pos;              // self.pos: the entry's sprite position {x=, y=} (the same table); NULL as nil
    num mult;                // self.mult: the entry's score multiplier (0 with no blind)
    bool disabled;           // self.disabled: its effect is off (Chicot, Luchador, selling...); nil as false
    Val discards_sub;        // self.discards_sub: N(discards The Water took), NIL as nil
    Val hands_sub;           // self.hands_sub: N(hands The Needle took), NIL as nil
    bool boss;               // self.boss: nil (no blind) as false
    bool blind_set;          // self.blind_set: the chip has appeared (set by the event of set_blind, or by load)
    bool triggered;          // self.triggered: the boss effect acted on this hand; nil as false (only truthiness is read)
    bool prepped;            // self.prepped: nil as false (The Fish, Crimson Heart)
    Table *hands;            // self.hands: The Eye's {hand name -> bool played}; NULL as nil
    const char *only_hand;   // self.only_hand: The Mouth's hand name; NULL as false and as nil (only truthiness is read)
    bool block_play;         // self.block_play: the Play button is held while the debuff text is announced; nil as false
    bool hovering;           // self.hovering; nil as false
    num chips;               // self.chips: the score to beat
    const char *chip_text;   // self.chip_text: number_format(chips); NULL as nil before the first set_blind
    const char *loc_name;    // self.loc_name: the localized name; NULL as nil before the first set_text
    const char *loc_debuff_text; // self.loc_debuff_text: the localized effect in one line, '' if none; NULL as nil
    Table *loc_debuff_lines; // self.loc_debuff_lines: {line 1, line 2} of the effect, '' when absent; never NULL and
                             // never replaced (the HUD holds the table: assign its elements with tseti)
    Colour colour;           // self.colour: a copy of G.C.BLACK taken at init (not read after)
    Colour dark_colour;      // self.dark_colour: darken(colour, 0.2) (not read after)
    struct { float mx, my, amt; } tilt_var; // the cursor, in pixels (Blind_draw sets mx, my every frame); never nil
    float ambient_tilt;      // 0.3
    float hover_tilt;        // 2 while hovered, 0 after; 0 as nil
    float dissolve;          // 0 = whole .. 1 = gone, eased by Blind_defeat (EventCfg.ref_float = &self->dissolve);
                             // 0 as nil
    Table *dissolve_colours; // {P(G.C.BLACK), P(G.C.RED)} once defeated (never reset); NULL as nil
};
extern Class Blind_class;

// self.children.animatedSprite; never NULL after init
static inline AnimatedSprite *Blind_sprite(Blind *self) {
    return (AnimatedSprite *)tobj(self->moveable.node.children, "animatedSprite");
}
// Copies the blind's states to its sprite: the Lua's shared `states` table (see the top of this file).
static inline void Blind_sync_states(Blind *self) {
    AnimatedSprite *s = Blind_sprite(self);
    if (s) s->sprite.moveable.node.states = self->moveable.node.states;
}
// blind[key] for the UI's ref_table = blind, ref_value = key bindings and other reads by name: the members a UI
// definition refers to, else the dyn table.
static inline Val Blind_field(Blind *self, const char *key) {
    if (streq(key, "chip_text")) return S(self->chip_text);
    if (streq(key, "loc_name")) return S(self->loc_name);
    if (streq(key, "loc_debuff_text")) return S(self->loc_debuff_text);
    if (streq(key, "name")) return S(self->name);
    if (streq(key, "chips")) return N(self->chips);
    if (streq(key, "dollars")) return N(self->dollars);
    if (streq(key, "mult")) return N(self->mult);
    return tget(self->moveable.node.object.dyn, key);
}

// blind.lua:5 Blind(X, Y, W, H) is NEW(Blind) + this. Game units. Makes the sprite (hidden) and adds self to
// G.I.CARD when the class is exactly Blind.
void Blind_init(Blind *self, float X, float Y, float W, float H);
// blind.lua:32 blind_col: a colour; NULL as nil: get_blind_main_colour(self.config.blind.key or ''). Eases
// G.C.DYN_UI.MAIN / DARK / BOSS_MAIN / BOSS_DARK.
void Blind_change_colour(Blind *self, const float *blind_col);
// blind.lua:48 Fills loc_name, loc_debuff_text and loc_debuff_lines from the localization; nothing without config.blind.
void Blind_set_text(Blind *self);
// blind.lua:78 blind: a G.P_BLINDS entry; NULL as nil: no blind (defeated, or a new run). reset: false as nil; true
// only re-applies the debuffs to the cards (nothing else changes). silent: false as nil; true: no juice and no sound.
void Blind_set_blind(Blind *self, Table *blind, bool reset, bool silent);
// blind.lua:218 first is not used. Announces loc_debuff_text over the play area once the hand can be selected.
void Blind_alert_debuff(Blind *self, bool first);
// blind.lua:270 The text to announce: The Wheel's probability and The Mouth's [hand] are added. Needs
// loc_debuff_text and config.blind.
const char *Blind_get_loc_debuff_text(Blind *self);
// blind.lua:276 silent: false as nil; true: no pings. Dissolves the chip and queues set_blind(nil, nil, true).
void Blind_defeat(Blind *self, bool silent);
// blind.lua:346 'Small', 'Big', 'Boss', or NULL as nil (no name, or '').
const char *Blind_get_type(Blind *self);
// blind.lua:356 Turns the boss effect off and undoes what it did.
void Blind_disable(Blind *self);
// blind.lua:417 Shakes the chip, with two sounds.
void Blind_wiggle(Blind *self);
// blind.lua:424 (Class.juice_up) _a < 0 as nil: 0.2. _b NIL as nil: 0.2. Juices the sprite, not the blind.
void Blind_juice_up(Blind *self, float _a, Val _b);
// blind.lua:428 (Class.hover)
void Blind_hover(Blind *self);
// blind.lua:440 (Class.stop_hover)
void Blind_stop_hover(Blind *self);
// blind.lua:446 (Class.draw)
void Blind_draw(Blind *self);
// blind.lua:464 The effects on pressing Play (The Hook, The Tooth, Crimson Heart, The Fish). Returns true when the
// Lua does (The Hook and The Tooth), false as nil.
bool Blind_press_play(Blind *self);
// blind.lua:510 Returns mult, hand_chips, modded: the first is returned, *out_hand_chips and *modded are always
// written (neither may be NULL). cards (array of O(Card *)), poker_hands and text are not used (may be NULL).
// The Flint halves both (rounded, at least 1 and 0).
num Blind_modify_hand(Blind *self, Table *cards, Table *poker_hands, const char *text, num mult, num hand_chips,
                      num *out_hand_chips, bool *modded);
// blind.lua:519 cards: array of O(Card *). hand: the poker hands found ({hand name -> array of hands},
// evaluate_poker_hand's result). handname: the hand played (the key of G.GAME.hands). check: false as nil; true
// only asks, without recording the hand or acting (The Eye, The Mouth, The Arm, The Ox). Returns true when the
// hand is not allowed, false as nil (The Arm and The Ox act and return nil).
bool Blind_debuff_hand(Blind *self, Table *cards, Table *hand, const char *handname, bool check);
// blind.lua:572 After the hand is drawn (Cerulean Bell, Crimson Heart); clears prepped.
void Blind_drawn_to_hand(Blind *self);
// blind.lua:605 Whether `card` drawn to `area` stays face down (The Wheel, The House, The Mark, The Fish; only
// for G.hand). Returns false as nil. Calls pseudorandom('wheel') only for The Wheel.
bool Blind_stay_flipped(Blind *self, CardArea *area, Card *card);
// blind.lua:624 Sets or clears the card's debuff. from_blind is not used (false as nil).
void Blind_debuff_card(Blind *self, Card *card, bool from_blind);
// blind.lua:654 (Class.move) dt: seconds.
void Blind_move(Blind *self, float dt);
// blind.lua:659 w, h: game units; NAN as nil (keeps self.T's).
void Blind_change_dim(Blind *self, float w, float h);
// blind.lua:667 Bobs the sprite around the blind's T and puts the other children on it.
void Blind_align(Blind *self);
// blind.lua:684 Returns {name, dollars, debuff, pos, mult, disabled, discards_sub, hands_sub, boss, config_blind
// (the key of G.P_BLINDS whose name is self.name, or ''), chips, chip_text, hands, only_hand, triggered}; a member
// that is nil in the Lua is left out of the table.
Table *Blind_save(Blind *self);
// blind.lua:712 blindTable: what save returned (never NULL).
void Blind_load(Blind *self, Table *blindTable);
#endif
