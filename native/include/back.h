// back.lua: Back, the deck the run is played with (Red Deck, Plasma Deck...): its name, its sprite position on the
// card backs' atlas and its effects on the run. Back = Object:extend(): not a Node, never drawn by itself. Two
// instances: G.GAME.selected_back (the run's; saved and loaded) and G.GAME.viewed_back (the one shown in the menus).
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below).
#ifndef BACK_H
#define BACK_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- Back
// in dyn: (NOT members; tget(self->object.dyn, "..."))
//   nothing of back.lua. Whatever another file reads on a back by another name lives there (and is nil).
struct Back {
    Object object;
    const char *name;     // self.name: the center's name ('Red Deck', 'Challenge Deck'...): what the rules compare
    Table *effect;        // self.effect: {center = the G.P_CENTERS entry (the same table), text_UI = '', config = a
                          // deep copy of center.config: hands, discards, dollars, joker_slot, hand_size, voucher,
                          // vouchers, consumables, spectral_rate, remove_faces, reroll_discount, edition,
                          // edition_count, randomize_rank_suit, ante_scaling, consumable_slot, no_interest,
                          // extra_hand_bonus, extra_discard_bonus}; saved as it is; never NULL after init
    const char *loc_name; // self.loc_name: the localized name
    Table *pos;           // self.pos: {x=, y=} on the atlas of backs ({x = 4, y = 0} while locked). init and change_to
                          // write x and y INTO the table (sprites and the save hold it); load replaces it; never NULL
};
extern Class Back_class;

// back.lua:5 Back(selected_back) is NEW(Back) + this. selected_back: a G.P_CENTERS entry of the set Back; NULL as
// nil: G.P_CENTERS.b_red.
void Back_init(Back *self, Table *selected_back);
// back.lua:22 loc_name, or localize('k_locked') while the center is locked.
const char *Back_get_name(Back *self);
// back.lua:26 Returns the UI definition (a G.UIT.ROOT node) that describes the deck. other: another center to
// describe in place of self's; NULL as nil. ui_scale: < 0 as nil: 0.9 (not used after that). min_dims: < 0 as nil:
// 0.7. challenge: the challenge's id ('c_omelette_1'...: G.GAME.challenge); NULL as nil.
Table *Back_generate_UI(Back *self, Table *other, float ui_scale, float min_dims, const char *challenge);
// back.lua:83 new_back: a center; NULL as nil: G.P_CENTERS.b_red. Like init, on the same object.
void Back_change_to(Back *self, Table *new_back);
// back.lua:97 Returns {name =, pos = self.pos, effect = self.effect (the tables themselves), key =
// effect.center.key or 'b_red'}.
Table *Back_save(Back *self);
// back.lua:108 args: {context = 'eval' | 'blind_amount' | 'final_scoring_step', chips =, mult =}; NULL as nil
// (nothing happens). Returns two values only for the Plasma Deck at 'final_scoring_step': the balanced chips are
// returned as N() and the balanced mult goes to *mult as N() (args.chips and args.mult are also rewritten); in
// every other case NIL is returned and *mult is set to NIL. mult may be NULL when the caller does not want it.
Val Back_trigger_effect(Back *self, Table *args, Val *mult);
// back.lua:174 Applies effect.config to G.GAME (starting_params, modifiers, used_vouchers) and queues the events
// that need the deck's cards. Card.apply_to_run(nil, center) is Card_apply_to_run(NULL, center).
void Back_apply_to_run(Back *self);
// back.lua:280 backTable: what save returned (never NULL). self.pos and self.effect BECOME the saved tables.
void Back_load(Back *self, Table *backTable);
#endif
