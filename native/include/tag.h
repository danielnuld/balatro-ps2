// tag.lua: Tag, the reward for skipping a blind: a small sprite in the HUD that waits for its moment
// (a `_context.type`) and then acts once. Tag = Object:extend(): NOT a Node, it has no transform and is not drawn;
// what is on screen is its tag_sprite (a Sprite) inside its HUD_tag (a UIBox). The run's tags are the array
// G.GAME.tags (O(Tag *), 1-based); their UIBoxes the parallel array G.HUD_tags.
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below).
//
// How the Lua's shapes are kept:
// - The Lua replaces the hover and stop_hover METHODS of one sprite (tag_sprite.hover = function..., tag.lua:513,
//   537). In C the sprite gets a class of its own: Tag_sprite_class (super Sprite_class, hover = Tag_sprite_hover,
//   stop_hover = Tag_sprite_stop_hover, the rest inherited). Tag_generate_UI makes the sprite with NEW(Sprite) +
//   Sprite_init and then sets its object.cls = &Tag_sprite_class (class_init(&Tag_sprite_class) first if it is not
//   ready). is_a(sprite, &Sprite_class) stays true. The closure's upvalue `self` (the tag) is read back from the
//   sprite's config.tag; its upvalue `tag_sprite` is the sprite itself, so `_self == tag_sprite` is always true.
// - Fields the Lua hangs on that Sprite, which has no such members: they are in the SPRITE's dyn
//   (sprite->moveable.node.object.dyn): float (true), hovering (bool), hover_tilt (number: 3 while hovered, 0
//   after; Sprite_draw_shader reads it from its draw_major, here the sprite itself), ability_UIBox_table (table,
//   set by Tag_get_uibox_table). Its config (moveable.node.config) is replaced by {tag = O(self), force_focus =
//   true}, to which hover adds h_popup and h_popup_config; its children.alert is the collection's alert.
// - `local lock = self.ID; G.CONTROLLER.locks[lock] = true` (tag.lua:132...) is a NUMBER key in the locks table:
//   tseti(G->CONTROLLER->locks, self->ID, B(true)), and NIL to release it (the ID may be 0: tseti takes any integer).
// - Tag_class fills the Class slots juice_up (Tag_juice_up) and remove (Tag_remove): level_up_hand(card, ...) is
//   given a Tag as its `card` (tag.lua:197) and calls card:juice_up(0.8, 0.5) on it.
#ifndef TAG_H
#define TAG_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- Tag
// in dyn: (NOT members; tget(self->object.dyn, "..."))
//   nothing of tag.lua. Whatever another file hangs on a tag lives there.
struct Tag {
    Object object;
    const char *key;      // self.key: the key of G.P_TAGS ('tag_double'...); never NULL after init
    Table *config;        // self.config: a deep copy of the prototype's config ({type = the context it acts on,
                          // dollars=, spawn_jokers=, skip_bonus=, dollars_per_discard=, dollars_per_hand=, max=,
                          // levels=, h_size=, odds=, ...}); never NULL after init
    Table *pos;           // self.pos: the prototype's sprite position {x=, y=} (the same table, not a copy)
    const char *name;     // self.name: 'Orbital Tag', 'Double Tag'...: what the rules compare
    num tally;            // self.tally: G.GAME.tag_tally when it was made: the tag's number in the run (the UI's `group`)
    bool triggered;       // self.triggered: it has acted (it acts once); not saved
    int ID;               // self.ID: G.tagid when it was made; the key of its controller lock
    Table *ability;       // self.ability: {orbital_hand = the hand's name or '[' .. localize('k_poker_hand') .. ']',
                          // blind_type = 'Small' / 'Big' / 'Boss' or nil}; saved as it is; never NULL after init
    UIBox *HUD_tag;       // self.HUD_tag: its box in the HUD (set by add_tag, UI_definitions.lua:1271); NULL as nil
                          // (a tag of the collection or of the blind select screen has none)
    Sprite *tag_sprite;   // self.tag_sprite: the sprite generate_UI made last; NULL as nil
    bool hide_ability;    // self.hide_ability: shown as undiscovered (UI_definitions.lua:3927); nil as false
};
extern Class Tag_class;
extern Class Tag_sprite_class; // the class of a tag's sprite: see the top of this file

// tag.lua:5 Tag(_tag, for_collection, _blind_type) is NEW(Tag) + this. _tag: a key of G.P_TAGS (an unknown key
// gives G.tag_undiscovered's prototype). for_collection: false as nil; true skips set_ability. _blind_type:
// 'Small', 'Big', 'Boss'; NULL as nil. Takes G.tagid and G.GAME.tag_tally and advances both.
void Tag_init(Tag *self, const char *_tag, bool for_collection, const char *_blind_type);
// tag.lua:24 Queues the "NOPE" text over HUD_tag, hides it and removes the tag (3 events).
void Tag_nope(Tag *self);
// tag.lua:62 Queues the message over HUD_tag, hides it, runs func as an event of its own (default trigger) and
// removes the tag (4 events). message: '+' in every call. _colour: the cover's colour; NULL as nil: G.C.GREEN.
// func + ctx: the closure, with event.h's EventFunc shape (returns true when done); never NULL in the game.
void Tag_yep(Tag *self, const char *message, const float *_colour, bool (*func)(void *ctx), void *ctx);
// tag.lua:103 Orbital Tag: picks ability.orbital_hand from G.orbital_hand or G.GAME.orbital_choices.
void Tag_set_ability(Tag *self);
// tag.lua:115 _context: {type = 'eval' | 'immediate' | 'new_blind_choice' | 'voucher_add' | 'tag_add' |
// 'round_start_bonus' | 'store_joker_create' | 'shop_start' | 'store_joker_modify' | 'shop_final_pass', and tag =
// O(Tag *) (tag_add), area = O(CardArea *) (store_joker_create), card = O(Card *) (store_joker_modify)}; never NULL.
// Returns what the Lua returns, whose type varies: NIL (nothing happened, or 'voucher_add' / 'tag_add');
// B(true); for 'eval' T({dollars=, condition=, pos=, tag = O(self)}); for 'store_joker_create' O(the Card made) or
// NIL (Rare Tag with no rare left: nope); for 'store_joker_modify' B(true) when an edition was applied, else NIL.
// Callers only test truthiness except state_events.lua:1184 ('eval') and UI_definitions.lua:756 (the card).
Val Tag_apply_to_run(Tag *self, Table *_context);
// tag.lua:470 Returns {key =, tally =, ability = self.ability (the table itself)}.
Table *Tag_save(Tag *self);
// tag.lua:478 tag_savetable: what save returned (never NULL). The tag was made by Tag_init first (game.lua).
void Tag_load(Tag *self, Table *tag_savetable);
// tag.lua:489 (Class.juice_up) Passes both on to tag_sprite's juice_up as they are (nothing without a sprite):
// _scale < 0 as nil, _rot NIL as nil, as node.h's juice_up.
void Tag_juice_up(Tag *self, float _scale, Val _rot);
// tag.lua:493 _size: the sprite's side, game units; < 0 as nil: 0.8. Returns two values: the UI definition node
// ({n = G.UIT.C, config = {align = "cm", ref_table = O(self), group = self.tally}, nodes = {the O node}}) is
// returned; the new Sprite goes to *tag_sprite (NULL: not wanted). Also stores the sprite in self.tag_sprite.
Table *Tag_generate_UI(Tag *self, float _size, Sprite **tag_sprite);
// tag.lua:513 The hover the sprite of a tag gets (the slot of Tag_sprite_class). _self: that sprite; the tag is
// tobj(_self->moveable.node.config, "tag"). Builds the popup; clears the collection's alert.
void Tag_sprite_hover(Sprite *_self);
// tag.lua:537 The stop_hover of the same sprite.
void Tag_sprite_stop_hover(Sprite *_self);
// tag.lua:545 tag_sprite NULL as nil: self.tag_sprite. Sets the sprite's ability_UIBox_table (in its dyn) from
// generate_card_ui and returns the sprite.
Sprite *Tag_get_uibox_table(Tag *self, Sprite *tag_sprite);
// tag.lua:563 Takes the tag out of G.GAME.tags. If it is not there the Lua's table.remove(t, nil) removes the LAST
// tag: keep that (tremove with pos 0).
void Tag_remove_from_game(Tag *self);
// tag.lua:571 (Class.remove) remove_from_game, re-attaches the next HUD box, takes HUD_tag out of G.HUD_tags and
// removes it. HUD_tag NULL is a Lua error (self.HUD_tag:remove()): ERROR.
void Tag_remove(Tag *self);
#endif
