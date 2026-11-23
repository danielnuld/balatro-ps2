// tag.lua (1-595): Tag, the reward for skipping a blind.
#include "all.h"

static void cls_juice_up(void *self, float amount, Val rot_amt) { Tag_juice_up(self, amount, rot_amt); }
static void cls_remove(void *self) { Tag_remove(self); }
static void cls_sprite_hover(void *self) { Tag_sprite_hover(self); }
static void cls_sprite_stop_hover(void *self) { Tag_sprite_stop_hover(self); }

// tag.lua:2
Class Tag_class = {.name = "Tag", .super = &Object_class, .juice_up = cls_juice_up, .remove = cls_remove};
// tag.lua:513, 537: the class of a tag's sprite (see tag.h)
Class Tag_sprite_class = {.name = "Sprite", .super = &Sprite_class, .hover = cls_sprite_hover, .stop_hover = cls_sprite_stop_hover};

// What the closures of this file capture (each uses the members it needs)
struct tag_ctx {
    Tag *self;
    int lock;
    Card *card;
    Table *_context;
    const char *key;      // a booster's key (or its prefix when rnd), or the edition's name
    bool rnd;             // the key ends in math.random(1, 2)
    const char *message;
    const float *_colour;
};

static struct tag_ctx *ctx_new(Tag *self) {
    struct tag_ctx *c = NEWCTX(struct tag_ctx);
    c->self = self;
    c->lock = self->ID;
    return c;
}

// self.HUD_tag, which the Lua indexes without a test
static UIBox *hud_tag(Tag *self) {
    if (!self->HUD_tag) ERROR("tag.lua: attempt to index field 'HUD_tag' (a nil value)");
    return self->HUD_tag;
}

// '[' .. localize('k_poker_hand') .. ']'
static const char *orbital_placeholder(void) { return fmt("[%s]", vstr(localize(S("k_poker_hand"), NULL))); }

// tag.lua:5
void Tag_init(Tag *self, const char *_tag, bool for_collection, const char *_blind_type) {
    self->key = str(_tag);
    Table *proto = ttab(G->P_TAGS, _tag);
    if (!proto) proto = G->tag_undiscovered;
    self->config = tcopy(ttab(proto, "config"));
    self->pos = ttab(proto, "pos");
    self->name = tstr(proto, "name");
    self->tally = tnum(G->GAME, "tag_tally"); // G.GAME.tag_tally or 0
    self->triggered = false;
    self->ID = G->tagid; // G.tagid = G.tagid or 0: 0 as nil
    G->tagid = G->tagid + 1;
    self->ability = TAB("orbital_hand", S(orbital_placeholder()), "blind_type", S(_blind_type));
    Val tag_tally = tget(G->GAME, "tag_tally");
    tsetn(G->GAME, "tag_tally", truthy(tag_tally) ? (tag_tally.n + 1) : 1);
    if (!for_collection) Tag_set_ability(self);
}

// tag.lua:28
static bool ev_nope_text(void *ctx) {
    struct tag_ctx *c = ctx;
    attention_text(TAB("text", S("NOPE"), "colour", P(G->C.WHITE), "scale", N(0.7), "hold",
                       N(0.3 / tnum(G->SETTINGS, "GAMESPEED")), "cover", O(c->self->HUD_tag), "cover_colour", P(G->C.BLACK),
                       "align", S("cm")));
    play_sound("cancel", 1.4f, 0.5f);
    return true;
}

// tag.lua:45
static bool ev_nope_hide(void *ctx) {
    struct tag_ctx *c = ctx;
    hud_tag(c->self)->moveable.node.states.visible = false;
    play_sound("cancel", 1.26f, 0.5f);
    return true;
}

// tag.lua:55, 96
static bool ev_remove(void *ctx) {
    struct tag_ctx *c = ctx;
    Tag_remove(c->self);
    return true;
}

// tag.lua:24
void Tag_nope(Tag *self) {
    struct tag_ctx *c = ctx_new(self);
    E_add(G->E_MANAGER, Event_new((EventCfg){.delay = 0.2, .trigger = "after", .func = ev_nope_text, .ctx = c}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_nope_hide, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.5, .func = ev_remove, .ctx = c}), NULL, false);
}

// tag.lua:68
static bool ev_yep_text(void *ctx) {
    struct tag_ctx *c = ctx;
    attention_text(TAB("text", S(c->message), "colour", P(G->C.WHITE), "scale", N(1), "hold",
                       N(0.3 / tnum(G->SETTINGS, "GAMESPEED")), "cover", O(c->self->HUD_tag), "cover_colour",
                       P((void *)(c->_colour ? c->_colour : G->C.GREEN)), "align", S("cm")));
    play_sound("generic1", 0.9 + math_random() * 0.1, 0.8f);
    play_sound("holo1", 1.2 + math_random() * 0.1, 0.4f);
    return true;
}

// tag.lua:84
static bool ev_yep_hide(void *ctx) {
    struct tag_ctx *c = ctx;
    hud_tag(c->self)->moveable.node.states.visible = false;
    return true;
}

// tag.lua:62
void Tag_yep(Tag *self, const char *message, const float *_colour, bool (*func)(void *ctx), void *ctx) {
    stop_use();

    struct tag_ctx *c = ctx_new(self);
    c->message = message;
    c->_colour = _colour;
    E_add(G->E_MANAGER, Event_new((EventCfg){.delay = 0.4, .trigger = "after", .func = ev_yep_text, .ctx = c}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_yep_hide, .ctx = c}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = func, .ctx = ctx}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.7, .func = ev_remove, .ctx = c}), NULL, false);
}

// tag.lua:103
void Tag_set_ability(Tag *self) {
    if (streq(self->name, "Orbital Tag")) {
        if (G->orbital_hand) {
            tsets(self->ability, "orbital_hand", G->orbital_hand);
        } else if (truthy(tget(self->ability, "blind_type"))) {
            Table *orbital_choices = ttab(G->GAME, "orbital_choices");
            if (orbital_choices) {
                // G.GAME.orbital_choices[G.GAME.round_resets.ante][self.ability.blind_type]
                Val choice = tget(tgetv(orbital_choices, tget(ttab(G->GAME, "round_resets"), "ante")).tab,
                                  tstr(self->ability, "blind_type"));
                if (truthy(choice)) tset(self->ability, "orbital_hand", choice);
            }
        }
    }
}

// ---------------------------------------------------------------- the closures of apply_to_run
static void unlock(struct tag_ctx *c) { tseti(G->CONTROLLER->locks, c->lock, NIL); } // G.CONTROLLER.locks[lock] = nil

// tag.lua:120, 336
static bool ev_noop(void *ctx) { return true; }

// tag.lua:135
static bool ev_topup(void *ctx) {
    struct tag_ctx *c = ctx;
    num spawn_jokers = tnum(c->self->config, "spawn_jokers");
    for (num i = 1; i <= spawn_jokers; i++) {
        if (G->jokers && CardArea_n(G->jokers) < tnum(G->jokers->moveable.node.config, "card_limit")) {
            Card *card = create_card("Joker", G->jokers, false, N(0), false, false, NULL, "top");
            Card_add_to_deck(card, false);
            CardArea_emplace(G->jokers, card, NULL, false);
        }
    }
    unlock(c);
    return true;
}

// tag.lua:150, 159, 168, 177, 199
static bool ev_unlock(void *ctx) {
    unlock(ctx);
    return true;
}

// tag.lua:183
static bool ev_economy(void *ctx) {
    struct tag_ctx *c = ctx;
    ease_dollars(lmin(tnum(c->self->config, "max"), lmax(0, tnum(G->GAME, "dollars"))), true);
    return true;
}

// tag.lua:210, 225, 240, 255, 270: the five booster tags, which differ in the key only
static bool ev_pack(void *ctx) {
    struct tag_ctx *c = ctx;
    const char *key = c->rnd ? fmt("%s%d", c->key, (int)math_random_range(1, 2)) : c->key;
    Transform *PT = G->play->moveable.node.T;
    Card *card = NEW(Card);
    Card_init(card, PT->x + PT->w / 2 - G->CARD_W * 1.27 / 2, PT->y + PT->h / 2 - G->CARD_H * 1.27 / 2, G->CARD_W * 1.27,
              G->CARD_H * 1.27, ttab(G->P_CARDS, "empty"), ttab(G->P_CENTERS, key),
              TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
    card->cost = 0;
    tsetb(card->moveable.node.object.dyn, "from_tag", true);
    FUNCS_use_card(NULL, card, false, false); // G.FUNCS.use_card({config = {ref_table = card}})
    Card_start_materialize(card, NULL, false, -1);
    unlock(c);
    return true;
}

// tag.lua:292
static bool ev_boss_unlock_outer(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_unlock, .ctx = ctx}), NULL, false);
    return true;
}

// tag.lua:287
static bool ev_boss(void *ctx) {
    G->from_boss_tag = true;
    FUNCS_reroll_boss(NULL);

    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_boss_unlock_outer, .ctx = ctx}), NULL, false);

    return true;
}

// tag.lua:304
static bool ev_voucher(void *ctx) {
    Table *voucher_tag = ttab(G->ARGS, "voucher_tag");
    if (!voucher_tag) {
        voucher_tag = tnew();
        tsett(G->ARGS, "voucher_tag", voucher_tag);
    }
    const char *voucher_key = get_next_voucher_key(true);
    tsetb(voucher_tag, voucher_key, true);
    Table *config = G->shop_vouchers->moveable.node.config;
    tsetn(config, "card_limit", tnum(config, "card_limit") + 1);
    Transform *VT = G->shop_vouchers->moveable.node.T;
    Card *card = NEW(Card);
    Card_init(card, VT->x + VT->w / 2, VT->y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"), ttab(G->P_CENTERS, voucher_key),
              TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
    create_shop_card_ui(card, "Voucher", G->shop_vouchers);
    Card_start_materialize(card, NULL, false, -1);
    CardArea_emplace(G->shop_vouchers, card, NULL, false);
    tset(G->ARGS, "voucher_tag", NIL);
    return true;
}

// tag.lua:323
static bool ev_double(void *ctx) {
    struct tag_ctx *c = ctx;
    Tag *tag = tobj(c->_context, "tag");
    if (tag->ability && truthy(tget(tag->ability, "orbital_hand"))) {
        G->orbital_hand = tstr(tag->ability, "orbital_hand");
    }
    Tag *new_tag = NEW(Tag);
    Tag_init(new_tag, tag->key, false, NULL);
    add_tag(new_tag);
    G->orbital_hand = NULL;
    unlock(c);
    return true;
}

// tag.lua:359, 373
static bool ev_store_joker(void *ctx) {
    struct tag_ctx *c = ctx;
    Card_start_materialize(c->card, NULL, false, -1);
    tsetb(c->card->ability, "couponed", true);
    Card_set_cost(c->card);
    return true;
}

// tag.lua:385
static bool ev_d6(void *ctx) {
    tsetn(ttab(G->GAME, "round_resets"), "temp_reroll_cost", 0);
    calculate_reroll_cost(true);
    return true;
}

// tag.lua:400, 411, 422, 433: c->key is the edition. Foil clears temp_edition after set_cost, the others first.
static bool ev_edition(void *ctx) {
    struct tag_ctx *c = ctx;
    Card *card = tobj(c->_context, "card");
    Table *dyn = card->moveable.node.object.dyn;
    bool foil = streq(c->key, "foil");
    if (!foil) tset(dyn, "temp_edition", NIL);
    Card_set_edition(card, TAB(c->key, B(true)), true, false);
    tsetb(card->ability, "couponed", true);
    Card_set_cost(card);
    if (foil) tset(dyn, "temp_edition", NIL);
    unlock(c);
    return true;
}

// tag.lua:450
static bool ev_coupon(void *ctx) {
    if (G->shop_jokers && G->shop_booster) {
        for (TIter it = titer(G->shop_jokers->cards); tnext(&it);) {
            Card *v = it.val.obj;
            tsetb(v->ability, "couponed", true);
            Card_set_cost(v);
        }
        for (TIter it = titer(G->shop_booster->cards); tnext(&it);) {
            Card *v = it.val.obj;
            tsetb(v->ability, "couponed", true);
            Card_set_cost(v);
        }
    }
    return true;
}

// tag.lua:115
Val Tag_apply_to_run(Tag *self, Table *_context) {
    if (!self->triggered && veq(tget(self->config, "type"), tget(_context, "type"))) {
        const char *type = tstr(_context, "type");
        if (streq(type, "eval")) {
            if (streq(self->name, "Investment Tag") && tbool(ttab(G->GAME, "last_blind"), "boss")) {
                Tag_yep(self, "+", G->C.GOLD, ev_noop, NULL);
                self->triggered = true;
                return T(TAB("dollars", tget(self->config, "dollars"), "condition", localize(S("ph_defeat_the_boss"), NULL),
                             "pos", T(self->pos), "tag", O(self)));
            }
        } else if (streq(type, "immediate")) {
            struct tag_ctx *c = ctx_new(self); // local lock = self.ID
            tseti(G->CONTROLLER->locks, c->lock, B(true));
            if (streq(self->name, "Top-up Tag")) {
                Tag_yep(self, "+", G->C.PURPLE, ev_topup, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Skip Tag")) {
                Tag_yep(self, "+", G->C.MONEY, ev_unlock, c);
                ease_dollars(tnum(G->GAME, "skips") * tnum(self->config, "skip_bonus"), false);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Garbage Tag")) {
                Tag_yep(self, "+", G->C.MONEY, ev_unlock, c);
                ease_dollars(tnum(G->GAME, "unused_discards") * tnum(self->config, "dollars_per_discard"), false);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Handy Tag")) {
                Tag_yep(self, "+", G->C.MONEY, ev_unlock, c);
                ease_dollars(tnum(G->GAME, "hands_played") * tnum(self->config, "dollars_per_hand"), false);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Economy Tag")) {
                Tag_yep(self, "+", G->C.MONEY, ev_unlock, c);
                E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_economy, .ctx = c}), NULL, false);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Orbital Tag")) {
                const char *orbital_hand = tstr(self->ability, "orbital_hand");
                Table *hand = ttab(ttab(G->GAME, "hands"), orbital_hand);
                if (!hand) ERROR("tag.lua:194: attempt to index G.GAME.hands['%s'] (a nil value)", orbital_hand);
                update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.8), "delay", N(0.3)),
                                 TAB("handname", S(orbital_hand), "chips", tget(hand, "chips"), "mult", tget(hand, "mult"),
                                     "level", tget(hand, "level")));
                level_up_hand(self, orbital_hand, false, tget(self->config, "levels"));
                update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(1.1), "delay", N(0)),
                                 TAB("mult", N(0), "chips", N(0), "handname", S(""), "level", S("")));
                Tag_yep(self, "+", G->C.MONEY, ev_unlock, c);
                self->triggered = true;
                return B(true);
            }
        } else if (streq(type, "new_blind_choice")) {
            struct tag_ctx *c = ctx_new(self); // local lock = self.ID
            tseti(G->CONTROLLER->locks, c->lock, B(true));
            if (streq(self->name, "Charm Tag")) {
                c->key = "p_arcana_mega_", c->rnd = true;
                Tag_yep(self, "+", G->C.PURPLE, ev_pack, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Meteor Tag")) {
                c->key = "p_celestial_mega_", c->rnd = true;
                Tag_yep(self, "+", G->C.SECONDARY_SET.Planet, ev_pack, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Ethereal Tag")) {
                c->key = "p_spectral_normal_1";
                Tag_yep(self, "+", G->C.SECONDARY_SET.Spectral, ev_pack, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Standard Tag")) {
                c->key = "p_standard_mega_1";
                Tag_yep(self, "+", G->C.SECONDARY_SET.Spectral, ev_pack, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Buffoon Tag")) {
                c->key = "p_buffoon_mega_1";
                Tag_yep(self, "+", G->C.SECONDARY_SET.Spectral, ev_pack, c);
                self->triggered = true;
                return B(true);
            }
            if (streq(self->name, "Boss Tag")) {
                tseti(G->CONTROLLER->locks, c->lock, B(true)); // the Lua's second `local lock`, the same ID
                Tag_yep(self, "+", G->C.GREEN, ev_boss, c);
                self->triggered = true;
                return B(true);
            }
        } else if (streq(type, "voucher_add")) {
            if (streq(self->name, "Voucher Tag")) {
                Tag_yep(self, "+", G->C.SECONDARY_SET.Voucher, ev_voucher, NULL);
                self->triggered = true;
            }
        } else if (streq(type, "tag_add")) {
            if (streq(self->name, "Double Tag") && !streq(((Tag *)tobj(_context, "tag"))->key, "tag_double")) {
                struct tag_ctx *c = ctx_new(self);
                c->_context = _context;
                tseti(G->CONTROLLER->locks, c->lock, B(true));
                Tag_yep(self, "+", G->C.BLUE, ev_double, c);
                self->triggered = true;
            }
        } else if (streq(type, "round_start_bonus")) {
            if (streq(self->name, "Juggle Tag")) {
                Tag_yep(self, "+", G->C.BLUE, ev_noop, NULL);
                CardArea_change_size(G->hand, tnum(self->config, "h_size"));
                Table *round_resets = ttab(G->GAME, "round_resets");
                tsetn(round_resets, "temp_handsize", tnum(round_resets, "temp_handsize") + tnum(self->config, "h_size"));
                self->triggered = true;
                return B(true);
            }
        } else if (streq(type, "store_joker_create")) {
            Card *card = NULL;
            CardArea *area = tobj(_context, "area");
            struct tag_ctx *c = ctx_new(self);
            if (streq(self->name, "Rare Tag")) {
                Table *rares_in_posession = ARR(N(0));
                for (int k = 1; k <= CardArea_n(G->jokers); k++) {
                    Table *center = ttab(CardArea_card(G->jokers, k)->moveable.node.config, "center");
                    if (veq(tget(center, "rarity"), N(3)) && !truthy(tget(rares_in_posession, tstr(center, "key")))) {
                        tseti(rares_in_posession, 1, N(tnumi(rares_in_posession, 1) + 1));
                        tsetb(rares_in_posession, tstr(center, "key"), true);
                    }
                }

                if (tlen(ttabi(G->P_JOKER_RARITY_POOLS, 3)) > tnumi(rares_in_posession, 1)) {
                    card = create_card("Joker", area, false, N(1), false, false, NULL, "rta");
                    create_shop_card_ui(card, "Joker", area);
                    card->moveable.node.states.visible = false;
                    c->card = card;
                    Tag_yep(self, "+", G->C.RED, ev_store_joker, c);
                } else {
                    Tag_nope(self);
                }
                self->triggered = true;
            } else if (streq(self->name, "Uncommon Tag")) {
                card = create_card("Joker", area, false, N(0.9), false, false, NULL, "uta");
                create_shop_card_ui(card, "Joker", area);
                card->moveable.node.states.visible = false;
                c->card = card;
                Tag_yep(self, "+", G->C.GREEN, ev_store_joker, c);
            }
            self->triggered = true;
            return O(card);
        } else if (streq(type, "shop_start")) {
            if (streq(self->name, "D6 Tag") && !tbool(G->GAME, "shop_d6ed")) {
                tsetb(G->GAME, "shop_d6ed", true);
                Tag_yep(self, "+", G->C.GREEN, ev_d6, NULL);
                self->triggered = true;
                return B(true);
            }
        } else if (streq(type, "store_joker_modify")) {
            bool _applied = false;
            Card *card = tobj(_context, "card");
            Table *dyn = card->moveable.node.object.dyn;
            if (!card->edition && !tbool(dyn, "temp_edition") && streq(tstr(card->ability, "set"), "Joker")) {
                struct tag_ctx *c = ctx_new(self); // local lock = self.ID
                c->_context = _context;
                tseti(G->CONTROLLER->locks, c->lock, B(true));
                if (streq(self->name, "Foil Tag")) {
                    c->key = "foil";
                } else if (streq(self->name, "Holographic Tag")) {
                    c->key = "holo";
                } else if (streq(self->name, "Polychrome Tag")) {
                    c->key = "polychrome";
                } else if (streq(self->name, "Negative Tag")) {
                    c->key = "negative";
                }
                if (c->key) { // the four branches of the Lua, which differ in the edition only (and ev_edition's order)
                    tsetb(dyn, "temp_edition", true);
                    Tag_yep(self, "+", G->C.DARK_EDITION, ev_edition, c);
                    _applied = true;
                }
                self->triggered = true;
            }

            return _applied ? B(true) : NIL;
        } else if (streq(type, "shop_final_pass")) {
            if (streq(self->name, "Coupon Tag") && (G->shop && !tbool(G->GAME, "shop_free"))) {
                tsetb(G->GAME, "shop_free", true);
                Tag_yep(self, "+", G->C.GREEN, ev_coupon, NULL);
                self->triggered = true;
                return B(true);
            }
        }
    }
    return NIL;
}

// tag.lua:470
Table *Tag_save(Tag *self) {
    return TAB("key", S(self->key), "tally", N(self->tally), "ability", T(self->ability));
}

// tag.lua:478
void Tag_load(Tag *self, Table *tag_savetable) {
    self->key = tstr(tag_savetable, "key");
    Table *proto = ttab(G->P_TAGS, self->key);
    if (!proto) proto = G->tag_undiscovered;
    self->config = tcopy(ttab(proto, "config"));
    self->pos = ttab(proto, "pos");
    self->name = tstr(proto, "name");
    self->tally = tnum(tag_savetable, "tally");
    self->ability = ttab(tag_savetable, "ability");
    // note: a nil G.GAME.tag_tally is an error in the Lua's math.max; here it counts as 0
    tsetn(G->GAME, "tag_tally", lmax(self->tally, tnum(G->GAME, "tag_tally")) + 1);
}

// tag.lua:489
void Tag_juice_up(Tag *self, float _scale, Val _rot) {
    if (self->tag_sprite) CALL(self->tag_sprite, juice_up, _scale, _rot);
}

// tag.lua:493
Table *Tag_generate_UI(Tag *self, float _size, Sprite **out_tag_sprite) {
    if (_size < 0) _size = 0.8f;

    Table *tag_sprite_tab = NULL;

    Sprite *tag_sprite = NEW(Sprite);
    Sprite_init(tag_sprite, 0, 0, _size * 1, _size * 1, ASSET_ATLAS_get("tags"),
                self->hide_ability ? ttab(G->tag_undiscovered, "pos") : self->pos);
    tag_sprite->moveable.node.T->scale = 1;
    tag_sprite_tab = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "ref_table", O(self), "group", N(self->tally))),
                         "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("w", N(_size * 1), "h", N(_size * 1), "colour", P(G->C.BLUE), "object", O(tag_sprite), "focus_with_object", B(true)))))
    )));
    Sprite_define_draw_steps(tag_sprite, ARR(
        T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
        T(TAB("shader", S("dissolve")))
    ));
    tsetb(tag_sprite->moveable.node.object.dyn, "float", true);
    tag_sprite->moveable.node.states.hover.can = true;
    tag_sprite->moveable.node.states.drag.can = false;
    tag_sprite->moveable.node.states.collide.can = true;
    tag_sprite->moveable.node.config = TAB("tag", O(self), "force_focus", B(true));

    // tag.lua:513, 537: tag_sprite.hover / stop_hover = function... (the sprite gets the class that has them: tag.h)
    if (!Tag_sprite_class.ready) class_init(&Tag_sprite_class);
    tag_sprite->moveable.node.object.cls = &Tag_sprite_class;

    CALL(tag_sprite, juice_up, -1, NIL);
    self->tag_sprite = tag_sprite;

    if (out_tag_sprite) *out_tag_sprite = tag_sprite;
    return tag_sprite_tab;
}

// tag.lua:513
void Tag_sprite_hover(Sprite *_self) {
    Tag *self = tobj(_self->moveable.node.config, "tag");
    Sprite *tag_sprite = _self; // the closure's upvalue is this same sprite
    Table *dyn = _self->moveable.node.object.dyn;
    if (!G->CONTROLLER->dragging.target || tbool(G->CONTROLLER->object.dyn, "using_touch")) {
        if (!tbool(dyn, "hovering") && _self->moveable.node.states.visible) {
            tsetb(dyn, "hovering", true);
            if (_self == tag_sprite) {
                tsetn(dyn, "hover_tilt", 3);
                CALL(_self, juice_up, 0.05f, N(0.02));
                play_sound("paper1", math_random() * 0.1 + 0.55, 0.42f);
                play_sound("tarot2", math_random() * 0.1 + 0.55, 0.09f);
            }

            Tag_get_uibox_table(self, tag_sprite);
            Table *config = _self->moveable.node.config;
            tsett(config, "h_popup", UIDEF_card_h_popup((Node *)_self));
            tsett(config, "h_popup_config", TAB("align", S("cl"), "offset", T(TAB("x", N(-0.1), "y", N(0))), "parent", O(_self)));
            Node_hover((Node *)_self);
            void *alert = tobj(_self->moveable.node.children, "alert");
            if (alert) {
                CALL(alert, remove);
                tset(_self->moveable.node.children, "alert", NIL);
                if (self->key && ttab(G->P_TAGS, self->key)) tsetb(ttab(G->P_TAGS, self->key), "alerted", true);
                Game_save_progress(G);
            }
        }
    }
}

// tag.lua:537
void Tag_sprite_stop_hover(Sprite *_self) {
    tsetb(_self->moveable.node.object.dyn, "hovering", false);
    Node_stop_hover((Node *)_self);
    tsetn(_self->moveable.node.object.dyn, "hover_tilt", 0);
}

// tag.lua:545
Sprite *Tag_get_uibox_table(Tag *self, Sprite *tag_sprite) {
    if (!tag_sprite) tag_sprite = self->tag_sprite;
    const char *name_to_check = self->name;
    Table *loc_vars = tnew();
    Table *cfg = self->config;
    if (streq(name_to_check, "Uncommon Tag")) {
    } else if (streq(name_to_check, "Investment Tag")) {
        loc_vars = ARR(tget(cfg, "dollars"));
    } else if (streq(name_to_check, "Handy Tag")) {
        loc_vars = ARR(tget(cfg, "dollars_per_hand"), N(tnum(cfg, "dollars_per_hand") * tnum(G->GAME, "hands_played")));
    } else if (streq(name_to_check, "Garbage Tag")) {
        loc_vars = ARR(tget(cfg, "dollars_per_discard"), N(tnum(cfg, "dollars_per_discard") * tnum(G->GAME, "unused_discards")));
    } else if (streq(name_to_check, "Juggle Tag")) {
        loc_vars = ARR(tget(cfg, "h_size"));
    } else if (streq(name_to_check, "Top-up Tag")) {
        loc_vars = ARR(tget(cfg, "spawn_jokers"));
    } else if (streq(name_to_check, "Skip Tag")) {
        loc_vars = ARR(tget(cfg, "skip_bonus"), N(tnum(cfg, "skip_bonus") * (tnum(G->GAME, "skips") + 1)));
    } else if (streq(name_to_check, "Orbital Tag")) {
        const char *orbital_hand = tstr(self->ability, "orbital_hand");
        loc_vars = ARR(streq(orbital_hand, orbital_placeholder()) ? S(orbital_hand) : localize(S(orbital_hand), "poker_hands"),
                       tget(cfg, "levels"));
    } else if (streq(name_to_check, "Economy Tag")) {
        loc_vars = ARR(tget(cfg, "max"));
    }
    tsett(tag_sprite->moveable.node.object.dyn, "ability_UIBox_table",
          generate_card_ui(ttab(G->P_TAGS, self->key), NULL, loc_vars, self->hide_ability ? "Undiscovered" : "Tag", NULL,
                           self->hide_ability, NULL, NULL));
    return tag_sprite;
}

// tag.lua:563
void Tag_remove_from_game(Tag *self) {
    int tag_key = 0; // 0 as nil: table.remove then takes the last one
    for (TIter it = titer(ttab(G->GAME, "tags")); tnext(&it);) {
        if (it.val.t == V_OBJ && it.val.obj == self) tag_key = (int)it.key.n;
    }
    tremove(ttab(G->GAME, "tags"), tag_key);
}

// tag.lua:571
void Tag_remove(Tag *self) {
    Tag_remove_from_game(self);
    int HUD_tag_key = 0; // 0 as nil
    if (!G->HUD_tags) ERROR("tag.lua:574: bad argument #1 to 'pairs' (table expected, got nil)");
    for (TIter it = titer(G->HUD_tags); tnext(&it);) {
        if (self->HUD_tag && it.val.t == V_OBJ && it.val.obj == self->HUD_tag) HUD_tag_key = (int)it.key.n;
    }

    if (HUD_tag_key) {
        UIBox *next = tobji(G->HUD_tags, HUD_tag_key + 1);
        if (G->HUD_tags && next) {
            if (HUD_tag_key == 1) {
                Moveable_set_alignment((Moveable *)next, TAB("type", S("bri"),
                    "offset", T(TAB("x", N(0.7), "y", N(0))),
                    "xy_bond", S("Weak"),
                    "major", O(G->ROOM_ATTACH)));
            } else {
                Moveable_set_role((Moveable *)next, TAB(
                    "xy_bond", S("Weak"),
                    "major", O(tobji(G->HUD_tags, HUD_tag_key - 1))));
            }
        }
        tremove(G->HUD_tags, HUD_tag_key);
    }

    UIBox_remove(hud_tag(self));
}
