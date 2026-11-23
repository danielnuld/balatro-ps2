// blind.lua:624-751: Blind's debuff_card, move, change_dim, align, save, load.
#include "all.h"

// blind.lua:624
void Blind_debuff_card(Blind *self, Card *card, bool from_blind) {
    if (self->debuff && !self->disabled && card->area != G->jokers) {
        Val suit = tget(self->debuff, "suit");
        if (truthy(suit) && Card_is_suit(card, vstr(suit), true, false)) {
            Card_set_debuff(card, true);
            return;
        }
        if (streq(tstr(self->debuff, "is_face"), "face") && Card_is_face(card, true)) {
            Card_set_debuff(card, true);
            return;
        }
        if (streq(self->name, "The Pillar") && tbool(card->ability, "played_this_ante")) {
            Card_set_debuff(card, true);
            return;
        }
        Val value = tget(self->debuff, "value");
        if (truthy(value) && veq(value, tget(card->base, "value"))) {
            Card_set_debuff(card, true);
            return;
        }
        Val nominal = tget(self->debuff, "nominal");
        if (truthy(nominal) && veq(nominal, tget(card->base, "nominal"))) {
            Card_set_debuff(card, true);
            return;
        }
    }
    if (streq(self->name, "Crimson Heart") && !self->disabled && card->area == G->jokers) {
        return;
    }
    if (streq(self->name, "Verdant Leaf") && !self->disabled && card->area != G->jokers) { Card_set_debuff(card, true); return; }
    Card_set_debuff(card, false);
}

// blind.lua:654
void Blind_move(Blind *self, float dt) {
    // note: the Lua's sprite shares the blind's states table; blind.h asks for the copy to be refreshed here.
    Blind_sync_states(self);
    Moveable_move((Moveable *)self, dt);
    Blind_align(self);
}

// blind.lua:659
void Blind_change_dim(Blind *self, float w, float h) {
    Transform *T = self->moveable.node.T;
    AnimatedSprite *sprite = Blind_sprite(self);
    Transform *sT = sprite->sprite.moveable.node.T;
    T->w = isnan(w) ? T->w : w;
    T->h = isnan(h) ? T->h : h;
    sT->w = isnan(w) ? T->w : w;
    sT->h = isnan(h) ? T->h : h;
    AnimatedSprite_rescale(sprite);
}

// blind.lua:667
void Blind_align(Blind *self) {
    Transform *T = self->moveable.node.T;
    for (TIter it = titer(self->moveable.node.children); tnext(&it);) {
        Node *v = (Node *)it.val.obj;
        if (it.key.t == V_STR && streq(it.key.s, "animatedSprite")) {
            // note: v.states is the blind's own states table (blind.lua:18)
            if (!self->moveable.node.states.drag.is) {
                // (float, as every animation: docs/conventions.md; wrap_angle: core.h)
                float REAL = (float)G->TIMERS.REAL;
                v->T->r = 0.02f * sinf(wrap_angle(2 * REAL + T->x));
                v->T->y = T->y + 0.03f * sinf(wrap_angle(0.666f * REAL + T->x));
                self->moveable.shadow_height = 0.1f - (0.04f + 0.03f * sinf(wrap_angle(0.666f * REAL + T->x)));
                v->T->x = T->x + 0.03f * sinf(wrap_angle(0.436f * REAL + T->x));
            }
        } else {
            v->T->x = T->x;
            v->T->y = T->y;
            v->T->r = T->r;
        }
    }
}

// blind.lua:684
Table *Blind_save(Blind *self) {
    // note: a nil member is left out (S/T of NULL are NIL). Of the booleans kept "nil as false", disabled is always
    // written (set_blind makes it false, never nil) and boss / triggered only when true: load reads all three by
    // truthiness.
    Table *blindTable = tnew();
    tset(blindTable, "name", S(self->name));
    tsetn(blindTable, "dollars", self->dollars);
    tset(blindTable, "debuff", T(self->debuff));
    tset(blindTable, "pos", T(self->pos));
    tsetn(blindTable, "mult", self->mult);
    tsetb(blindTable, "disabled", self->disabled);
    tset(blindTable, "discards_sub", self->discards_sub);
    tset(blindTable, "hands_sub", self->hands_sub);
    // note: self.boss is nil until a blind is set and a boolean after (blind.lua:91): false is written too
    if (self->boss || truthy(tget(ttab(self->moveable.node.config, "blind"), "name"))) tsetb(blindTable, "boss", self->boss);
    tsets(blindTable, "config_blind", "");
    tsetn(blindTable, "chips", self->chips);
    tset(blindTable, "chip_text", S(self->chip_text));
    tset(blindTable, "hands", T(self->hands));
    tset(blindTable, "only_hand", S(self->only_hand));
    if (self->triggered) tsetb(blindTable, "triggered", true);

    for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
        Table *v = it.val.t == V_TAB ? it.val.tab : NULL;
        if (v && truthy(tget(v, "name")) && self->name && streq(tstr(v, "name"), self->name)) {
            tset(blindTable, "config_blind", it.key);
        }
    }

    return blindTable;
}

// blind.lua:712
void Blind_load(Blind *self, Table *blindTable) {
    Table *config = self->moveable.node.config;
    Table *proto = NULL;
    Val pv = tgetv(G->P_BLINDS, tget(blindTable, "config_blind"));
    if (pv.t == V_TAB) proto = pv.tab;
    tsett(config, "blind", proto ? proto : tnew());
    Table *cblind = ttab(config, "blind");

    self->name = tstr(blindTable, "name");
    self->dollars = tnum(blindTable, "dollars");
    self->debuff = ttab(blindTable, "debuff");
    self->pos = ttab(blindTable, "pos");
    self->mult = tnum(blindTable, "mult");
    self->disabled = tbool(blindTable, "disabled");
    self->discards_sub = tget(blindTable, "discards_sub");
    self->hands_sub = tget(blindTable, "hands_sub");
    self->boss = tbool(blindTable, "boss");
    self->chips = tnum(blindTable, "chips");
    self->chip_text = tstr(blindTable, "chip_text");
    self->hands = ttab(blindTable, "hands");
    self->only_hand = tstr(blindTable, "only_hand");
    self->triggered = tbool(blindTable, "triggered");

    Val boss = tget(cblind, "boss");
    tsetn(ttab(G->ARGS, "spin"), "real",
          (tbool(G->SETTINGS, "reduced_motion") ? 0 : 1) *
              (truthy(boss) ? (truthy(tget(boss.t == V_TAB ? boss.tab : NULL, "showdown")) ? 1 : 0.3) : 0));

    AnimatedSprite *sprite = Blind_sprite(self);
    if (proto) {
        self->blind_set = true;
        self->moveable.node.states.visible = true; // the shared states (blind.h)
        Blind_sync_states(self);
        CALL(sprite, set_sprite_pos, ttab(cblind, "pos"));
        CALL(sprite, juice_up, 0.3f, NIL);
        Blind_align(self);
        CALL(sprite, hard_set_VT);
    } else {
        self->moveable.node.states.visible = false;
        Blind_sync_states(self);
    }

    Blind_sync_states(self); // blind.lua:743 self.children.animatedSprite.states = self.states
    Blind_change_colour(self, NULL);
    if (self->dollars > 0) {
        tsets(ttab(G->GAME, "current_round"), "dollars_to_be_earned",
              str_rep(vstr(localize(S("$"), NULL)), (int)self->dollars));
        UIElement *e = UIBox_get_UIE_by_ID(G->HUD_blind, S("dollars_to_be_earned"), NULL);
        DynaText_pop_in((DynaText *)tobj(e->moveable.node.config, "object"), 0);
        G->HUD_blind->moveable.alignment.offset.y = 0;
    }
    Blind_set_text(self);
}
