// card.lua:1-644: the Card class object, init, sprites, ability, cost, edition, seal, stickers, debuff, add_to_deck.
#include "all.h"

// card.lua:2 Card = Moveable:extend(). The entries are the methods card.lua overrides (the others are inherited).
static void cls_update(void *self, float dt) { Card_update(self, dt); }
static void cls_move(void *self, float dt) { Card_move(self, dt); }
static void cls_draw(void *self) { Card_draw(self, NULL); }
static void cls_remove(void *self) { Card_remove(self); }
static void cls_hover(void *self) { Card_hover(self); }
static void cls_stop_hover(void *self) { Card_stop_hover(self); }
static void cls_click(void *self) { Card_click(self); }
static void cls_release(void *self, void *dragged) { Card_release(self, dragged); }
static void cls_hard_set_T(void *self, float X, float Y, float W, float H) { Card_hard_set_T(self, X, Y, W, H); }
static void cls_juice_up(void *self, float amount, Val rot_amt) { Card_juice_up(self, amount, rot_amt); }
Class Card_class = {
    .name = "Card",
    .super = &Moveable_class,
    .update = cls_update,
    .move = cls_move,
    .draw = cls_draw,
    .remove = cls_remove,
    .hover = cls_hover,
    .stop_hover = cls_stop_hover,
    .click = cls_click,
    .release = cls_release,
    .hard_set_T = cls_hard_set_T,
    .juice_up = cls_juice_up,
};

// card.lua:155-157 (and 188-190, 214-216): child.states.hover/click/drag = self.states.hover/click/drag.
// note: the Lua shares the three tables; States is by value in node.h, so this copies them once. Whoever writes a
// card's states.hover / click / drag later must mirror them to the front, center and back sprites.
static void share_states(Card *self, Sprite *s) {
    s->moveable.node.states.hover = self->moveable.node.states.hover;
    s->moveable.node.states.click = self->moveable.node.states.click;
    s->moveable.node.states.drag = self->moveable.node.states.drag;
}

// Sprite(self.T.x, self.T.y, self.T.w, self.T.h, atlas, pos)
static Sprite *new_sprite(Card *self, Atlas *atlas, Table *pos) {
    Transform *T_ = self->moveable.node.T;
    Sprite *s = NEW(Sprite);
    Sprite_init(s, T_->x, T_->y, T_->w, T_->h, atlas, pos);
    return s;
}

// check_for_unlock({type = ...})
static void unlock_type(const char *type) { check_for_unlock(TAB("type", S(type))); }

// (center.discovered or self.bypass_discovery_center)
static bool disc_or_bypass(Card *self, Table *center) { return tbool(center, "discovered") || self->bypass_discovery_center; }

// card.lua:5
void Card_init(Card *self, float X, float Y, float W, float H, Table *card, Table *center, Table *params) {
    Node *node = &self->moveable.node;
    self->params = params ? params : tnew();

    Moveable_init(&self->moveable, X, Y, W, H);

    node->CT = &self->moveable.VT;
    node->config = TAB("card", T(card ? card : tnew()), "center", T(center));
    self->tilt_var.mx = 0, self->tilt_var.my = 0, self->tilt_var.dx = 0, self->tilt_var.dy = 0, self->tilt_var.amt = 0;
    self->ambient_tilt = 0.2f;

    node->states.collide.can = true;
    node->states.hover.can = true;
    node->states.drag.can = true;
    node->states.click.can = true;

    // note: playing_card is an int member, 0 as nil; a truthy non-number (no caller passes one) counts as 1
    Val pc = tget(self->params, "playing_card");
    self->playing_card = pc.t == V_NUM ? (int)pc.n : (truthy(pc) ? 1 : 0);
    G->sort_id = G->sort_id + 1;
    self->sort_id = G->sort_id;

    if (tbool(self->params, "viewed_back")) self->back = "viewed_back";
    else self->back = "selected_back";
    self->bypass_discovery_center = tbool(self->params, "bypass_discovery_center");
    self->bypass_discovery_ui = tbool(self->params, "bypass_discovery_ui");
    self->bypass_lock = tbool(self->params, "bypass_lock");
    self->no_ui = tbool(ttab(node->config, "card"), "no_ui");
    node->children = tnew();
    self->base_cost = 0;
    self->extra_cost = 0;
    self->cost = 0;
    self->sell_cost = 0;
    self->sell_cost_label = N(0);
    Moveable *shadow = NEW(Moveable);
    Moveable_init(shadow, 0, 0, 0, 0);
    tseto(node->children, "shadow", shadow);
    self->unique_val = 1 - (num)node->ID / 1603301;
    self->edition = NULL;
    self->moveable.zoom = true;
    Card_set_ability(self, center, true, false);
    Card_set_base(self, card, true);

    float r = 3.6f * ((float)math_random() - 0.5f); // three calls, in the order r, x, y
    float x = (float)math_random();
    float y = (float)math_random();
    self->discard_pos.r = r, self->discard_pos.x = x, self->discard_pos.y = y;

    self->facing = "front";
    self->sprite_facing = "front";
    self->flipping = NULL;
    self->area = NULL;
    self->highlighted = false;
    self->click_timeout = 0.3f;
    node->T->scale = 0.95f;
    self->debuff = false;

    self->rank = 0;
    self->added_to_deck = false;

    Sprite *front = Card_child(self, "front"), *back = Card_child(self, "back"), *centre = Card_child(self, "center");
    if (front) front->moveable.VT.w = 0;
    back->moveable.VT.w = 0;
    centre->moveable.VT.w = 0;

    // note: layered_parallax = nil cannot be held by a Vec2: {0, 0} stands for it. prep_draw then falls back to the
    // parent's (this card's), which is the area's and is {0, 0} for every card area of the game.
    if (front) front->moveable.node.parent = node, front->moveable.layered_parallax = (Vec2){0, 0};
    back->moveable.node.parent = node, back->moveable.layered_parallax = (Vec2){0, 0};
    centre->moveable.node.parent = node, centre->moveable.layered_parallax = (Vec2){0, 0};

    Card_set_cost(self);

    if (node->object.cls == &Card_class) {
        tinsert(G->I.CARD, O(self));
    }
}

// card.lua:79
void Card_update_alert(Card *self) {
    const char *set = Card_ab(self)->set;
    if (streq(set, "Joker") || streq(set, "Voucher") || Card_ab(self)->has_consumeable || streq(set, "Edition") || streq(set, "Booster")) {
        Table *center = Card_cen(self)->center;
        if (self->area && tbool(self->area->moveable.node.config, "collection") && center) {
            void *alert = Card_child(self, "alert");
            if (tbool(center, "alerted") && alert) {
                CALL(alert, remove);
                tset(self->moveable.node.children, "alert", NIL);
            } else if (!tbool(center, "alerted") && !alert && tbool(center, "discovered")) {
                bool odd = streq(set, "Voucher") && lmod(tnum(center, "order"), 2) == 1;
                tseto(self->moveable.node.children, "alert",
                      UIBox_new(TAB("definition", T(create_UIBox_card_alert(NULL)),
                                    "config", T(TAB("align", S(odd ? "tli" : "tri"),
                                                    "offset", T(TAB("x", N(odd ? 0.1 : -0.1), "y", N(0.1))),
                                                    "parent", O(self))))));
            }
        }
    }
}

// card.lua:97
void Card_set_base(Card *self, Table *card, bool initial) {
    Table *config = self->moveable.node.config;
    if (!card) card = tnew();

    tsett(config, "card", card);
    for (TIter it = titer(G->P_CARDS); tnext(&it);) {
        if (it.val.t == V_TAB && it.val.tab == card) tset(config, "card_key", it.key);
    }

    if (!tempty(card)) {
        Card_set_sprites(self, NULL, card);
    }

    Val suit_base_nominal_original = NIL;
    if (self->base && truthy(tget(self->base, "suit_nominal_original"))) suit_base_nominal_original = tget(self->base, "suit_nominal_original");
    float *colour = C_SUITS_get(tstr(card, "suit"));
    self->base = TAB("name", tget(card, "name"),
                     "suit", tget(card, "suit"),
                     "value", tget(card, "value"),
                     "nominal", N(0),
                     "suit_nominal", N(0),
                     "face_nominal", N(0),
                     "colour", P(colour),
                     "times_played", N(0));
    Table *base = self->base;

    // card.lua:122-134, the elseif chain as its table: value, nominal, face_nominal (0: not written), id
    static const struct { const char *value; num nominal, face_nominal, id; } ranks[] = {
        {"2", 2, 0, 2}, {"3", 3, 0, 3}, {"4", 4, 0, 4}, {"5", 5, 0, 5}, {"6", 6, 0, 6}, {"7", 7, 0, 7}, {"8", 8, 0, 8},
        {"9", 9, 0, 9}, {"10", 10, 0, 10}, {"Jack", 10, 0.1, 11}, {"Queen", 10, 0.2, 12}, {"King", 10, 0.3, 13}, {"Ace", 11, 0.4, 14}};
    const char *value = tstr(base, "value");
    for (size_t i = 0; i < sizeof(ranks) / sizeof(ranks[0]); i++) {
        if (streq(value, ranks[i].value)) {
            tsetn(base, "nominal", ranks[i].nominal);
            if (ranks[i].face_nominal != 0) tsetn(base, "face_nominal", ranks[i].face_nominal);
            tsetn(base, "id", ranks[i].id);
            break;
        }
    }

    if (initial) tset(base, "original_value", tget(base, "value"));

    const char *suit = tstr(base, "suit");
    bool had = truthy(suit_base_nominal_original);
    if (streq(suit, "Diamonds")) { tsetn(base, "suit_nominal", 0.01); tset(base, "suit_nominal_original", had ? suit_base_nominal_original : N(0.001)); }
    else if (streq(suit, "Clubs")) { tsetn(base, "suit_nominal", 0.02); tset(base, "suit_nominal_original", had ? suit_base_nominal_original : N(0.002)); }
    else if (streq(suit, "Hearts")) { tsetn(base, "suit_nominal", 0.03); tset(base, "suit_nominal_original", had ? suit_base_nominal_original : N(0.003)); }
    else if (streq(suit, "Spades")) { tsetn(base, "suit_nominal", 0.04); tset(base, "suit_nominal_original", had ? suit_base_nominal_original : N(0.004)); }

    if (!initial) Blind_debuff_card(tobj(G->GAME, "blind"), self, false);
    if (self->playing_card && !initial) unlock_type("modify_deck");
}

// card.lua:147
void Card_set_sprites(Card *self, Table *_center, Table *_front) {
    Table *children = self->moveable.node.children;
    if (_front) {
        Table *_pos = NULL;
        Atlas *_atlas = get_front_spriteinfo(_front, &_pos);
        Sprite *front = Card_child(self, "front");
        if (front) {
            front->atlas = _atlas;
            CALL(front, set_sprite_pos, _pos);
        } else {
            front = new_sprite(self, _atlas, _pos);
            tseto(children, "front", front);
            share_states(self, front);
            front->moveable.node.states.collide.can = false;
            Moveable_set_role(&front->moveable, TAB("major", O(self), "role_type", S("Glued"), "draw_major", O(self)));
        }
    }
    if (_center) {
        const char *set = tstr(_center, "set");
        Table *cfg_center = ttab(self->moveable.node.config, "center"); // self.config.center
        bool bypass = tbool(self->params, "bypass_discovery_center");
        if (set) {
            Sprite *centre = Card_child(self, "center");
            if (centre) {
                // (_center.atlas or (set == 'Joker' or consumeable or set == 'Voucher') and _center.set) or 'centers'
                const char *akey = tstr(_center, "atlas");
                if (!akey && (streq(set, "Joker") || tbool(_center, "consumeable") || streq(set, "Voucher"))) akey = set;
                if (!akey) akey = "centers";
                centre->atlas = ASSET_ATLAS_get(akey);
                CALL(centre, set_sprite_pos, ttab(_center, "pos"));
            } else {
                if (streq(set, "Joker") && !tbool(_center, "unlocked") && !bypass) {
                    centre = new_sprite(self, ASSET_ATLAS_get("Joker"), ttab(G->j_locked, "pos"));
                } else if (streq(tstr(cfg_center, "set"), "Voucher") && !tbool(cfg_center, "unlocked") && !bypass) {
                    centre = new_sprite(self, ASSET_ATLAS_get("Voucher"), ttab(G->v_locked, "pos"));
                } else if (tbool(cfg_center, "consumeable") && tbool(cfg_center, "demo")) {
                    centre = new_sprite(self, ASSET_ATLAS_get("Tarot"), ttab(G->c_locked, "pos"));
                } else if (!bypass && (streq(set, "Edition") || streq(set, "Joker") || tbool(_center, "consumeable") || streq(set, "Voucher") || streq(set, "Booster")) &&
                           !tbool(_center, "discovered")) {
                    Table *pos = streq(set, "Joker") ? ttab(G->j_undiscovered, "pos")
                               : streq(set, "Edition") ? ttab(G->j_undiscovered, "pos")
                               : streq(set, "Tarot") ? ttab(G->t_undiscovered, "pos")
                               : streq(set, "Planet") ? ttab(G->p_undiscovered, "pos")
                               : streq(set, "Spectral") ? ttab(G->s_undiscovered, "pos")
                               : streq(set, "Voucher") ? ttab(G->v_undiscovered, "pos")
                               : streq(set, "Booster") ? ttab(G->booster_undiscovered, "pos")
                               : NULL; // the Lua's false
                    centre = new_sprite(self, ASSET_ATLAS_get(tstr(_center, "atlas") ? tstr(_center, "atlas") : set), pos);
                } else if (streq(set, "Joker") || tbool(_center, "consumeable") || streq(set, "Voucher")) {
                    centre = new_sprite(self, ASSET_ATLAS_get(set), ttab(cfg_center, "pos"));
                } else {
                    centre = new_sprite(self, ASSET_ATLAS_get(tstr(_center, "atlas") ? tstr(_center, "atlas") : "centers"), ttab(_center, "pos"));
                }
                tseto(children, "center", centre);
                share_states(self, centre);
                centre->moveable.node.states.collide.can = false;
                Moveable_set_role(&centre->moveable, TAB("major", O(self), "role_type", S("Glued"), "draw_major", O(self)));
            }
            const char *name = tstr(_center, "name");
            if (streq(name, "Half Joker") && disc_or_bypass(self, _center)) {
                centre->scale.y = centre->scale.y / 1.7f;
            }
            if (streq(name, "Photograph") && disc_or_bypass(self, _center)) {
                centre->scale.y = centre->scale.y / 1.2f;
            }
            if (streq(name, "Square Joker") && disc_or_bypass(self, _center)) {
                centre->scale.y = centre->scale.x;
            }
        }

        if (truthy(tget(_center, "soul_pos"))) {
            // note: as the Lua, a previous floating_sprite is replaced without being removed
            Sprite *floating_sprite = new_sprite(self, ASSET_ATLAS_get("Joker"), ttab(cfg_center, "soul_pos"));
            tseto(children, "floating_sprite", floating_sprite);
            floating_sprite->moveable.role.draw_major = &self->moveable;
            floating_sprite->moveable.node.states.hover.can = false;
            floating_sprite->moveable.node.states.click.can = false;
        }

        if (!Card_child(self, "back")) {
            // self.params.bypass_back or (self.playing_card and G.GAME[self.back].pos or G.P_CENTERS['b_red'].pos)
            Table *pos = ttab(self->params, "bypass_back");
            if (!pos && self->playing_card) {
                Back *b = tobj(G->GAME, self->back);
                if (!b) ERROR("card.lua:213: G.GAME.%s is nil", self->back);
                pos = b->pos;
            }
            if (!pos) pos = ttab(ttab(G->P_CENTERS, "b_red"), "pos");
            Sprite *back = new_sprite(self, ASSET_ATLAS_get("centers"), pos);
            tseto(children, "back", back);
            share_states(self, back);
            back->moveable.node.states.collide.can = false;
            Moveable_set_role(&back->moveable, TAB("major", O(self), "role_type", S("Glued"), "draw_major", O(self)));
        }
    }
}

// card.lua:262 the delayed set_sprites
struct ev_set_sprites_ctx { Card *self; Table *center; };
static bool ev_set_sprites(void *ctx) {
    struct ev_set_sprites_ctx *c = ctx;
    // note: reads REMOVED of a card that may have been removed since; valid only while removed cards are not freed
    if (!c->self->moveable.node.REMOVED) {
        Card_set_sprites(c->self, c->center, NULL);
    }
    return true;
}

// center.config[key] or dflt
static Val cfg_or(Table *cfg, const char *key, Val dflt) {
    Val v = tget(cfg, key);
    return truthy(v) ? v : dflt;
}

// card.lua:223
void Card_set_ability(Card *self, Table *center, bool initial, bool delay_sprites) {
    Transform *T_ = self->moveable.node.T;
    Table *config = self->moveable.node.config;
    float X = T_->x, Y = T_->y, W = T_->w, H = T_->h;

    Table *old_center = ttab(config, "center");
    tsett(config, "center", center);
    self->sticker_run = NULL;
    for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
        if (it.val.t == V_TAB && it.val.tab == center) tset(config, "center_key", it.key);
    }

    if (tbool(self->params, "discover") && !tbool(center, "discovered")) {
        unlock_card(center);
        discover_card(center);
    }

    const char *cname = tstr(center, "name");
    if (streq(cname, "Half Joker") && disc_or_bypass(self, center)) {
        H = H / 1.7f;
        T_->h = H;
    }

    if (streq(cname, "Photograph") && disc_or_bypass(self, center)) {
        H = H / 1.2f;
        T_->h = H;
    }

    if (streq(cname, "Square Joker") && disc_or_bypass(self, center)) {
        H = W;
        T_->h = H;
    }

    if (streq(cname, "Wee Joker") && disc_or_bypass(self, center)) {
        H = H * 0.7f;
        W = W * 0.7f;
        T_->h = H;
        T_->w = W;
    }

    if (delay_sprites) {
        struct ev_set_sprites_ctx *c = NEWCTX(struct ev_set_sprites_ctx);
        c->self = self, c->center = center;
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_set_sprites, .ctx = c}), NULL, false);
    } else {
        Card_set_sprites(self, center, NULL);
    }

    Table *old = self->ability;
    if (old && old_center && truthy(tpath(old_center, "config.bonus"))) {
        tsetn(old, "bonus", tnum(old, "bonus") - vnum(tpath(old_center, "config.bonus")));
    }

    Table *cfg = ttab(center, "config");
    Val extra = tget(cfg, "extra"); // copy_table(center.config.extra) or nil
    if (extra.t == V_TAB) extra = T(copy_table(extra.tab));
    else if (!truthy(extra)) extra = NIL;
    self->ability = TAB("name", tget(center, "name"),
                        "effect", tget(center, "effect"),
                        "set", tget(center, "set"),
                        "mult", cfg_or(cfg, "mult", N(0)),
                        "h_mult", cfg_or(cfg, "h_mult", N(0)),
                        "h_x_mult", cfg_or(cfg, "h_x_mult", N(0)),
                        "h_dollars", cfg_or(cfg, "h_dollars", N(0)),
                        "p_dollars", cfg_or(cfg, "p_dollars", N(0)),
                        "t_mult", cfg_or(cfg, "t_mult", N(0)),
                        "t_chips", cfg_or(cfg, "t_chips", N(0)),
                        "x_mult", cfg_or(cfg, "Xmult", N(1)),
                        "h_size", cfg_or(cfg, "h_size", N(0)),
                        "d_size", cfg_or(cfg, "d_size", N(0)),
                        "extra", extra,
                        "extra_value", N(0),
                        "type", cfg_or(cfg, "type", S("")),
                        "order", cfg_or(center, "order", NIL),
                        "forced_selection", cfg_or(old, "forced_selection", NIL),
                        "perma_bonus", cfg_or(old, "perma_bonus", N(0)));
    Table *ability = self->ability;

    Val bonus = tget(ability, "bonus");
    tsetn(ability, "bonus", (truthy(bonus) ? vnum(bonus) : 0) + vnum(cfg_or(cfg, "bonus", N(0))));

    if (tbool(center, "consumeable")) {
        tsett(ability, "consumeable", cfg);
    }

    const char *name = tstr(ability, "name");
    if (streq(name, "Gold Card") && streq(self->seal, "Gold") && self->playing_card) {
        unlock_type("double_gold");
    }
    if (streq(name, "Invisible Joker")) {
        tsetn(ability, "invis_rounds", 0);
    }
    if (streq(name, "To Do List")) {
        // note: pairs(G.GAME.hands) is hash order in the Lua; here the insertion order of the table
        Table *_poker_hands = tnew();
        for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) {
            if (tbool(it.val.t == V_TAB ? it.val.tab : NULL, "visible")) tinsert(_poker_hands, it.key);
        }
        Val old_hand = tget(ability, "to_do_poker_hand");
        tset(ability, "to_do_poker_hand", NIL);

        while (!truthy(tget(ability, "to_do_poker_hand"))) {
            bool title = self->area && streq(tstr(self->area->moveable.node.config, "type"), "title");
            tset(ability, "to_do_poker_hand", pseudorandom_element(_poker_hands, N(pseudoseed(title ? "false_to_do" : "to_do", NULL)), NULL));
            if (veq(tget(ability, "to_do_poker_hand"), old_hand)) tset(ability, "to_do_poker_hand", NIL);
        }
    }
    if (streq(name, "Caino")) {
        tsetn(ability, "caino_xmult", 1);
    }
    if (streq(name, "Yorick")) {
        tset(ability, "yorick_discards", tget(ttab(ability, "extra"), "discards"));
    }
    if (streq(name, "Loyalty Card")) {
        tsetn(ability, "burnt_hand", 0);
        tset(ability, "loyalty_remaining", tget(ttab(ability, "extra"), "every"));
    }

    self->base_cost = vnum(cfg_or(center, "cost", N(1)));

    tset(ability, "hands_played_at_create", G->GAME ? cfg_or(G->GAME, "hands_played", N(0)) : N(0));

    const char *set = tstr(ability, "set");
    self->label = tstr(center, "label") ? tstr(center, "label") : tstr(ttab(config, "card"), "label") ? tstr(ttab(config, "card"), "label") : set;
    if (streq(set, "Joker")) self->label = name;
    if (streq(set, "Edition")) self->label = name;
    if (tbool(ability, "consumeable")) self->label = name;
    if (streq(set, "Voucher")) self->label = name;
    if (streq(set, "Booster")) {
        self->label = name;
        self->moveable.mouse_damping = 1.5f;
    }

    if (!G->OVERLAY_MENU) {
        for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
            if (streq(tstr(it.val.t == V_TAB ? it.val.tab : NULL, "name"), name)) {
                tsetv(ttab(G->GAME, "used_jokers"), it.key, B(true));
            }
        }
    }

    if (G->jokers && self->area == G->jokers) {
        unlock_type("modify_jokers");
    }

    if (G->consumeables && self->area == G->consumeables) {
        unlock_type("modify_jokers");
    }

    if (!initial) Blind_debuff_card(tobj(G->GAME, "blind"), self, false);
    if (self->playing_card && !initial) unlock_type("modify_deck");
}

// card.lua:369
void Card_set_cost(Card *self) {
    Table *GAME = G->GAME, *ability = self->ability, *edition = self->edition;
    const char *set = tstr(ability, "set");
    self->extra_cost = 0 + tnum(GAME, "inflation");
    if (edition) {
        self->extra_cost = self->extra_cost + (tbool(edition, "holo") ? 3 : 0) + (tbool(edition, "foil") ? 2 : 0) +
                           (tbool(edition, "polychrome") ? 5 : 0) + (tbool(edition, "negative") ? 5 : 0);
    }
    self->cost = lmax(1, floor((self->base_cost + self->extra_cost + 0.5) * (100 - tnum(GAME, "discount_percent")) / 100));
    if (streq(set, "Booster") && tbool(ttab(GAME, "modifiers"), "booster_ante_scaling")) self->cost = self->cost + tnum(ttab(GAME, "round_resets"), "ante") - 1;
    if (streq(set, "Booster") && !tbool(G->SETTINGS, "tutorial_complete") && tbool(G->SETTINGS, "tutorial_progress") &&
        !tbool(ttab(ttab(G->SETTINGS, "tutorial_progress"), "completed_parts"), "shop_1")) {
        self->cost = self->cost + 3;
    }
    if ((streq(set, "Planet") || (streq(set, "Booster") && str_find(tstr(ability, "name"), "Celestial", 1))) && tlen(find_joker("Astronomer", false)) > 0) self->cost = 0;
    if (tbool(ability, "rental")) self->cost = 1;
    Val extra_value = tget(ability, "extra_value");
    self->sell_cost = lmax(1, floor(self->cost / 2)) + (truthy(extra_value) ? vnum(extra_value) : 0);
    if (self->area && tbool(ability, "couponed") && (self->area == G->shop_jokers || self->area == G->shop_booster)) self->cost = 0;
    self->sell_cost_label = streq(self->facing, "back") ? S("?") : N(self->sell_cost);
}

// card.lua:438
static bool ev_edition_juice(void *ctx) {
    Card *self = ctx;
    CALL(self, juice_up, 1, N(0.5));
    if (tbool(self->edition, "foil")) play_sound("foil1", 1.2f, 0.4f);
    if (tbool(self->edition, "holo")) play_sound("holo1", 1.2f * 1.58f, 0.4f);
    if (tbool(self->edition, "polychrome")) play_sound("polychrome1", 1.2f, 0.7f);
    if (tbool(self->edition, "negative")) play_sound("negative", 1.5f, 0.4f);
    return true;
}
// card.lua:450
static bool ev_edition_unlock(void *ctx) {
    tsetb(G->CONTROLLER->locks, "edition", false);
    return true;
}

// G.P_CENTERS[key].discovered or discover_card(it) (card.lua:422-428, 578-584)
static void discover_edition(Card *self) {
    Table *c = ttab(G->P_CENTERS, self->edition ? fmt("e_%s", tstr(self->edition, "type")) : "e_base");
    if (!tbool(c, "discovered")) {
        discover_card(c);
    }
}

// card.lua:387
void Card_set_edition(Card *self, Table *edition, bool immediate, bool silent) {
    self->edition = NULL;
    if (!edition) return;
    if (tbool(edition, "holo")) {
        if (!self->edition) self->edition = tnew();
        tset(self->edition, "mult", tpath(G->P_CENTERS, "e_holo.config.extra"));
        tsetb(self->edition, "holo", true);
        tsets(self->edition, "type", "holo");
    } else if (tbool(edition, "foil")) {
        if (!self->edition) self->edition = tnew();
        tset(self->edition, "chips", tpath(G->P_CENTERS, "e_foil.config.extra"));
        tsetb(self->edition, "foil", true);
        tsets(self->edition, "type", "foil");
    } else if (tbool(edition, "polychrome")) {
        if (!self->edition) self->edition = tnew();
        tset(self->edition, "x_mult", tpath(G->P_CENTERS, "e_polychrome.config.extra"));
        tsetb(self->edition, "polychrome", true);
        tsets(self->edition, "type", "polychrome");
    } else if (tbool(edition, "negative")) {
        if (!self->edition) {
            self->edition = tnew();
            if (self->added_to_deck) { // Need to override if adding negative to an existing joker
                if (tbool(self->ability, "consumeable")) {
                    Table *c = G->consumeables->moveable.node.config;
                    tsetn(c, "card_limit", tnum(c, "card_limit") + 1);
                } else {
                    Table *c = G->jokers->moveable.node.config;
                    tsetn(c, "card_limit", tnum(c, "card_limit") + 1);
                }
            }
        }
        tsetb(self->edition, "negative", true);
        tsets(self->edition, "type", "negative");
    }

    if (self->area && self->area == G->jokers) {
        discover_edition(self);
    }

    if (self->edition && !silent) {
        tsetb(G->CONTROLLER->locks, "edition", true);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = !immediate ? 0.2 : 0, .not_blockable = immediate, .func = ev_edition_juice, .ctx = self}), NULL, false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_edition_unlock}), NULL, false);
    }

    if (G->jokers && self->area == G->jokers) {
        unlock_type("modify_jokers");
    }

    Card_set_cost(self);
}

// card.lua:478
static bool ev_seal_juice(void *ctx) {
    Card *self = ctx;
    CALL(self, juice_up, 0.3f, N(0.3));
    play_sound("gold_seal", 1.2f, 0.4f);
    return true;
}
// card.lua:487
static bool ev_seal_unlock(void *ctx) {
    tsetb(G->CONTROLLER->locks, "seal", false);
    return true;
}

// card.lua:464
void Card_set_seal(Card *self, const char *_seal, bool silent, bool immediate) {
    self->seal = NULL;
    if (_seal) {
        self->seal = str(_seal);
        if (!silent) {
            tsetb(G->CONTROLLER->locks, "seal", true);
            if (immediate) {
                CALL(self, juice_up, 0.3f, N(0.3));
                play_sound("gold_seal", 1.2f, 0.4f);
                tsetb(G->CONTROLLER->locks, "seal", false);
            } else {
                E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .func = ev_seal_juice, .ctx = self}), NULL, false);
                E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.15, .func = ev_seal_unlock}), NULL, false);
            }
        }
    }
    if (streq(tstr(self->ability, "name"), "Gold Card") && streq(self->seal, "Gold") && self->playing_card) {
        unlock_type("double_gold");
    }
    Card_set_cost(self);
}

// card.lua:501
const char *Card_get_seal(Card *self, bool bypass_debuff) {
    if (self->debuff && !bypass_debuff) return NULL;
    return self->seal;
}

// card.lua:506
void Card_set_eternal(Card *self, bool _eternal) {
    tset(self->ability, "eternal", NIL);
    if (tbool(ttab(self->moveable.node.config, "center"), "eternal_compat") && !tbool(self->ability, "perishable")) {
        tsetb(self->ability, "eternal", _eternal);
    }
}

// card.lua:513
void Card_set_perishable(Card *self, bool _perishable) {
    tset(self->ability, "perishable", NIL);
    if (tbool(ttab(self->moveable.node.config, "center"), "perishable_compat") && !tbool(self->ability, "eternal")) {
        tsetb(self->ability, "perishable", true);
        tset(self->ability, "perish_tally", tget(G->GAME, "perishable_rounds"));
    }
}

// card.lua:521
void Card_set_rental(Card *self, bool _rental) {
    tsetb(self->ability, "rental", _rental);
    Card_set_cost(self);
}

// card.lua:526
void Card_set_debuff(Card *self, bool should_debuff) {
    if (tbool(self->ability, "perishable") && tnum(self->ability, "perish_tally") <= 0) {
        if (!self->debuff) {
            self->debuff = true;
            if (self->area == G->jokers) Card_remove_from_deck(self, true);
        }
        return;
    }
    if (should_debuff != self->debuff) {
        if (self->area == G->jokers) { if (should_debuff) Card_remove_from_deck(self, true); else Card_add_to_deck(self, true); }
        self->debuff = should_debuff;
    }
}

// card.lua:540
void Card_remove_UI(Card *self) {
    tset(self->moveable.node.object.dyn, "ability_UIBox_table", NIL);
    tset(self->moveable.node.config, "h_popup", NIL);
    tset(self->moveable.node.config, "h_popup_config", NIL);
    self->no_ui = true;
}

// card.lua:547
void Card_change_suit(Card *self, const char *new_suit) {
    const char *new_code = streq(new_suit, "Diamonds") ? "D_" :
                           streq(new_suit, "Spades") ? "S_" :
                           streq(new_suit, "Clubs") ? "C_" :
                           streq(new_suit, "Hearts") ? "H_" : NULL;
    const char *value = tstr(self->base, "value");
    const char *new_val = streq(value, "Ace") ? "A" :
                          streq(value, "King") ? "K" :
                          streq(value, "Queen") ? "Q" :
                          streq(value, "Jack") ? "J" :
                          streq(value, "10") ? "T" :
                          value;
    if (!new_code || !new_val) ERROR("card.lua:558: attempt to concatenate a %s value", new_code ? "nil" : "boolean");
    Table *new_card = ttab(G->P_CARDS, fmt("%s%s", new_code, new_val));

    Card_set_base(self, new_card, false);
    Blind_debuff_card(tobj(G->GAME, "blind"), self, false);
}

// card.lua:617
static bool ev_set_costs(void *ctx) {
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        // v.set_cost: G.I.CARD also holds the Blind and the Card_Characters, which have none
        if (is_a(it.val.obj, &Card_class)) Card_set_cost(it.val.obj);
    }
    return true;
}
// card.lua:641
static bool ev_reset_blind(void *ctx) {
    Blind_set_blind(tobj(G->GAME, "blind"), NULL, true, false);
    return true;
}

// card.lua:564
void Card_add_to_deck(Card *self, bool from_debuff) {
    Table *center = ttab(self->moveable.node.config, "center"), *ability = self->ability, *GAME = G->GAME;
    if (!tbool(center, "discovered")) {
        discover_card(center);
    }
    if (!self->added_to_deck) {
        self->added_to_deck = true;
        const char *set = tstr(ability, "set"), *name = tstr(ability, "name");
        if (streq(set, "Enhanced") || streq(set, "Default")) {
            if (streq(name, "Gold Card") && streq(self->seal, "Gold") && self->playing_card) {
                unlock_type("double_gold");
            }
            return;
        }

        discover_edition(self);
        if (tnum(ability, "h_size") != 0) {
            CardArea_change_size(G->hand, tnum(ability, "h_size"));
        }
        if (tnum(ability, "d_size") > 0) {
            Table *round_resets = ttab(GAME, "round_resets");
            tsetn(round_resets, "discards", tnum(round_resets, "discards") + tnum(ability, "d_size"));
            ease_discard(tnum(ability, "d_size"), false, false);
        }
        if (streq(name, "Credit Card")) {
            tsetn(GAME, "bankrupt_at", tnum(GAME, "bankrupt_at") - tnum(ability, "extra"));
        }
        Blind *blind = tobj(GAME, "blind");
        if (streq(name, "Chicot") && blind && blind->boss && !blind->disabled) {
            Blind_disable(blind);
            play_sound("timpani", -1, -1);
            card_eval_status_text(self, "extra", 0, -1, NULL, TAB("message", localize(S("ph_boss_disabled"), NULL)));
        }
        if (streq(name, "Chaos the Clown")) {
            Table *current_round = ttab(GAME, "current_round");
            tsetn(current_round, "free_rerolls", tnum(current_round, "free_rerolls") + 1);
            calculate_reroll_cost(true);
        }
        if (streq(name, "Turtle Bean")) {
            CardArea_change_size(G->hand, tnum(ttab(ability, "extra"), "h_size"));
        }
        if (streq(name, "Oops! All 6s")) {
            Table *probabilities = ttab(GAME, "probabilities");
            for (TIter it = titer(probabilities); tnext(&it);) {
                tsetv(probabilities, it.key, N(vnum(it.val) * 2));
            }
        }
        if (streq(name, "To the Moon")) {
            tsetn(GAME, "interest_amount", tnum(GAME, "interest_amount") + tnum(ability, "extra"));
        }
        if (streq(name, "Astronomer")) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_set_costs}), NULL, false);
        }
        if (streq(name, "Troubadour")) {
            Table *round_resets = ttab(GAME, "round_resets");
            CardArea_change_size(G->hand, tnum(ttab(ability, "extra"), "h_size"));
            tsetn(round_resets, "hands", tnum(round_resets, "hands") + tnum(ttab(ability, "extra"), "h_plays"));
        }
        if (streq(name, "Stuntman")) {
            CardArea_change_size(G->hand, -tnum(ttab(ability, "extra"), "h_size"));
        }
        if (self->edition && tbool(self->edition, "negative")) {
            if (from_debuff) {
                tset(ability, "queue_negative_removal", NIL);
            } else {
                if (tbool(ability, "consumeable")) {
                    Table *c = G->consumeables->moveable.node.config;
                    tsetn(c, "card_limit", tnum(c, "card_limit") + 1);
                } else {
                    Table *c = G->jokers->moveable.node.config;
                    tsetn(c, "card_limit", tnum(c, "card_limit") + 1);
                }
            }
        }
        if (blind) E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_reset_blind}), NULL, false);
    }
}
