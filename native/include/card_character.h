// card_character.lua: Card_Character, Jimbo: a joker card that stands on screen and talks (the tutorial, the
// win and game over screens, the demo's call to action), with a speech bubble and a button.
// Card_Character = Moveable:extend().
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn (see the "in dyn:" block below).
//
// How the Lua's shapes are kept:
// - self.config is the Node's config table (moveable.node.config): args (the constructor's table), button_align
//   and speech_bubble_align (the config tables given to the two UIBoxes).
// - Children, by name in moveable.node.children (O(obj); absent as nil; Card_Character_child): card (Card, always
//   there after init), particles (Particles, always there after init), button, speech_bubble (UIBox; absent until
//   add_button / add_speech_bubble). Card_Character_remove walks the whole table (remove_all).
// - The Lua replaces the hover METHOD of its card by Node's (self.children.card.hover = Node.hover,
//   card_character.lua:23). In C the card gets a class of its own: Card_Character_card_class (super Card_class,
//   hover = Node_hover, the rest inherited). Card_Character_init makes the card with NEW(Card) + Card_init and then
//   sets its object.cls = &Card_Character_card_class (class_init(&Card_Character_card_class) first if it is not
//   ready). is_a(card, &Card_class) stays true.
// - self.children.card.jimbo = self is a field of the CARD, which has no such member: it is in the card's dyn,
//   tseto(card->moveable.node.object.dyn, "jimbo", self) (read by controller.lua:1091 and common_events.lua:800).
// - Card_Character_init adds self to G.I.CARD (card_character.lua:43): test with is_a before treating an entry of
//   that list as a Card.
#ifndef CARD_CHARACTER_H
#define CARD_CHARACTER_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- Card_Character
// Inherited members the Lua writes: states.collide.can = false.
//
// in dyn: (NOT members; tget(self->moveable.node.object.dyn, "..."))
//   nothing of card_character.lua. Whatever another file hangs on it lives there.
struct Card_Character {
    Moveable moveable;
    bool talking;      // self.talking: say_stuff is still running (the button is not drawn meanwhile); nil as false
    int last_said;     // self.last_said: the last random voice number, 1..11; 0 as nil
    UIBox *highlight;  // self.highlight: nothing in the game sets it (nor self.children.highlight): always NULL, so
                       // the first block of Card_Character_draw never runs; kept because draw tests it every frame
};
extern Class Card_Character_class;
extern Class Card_Character_card_class; // the class of Jimbo's card: see the top of this file

// self.children.<name> ('card', 'particles', 'button', 'speech_bubble'), or NULL
static inline void *Card_Character_child(Card_Character *self, const char *name) {
    return tobj(self->moveable.node.children, name);
}

// card_character.lua:5 Card_Character(args) is NEW(Card_Character) + this. args: {x =, y =, w =, h =, center =};
// never NULL (the Lua indexes it); defaults x 1, y 1, w G.CARD_W*1.1, h G.CARD_H*1.1, center G.P_CENTERS.j_joker.
// Becomes config.args. Makes the card (materializing) and the particles; adds self to G.I.CARD when the class is
// exactly Card_Character.
void Card_Character_init(Card_Character *self, Table *args);
// card_character.lua:47 (Class.move) dt: seconds. Only Moveable's.
void Card_Character_move(Card_Character *self, float dt);
// card_character.lua:51 (Class.hard_set_VT)
void Card_Character_hard_set_VT(Card_Character *self);
// card_character.lua:58 Centres the card on self.T.
void Card_Character_align(Card_Character *self);
// card_character.lua:65 button: the label's text; NULL as nil ("NONE", create_UIBox_character_button's default).
// func: the name of the G.FUNCS callback of the click; NULL as nil. colour: NULL as nil (G.C.RED). update_func: the
// name of the G.FUNCS callback run every frame; NULL as nil. snap_to: false as nil; true moves the cursor to the
// button. yoff: game units; NAN as nil: 0.3.
void Card_Character_add_button(Card_Character *self, const char *button, const char *func, const float *colour,
                               const char *update_func, bool snap_to, float yoff);
// card_character.lua:75 text_key: the type varies: S(a key of the localization's tutorial / quips texts), or T(a
// table of lines, button_callbacks.lua:1704 and UI_definitions.lua:61), passed on to G.UIDEF.speech_bubble; NIL as
// nil. align: NULL as nil: 'bm'. loc_vars: NULL as nil ({quip = true} picks the quips). The bubble starts hidden.
void Card_Character_add_speech_bubble(Card_Character *self, Val text_key, const char *align, Table *loc_vars);
// card_character.lua:92
void Card_Character_remove_button(Card_Character *self);
// card_character.lua:96
void Card_Character_remove_speech_bubble(Card_Character *self);
// card_character.lua:100 n: how many voice blips. not_first: false as nil (the outside call: shows the bubble after
// 0.1 s and starts); true is the recursion, one blip per event of the 'tutorial' queue. Uses math.random (not the
// seeded generator) 2 or more times per blip.
void Card_Character_say_stuff(Card_Character *self, int n, bool not_first);
// card_character.lua:133 (Class.draw) The Lua's dt argument is never passed nor used: it is dropped.
void Card_Character_draw(Card_Character *self);
// card_character.lua:155 (Class.remove) Clears G.jimboed, removes the children, leaves G.I.CARD.
void Card_Character_remove(Card_Character *self);
#endif
