// card.lua 1681-2290: Card:open, redeem, apply_to_run, explode, shatter, start_dissolve, start_materialize,
// calculate_seal, calculate_rental, calculate_perishable.
#include "all.h"

#define NODE(c) ((c)->moveable.node)
#define DYN(c) ((c)->moveable.node.object.dyn)

// name:find('Arcana'): the patterns used here have no magic characters
static bool name_has(Card *self, const char *word) { return str_find(tstr(self->ability, "name"), word, 1) != 0; }

// card.lua:1803, 1861, 1920 for k, v in pairs(G.I.CARD) do if v.set_cost then v:set_cost() end end
// note: G.I.CARD also holds the Blind and the Card_Characters; only Cards have set_cost.
static void set_cost_all(void) {
    for (TIter it = titer(G->I.CARD); tnext(&it);)
        if (is_a(it.val.obj, &Card_class)) Card_set_cost((Card *)it.val.obj);
}
static bool ev_set_cost_all(void *ctx) {
    set_cost_all();
    return true;
}

// ---------------------------------------------------------------- Card:open
struct open_ctx {
    Card *self;
    Table *pack_cards; // card.lua:1723, shared by the two events below
};

// card.lua:1712
static bool ev_open_pay(void *ctx) {
    Card *self = ctx;
    inc_career_stat("c_shop_dollars_spent", self->cost);
    Card_juice_up(self, -1, NIL);
    return true;
}

// card.lua:1725
static bool ev_open_create(void *ctx) {
    struct open_ctx *c = ctx;
    Card *self = c->self;
    num _size = tnum(self->ability, "extra");

    for (int i = 1; i <= _size; i++) {
        Card *card = NULL;
        if (name_has(self, "Arcana")) {
            if (tbool(ttab(G->GAME, "used_vouchers"), "v_omen_globe") && pseudorandom(S("omen_globe"), NIL, NIL) > 0.8) {
                card = create_card("Spectral", G->pack_cards, false, NIL, true, true, NULL, "ar2");
            } else {
                card = create_card("Tarot", G->pack_cards, false, NIL, true, true, NULL, "ar1");
            }
        } else if (name_has(self, "Celestial")) {
            if (tbool(ttab(G->GAME, "used_vouchers"), "v_telescope") && i == 1) {
                const char *_planet = NULL, *_hand = NULL;
                num _tally = 0;
                Table *hands = ttab(G->GAME, "hands");
                for (int k = 1; truthy(tgeti(G->handlist, k)); k++) {
                    const char *v = tstri(G->handlist, k);
                    Table *h = ttab(hands, v);
                    if (tbool(h, "visible") && tnum(h, "played") > _tally) {
                        _hand = v;
                        _tally = tnum(h, "played");
                    }
                }
                if (_hand) {
                    for (TIter it = titer(ttab(G->P_CENTER_POOLS, "Planet")); tnext(&it);) {
                        Table *v = it.val.tab;
                        if (streq(tstr(ttab(v, "config"), "hand_type"), _hand)) _planet = tstr(v, "key");
                    }
                }
                card = create_card("Planet", G->pack_cards, false, NIL, true, true, _planet, "pl1");
            } else {
                card = create_card("Planet", G->pack_cards, false, NIL, true, true, NULL, "pl1");
            }
        } else if (name_has(self, "Spectral")) {
            card = create_card("Spectral", G->pack_cards, false, NIL, true, true, NULL, "spe");
        } else if (name_has(self, "Standard")) {
            const char *ante = number_tostring(tnum(ttab(G->GAME, "round_resets"), "ante"));
            card = create_card((pseudorandom(N(pseudoseed(fmt("stdset%s", ante), NULL)), NIL, NIL) > 0.6) ? "Enhanced" : "Base",
                               G->pack_cards, false, NIL, false, true, NULL, "sta");
            num edition_rate = 2;
            Table *edition = poll_edition(fmt("standard_edition%s", ante), edition_rate, true, false);
            Card_set_edition(card, edition, false, false);
            num seal_rate = 10;
            num seal_poll = pseudorandom(N(pseudoseed(fmt("stdseal%s", ante), NULL)), NIL, NIL);
            if (seal_poll > 1 - 0.02 * seal_rate) {
                num seal_type = pseudorandom(N(pseudoseed(fmt("stdsealtype%s", ante), NULL)), NIL, NIL);
                if (seal_type > 0.75) Card_set_seal(card, "Red", false, false);
                else if (seal_type > 0.5) Card_set_seal(card, "Blue", false, false);
                else if (seal_type > 0.25) Card_set_seal(card, "Gold", false, false);
                else Card_set_seal(card, "Purple", false, false);
            }
        } else if (name_has(self, "Buffoon")) {
            card = create_card("Joker", G->pack_cards, false, NIL, true, true, NULL, "buf");
        }
        if (!card) ERROR("card.lua:1777: attempt to index local 'card' (a nil value)");
        NODE(card).T->x = NODE(self).T->x;
        NODE(card).T->y = NODE(self).T->y;
        Card_start_materialize(card, ARR(P(G->C.WHITE), P(G->C.WHITE)), false, 1.5 * (float)tnum(G->SETTINGS, "GAMESPEED"));
        tseti(c->pack_cards, i, O(card));
    }
    return true;
}

// card.lua:1785: returns nil (called again) until the pack's area is on screen
static bool ev_open_emplace(void *ctx) {
    struct open_ctx *c = ctx;
    if (G->pack_cards) {
        if (G->pack_cards && G->pack_cards->moveable.VT.y < G->ROOM->T->h) {
            for (int k = 1; truthy(tgeti(c->pack_cards, k)); k++) CardArea_emplace(G->pack_cards, tobji(c->pack_cards, k), NULL, false);
            return true;
        }
    }
    return false;
}

// card.lua:1721
static bool ev_open(void *ctx) {
    Card *self = ctx;
    Card_explode(self, NULL, -1);
    struct open_ctx *c = NEWCTX(struct open_ctx);
    c->self = self;
    c->pack_cards = tnew();

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 1.3 * sqrt(tnum(G->SETTINGS, "GAMESPEED")),
        .not_blockable = true, .not_blocking = true, .func = ev_open_create, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 1.3 * sqrt(tnum(G->SETTINGS, "GAMESPEED")),
        .not_blockable = true, .not_blocking = true, .func = ev_open_emplace, .ctx = c}), NULL, false);

    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("open_booster", B(true), "card", O(self)));
    }

    if (tbool(ttab(G->GAME, "modifiers"), "inflation")) {
        tsetn(G->GAME, "inflation", tnum(G->GAME, "inflation") + 1);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_set_cost_all}), NULL, false);
    }

    return true;
}

// card.lua:1681
void Card_open(Card *self) {
    if (streq(tstr(self->ability, "set"), "Booster")) {
        stop_use();
        G->STATE_COMPLETE = false;
        tsetb(DYN(self), "opening", true);

        Table *center = ttab(NODE(self).config, "center");
        if (!tbool(center, "discovered")) {
            discover_card(center);
        }
        // note: the Lua shares this states table with the card's sprites (card.h, Card_set_sprites)
        NODE(self).states.hover.can = false;

        if (name_has(self, "Arcana")) {
            G->STATE = STATES_TAROT_PACK;
            tset(G->GAME, "pack_size", tget(self->ability, "extra"));
        } else if (name_has(self, "Celestial")) {
            G->STATE = STATES_PLANET_PACK;
            tset(G->GAME, "pack_size", tget(self->ability, "extra"));
        } else if (name_has(self, "Spectral")) {
            G->STATE = STATES_SPECTRAL_PACK;
            tset(G->GAME, "pack_size", tget(self->ability, "extra"));
        } else if (name_has(self, "Standard")) {
            G->STATE = STATES_STANDARD_PACK;
            tset(G->GAME, "pack_size", tget(self->ability, "extra"));
        } else if (name_has(self, "Buffoon")) {
            G->STATE = STATES_BUFFOON_PACK;
            tset(G->GAME, "pack_size", tget(self->ability, "extra"));
        }

        Val choose = tget(ttab(center, "config"), "choose");
        tset(G->GAME, "pack_choices", truthy(choose) ? choose : N(1));

        if (self->cost > 0) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_open_pay, .ctx = self}), NULL, false);
            ease_dollars(-self->cost, false);
        } else {
            delay(0.2, NULL);
        }

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.4, .func = ev_open, .ctx = self}), NULL, false);
    }
}

// ---------------------------------------------------------------- Card:redeem
struct redeem_ctx {
    Card *self;
    DynaText *top_dynatext, *bot_dynatext; // upvalues assigned by the first event, read by the third
};

// card.lua:1826
static bool ev_redeem_text(void *ctx) {
    struct redeem_ctx *c = ctx;
    Card *self = c->self;
    Table *center = ttab(NODE(self).config, "center");
    c->top_dynatext = DynaText_new(TAB("string", localize(T(TAB("type", S("name_text"), "set", tget(center, "set"), "key", tget(center, "key"))), NULL),
        "colours", T(ARR(P(G->C.WHITE))), "rotate", N(1), "shadow", B(true), "bump", B(true), "float", B(true), "scale", N(0.9),
        "pop_in", N(0.6 / G->SPEEDFACTOR), "pop_in_rate", N(1.5 * G->SPEEDFACTOR)));
    c->bot_dynatext = DynaText_new(TAB("string", localize(S("k_redeemed_ex"), NULL),
        "colours", T(ARR(P(G->C.WHITE))), "rotate", N(2), "shadow", B(true), "bump", B(true), "float", B(true), "scale", N(0.9),
        "pop_in", N(1.4 / G->SPEEDFACTOR), "pop_in_rate", N(1.5 * G->SPEEDFACTOR), "pitch_shift", N(0.25)));
    Card_juice_up(self, 0.3f, N(0.5));
    play_sound("card1", -1, -1);
    play_sound("coin1", -1, -1);
    tseto(NODE(self).children, "top_disp", UIBox_new(TAB(
        "definition", T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "r", N(0.15), "colour", P(G->C.CLEAR), "padding", N(0.15))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(c->top_dynatext)))))
        )))),
        "config", T(TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(0))), "parent", O(self))))));
    tseto(NODE(self).children, "bot_disp", UIBox_new(TAB(
        "definition", T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "r", N(0.15), "colour", P(G->C.CLEAR), "padding", N(0.15))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(c->bot_dynatext)))))
        )))),
        "config", T(TAB("align", S("bm"), "offset", T(TAB("x", N(0), "y", N(0))), "parent", O(self))))));
    return true;
}

// card.lua:1866
static bool ev_redeem_pop_out(void *ctx) {
    struct redeem_ctx *c = ctx;
    DynaText_pop_out(c->top_dynatext, 4);
    DynaText_pop_out(c->bot_dynatext, 4);
    return true;
}

// card.lua:1871
static bool ev_redeem_remove(void *ctx) {
    struct redeem_ctx *c = ctx;
    Card *self = c->self;
    UIBox_remove(Card_child(self, "top_disp"));
    tset(NODE(self).children, "top_disp", NIL);
    UIBox_remove(Card_child(self, "bot_disp"));
    tset(NODE(self).children, "bot_disp", NIL);
    return true;
}

// card.lua:1813
void Card_redeem(Card *self) {
    if (streq(tstr(self->ability, "set"), "Voucher")) {
        stop_use();
        Table *center = ttab(NODE(self).config, "center");
        if (!tbool(center, "discovered")) {
            discover_card(center);
        }
        if (tbool(DYN(self), "shop_voucher")) tset(ttab(G->GAME, "current_round"), "voucher", NIL);

        // note: the Lua shares this states table with the card's sprites (card.h, Card_set_sprites)
        NODE(self).states.hover.can = false;
        tsetb(ttab(G->GAME, "used_vouchers"), tstr(NODE(self).config, "center_key"), true);
        struct redeem_ctx *c = NEWCTX(struct redeem_ctx);
        c->self = self;
        c->top_dynatext = NULL;
        c->bot_dynatext = NULL;

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.4, .func = ev_redeem_text, .ctx = c}), NULL, false);
        ease_dollars(-self->cost, false);
        inc_career_stat("c_shop_dollars_spent", self->cost);
        inc_career_stat("c_vouchers_bought", 1);
        set_voucher_usage(self);
        check_for_unlock(TAB("type", S("run_redeem")));
        tset(ttab(G->GAME, "current_round"), "voucher", NIL);

        Card_apply_to_run(self, NULL);

        delay(0.6, NULL);
        for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("buying_card", B(true), "card", O(self)));
        }
        if (tbool(ttab(G->GAME, "modifiers"), "inflation")) {
            tsetn(G->GAME, "inflation", tnum(G->GAME, "inflation") + 1);
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_set_cost_all}), NULL, false);
        }
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 2.6, .func = ev_redeem_pop_out, .ctx = c}), NULL, false);

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.5, .func = ev_redeem_remove, .ctx = c}), NULL, false);
    }
}

// ---------------------------------------------------------------- Card:apply_to_run
// the local center_table {name, extra} plus self, as the closures capture them
struct apply_ctx {
    Card *self;
    const char *name;
    Val extra;
};

// card.lua:1886
static bool ev_apply_overstock(void *ctx) {
    change_shop_size(1);
    return true;
}
// card.lua:1891
static bool ev_apply_tarot_rate(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tsetn(G->GAME, "tarot_rate", 4 * vnum(center_table->extra));
    return true;
}
// card.lua:1896
static bool ev_apply_planet_rate(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tsetn(G->GAME, "planet_rate", 4 * vnum(center_table->extra));
    return true;
}
// card.lua:1901
static bool ev_apply_edition_rate(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tset(G->GAME, "edition_rate", center_table->extra);
    return true;
}
// card.lua:1906
static bool ev_apply_playing_card_rate(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tset(G->GAME, "playing_card_rate", center_table->extra);
    return true;
}
// card.lua:1913
static bool ev_apply_crystal_ball(void *ctx) {
    Table *config = G->consumeables->moveable.node.config;
    tsetn(config, "card_limit", tnum(config, "card_limit") + 1);
    return true;
}
// card.lua:1918
static bool ev_apply_discount(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tset(G->GAME, "discount_percent", center_table->extra);
    set_cost_all();
    return true;
}
// card.lua:1926 (reads self.ability.extra, not center_table.extra, when it runs)
static bool ev_apply_reroll(void *ctx) {
    struct apply_ctx *c = ctx;
    if (!c->self) ERROR("card.lua:1927: attempt to index local 'self' (a nil value)");
    Table *round_resets = ttab(G->GAME, "round_resets"), *current_round = ttab(G->GAME, "current_round");
    tsetn(round_resets, "reroll_cost", tnum(round_resets, "reroll_cost") - tnum(c->self->ability, "extra"));
    tsetn(current_round, "reroll_cost", lmax(0, tnum(current_round, "reroll_cost") - tnum(c->self->ability, "extra")));
    return true;
}
// card.lua:1932
static bool ev_apply_interest_cap(void *ctx) {
    struct apply_ctx *center_table = ctx;
    tset(G->GAME, "interest_cap", center_table->extra);
    return true;
}
// card.lua:1951
static bool ev_apply_antimatter(void *ctx) {
    if (G->jokers) {
        Table *config = G->jokers->moveable.node.config;
        tsetn(config, "card_limit", tnum(config, "card_limit") + 1);
    }
    return true;
}

// card.lua:1880
void Card_apply_to_run(Card *self, Table *center) {
    struct apply_ctx *center_table = NEWCTX(struct apply_ctx);
    center_table->self = self;
    // name = center and center.name or self and self.ability.name
    center_table->name = (center && tstr(center, "name")) ? tstr(center, "name") : (self ? tstr(self->ability, "name") : NULL);
    // extra = center and center.config.extra or self and self.ability.extra
    Val extra = center ? tget(ttab(center, "config"), "extra") : NIL;
    center_table->extra = truthy(extra) ? extra : (self ? tget(self->ability, "extra") : NIL);
    const char *name = center_table->name;
    num ex = vnum(center_table->extra);
    Table *round_resets = ttab(G->GAME, "round_resets");

    if (streq(name, "Overstock") || streq(name, "Overstock Plus")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_overstock}), NULL, false);
    }
    if (streq(name, "Tarot Merchant") || streq(name, "Tarot Tycoon")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_tarot_rate, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Planet Merchant") || streq(name, "Planet Tycoon")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_planet_rate, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Hone") || streq(name, "Glow Up")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_edition_rate, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Magic Trick") || streq(name, "Illusion")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_playing_card_rate, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Telescope") || streq(name, "Observatory")) {
    }
    if (streq(name, "Crystal Ball")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_crystal_ball}), NULL, false);
    }
    if (streq(name, "Clearance Sale") || streq(name, "Liquidation")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_discount, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Reroll Surplus") || streq(name, "Reroll Glut")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_reroll, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Seed Money") || streq(name, "Money Tree")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_interest_cap, .ctx = center_table}), NULL, false);
    }
    if (streq(name, "Grabber") || streq(name, "Nacho Tong")) {
        tsetn(round_resets, "hands", tnum(round_resets, "hands") + ex);
        ease_hands_played(ex, false);
    }
    if (streq(name, "Paint Brush") || streq(name, "Palette")) {
        CardArea_change_size(G->hand, 1);
    }
    if (streq(name, "Wasteful") || streq(name, "Recyclomancy")) {
        tsetn(round_resets, "discards", tnum(round_resets, "discards") + ex);
        ease_discard(ex, false, false);
    }
    if (streq(name, "Blank")) {
        check_for_unlock(TAB("type", S("blank_redeems")));
    }
    if (streq(name, "Antimatter")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_apply_antimatter}), NULL, false);
    }
    if (streq(name, "Hieroglyph") || streq(name, "Petroglyph")) {
        ease_ante(-ex);
        Val blind_ante = tget(round_resets, "blind_ante");
        tset(round_resets, "blind_ante", truthy(blind_ante) ? blind_ante : tget(round_resets, "ante"));
        tsetn(round_resets, "blind_ante", tnum(round_resets, "blind_ante") - ex);

        if (streq(name, "Hieroglyph")) {
            tsetn(round_resets, "hands", tnum(round_resets, "hands") - ex);
            ease_hands_played(-ex, false);
        }
        if (streq(name, "Petroglyph")) {
            tsetn(round_resets, "discards", tnum(round_resets, "discards") - ex);
            ease_discard(-ex, false, false);
        }
    }
}

// ---------------------------------------------------------------- dissolves
// func = (function() self:remove() return true end)
static bool ev_remove_self(void *ctx) {
    CALL(ctx, remove);
    return true;
}

static Particles *new_particles(Table *config) {
    Particles *p = NEW(Particles);
    Particles_init(p, 0, 0, 0, 0, config);
    return p;
}

struct explode_ctx {
    Card *self;
    num start_time, explode_time, percent;
    Particles *childParts1, *childParts2; // childParts2 is assigned by the third event and read by the fourth
};

// card.lua:2004: runs every pass until 1.5*explode_time have gone by
static bool ev_explode_wobble(void *ctx) {
    struct explode_ctx *c = ctx;
    Card *self = c->self;
    num TOTAL = G->TIMERS.TOTAL;
    if (self->moveable.juice.on) {
        c->percent = (TOTAL - c->start_time) / c->explode_time;
        self->moveable.juice.r = 0.05 * (sin(5 * TOTAL) + cos(0.33 + 41.15332 * TOTAL) + cos(67.12 * TOTAL)) * c->percent;
        self->moveable.juice.scale = c->percent * 0.15;
    }
    if (TOTAL - c->start_time > 1.5 * c->explode_time) return true;
    return false;
}

// card.lua:2027
static bool ev_explode_burst(void *ctx) {
    struct explode_ctx *c = ctx;
    Card *self = c->self;
    c->childParts2 = new_particles(TAB(
        "timer_type", S("TOTAL"),
        "pulse_max", N(30),
        "timer", N(0.003),
        "scale", N(0.6),
        "speed", N(15),
        "lifespan", N(0.5),
        "attach", O(self),
        "colours", T(self->dissolve_colours)));
    Moveable_set_role((Moveable *)c->childParts2, TAB("r_bond", S("Weak")));
    // note: ref_float into the card: Card_remove runs at 1.5*explode_time, after this ease ends (1.0*explode_time)
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .ref_float = &self->dissolve,
        .ease_to = 1,
        .delay = 0.1 * c->explode_time}), NULL, false);
    Card_juice_up(self, -1, NIL);
    G->VIBRATION = G->VIBRATION + 1;
    play_sound("explosion_release1", -1, -1);
    Particles_fade(c->childParts1, 0.3 * c->explode_time, -1);
    return true;
}

// card.lua:2058
static bool ev_explode_parts2_scale(void *ctx) {
    struct explode_ctx *c = ctx;
    if (!c->childParts2) ERROR("card.lua:2059: ease with a nil ref_table (childParts2)");
    // note: ref_float into the Particles; it must outlive this ease (0.1*explode_time), as the Lua's table does
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .not_blocking = true,
        .ref_float = &c->childParts2->scale,
        .ease_to = 0,
        .delay = 0.1 * c->explode_time}), NULL, false);
    return true;
}

// card.lua:1973
void Card_explode(Card *self, Table *dissolve_colours, num explode_time_fac) {
    num explode_time = 1.3 * (explode_time_fac < 0 ? 1 : explode_time_fac) * (sqrt(tnum(G->SETTINGS, "GAMESPEED")));
    self->dissolve = 0;
    self->dissolve_colours = dissolve_colours ? dissolve_colours : ARR(P(G->C.WHITE));

    num start_time = G->TIMERS.TOTAL;
    num percent = 0;
    play_sound("explosion_buildup1", -1, -1);
    // self.juice = {scale = 0, r = 0, handled_elsewhere = true, start_time =, end_time =} (scale_amt, r_amt: nil)
    self->moveable.juice.on = true;
    self->moveable.juice.scale = 0;
    self->moveable.juice.r = 0;
    self->moveable.juice.scale_amt = 0;
    self->moveable.juice.r_amt = 0;
    self->moveable.juice.handled_elsewhere = true;
    self->moveable.juice.start_time = start_time;
    self->moveable.juice.end_time = start_time + explode_time;

    struct explode_ctx *c = NEWCTX(struct explode_ctx);
    c->self = self;
    c->start_time = start_time;
    c->explode_time = explode_time;
    c->percent = percent;
    c->childParts1 = new_particles(TAB(
        "timer_type", S("TOTAL"),
        "timer", N(0.01 * explode_time),
        "scale", N(0.2),
        "speed", N(2),
        "lifespan", N(0.2 * explode_time),
        "attach", O(self),
        "colours", T(self->dissolve_colours),
        "fill", B(true)));
    c->childParts2 = NULL;

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .not_blockable = true,
        .func = ev_explode_wobble, .ctx = c}), NULL, false);
    // note: ref_float into the card: Card_remove runs at 1.5*explode_time, after this ease ends
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .ref_float = &self->dissolve,
        .ease_to = 0.3,
        .delay = 0.9 * explode_time}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.9 * explode_time,
        .func = ev_explode_burst, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 1.4 * explode_time,
        .func = ev_explode_parts2_scale, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 1.5 * explode_time,
        .func = ev_remove_self, .ctx = self}), NULL, false);
}

// childParts:fade(k*dissolve_time)
struct fade_ctx {
    Particles *childParts;
    num delay;
};
static bool ev_fade(void *ctx) {
    struct fade_ctx *c = ctx;
    Particles_fade(c->childParts, c->delay, -1);
    return true;
}
static struct fade_ctx *fade_ctx_new(Particles *childParts, num delay) {
    struct fade_ctx *c = NEWCTX(struct fade_ctx);
    c->childParts = childParts;
    c->delay = delay;
    return c;
}

// card.lua:2103
static bool ev_shatter_sound(void *ctx) {
    int n = (int)math_random_range(1, 6);
    float per = math_random() * 0.2 + 0.9;
    play_sound(fmt("glass%d", n), per, 0.5f);
    per = math_random() * 0.2 + 0.9;
    play_sound("generic1", per, 0.5f);
    return true;
}

// card.lua:2079
void Card_shatter(Card *self) {
    num dissolve_time = 0.7;
    // note: the Lua makes a new {1,1,1,0.8} per call; nothing writes into it, so one static colour serves
    static float shatter_colour[4] = {1, 1, 1, 0.8f};
    tsetb(DYN(self), "shattered", true);
    self->dissolve = 0;
    self->dissolve_colours = ARR(P(shatter_colour));
    Card_juice_up(self, -1, NIL);
    Particles *childParts = new_particles(TAB(
        "timer_type", S("TOTAL"),
        "timer", N(0.007 * dissolve_time),
        "scale", N(0.3),
        "speed", N(4),
        "lifespan", N(0.5 * dissolve_time),
        "attach", O(self),
        "colours", T(self->dissolve_colours),
        "fill", B(true)));
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.5 * dissolve_time,
        .func = ev_fade, .ctx = fade_ctx_new(childParts, 0.15 * dissolve_time)}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .not_blockable = true,
        .func = ev_shatter_sound}), NULL, false);
    // note: ref_float into the card: the ease ends at 0.5*dissolve_time, Card_remove runs at 0.55
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .ref_float = &self->dissolve,
        .ease_to = 1,
        .delay = 0.5 * dissolve_time}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.55 * dissolve_time,
        .func = ev_remove_self, .ctx = self}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.51 * dissolve_time}), NULL, false);
}

// card.lua:2155
static bool ev_dissolve_sound(void *ctx) {
    float per = math_random() * 0.2 + 0.9;
    play_sound("whoosh2", per, 0.5f);
    int n = (int)math_random_range(1, 5);
    per = math_random() * 0.2 + 0.9;
    play_sound(fmt("crumple%d", n), per, 0.5f);
    return true;
}

// card.lua:2130
void Card_start_dissolve(Card *self, Table *dissolve_colours, bool silent, num dissolve_time_fac, bool no_juice) {
    num dissolve_time = 0.7 * (dissolve_time_fac < 0 ? 1 : dissolve_time_fac);
    self->dissolve = 0;
    self->dissolve_colours = dissolve_colours ? dissolve_colours
        : ARR(P(G->C.BLACK), P(G->C.ORANGE), P(G->C.RED), P(G->C.GOLD), P(G->C.JOKER_GREY));
    if (!no_juice) Card_juice_up(self, -1, NIL);
    Particles *childParts = new_particles(TAB(
        "timer_type", S("TOTAL"),
        "timer", N(0.01 * dissolve_time),
        "scale", N(0.1),
        "speed", N(2),
        "lifespan", N(0.7 * dissolve_time),
        "attach", O(self),
        "colours", T(self->dissolve_colours),
        "fill", B(true)));
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.7 * dissolve_time,
        .func = ev_fade, .ctx = fade_ctx_new(childParts, 0.3 * dissolve_time)}), NULL, false);
    if (!silent) {
        E_add(G->E_MANAGER, Event_new((EventCfg){
            .not_blockable = true,
            .func = ev_dissolve_sound}), NULL, false);
    }
    // note: ref_float into the card: the ease ends at 1*dissolve_time, Card_remove runs at 1.05
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .ref_float = &self->dissolve,
        .ease_to = 1,
        .delay = 1 * dissolve_time}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 1.05 * dissolve_time,
        .func = ev_remove_self, .ctx = self}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 1.051 * dissolve_time}), NULL, false);
}

// card.lua:2212
static bool ev_materialize_sound(void *ctx) {
    float per = math_random() * 0.1 + 0.6;
    play_sound("whoosh1", per, 0.3f);
    int n = (int)math_random_range(1, 5);
    per = math_random() * 0.2 + 1.2;
    play_sound(fmt("crumple%d", n), per, 0.8f);
    return true;
}
// card.lua:2223
static bool ev_materialize_stop(void *ctx) {
    Card *self = ctx;
    Particles *particles = Card_child(self, "particles");
    if (particles) particles->max = 0;
    return true;
}
// card.lua:2238
static bool ev_materialize_end(void *ctx) {
    Card *self = ctx;
    NODE(self).states.hover.can = true;
    Particles *particles = Card_child(self, "particles");
    if (particles) {
        CALL(particles, remove);
        tset(NODE(self).children, "particles", NIL);
    }
    return true;
}

// card.lua:2183
void Card_start_materialize(Card *self, Table *dissolve_colours, bool silent, num timefac) {
    num dissolve_time = 0.6 * (timefac < 0 ? 1 : timefac);
    NODE(self).states.visible = true;
    NODE(self).states.hover.can = false;
    self->dissolve = 1;
    const char *set = tstr(self->ability, "set");
    if (dissolve_colours) {
        self->dissolve_colours = dissolve_colours;
    } else if (streq(set, "Joker")) {
        // {G.C.RARITY[self.config.center.rarity]}: an empty table when the center has no rarity
        Val rarity = tget(ttab(NODE(self).config, "center"), "rarity");
        self->dissolve_colours = (rarity.t == V_NUM && rarity.n >= 1 && rarity.n <= 4) ? ARR(P(G->C.RARITY[(int)rarity.n])) : tnew();
    } else if (streq(set, "Planet")) {
        self->dissolve_colours = ARR(P(G->C.SECONDARY_SET.Planet));
    } else if (streq(set, "Tarot")) {
        self->dissolve_colours = ARR(P(G->C.SECONDARY_SET.Tarot));
    } else if (streq(set, "Spectral")) {
        self->dissolve_colours = ARR(P(G->C.SECONDARY_SET.Spectral));
    } else if (streq(set, "Booster")) {
        self->dissolve_colours = ARR(P(G->C.BOOSTER));
    } else if (streq(set, "Voucher")) {
        self->dissolve_colours = ARR(P(G->C.SECONDARY_SET.Voucher), P(G->C.CLEAR));
    } else {
        self->dissolve_colours = ARR(P(G->C.GREEN));
    }
    Card_juice_up(self, -1, NIL);
    tseto(NODE(self).children, "particles", new_particles(TAB(
        "timer_type", S("TOTAL"),
        "timer", N(0.025 * dissolve_time),
        "scale", N(0.25),
        "speed", N(3),
        "lifespan", N(0.7 * dissolve_time),
        "attach", O(self),
        "colours", T(self->dissolve_colours),
        "fill", B(true))));
    if (!silent) {
        if (!truthy(G->last_materialized) || G->last_materialized.n + 0.01 < G->TIMERS.REAL || G->last_materialized.n > G->TIMERS.REAL) {
            G->last_materialized = N(G->TIMERS.REAL);
            E_add(G->E_MANAGER, Event_new((EventCfg){
                .not_blockable = true,
                .func = ev_materialize_sound}), NULL, false);
        }
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 0.5 * dissolve_time,
        .func = ev_materialize_stop, .ctx = self}), NULL, false);
    // note: ref_float into the card: dangles if the card is removed within 1*dissolve_time (the Lua keeps the table)
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .not_blockable = true,
        .ref_float = &self->dissolve,
        .ease_to = 0,
        .delay = 1 * dissolve_time}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .not_blockable = true,
        .delay = 1.05 * dissolve_time,
        .func = ev_materialize_end, .ctx = self}), NULL, false);
}

// ---------------------------------------------------------------- seals and stickers
// card.lua:2259
static bool ev_purple_seal(void *ctx) {
    Card *card = create_card("Tarot", G->consumeables, false, NIL, false, false, NULL, "8ba");
    Card_add_to_deck(card, false);
    CardArea_emplace(G->consumeables, card, NULL, false);
    tsetn(G->GAME, "consumeable_buffer", 0);
    return true;
}

// card.lua:2242
Table *Card_calculate_seal(Card *self, Table *context) {
    if (self->debuff) return NULL;
    if (tbool(context, "repetition")) {
        if (streq(self->seal, "Red")) {
            return TAB(
                "message", localize(S("k_again_ex"), NULL),
                "repetitions", N(1),
                "card", O(self));
        }
    }
    if (tbool(context, "discard")) {
        if (streq(self->seal, "Purple") && CardArea_n(G->consumeables) + tnum(G->GAME, "consumeable_buffer") < tnum(G->consumeables->moveable.node.config, "card_limit")) {
            tsetn(G->GAME, "consumeable_buffer", tnum(G->GAME, "consumeable_buffer") + 1);
            E_add(G->E_MANAGER, Event_new((EventCfg){
                .trigger = "before",
                .delay = 0.0,
                .func = ev_purple_seal}), NULL, false);
            card_eval_status_text(self, "extra", 0, -1, NULL, TAB("message", localize(S("k_plus_tarot"), NULL), "colour", P(G->C.PURPLE)));
        }
    }
    return NULL;
}

// card.lua:2271
void Card_calculate_rental(Card *self) {
    if (tbool(self->ability, "rental")) {
        ease_dollars(-tnum(G->GAME, "rental_rate"), false);
        card_eval_status_text(self, "dollars", -tnum(G->GAME, "rental_rate"), -1, NULL, NULL);
    }
}

// card.lua:2278
void Card_calculate_perishable(Card *self) {
    if (tbool(self->ability, "perishable") && tnum(self->ability, "perish_tally") > 0) {
        if (tnum(self->ability, "perish_tally") == 1) {
            tsetn(self->ability, "perish_tally", 0);
            card_eval_status_text(self, "extra", 0, -1, NULL, TAB("message", localize(S("k_disabled_ex"), NULL), "colour", P(G->C.FILTER), "delay", N(0.45)));
            Card_set_debuff(self, false); // self:set_debuff(): see card.h
        } else {
            tsetn(self->ability, "perish_tally", tnum(self->ability, "perish_tally") - 1);
            card_eval_status_text(self, "extra", 0, -1, NULL, TAB("message", localize(T(TAB("type", S("variable"), "key", S("a_remaining"), "vars", T(ARR(N(tnum(self->ability, "perish_tally")))))), NULL), "colour", P(G->C.FILTER), "delay", N(0.45)));
        }
    }
}
