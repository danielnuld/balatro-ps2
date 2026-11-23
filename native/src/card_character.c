// card_character.lua:1-164: Card_Character (Jimbo), the whole file.
#include "all.h"

#define STATES(o) ((o)->moveable.node.states)
#define CONFIG(self) ((self)->moveable.node.config)
#define CHILDREN(self) ((self)->moveable.node.children)

// ---------------------------------------------------------------- classes (card_character.lua:2 Card_Character = Moveable:extend())
static void cls_move(void *self, float dt) { Card_Character_move(self, dt); }
static void cls_hard_set_VT(void *self) { Card_Character_hard_set_VT(self); }
static void cls_draw(void *self) { Card_Character_draw(self); }
static void cls_remove(void *self) { Card_Character_remove(self); }
Class Card_Character_class = {
    .name = "Card_Character",
    .super = &Moveable_class,
    .move = cls_move,
    .hard_set_VT = cls_hard_set_VT,
    .draw = cls_draw,
    .remove = cls_remove,
};

// card_character.lua:23 self.children.card.hover = Node.hover: the class of Jimbo's card (see card_character.h)
static void cls_card_hover(void *self) { Node_hover(self); }
Class Card_Character_card_class = {
    .name = "Card",
    .super = &Card_class,
    .hover = cls_card_hover,
};

// card_character.lua:5
void Card_Character_init(Card_Character *self, Table *args) {
    Val x = tget(args, "x"), y = tget(args, "y"), w = tget(args, "w"), h = tget(args, "h");
    Moveable_init(&self->moveable, truthy(x) ? (float)vnum(x) : 1, truthy(y) ? (float)vnum(y) : 1,
                  truthy(w) ? (float)vnum(w) : G->CARD_W * 1.1f, truthy(h) ? (float)vnum(h) : G->CARD_H * 1.1f);

    STATES(self).collide.can = false;

    CHILDREN(self) = tnew();
    CONFIG(self) = TAB("args", T(args));
    Val center = tget(args, "center");
    Card *card = NEW(Card);
    Card_init(card, self->moveable.node.T->x, self->moveable.node.T->y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"),
              truthy(center) ? center.tab : ttab(G->P_CENTERS, "j_joker"), TAB("bypass_discovery_center", B(true)));
    tseto(CHILDREN(self), "card", card);
    STATES(card).visible = false;
    Card_start_materialize(card, ARR(P(G->C.BLUE), P(G->C.WHITE), P(G->C.RED)), false, -1);
    Moveable_set_alignment(&card->moveable, TAB("major", O(self), "type", S("cm"), "offset", T(TAB("x", N(0), "y", N(0)))));
    tseto(card->moveable.node.object.dyn, "jimbo", self);
    STATES(card).collide.can = true;
    STATES(card).focus.can = false;
    STATES(card).hover.can = true;
    STATES(card).drag.can = false;
    // :23 card.hover = Node.hover: the card's class is swapped (card_character.h)
    if (!Card_Character_card_class.ready) class_init(&Card_Character_card_class);
    card->moveable.node.object.cls = &Card_Character_card_class;

    Particles *particles = NEW(Particles);
    Particles_init(particles, 0, 0, 0, 0,
                   TAB("timer", N(0.03), "scale", N(0.3), "speed", N(1.2), "lifespan", N(2), "attach", O(self), "colours",
                       T(ARR(P(G->C.RED), P(G->C.BLUE), P(G->C.ORANGE))), "fill", B(true)));
    tseto(CHILDREN(self), "particles", particles);
    particles->moveable.static_rotation = true;
    Moveable_set_role(&particles->moveable,
                      TAB("role_type", S("Minor"), "xy_bond", S("Weak"), "r_bond", S("Strong"), "major", O(self)));

    if (self->moveable.node.object.cls == &Card_Character_class) {
        tinsert(G->I.CARD, O(self));
    }
}

// card_character.lua:47
void Card_Character_move(Card_Character *self, float dt) {
    Moveable_move(&self->moveable, dt);
}

// card_character.lua:51
void Card_Character_hard_set_VT(Card_Character *self) {
    Moveable_align_to_major(&self->moveable);
    Moveable_hard_set_VT(&self->moveable);
    Card_Character_align(self);
    CALL(Card_Character_child(self, "card"), hard_set_VT);
}

// card_character.lua:58
void Card_Character_align(Card_Character *self) {
    Card *card = Card_Character_child(self, "card");
    if (card) {
        Transform *T = self->moveable.node.T, *cT = card->moveable.node.T;
        cT->x = T->x + (T->w - cT->w) / 2;
        cT->y = T->y + (T->h - cT->h) / 2;
    }
}

// card_character.lua:65
void Card_Character_add_button(Card_Character *self, const char *button, const char *func, const float *colour,
                               const char *update_func, bool snap_to, float yoff) {
    if (Card_Character_child(self, "button")) CALL(Card_Character_child(self, "button"), remove);
    tsett(CONFIG(self), "button_align",
          TAB("align", S("bm"), "offset", T(TAB("x", N(0), "y", N(isnan(yoff) ? 0.3 : yoff))), "major", O(self), "parent",
              O(self)));
    Table *definition = create_UIBox_character_button(
        TAB("button", S(button), "func", S(func), "colour", colour ? P((void *)colour) : NIL, "update_func", S(update_func),
            "maxw", N(3)));
    tseto(CHILDREN(self), "button",
          UIBox_new(TAB("definition", T(definition), "config", T(ttab(CONFIG(self), "button_align")))));
    if (snap_to) Controller_snap_to(G->CONTROLLER, TAB("node", O(Card_Character_child(self, "button"))));
}

// card_character.lua:75
void Card_Character_add_speech_bubble(Card_Character *self, Val text_key, const char *align, Table *loc_vars) {
    if (Card_Character_child(self, "speech_bubble")) CALL(Card_Character_child(self, "speech_bubble"), remove);
    tsett(CONFIG(self), "speech_bubble_align",
          TAB("align", S(align ? align : "bm"), "offset", T(TAB("x", N(0), "y", N(0))), "parent", O(self)));
    Table *definition = UIDEF_speech_bubble(text_key, loc_vars);
    UIBox *speech_bubble = UIBox_new(TAB("definition", T(definition), "config", T(ttab(CONFIG(self), "speech_bubble_align"))));
    tseto(CHILDREN(self), "speech_bubble", speech_bubble);
    Moveable_set_role(&speech_bubble->moveable,
                      TAB("role_type", S("Minor"), "xy_bond", S("Weak"), "r_bond", S("Strong"), "major", O(self)));
    STATES(speech_bubble).visible = false;
}

// card_character.lua:92
void Card_Character_remove_button(Card_Character *self) {
    if (Card_Character_child(self, "button")) {
        CALL(Card_Character_child(self, "button"), remove);
        tset(CHILDREN(self), "button", NIL);
    }
}

// card_character.lua:96
void Card_Character_remove_speech_bubble(Card_Character *self) {
    if (Card_Character_child(self, "speech_bubble")) {
        CALL(Card_Character_child(self, "speech_bubble"), remove);
        tset(CHILDREN(self), "speech_bubble", NIL);
    }
}

struct say_stuff_ctx { Card_Character *self; int n; };

// card_character.lua:106
static bool ev_say_stuff_first(void *ctx) {
    struct say_stuff_ctx *c = ctx;
    UIBox *speech_bubble = Card_Character_child(c->self, "speech_bubble");
    if (speech_bubble) STATES(speech_bubble).visible = true;
    Card_Character_say_stuff(c->self, c->n, true);
    return true;
}

// card_character.lua:125
static bool ev_say_stuff_next(void *ctx) {
    struct say_stuff_ctx *c = ctx;
    Card_Character_say_stuff(c->self, c->n - 1, true);
    return true;
}

// card_character.lua:100
void Card_Character_say_stuff(Card_Character *self, int n, bool not_first) {
    self->talking = true;
    struct say_stuff_ctx *c = NEWCTX(struct say_stuff_ctx);
    c->self = self, c->n = n;
    if (!not_first) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_say_stuff_first, .ctx = c}), NULL,
              false);
    } else {
        if (n <= 0) { self->talking = false; return; }
        int new_said = (int)math_random_range(1, 11);
        while (new_said == self->last_said) {
            new_said = (int)math_random_range(1, 11);
        }
        self->last_said = new_said;
        // note: locals keep the Lua's left-to-right order of the two random calls
        const char *voice = fmt("voice%d", (int)math_random_range(1, 11));
        float per = G->SPEEDFACTOR * (math_random() * 0.2 + 1);
        play_sound(voice, per, 0.5f);
        Card_juice_up(Card_Character_child(self, "card"), -1, NIL);
        E_add(G->E_MANAGER,
              Event_new((EventCfg){.trigger = "after", .not_blockable = true, .not_blocking = true, .delay = 0.13,
                                   .func = ev_say_stuff_next, .ctx = c}),
              "tutorial", false);
    }
}

// card_character.lua:133
void Card_Character_draw(Card_Character *self) {
    if (self->highlight) {
        CALL(Card_Character_child(self, "highlight"), draw);
        CALL(self->highlight, draw);
        // note: :137 self.highlight.draw_children is a UIElement method; a UIBox has none, so the call never runs
        // (and highlight is always NULL: card_character.h)
    }
    if (Card_Character_child(self, "particles")) {
        CALL(Card_Character_child(self, "particles"), draw);
    }
    if (Card_Character_child(self, "speech_bubble")) {
        CALL(Card_Character_child(self, "speech_bubble"), draw);
    }
    if (Card_Character_child(self, "button") && !self->talking) {
        CALL(Card_Character_child(self, "button"), draw);
    }
    if (Card_Character_child(self, "card")) {
        CALL(Card_Character_child(self, "card"), draw);
    }
    add_to_drawhash(self);
    Node_draw_boundingrect(&self->moveable.node);
}

// card_character.lua:155
void Card_Character_remove(Card_Character *self) {
    G->jimboed = false;
    remove_all(CHILDREN(self));
    // note: pairs over the array while table.remove shifts it: the index goes on to k+1, as the Lua's next does
    for (int k = 1; k <= tlen(G->I.CARD); k++) {
        if (tobji(G->I.CARD, k) == self) {
            tremove(G->I.CARD, k);
        }
    }
    Moveable_remove(&self->moveable);
}
