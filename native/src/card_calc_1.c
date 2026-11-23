// card.lua:2291-2873: Card:calculate_joker (debuff, Planet, Blueprint, Brainstorm and the dispatch) and the context
// branches open_booster ... discard (Card_calculate_joker_a).
#include "all.h"

// ---------------------------------------------------------------- file-local shorthands
#define DYN(c) ((c)->moveable.node.object.dyn)
#define SLICED(c) tbool(DYN(c), "getting_sliced")
#define ACFG(area) ((area)->moveable.node.config) // area.config
#define CUR_ROUND ttab(G->GAME, "current_round")
#define LOC(key) localize(S(key), NULL)
#define ADD_EV(fn, c) E_add(G->E_MANAGER, Event_new((EventCfg){.func = (fn), .ctx = (c)}), NULL, false)

// card_eval_status_text(card, 'extra', nil, nil, nil, extra)
static void cest(Card *card, Table *extra) { card_eval_status_text(card, "extra", 0, -1, NULL, extra); }
// localize{type = 'variable', key = key, vars = {v}}
static Val locv(const char *key, Val v) {
    return localize(T(TAB("type", S("variable"), "key", S(key), "vars", T(ARR(v)))), NULL);
}
// context.blueprint_card or self
static Card *bc(Table *context, Card *self) {
    Card *b = tobj(context, "blueprint_card");
    return b ? b : self;
}
// t[key] = t[key] + n
static void tadd(Table *t, const char *key, num n) { tsetn(t, key, tnum(t, key) + n); }
// localize('$')..v
static Val dollar_msg(Val v) { return S(fmt("%s%s", vstr(LOC("$")), tostring(v))); }
// #G.consumeables.cards + G.GAME.consumeable_buffer < G.consumeables.config.card_limit
static bool consumeable_room(void) {
    return CardArea_n(G->consumeables) + tnum(G->GAME, "consumeable_buffer") < tnum(ACFG(G->consumeables), "card_limit");
}

struct sc_ctx { Card *self; Table *context; };           // closures that capture self and context
struct cnt_ctx { Card *self; num n; };                   // closures that capture self and a count
static struct sc_ctx *sc_new(Card *self, Table *context) {
    struct sc_ctx *c = NEWCTX(struct sc_ctx);
    c->self = self, c->context = context;
    return c;
}
static struct cnt_ctx *cnt_new(Card *self, num n) {
    struct cnt_ctx *c = NEWCTX(struct cnt_ctx);
    c->self = self, c->n = n;
    return c;
}

// ---------------------------------------------------------------- event closures
// card.lua:2342 (Hallucination), :2551 (Cartomancer), :2610 (Sixth Sense): create a consumeable; ctx is {type, key_append}
struct mk_ctx { const char *type, *key_append; };
static bool ev_create_consumeable(void *ctx) {
    struct mk_ctx *c = ctx;
    Card *card = create_card(c->type, G->consumeables, false, NIL, false, false, NULL, c->key_append);
    Card_add_to_deck(card, false);
    CardArea_emplace(G->consumeables, card, NULL, false);
    tsetn(G->GAME, "consumeable_buffer", 0);
    return true;
}
static struct mk_ctx *mk_new(const char *type, const char *key_append) {
    struct mk_ctx *c = NEWCTX(struct mk_ctx);
    c->type = type, c->key_append = key_append;
    return c;
}

// card.lua:2363 Diet Cola
static bool ev_diet_cola(void *ctx) {
    Tag *tag = NEW(Tag);
    Tag_init(tag, "tag_double", false, NULL);
    add_tag(tag);
    play_sound("generic1", (float)(0.9 + math_random() * 0.1), 0.8f);
    play_sound("holo1", (float)(1.2 + math_random() * 0.1), 0.4f);
    return true;
}

// card.lua:2372 Invisible Joker's eval
static Val eval_invisible(void *ctx, int argc, Val *argv) {
    Card *card = argv[0].obj;
    return B(veq(tget(card->ability, "loyalty_remaining"), N(0)) && !G->RESET_JIGGLES);
}
// card.lua:2484 DNA's eval
static Val eval_dna(void *ctx, int argc, Val *argv) { return B(tnum(CUR_ROUND, "hands_played") == 0); }
// card.lua:2488 Trading Card's eval
static Val eval_trading(void *ctx, int argc, Val *argv) {
    return B(tnum(CUR_ROUND, "discards_used") == 0 && !G->RESET_JIGGLES);
}

// card.lua:2399 Campfire
static bool ev_campfire(void *ctx) {
    cest(ctx, TAB("message", LOC("k_upgrade_ex")));
    return true;
}
// card.lua:2407 Flash Card
static bool ev_flash_card(void *ctx) {
    Card *self = ctx;
    cest(self, TAB("message", locv("a_mult", tget(self->ability, "mult")), "colour", P(G->C.MULT)));
    return true;
}
// card.lua:2416 Perkeo
static bool ev_perkeo(void *ctx) {
    Card *card = copy_card(pseudorandom_element(G->consumeables->cards, N(pseudoseed("perkeo", NULL)), NULL).obj, NULL, -1,
                           NIL, false);
    Card_set_edition(card, TAB("negative", B(true)), true, false);
    Card_add_to_deck(card, false);
    CardArea_emplace(G->consumeables, card, NULL, false);
    return true;
}
// card.lua:2431 Throwback
static bool ev_throwback(void *ctx) {
    Card *self = ctx;
    cest(self, TAB("message", locv("a_xmult", tget(self->ability, "x_mult")), "colour", P(G->C.RED), "card", O(self)));
    return true;
}
// card.lua:2445 Red Card
static bool ev_red_card(void *ctx) {
    Card *self = ctx;
    cest(self, TAB("message", locv("a_mult", tget(self->ability, "extra")), "colour", P(G->C.RED), "delay", N(0.45), "card",
                   O(self)));
    return true;
}
// card.lua:2465 Certificate
static bool ev_certificate(void *ctx) {
    struct sc_ctx *c = ctx;
    Card *_card = create_playing_card(TAB("front", pseudorandom_element(G->P_CARDS, N(pseudoseed("cert_fr", NULL)), NULL),
                                          "center", tget(G->P_CENTERS, "c_base")),
                                      G->hand, false, false, ARR(P(G->C.SECONDARY_SET.Enhanced)));
    num seal_type = pseudorandom(N(pseudoseed("certsl", NULL)), NIL, NIL);
    if (seal_type > 0.75) Card_set_seal(_card, "Red", true, false);
    else if (seal_type > 0.5) Card_set_seal(_card, "Blue", true, false);
    else if (seal_type > 0.25) Card_set_seal(_card, "Gold", true, false);
    else Card_set_seal(_card, "Purple", true, false);
    Blind_debuff_card(tobj(G->GAME, "blind"), _card, false);
    CardArea_sort(G->hand, NULL);
    Card *b = tobj(c->context, "blueprint_card");
    if (b) Card_juice_up(b, -1, NIL); else Card_juice_up(c->self, -1, NIL);
    return true;
}
// card.lua:2495 Chicot, inner
static bool ev_chicot_inner(void *ctx) {
    Blind_disable(tobj(G->GAME, "blind"));
    play_sound("timpani", -1, -1);
    delay(0.4, NULL);
    return true;
}
// card.lua:2494 Chicot, outer
static bool ev_chicot(void *ctx) {
    ADD_EV(ev_chicot_inner, NULL);
    cest(ctx, TAB("message", LOC("ph_boss_disabled")));
    return true;
}
// card.lua:2513 Madness
struct madness_ctx { Card *self; Table *context; Card *joker_to_destroy; };
static bool ev_madness(void *ctx) {
    struct madness_ctx *c = ctx;
    Card_juice_up(bc(c->context, c->self), 0.8f, N(0.8));
    Card_start_dissolve(c->joker_to_destroy, ARR(P(G->C.RED)), false, 1.6, false);
    return true;
}
// card.lua:2523 Burglar
static bool ev_burglar(void *ctx) {
    struct sc_ctx *c = ctx;
    ease_discard(-tnum(CUR_ROUND, "discards_left"), false, true);
    ease_hands_played(tnum(c->self->ability, "extra"), false);
    cest(bc(c->context, c->self), TAB("message", locv("a_hands", tget(c->self->ability, "extra"))));
    return true;
}
// card.lua:2533 Riff-raff; ctx: jokers_to_create
static bool ev_riff_raff(void *ctx) {
    num jokers_to_create = *(num *)ctx;
    for (num i = 1; i <= jokers_to_create; i++) {
        Card *card = create_card("Joker", G->jokers, false, N(0), false, false, NULL, "rif");
        Card_add_to_deck(card, false);
        CardArea_emplace(G->jokers, card, NULL, false);
        Card_start_materialize(card, NULL, false, -1);
        tsetn(G->GAME, "joker_buffer", 0);
    }
    return true;
}
// card.lua:2548 Cartomancer, outer (the inner one is ev_create_consumeable)
static bool ev_cartomancer(void *ctx) {
    struct sc_ctx *c = ctx;
    ADD_EV(ev_create_consumeable, mk_new("Tarot", "car"));
    cest(bc(c->context, c->self), TAB("message", LOC("k_plus_tarot"), "colour", P(G->C.PURPLE)));
    return true;
}
// card.lua:2570 Ceremonial Dagger
struct dagger_ctx { Card *self, *sliced_card; };
static bool ev_dagger(void *ctx) {
    struct dagger_ctx *c = ctx;
    tsetn(G->GAME, "joker_buffer", 0);
    tadd(c->self->ability, "mult", c->sliced_card->sell_cost * 2);
    Card_juice_up(c->self, 0.8f, N(0.8));
    Card_start_dissolve(c->sliced_card, ARR(P(HEX("57ecab"))), false, 1.6, false);
    play_sound("slice1", (float)(0.96 + math_random() * 0.08), -1);
    return true;
}
// card.lua:2582 Marble Joker, the stone card
static bool ev_marble_card(void *ctx) {
    Table *front = pseudorandom_element(G->P_CARDS, N(pseudoseed("marb_fr", NULL)), NULL).tab;
    G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1, with 0 as nil
    Transform *PT = G->play->moveable.node.T;
    Card *card = NEW(Card);
    Card_init(card, PT->x + PT->w / 2, PT->y, G->CARD_W, G->CARD_H, front, ttab(G->P_CENTERS, "m_stone"),
              TAB("playing_card", N(G->playing_card)));
    Card_start_materialize(card, ARR(P(G->C.SECONDARY_SET.Enhanced)), false, -1);
    CardArea_emplace(G->play, card, NULL, false);
    tinsert(G->playing_cards, O(card));
    return true;
}
// card.lua:2594 Marble Joker, the deck's limit
static bool ev_marble_limit(void *ctx) {
    tadd(ACFG(G->deck), "card_limit", 1);
    return true;
}
// card.lua:2634 Caino (cards_destroyed), inner
static bool ev_caino_inner(void *ctx) {
    struct cnt_ctx *c = ctx;
    tadd(c->self->ability, "caino_xmult", c->n * tnum(c->self->ability, "extra"));
    return true;
}
// card.lua:2632 Caino (cards_destroyed), outer
static bool ev_caino_outer(void *ctx) {
    struct cnt_ctx *c = ctx;
    ADD_EV(ev_caino_inner, c);
    cest(c->self, TAB("message", locv("a_xmult", N(tnum(c->self->ability, "caino_xmult") + c->n * tnum(c->self->ability, "extra")))));
    return true;
}
// card.lua:2658, :2696 Glass Joker, inner
static bool ev_glass_inner(void *ctx) {
    struct cnt_ctx *c = ctx;
    tadd(c->self->ability, "x_mult", tnum(c->self->ability, "extra") * c->n);
    return true;
}
// card.lua:2656, :2694 Glass Joker, outer (the two closures are the same code)
static bool ev_glass_outer(void *ctx) {
    struct cnt_ctx *c = ctx;
    ADD_EV(ev_glass_inner, c);
    cest(c->self, TAB("message", locv("a_xmult", N(tnum(c->self->ability, "x_mult") + tnum(c->self->ability, "extra") * c->n))));
    return true;
}
// card.lua:2681 Caino (remove_playing_cards)
static bool ev_caino_removed(void *ctx) {
    Card *self = ctx;
    cest(self, TAB("message", locv("a_xmult", tget(self->ability, "caino_xmult"))));
    return true;
}
// card.lua:2717 Glass Joker (The Hanged Man), :2730 Constellation (the same code)
static bool ev_xmult_text(void *ctx) {
    Card *self = ctx;
    cest(self, TAB("message", locv("a_xmult", tget(self->ability, "x_mult"))));
    return true;
}
// card.lua:2724 Fortune Teller
static bool ev_fortune_teller(void *ctx) {
    cest(ctx, TAB("message", locv("a_mult", tpath(G->GAME, "consumeable_usage_total.tarot"))));
    return true;
}
// card.lua:2740 Matador
static bool ev_dollar_buffer(void *ctx) {
    tsetn(G->GAME, "dollar_buffer", 0);
    return true;
}
// card.lua:2767 Ramen, inner
static bool ev_eaten_inner(void *ctx) {
    Card *self = ctx;
    CardArea_remove_card(G->jokers, self, false);
    Card_remove(self);
    // self = nil: only clears the closure's upvalue
    return true;
}
// card.lua:2760 Ramen, outer
static bool ev_eaten(void *ctx) {
    Card *self = ctx;
    play_sound("tarot1", -1, -1);
    self->moveable.node.T->r = -0.2f;
    Card_juice_up(self, 0.3f, N(0.4));
    self->moveable.node.states.drag.is = true;
    ((Moveable *)Card_child(self, "center"))->pinch.x = true;
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = 0.3, .not_blockable = true, .func = ev_eaten_inner, .ctx = self}),
          NULL, false);
    return true;
}
// card.lua:2865 Faceless Joker
static bool ev_faceless(void *ctx) {
    struct sc_ctx *c = ctx;
    Table *extra = ttab(c->self->ability, "extra");
    ease_dollars(tnum(extra, "dollars"), false);
    cest(bc(c->context, c->self), TAB("message", dollar_msg(tget(extra, "dollars")), "colour", P(G->C.MONEY), "delay", N(0.45)));
    return true;
}

// ---------------------------------------------------------------- the branches of Card_calculate_joker_a
// Each returns what the Lua returns from the branch (NIL when it falls out).

// card.lua:2335 context.open_booster
static Val jk_open_booster(Card *self, Table *context) {
    Table *ability = self->ability;
    if (streq(tstr(ability, "name"), "Hallucination") && consumeable_room()) {
        if (pseudorandom(S(fmt("halu%s", tostring(tpath(G->GAME, "round_resets.ante")))), NIL, NIL) <
            tnum(ttab(G->GAME, "probabilities"), "normal") / tnum(ability, "extra")) {
            tadd(G->GAME, "consumeable_buffer", 1);
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.trigger = "before", .delay = 0.0, .func = ev_create_consumeable, .ctx = mk_new("Tarot", "hal")}),
                  NULL, false);
            cest(self, TAB("message", LOC("k_plus_tarot"), "colour", P(G->C.PURPLE)));
        }
    }
    return NIL;
}

// card.lua:2354 context.selling_self
static Val jk_selling_self(Card *self, Table *context) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    if (streq(name, "Luchador")) {
        Blind *blind = tobj(G->GAME, "blind");
        if (blind && ((!blind->disabled) && streq(Blind_get_type(blind), "Boss"))) {
            cest(bc(context, self), TAB("message", LOC("ph_boss_disabled")));
            Blind_disable(blind);
        }
    }
    if (streq(name, "Diet Cola")) ADD_EV(ev_diet_cola, NULL);
    if (streq(name, "Invisible Joker") && (tnum(ability, "invis_rounds") >= tnum(ability, "extra")) && !truthy(tget(context, "blueprint"))) {
        juice_card_until(self, FN(eval_invisible, NULL), true, -1);
        Table *jokers = tnew();
        for (int i = 1; i <= CardArea_n(G->jokers); i++) {
            if (CardArea_card(G->jokers, i) != self) tinsert(jokers, O(CardArea_card(G->jokers, i)));
        }
        if (tlen(jokers) > 0) {
            if (CardArea_n(G->jokers) <= tnum(ACFG(G->jokers), "card_limit")) {
                cest(bc(context, self), TAB("message", LOC("k_duplicated_ex")));
                Card *chosen_joker = pseudorandom_element(jokers, N(pseudoseed("invisible", NULL)), NULL).obj;
                Card *card = copy_card(chosen_joker, NULL, -1, NIL, chosen_joker->edition && tbool(chosen_joker->edition, "negative"));
                if (truthy(tget(card->ability, "invis_rounds"))) tsetn(card->ability, "invis_rounds", 0);
                Card_add_to_deck(card, false);
                CardArea_emplace(G->jokers, card, NULL, false);
            } else {
                cest(bc(context, self), TAB("message", LOC("k_no_room_ex")));
            }
        } else {
            cest(bc(context, self), TAB("message", LOC("k_no_other_jokers")));
        }
    }
    return NIL;
}

// card.lua:2456 context.playing_card_added and not self.getting_sliced
static Val jk_playing_card_added(Card *self, Table *context) {
    Table *ability = self->ability;
    Table *cards = ttab(context, "cards");
    if (streq(tstr(ability, "name"), "Hologram") && !truthy(tget(context, "blueprint")) && cards && truthy(tgeti(cards, 1))) {
        tadd(ability, "x_mult", tlen(cards) * tnum(ability, "extra"));
        cest(self, TAB("message", locv("a_xmult", tget(ability, "x_mult"))));
    }
    return NIL;
}

// card.lua:2462 context.first_hand_drawn
static Val jk_first_hand_drawn(Card *self, Table *context) {
    const char *name = tstr(self->ability, "name");
    bool blueprint = truthy(tget(context, "blueprint"));
    if (streq(name, "Certificate")) {
        ADD_EV(ev_certificate, sc_new(self, context));
        playing_card_joker_effects(ARR(B(true)));
    }
    if (streq(name, "DNA") && !blueprint) juice_card_until(self, FN(eval_dna, NULL), true, -1);
    if (streq(name, "Trading Card") && !blueprint) juice_card_until(self, FN(eval_trading, NULL), true, -1);
    return NIL;
}

// card.lua:2491 context.setting_blind and not self.getting_sliced
static Val jk_setting_blind(Card *self, Table *context) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    bool boss = tbool(ttab(context, "blind"), "boss"); // context.blind: the blind's prototype
    if (streq(name, "Chicot") && !truthy(tget(context, "blueprint")) && boss && !SLICED(self)) ADD_EV(ev_chicot, self);
    if (streq(name, "Madness") && !truthy(tget(context, "blueprint")) && !boss) {
        tadd(ability, "x_mult", tnum(ability, "extra"));
        Table *destructable_jokers = tnew();
        for (int i = 1; i <= CardArea_n(G->jokers); i++) {
            Card *j = CardArea_card(G->jokers, i);
            if (j != self && !tbool(j->ability, "eternal") && !SLICED(j)) tinsert(destructable_jokers, O(j));
        }
        Card *joker_to_destroy = tlen(destructable_jokers) > 0
                                     ? pseudorandom_element(destructable_jokers, N(pseudoseed("madness", NULL)), NULL).obj
                                     : NULL;
        if (joker_to_destroy && !SLICED(bc(context, self))) {
            tsetb(DYN(joker_to_destroy), "getting_sliced", true);
            struct madness_ctx *c = NEWCTX(struct madness_ctx);
            c->self = self, c->context = context, c->joker_to_destroy = joker_to_destroy;
            ADD_EV(ev_madness, c);
        }
        if (!SLICED(bc(context, self))) cest(bc(context, self), TAB("message", locv("a_xmult", tget(ability, "x_mult"))));
    }
    if (streq(name, "Burglar") && !SLICED(bc(context, self))) ADD_EV(ev_burglar, sc_new(self, context));
    if (streq(name, "Riff-raff") && !SLICED(bc(context, self)) &&
        CardArea_n(G->jokers) + tnum(G->GAME, "joker_buffer") < tnum(ACFG(G->jokers), "card_limit")) {
        num *jokers_to_create = NEWCTX(num);
        *jokers_to_create = lmin(2, tnum(ACFG(G->jokers), "card_limit") - (CardArea_n(G->jokers) + tnum(G->GAME, "joker_buffer")));
        tadd(G->GAME, "joker_buffer", *jokers_to_create);
        ADD_EV(ev_riff_raff, jokers_to_create);
        cest(bc(context, self), TAB("message", LOC("k_plus_joker"), "colour", P(G->C.BLUE)));
    }
    if (streq(name, "Cartomancer") && !SLICED(bc(context, self)) && consumeable_room()) {
        tadd(G->GAME, "consumeable_buffer", 1);
        ADD_EV(ev_cartomancer, sc_new(self, context));
    }
    if (streq(name, "Ceremonial Dagger") && !truthy(tget(context, "blueprint"))) {
        int my_pos = 0; // 0 as nil
        for (int i = 1; i <= CardArea_n(G->jokers); i++) {
            if (CardArea_card(G->jokers, i) == self) { my_pos = i; break; }
        }
        Card *next = my_pos ? CardArea_card(G->jokers, my_pos + 1) : NULL;
        if (my_pos && next && !SLICED(self) && !tbool(next->ability, "eternal") && !SLICED(next)) {
            Card *sliced_card = next;
            tsetb(DYN(sliced_card), "getting_sliced", true);
            tadd(G->GAME, "joker_buffer", -1);
            struct dagger_ctx *c = NEWCTX(struct dagger_ctx);
            c->self = self, c->sliced_card = sliced_card;
            ADD_EV(ev_dagger, c);
            cest(self, TAB("message", locv("a_mult", N(tnum(ability, "mult") + 2 * sliced_card->sell_cost)), "colour", P(G->C.RED),
                           "no_juice", B(true)));
        }
    }
    if (streq(name, "Marble Joker") && !SLICED(bc(context, self))) {
        ADD_EV(ev_marble_card, NULL);
        cest(bc(context, self), TAB("message", LOC("k_plus_stone"), "colour", P(G->C.SECONDARY_SET.Enhanced)));
        ADD_EV(ev_marble_limit, NULL);
        draw_card(G->play, G->deck, 90, "up", false, NULL, -1, false, false, -1, false);
        playing_card_joker_effects(ARR(B(true)));
    }
    return NIL;
}

// card.lua:2603 context.destroying_card and not context.blueprint
static Val jk_destroying_card(Card *self, Table *context) {
    Table *full_hand = ttab(context, "full_hand");
    if (streq(tstr(self->ability, "name"), "Sixth Sense") && tlen(full_hand) == 1 && Card_get_id(tobji(full_hand, 1)) == 6 &&
        tnum(CUR_ROUND, "hands_played") == 0) {
        if (consumeable_room()) {
            tadd(G->GAME, "consumeable_buffer", 1);
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.trigger = "before", .delay = 0.0, .func = ev_create_consumeable, .ctx = mk_new("Spectral", "sixth")}),
                  NULL, false);
            cest(bc(context, self), TAB("message", LOC("k_plus_spectral"), "colour", P(G->C.SECONDARY_SET.Spectral)));
        }
        return B(true);
    }
    return NIL;
}

// card.lua:2622 context.cards_destroyed (no caller sets it in 1.0.1o; translated as it is)
static Val jk_cards_destroyed(Card *self, Table *context) {
    const char *name = tstr(self->ability, "name");
    Table *glass_shattered = ttab(context, "glass_shattered");
    Val v;
    if (streq(name, "Caino") && !truthy(tget(context, "blueprint"))) {
        num faces = 0;
        for (int k = 1; truthy(v = tgeti(glass_shattered, k)); k++) {
            if (Card_is_face(v.obj, false)) faces = faces + 1;
        }
        if (faces > 0) ADD_EV(ev_caino_outer, cnt_new(self, faces));
        return NIL;
    }
    if (streq(name, "Glass Joker") && !truthy(tget(context, "blueprint"))) {
        num glasses = 0;
        for (int k = 1; truthy(v = tgeti(glass_shattered, k)); k++) {
            if (tbool(DYN((Card *)v.obj), "shattered")) glasses = glasses + 1;
        }
        if (glasses > 0) ADD_EV(ev_glass_outer, cnt_new(self, glasses));
        return NIL;
    }
    return NIL;
}

// card.lua:2672 context.remove_playing_cards
static Val jk_remove_playing_cards(Card *self, Table *context) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    Table *removed = ttab(context, "removed");
    Val val;
    if (streq(name, "Caino") && !truthy(tget(context, "blueprint"))) {
        num face_cards = 0;
        for (int k = 1; truthy(val = tgeti(removed, k)); k++) {
            if (Card_is_face(val.obj, false)) face_cards = face_cards + 1;
        }
        if (face_cards > 0) {
            tadd(ability, "caino_xmult", face_cards * tnum(ability, "extra"));
            ADD_EV(ev_caino_removed, self);
        }
        return NIL;
    }
    if (streq(name, "Glass Joker") && !truthy(tget(context, "blueprint"))) {
        num glass_cards = 0;
        for (int k = 1; truthy(val = tgeti(removed, k)); k++) {
            if (tbool(DYN((Card *)val.obj), "shattered")) glass_cards = glass_cards + 1;
        }
        if (glass_cards > 0) ADD_EV(ev_glass_outer, cnt_new(self, glass_cards));
        return NIL;
    }
    return NIL;
}

// card.lua:2708 context.using_consumeable
static Val jk_using_consumeable(Card *self, Table *context) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    Card *consumeable = tobj(context, "consumeable");
    if (streq(name, "Glass Joker") && !truthy(tget(context, "blueprint")) && streq(tstr(consumeable->ability, "name"), "The Hanged Man")) {
        num shattered_glass = 0;
        Val val;
        for (int k = 1; truthy(val = tgeti(G->hand->highlighted, k)); k++) {
            if (streq(tstr(((Card *)val.obj)->ability, "name"), "Glass Card")) shattered_glass = shattered_glass + 1;
        }
        if (shattered_glass > 0) {
            tadd(ability, "x_mult", tnum(ability, "extra") * shattered_glass);
            ADD_EV(ev_xmult_text, self);
        }
        return NIL;
    }
    if (streq(name, "Fortune Teller") && !truthy(tget(context, "blueprint")) && streq(tstr(consumeable->ability, "set"), "Tarot")) {
        ADD_EV(ev_fortune_teller, self);
    }
    if (streq(name, "Constellation") && !truthy(tget(context, "blueprint")) && streq(tstr(consumeable->ability, "set"), "Planet")) {
        tadd(ability, "x_mult", tnum(ability, "extra"));
        ADD_EV(ev_xmult_text, self);
        return NIL;
    }
    return NIL;
}

// card.lua:2735 context.debuffed_hand
static Val jk_debuffed_hand(Card *self, Table *context) {
    Table *ability = self->ability;
    if (streq(tstr(ability, "name"), "Matador")) {
        if (((Blind *)tobj(G->GAME, "blind"))->triggered) {
            ease_dollars(tnum(ability, "extra"), false);
            tsetn(G->GAME, "dollar_buffer", tnum(G->GAME, "dollar_buffer") + tnum(ability, "extra")); // (dollar_buffer or 0) + extra
            ADD_EV(ev_dollar_buffer, NULL);
            return T(TAB("message", dollar_msg(tget(ability, "extra")), "dollars", tget(ability, "extra"), "colour", P(G->C.MONEY)));
        }
    }
    return NIL;
}

// card.lua:2748 context.pre_discard
static Val jk_pre_discard(Card *self, Table *context) {
    if (streq(tstr(self->ability, "name"), "Burnt Joker") && tnum(CUR_ROUND, "discards_used") <= 0 && !truthy(tget(context, "hook"))) {
        const char *disp_text; // the Lua's second return (loc_disp_text), not used
        const char *text = FUNCS_get_poker_hand_info(G->hand->highlighted, &disp_text, NULL, NULL, NULL);
        cest(bc(context, self), TAB("message", LOC("k_upgrade_ex")));
        Table *hand = ttab(ttab(G->GAME, "hands"), text);
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.8), "delay", N(0.3)),
                         TAB("handname", localize(S(text), "poker_hands"), "chips", tget(hand, "chips"), "mult", tget(hand, "mult"),
                             "level", tget(hand, "level")));
        level_up_hand(bc(context, self), text, false, N(1));
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(1.1), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "handname", S(""), "level", S("")));
    }
    return NIL;
}

// card.lua:2756 context.discard
static Val jk_discard(Card *self, Table *context) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    Table *full_hand = ttab(context, "full_hand");
    Card *other_card = tobj(context, "other_card");
    if (streq(name, "Ramen") && !truthy(tget(context, "blueprint"))) {
        if (tnum(ability, "x_mult") - tnum(ability, "extra") <= 1) {
            ADD_EV(ev_eaten, self);
            return T(TAB("message", LOC("k_eaten_ex"), "colour", P(G->C.FILTER)));
        } else {
            tsetn(ability, "x_mult", tnum(ability, "x_mult") - tnum(ability, "extra"));
            return T(TAB("delay", N(0.2), "message", locv("a_xmult_minus", tget(ability, "extra")), "colour", P(G->C.RED)));
        }
    }
    if (streq(name, "Yorick") && !truthy(tget(context, "blueprint"))) {
        if (tnum(ability, "yorick_discards") <= 1) {
            Table *extra = ttab(ability, "extra");
            tset(ability, "yorick_discards", tget(extra, "discards"));
            tadd(ability, "x_mult", tnum(extra, "xmult"));
            return T(TAB("delay", N(0.2), "message", locv("a_xmult", tget(ability, "x_mult")), "colour", P(G->C.RED)));
        } else {
            tadd(ability, "yorick_discards", -1);
        }
        return NIL;
    }
    if (streq(name, "Trading Card") && !truthy(tget(context, "blueprint")) && tnum(CUR_ROUND, "discards_used") <= 0 &&
        tlen(full_hand) == 1) {
        ease_dollars(tnum(ability, "extra"), false);
        return T(TAB("message", dollar_msg(tget(ability, "extra")), "colour", P(G->C.MONEY), "delay", N(0.45), "remove", B(true),
                     "card", O(self)));
    }

    if (streq(name, "Castle") && !other_card->debuff &&
        Card_is_suit(other_card, tstr(ttab(CUR_ROUND, "castle_card"), "suit"), false, false) && !truthy(tget(context, "blueprint"))) {
        Table *extra = ttab(ability, "extra");
        tadd(extra, "chips", tnum(extra, "chip_mod"));
        return T(TAB("message", LOC("k_upgrade_ex"), "card", O(self), "colour", P(G->C.CHIPS)));
    }
    if (streq(name, "Mail-In Rebate") && !other_card->debuff &&
        veq(N(Card_get_id(other_card)), tget(ttab(CUR_ROUND, "mail_card"), "id"))) {
        ease_dollars(tnum(ability, "extra"), false);
        return T(TAB("message", dollar_msg(tget(ability, "extra")), "colour", P(G->C.MONEY), "card", O(self)));
    }
    if (streq(name, "Hit the Road") && !other_card->debuff && Card_get_id(other_card) == 11 && !truthy(tget(context, "blueprint"))) {
        tadd(ability, "x_mult", tnum(ability, "extra"));
        return T(TAB("message", locv("a_xmult", tget(ability, "x_mult")), "colour", P(G->C.RED), "delay", N(0.45), "card", O(self)));
    }
    if (streq(name, "Green Joker") && !truthy(tget(context, "blueprint")) &&
        veq(tget(context, "other_card"), tgeti(full_hand, tlen(full_hand)))) {
        Table *extra = ttab(ability, "extra");
        num prev_mult = tnum(ability, "mult");
        tsetn(ability, "mult", lmax(0, tnum(ability, "mult") - tnum(extra, "discard_sub")));
        if (tnum(ability, "mult") != prev_mult) {
            return T(TAB("message", locv("a_mult_minus", tget(extra, "discard_sub")), "colour", P(G->C.RED), "card", O(self)));
        }
    }

    if (streq(name, "Faceless Joker") && veq(tget(context, "other_card"), tgeti(full_hand, tlen(full_hand)))) {
        num face_cards = 0;
        Val v;
        for (int k = 1; truthy(v = tgeti(full_hand, k)); k++) {
            if (Card_is_face(v.obj, false)) face_cards = face_cards + 1;
        }
        if (face_cards >= tnum(ttab(ability, "extra"), "faces")) {
            ADD_EV(ev_faceless, sc_new(self, context));
            return NIL;
        }
    }
    return NIL;
}

// ---------------------------------------------------------------- the functions of card.h

// card.lua:2291
Val Card_calculate_joker(Card *self, Table *context) {
    Table *ability = self->ability;
    if (self->debuff) return NIL;
    if (streq(tstr(ability, "set"), "Planet") && !self->debuff) {
        if (truthy(tget(context, "joker_main"))) {
            if (truthy(tget(ttab(G->GAME, "used_vouchers"), "v_observatory")) &&
                veq(tget(ttab(ability, "consumeable"), "hand_type"), tget(context, "scoring_name"))) {
                Val extra = tpath(G->P_CENTERS, "v_observatory.config.extra");
                return T(TAB("message", locv("a_xmult", extra), "Xmult_mod", extra));
            }
        }
    }
    if (streq(tstr(ability, "set"), "Joker") && !self->debuff) {
        const char *name = tstr(ability, "name");
        if (streq(name, "Blueprint")) {
            Card *other_joker = NULL;
            for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                if (CardArea_card(G->jokers, i) == self) other_joker = CardArea_card(G->jokers, i + 1);
            }
            if (other_joker && other_joker != self) {
                Val bp = tget(context, "blueprint");
                tsetn(context, "blueprint", truthy(bp) ? bp.n + 1 : 1);
                if (!truthy(tget(context, "blueprint_card"))) tseto(context, "blueprint_card", self);
                if (tnum(context, "blueprint") > CardArea_n(G->jokers) + 1) return NIL;
                Val other_joker_ret = Card_calculate_joker(other_joker, context);
                if (truthy(other_joker_ret)) {
                    // note: always a table here (the only other return, Sixth Sense's true, needs not context.blueprint)
                    if (other_joker_ret.t != V_TAB) ERROR("card.lua:2315 attempt to index a boolean value");
                    tseto(other_joker_ret.tab, "card", bc(context, self));
                    tset(other_joker_ret.tab, "colour", P(G->C.BLUE));
                    return other_joker_ret;
                }
            }
        }
        if (streq(name, "Brainstorm")) {
            Card *other_joker = CardArea_card(G->jokers, 1);
            if (other_joker && other_joker != self) {
                Val bp = tget(context, "blueprint");
                tsetn(context, "blueprint", truthy(bp) ? bp.n + 1 : 1);
                if (!truthy(tget(context, "blueprint_card"))) tseto(context, "blueprint_card", self);
                if (tnum(context, "blueprint") > CardArea_n(G->jokers) + 1) return NIL;
                Val other_joker_ret = Card_calculate_joker(other_joker, context);
                if (truthy(other_joker_ret)) {
                    if (other_joker_ret.t != V_TAB) ERROR("card.lua:2329 attempt to index a boolean value");
                    tseto(other_joker_ret.tab, "card", bc(context, self));
                    tset(other_joker_ret.tab, "colour", P(G->C.RED));
                    return other_joker_ret;
                }
            }
        }
        // card.lua:2335-4062 the elseif chain, in three parts (card.h)
        Val ret = NIL;
        if (Card_calculate_joker_a(self, context, &ret)) return ret;
        if (Card_calculate_joker_b(self, context, &ret)) return ret;
        return Card_calculate_joker_c(self, context);
    }
    return NIL;
}

// card.lua:2335-2873
bool Card_calculate_joker_a(Card *self, Table *context, Val *ret) {
    Table *ability = self->ability;
    const char *name = tstr(ability, "name");
    bool blueprint = truthy(tget(context, "blueprint"));
    if (truthy(tget(context, "open_booster"))) {
        *ret = jk_open_booster(self, context);
    } else if (truthy(tget(context, "buying_card"))) {
        *ret = NIL; // card.lua:2352 empty
    } else if (truthy(tget(context, "selling_self"))) {
        *ret = jk_selling_self(self, context);
    } else if (truthy(tget(context, "selling_card"))) { // card.lua:2395
        if (streq(name, "Campfire") && !blueprint) {
            tadd(ability, "x_mult", tnum(ability, "extra"));
            ADD_EV(ev_campfire, self);
        }
        *ret = NIL;
    } else if (truthy(tget(context, "reroll_shop"))) { // card.lua:2403
        if (streq(name, "Flash Card") && !blueprint) {
            tadd(ability, "mult", tnum(ability, "extra"));
            ADD_EV(ev_flash_card, self);
        }
        *ret = NIL;
    } else if (truthy(tget(context, "ending_shop"))) { // card.lua:2412
        if (streq(name, "Perkeo")) {
            if (CardArea_card(G->consumeables, 1)) {
                ADD_EV(ev_perkeo, NULL);
                cest(bc(context, self), TAB("message", LOC("k_duplicated_ex")));
            }
        }
        *ret = NIL;
    } else if (truthy(tget(context, "skip_blind"))) { // card.lua:2428
        if (streq(name, "Throwback") && !blueprint) ADD_EV(ev_throwback, self);
        *ret = NIL;
    } else if (truthy(tget(context, "skipping_booster"))) { // card.lua:2441
        if (streq(name, "Red Card") && !blueprint) {
            tadd(ability, "mult", tnum(ability, "extra"));
            ADD_EV(ev_red_card, self);
        }
        *ret = NIL;
    } else if (truthy(tget(context, "playing_card_added")) && !SLICED(self)) {
        *ret = jk_playing_card_added(self, context);
    } else if (truthy(tget(context, "first_hand_drawn"))) {
        *ret = jk_first_hand_drawn(self, context);
    } else if (truthy(tget(context, "setting_blind")) && !SLICED(self)) {
        *ret = jk_setting_blind(self, context);
    } else if (truthy(tget(context, "destroying_card")) && !blueprint) {
        *ret = jk_destroying_card(self, context);
    } else if (truthy(tget(context, "cards_destroyed"))) {
        *ret = jk_cards_destroyed(self, context);
    } else if (truthy(tget(context, "remove_playing_cards"))) {
        *ret = jk_remove_playing_cards(self, context);
    } else if (truthy(tget(context, "using_consumeable"))) {
        *ret = jk_using_consumeable(self, context);
    } else if (truthy(tget(context, "debuffed_hand"))) {
        *ret = jk_debuffed_hand(self, context);
    } else if (truthy(tget(context, "pre_discard"))) {
        *ret = jk_pre_discard(self, context);
    } else if (truthy(tget(context, "discard"))) {
        *ret = jk_discard(self, context);
    } else {
        return false;
    }
    return true;
}
