// card.lua:4659-4771: Card:load, Card:remove
#include "all.h"

// card.lua:4659
void Card_load(Card *self, Table *cardTable, Card *other_card) {
    float scale = 1;
    Table *config = tnew(); // 4661: drops every previous config key
    self->moveable.node.config = config;
    Table *save_fields = ttab(cardTable, "save_fields");
    const char *center_key = tstr(save_fields, "center");
    tsets(config, "center_key", center_key);
    Table *center = center_key ? ttab(G->P_CENTERS, center_key) : NULL;
    tsett(config, "center", center);
    self->params = ttab(cardTable, "params");
    self->sticker_run = NULL;

    float H = G->CARD_H;
    float W = G->CARD_W;
    Transform *T_ = self->moveable.node.T;
    const char *name = tstr(center, "name");
    if (streq(name, "Half Joker")) {
        T_->h = H * scale / 1.7f * scale;
        T_->w = W * scale;
    } else if (streq(name, "Wee Joker")) {
        T_->h = H * scale * 0.7f * scale;
        T_->w = W * scale * 0.7f * scale;
    } else if (streq(name, "Photograph")) {
        T_->h = H * scale / 1.2f * scale;
        T_->w = W * scale;
    } else if (streq(name, "Square Joker")) {
        H = W;
        T_->h = H * scale;
        T_->w = W * scale;
    } else if (streq(tstr(center, "set"), "Booster")) {
        T_->h = H * 1.27f;
        T_->w = W * 1.27f;
    } else {
        T_->h = H * scale;
        T_->w = W * scale;
    }
    // note: 4689 is `self.VT.h = self.T.H` (capital H: nil). A float cannot be nil; T.h is the value the next
    // move / hard_set_T gives it (card-d.md, trap 3).
    self->moveable.VT.h = T_->h;
    self->moveable.VT.w = T_->w;

    const char *card_key = tstr(save_fields, "card");
    tsets(config, "card_key", card_key);
    Table *card = card_key ? ttab(G->P_CARDS, card_key) : NULL; // nil when card_key is nil
    tsett(config, "card", card);

    self->no_ui = tbool(cardTable, "no_ui");
    self->base_cost = tnum(cardTable, "base_cost");
    self->extra_cost = tnum(cardTable, "extra_cost");
    self->cost = tnum(cardTable, "cost");
    self->sell_cost = tnum(cardTable, "sell_cost");
    self->facing = tstr(cardTable, "facing");
    self->sprite_facing = tstr(cardTable, "sprite_facing");
    self->flipping = tstr(cardTable, "flipping");
    self->highlighted = tbool(cardTable, "highlighted");
    self->debuff = tbool(cardTable, "debuff");
    self->rank = (int)tnum(cardTable, "rank");
    self->added_to_deck = tbool(cardTable, "added_to_deck");
    self->label = tstr(cardTable, "label");
    self->playing_card = (int)tnum(cardTable, "playing_card");
    self->base = ttab(cardTable, "base");
    // note: base.colour comes back from a save as a table of 4 numbers (recursive_table_cull wrote it so); in C a
    // colour in a table is P(float *): a colour of its own, as the Lua's loaded table is (not G.C.SUITS's any more).
    if (tget(self->base, "colour").t == V_TAB) {
        Table *ct = ttab(self->base, "colour");
        float *colour = malloc(4 * sizeof(float));
        for (int i = 0; i < 4; i++) colour[i] = (float)vnum(tgeti(ct, i + 1));
        tset(self->base, "colour", P(colour));
    }
    self->sort_id = (int)tnum(cardTable, "sort_id");
    self->bypass_discovery_center = tbool(cardTable, "bypass_discovery_center");
    self->bypass_discovery_ui = tbool(cardTable, "bypass_discovery_ui");
    self->bypass_lock = tbool(cardTable, "bypass_lock");

    self->ability = ttab(cardTable, "ability");
    self->pinned = tbool(cardTable, "pinned");
    self->edition = ttab(cardTable, "edition");
    self->seal = tstr(cardTable, "seal");

    remove_all(self->moveable.node.children);
    self->moveable.node.children = tnew();
    Moveable *shadow = NEW(Moveable);
    Moveable_init(shadow, 0, 0, 0, 0);
    tseto(self->moveable.node.children, "shadow", shadow);

    Card_set_sprites(self, center, card);
}

// card.lua:4727
void Card_remove(Card *self) {
    tsetb(self->moveable.node.object.dyn, "removed", true);

    if (self->area) CardArea_remove_card(self->area, self, false);

    Card_remove_from_deck(self, false);
    if (tbool(self->ability, "queue_negative_removal")) {
        if (tbool(self->ability, "consumeable")) {
            Table *cfg = G->consumeables->moveable.node.config;
            tsetn(cfg, "card_limit", tnum(cfg, "card_limit") - 1);
        } else {
            Table *cfg = G->jokers->moveable.node.config;
            tsetn(cfg, "card_limit", tnum(cfg, "card_limit") - 1);
        }
    }

    if (!G->OVERLAY_MENU) {
        for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
            // note: veq, not streq: the Lua's == also holds for nil == nil
            if (veq(tget(it.val.tab, "name"), tget(self->ability, "name"))) {
                if (tempty(find_joker(tstr(self->ability, "name"), true))) {
                    tsetv(ttab(G->GAME, "used_jokers"), it.key, NIL);
                }
            }
        }
    }

    if (G->playing_cards) {
        for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
            if (tobji(G->playing_cards, k) == self) {
                tremove(G->playing_cards, k);
                break;
            }
        }
        for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
            ((Card *)tobji(G->playing_cards, k))->playing_card = k;
        }
    }

    remove_all(self->moveable.node.children);

    // note: pairs() over an array while table.remove shifts it: the walk goes on at k+1 over the live length (no
    // break), which is what this loop does.
    for (int k = 1; k <= tlen(G->I.CARD); k++) {
        if (tobji(G->I.CARD, k) == self) {
            tremove(G->I.CARD, k);
        }
    }
    Moveable_remove((Moveable *)self);
}
