// card.lua:1091-1680: Card:use_consumeable, can_use_consumeable, check_use, sell_card, can_sell_card,
// calculate_dollar_bonus.
#include "all.h"

// What the event closures of this file capture (each closure uses the members it needs).
typedef struct {
    Card *self, *used_tarot;
    Card *other;        // rightmost (Death), conv_card (seals), chosen_joker (Ankh)
    int i;              // the loop index
    float percent;
    bool from_cards;    // the flip events: G.hand.cards (Sigil / Ouija) instead of G.hand.highlighted
    const char *s;      // _suit (Sigil), _rank (Ouija)
    Table *t;           // destroyed_cards, deletable_jokers, temp_pool
    CardArea *area;     // sell_card's area
} Ctx;

static Ctx *ctx_new(Card *self, Card *used_tarot) {
    Ctx *c = NEWCTX(Ctx);
    c->self = self, c->used_tarot = used_tarot;
    return c;
}
static void add_after(num delay_, EventFunc func, void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = delay_, .func = func, .ctx = ctx}), NULL, false);
}
static bool name_is(Card *card, const char *name) { return streq(tstr(card->ability, "name"), name); }
static Table *node_config(void *node) { return ((Node *)node)->config; }

// card.lua:1102, 1223, 1275, 1294, 1348 (after 0.4) and 1180 (immediate): the same body
static bool ev_tarot1_juice(void *ctx) {
    Ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    return true;
}
// card.lua:1108, 1229
static bool ev_flip_card1(void *ctx) {
    Ctx *c = ctx;
    Table *list = c->from_cards ? G->hand->cards : G->hand->highlighted; // looked up when the event runs, as the Lua
    Card_flip(tobji(list, c->i));
    play_sound("card1", c->percent, -1);
    Card_juice_up(tobji(list, c->i), 0.3f, N(0.3));
    return true;
}
// card.lua:1148, 1260
static bool ev_flip_tarot2(void *ctx) {
    Ctx *c = ctx;
    Table *list = c->from_cards ? G->hand->cards : G->hand->highlighted;
    Card_flip(tobji(list, c->i));
    play_sound("tarot2", c->percent, 0.6f);
    Card_juice_up(tobji(list, c->i), 0.3f, N(0.3));
    return true;
}
// card.lua:1115
static bool ev_death(void *ctx) {
    Ctx *c = ctx;
    Card *h = tobji(G->hand->highlighted, c->i);
    if (h != c->other) copy_card(c->other, h, -1, NIL, false);
    return true;
}
// card.lua:1123
static bool ev_strength(void *ctx) {
    Ctx *c = ctx;
    Card *card = tobji(G->hand->highlighted, c->i);
    const char *suit_prefix = fmt("%s_", str_sub(tstr(card->base, "suit"), 1, 1));
    num id = tnum(card->base, "id");
    num rank = id == 14 ? 2 : lmin(id + 1, 14);
    const char *rank_suffix;
    if (rank < 10) rank_suffix = number_tostring(rank);
    else if (rank == 10) rank_suffix = "T";
    else if (rank == 11) rank_suffix = "J";
    else if (rank == 12) rank_suffix = "Q";
    else if (rank == 13) rank_suffix = "K";
    else if (rank == 14) rank_suffix = "A";
    else rank_suffix = number_tostring(rank); // the Lua concatenates the number as it is
    Card_set_base(card, ttab(G->P_CARDS, fmt("%s%s", suit_prefix, rank_suffix)), false);
    return true;
}
// card.lua:1139
static bool ev_suit_conv(void *ctx) {
    Ctx *c = ctx;
    Card_change_suit(tobji(G->hand->highlighted, c->i), tstr(ttab(c->self->ability, "consumeable"), "suit_conv"));
    return true;
}
// card.lua:1143
static bool ev_mod_conv(void *ctx) {
    Ctx *c = ctx;
    Card_set_ability(tobji(G->hand->highlighted, c->i),
                     ttab(G->P_CENTERS, tstr(ttab(c->self->ability, "consumeable"), "mod_conv")), false, false);
    return true;
}
// card.lua:1150, 1190
static bool ev_unhighlight_all(void *ctx) {
    CardArea_unhighlight_all(G->hand);
    return true;
}
// card.lua:1155
static bool ev_black_hole_1(void *ctx) {
    Ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    Card_juice_up(c->self, 0.8f, N(0.5));
    G->TAROT_INTERRUPT_PULSE = true;
    return true;
}
// card.lua:1161
static bool ev_black_hole_2(void *ctx) {
    Ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    Card_juice_up(c->self, 0.8f, N(0.5));
    return true;
}
// card.lua:1166
static bool ev_black_hole_3(void *ctx) {
    Ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    Card_juice_up(c->self, 0.8f, N(0.5));
    G->TAROT_INTERRUPT_PULSE = false;
    return true;
}
// card.lua:1185
static bool ev_set_seal(void *ctx) {
    Ctx *c = ctx;
    Card_set_seal(c->other, tstr(c->self->ability, "extra"), false, true);
    return true;
}
// card.lua:1193
static bool ev_aura(void *ctx) {
    Ctx *c = ctx;
    Table *edition = poll_edition("aura", -1, true, true);
    Card *aura_card = tobji(G->hand->highlighted, 1);
    Card_set_edition(aura_card, edition, true, false);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    return true;
}
// card.lua:1203
static bool ev_cryptid(void *ctx) {
    Ctx *c = ctx;
    bool _first_dissolve = false;
    Table *new_cards = tnew();
    num extra = tnum(c->self->ability, "extra");
    for (int i = 1; i <= extra; i++) {
        G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1: 0 as nil
        Card *_card = copy_card(tobji(G->hand->highlighted, 1), NULL, -1, N(G->playing_card), false);
        Card_add_to_deck(_card, false);
        tsetn(node_config(G->deck), "card_limit", tnum(node_config(G->deck), "card_limit") + 1);
        tinsert(G->playing_cards, O(_card));
        CardArea_emplace(G->hand, _card, NULL, false);
        Card_start_materialize(_card, NULL, _first_dissolve, -1);
        _first_dissolve = true;
        tseti(new_cards, tlen(new_cards) + 1, O(_card));
    }
    playing_card_joker_effects(new_cards);
    return true;
}
// card.lua:1235
static bool ev_sigil(void *ctx) {
    Ctx *c = ctx;
    Card *card = tobji(G->hand->cards, c->i);
    const char *suit_prefix = fmt("%s_", c->s);
    num id = tnum(card->base, "id");
    const char *rank_suffix = NULL;
    if (id < 10) rank_suffix = number_tostring(id);
    else if (id == 10) rank_suffix = "T";
    else if (id == 11) rank_suffix = "J";
    else if (id == 12) rank_suffix = "Q";
    else if (id == 13) rank_suffix = "K";
    else if (id == 14) rank_suffix = "A";
    else ERROR("card.lua:1242: attempt to concatenate local 'rank_suffix' (a boolean value)");
    Card_set_base(card, ttab(G->P_CARDS, fmt("%s%s", suit_prefix, rank_suffix)), false);
    return true;
}
// card.lua:1249
static bool ev_ouija(void *ctx) {
    Ctx *c = ctx;
    Card *card = tobji(G->hand->cards, c->i);
    const char *suit_prefix = fmt("%s_", str_sub(tstr(card->base, "suit"), 1, 1));
    const char *rank_suffix = c->s;
    Card_set_base(card, ttab(G->P_CARDS, fmt("%s%s", suit_prefix, rank_suffix)), false);
    return true;
}
// card.lua:1282
static bool ev_hanged_man(void *ctx) {
    int n = tlen(G->hand->highlighted);
    for (int i = n; i >= 1; i--) {
        Card *card = tobji(G->hand->highlighted, i);
        if (name_is(card, "Glass Card")) Card_shatter(card);
        else Card_start_dissolve(card, NULL, i == tlen(G->hand->highlighted), -1, false);
    }
    return true;
}
// card.lua:1301 (silent_last false: i ~= #destroyed_cards) and card.lua:1355 (silent_last true: i == #destroyed_cards)
static void destroy_cards(Table *destroyed_cards, bool silent_last) {
    for (int i = tlen(destroyed_cards); i >= 1; i--) {
        Card *card = tobji(destroyed_cards, i);
        if (name_is(card, "Glass Card")) Card_shatter(card);
        else Card_start_dissolve(card, NULL, silent_last ? i == tlen(destroyed_cards) : i != tlen(destroyed_cards), -1, false);
    }
}
// card.lua:1301
static bool ev_destroy_random(void *ctx) {
    destroy_cards(((Ctx *)ctx)->t, false);
    return true;
}
// card.lua:1355
static bool ev_destroy_immolate(void *ctx) {
    destroy_cards(((Ctx *)ctx)->t, true);
    return true;
}
// card.lua:1314
static bool ev_spectral_create(void *ctx) {
    Ctx *c = ctx;
    Card *self = c->self;
    Table *cards = tnew();
    num extra = tnum(self->ability, "extra");
    for (int i = 1; i <= extra; i++) {
        tseti(cards, i, B(true));
        const char *_suit = NULL, *_rank = NULL;
        if (name_is(self, "Familiar")) {
            _rank = pseudorandom_element(ARR(S("J"), S("Q"), S("K")), N(pseudoseed("familiar_create", NULL)), NULL).s;
            _suit = pseudorandom_element(ARR(S("S"), S("H"), S("D"), S("C")), N(pseudoseed("familiar_create", NULL)), NULL).s;
        } else if (name_is(self, "Grim")) {
            _rank = "A";
            _suit = pseudorandom_element(ARR(S("S"), S("H"), S("D"), S("C")), N(pseudoseed("grim_create", NULL)), NULL).s;
        } else if (name_is(self, "Incantation")) {
            _rank = pseudorandom_element(ARR(S("2"), S("3"), S("4"), S("5"), S("6"), S("7"), S("8"), S("9"), S("T")),
                                         N(pseudoseed("incantation_create", NULL)), NULL).s;
            _suit = pseudorandom_element(ARR(S("S"), S("H"), S("D"), S("C")), N(pseudoseed("incantation_create", NULL)), NULL).s;
        }
        if (!_suit) _suit = "S";
        if (!_rank) _rank = "A";
        Table *cen_pool = tnew();
        for (TIter it = titer(ttab(G->P_CENTER_POOLS, "Enhanced")); tnext(&it);) {
            Table *v = it.val.tab;
            if (!streq(tstr(v, "key"), "m_stone")) tseti(cen_pool, tlen(cen_pool) + 1, T(v));
        }
        Val front = T(ttab(G->P_CARDS, fmt("%s_%s", _suit, _rank)));
        Val center = pseudorandom_element(cen_pool, N(pseudoseed("spe_card", NULL)), NULL);
        create_playing_card(TAB("front", front, "center", center), G->hand, false, i != 1,
                            ARR(P(G->C.SECONDARY_SET.Spectral)));
    }
    playing_card_joker_effects(cards);
    return true;
}
// card.lua:1343
static bool immolate_less(Val va, Val vb, void *ctx) {
    Card *a = va.obj, *b = vb.obj;
    return !a->playing_card || !b->playing_card || a->playing_card < b->playing_card;
}
// card.lua:1374
static bool ev_fool(void *ctx) {
    Ctx *c = ctx;
    if (tnum(node_config(G->consumeables), "card_limit") > CardArea_n(G->consumeables)) {
        play_sound("timpani", -1, -1);
        Card *card = create_card("Tarot_Planet", G->consumeables, false, NIL, false, false,
                                 tstr(G->GAME, "last_tarot_planet"), "fool");
        Card_add_to_deck(card, false);
        CardArea_emplace(G->consumeables, card, NULL, false);
        Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    }
    return true;
}
// card.lua:1386
static bool ev_hermit(void *ctx) {
    Ctx *c = ctx;
    play_sound("timpani", -1, -1);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    ease_dollars(lmax(0, lmin(tnum(G->GAME, "dollars"), tnum(c->self->ability, "extra"))), true);
    return true;
}
// card.lua:1394
static bool ev_temperance(void *ctx) {
    Ctx *c = ctx;
    play_sound("timpani", -1, -1);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    ease_dollars(tnum(c->self->ability, "money"), true);
    return true;
}
// card.lua:1403
static bool ev_emperor_priestess(void *ctx) {
    Ctx *c = ctx;
    Card *self = c->self;
    if (tnum(node_config(G->consumeables), "card_limit") > CardArea_n(G->consumeables)) {
        play_sound("timpani", -1, -1);
        // note: (name == 'The Emperor' and 'Tarot') or (name == 'The High Priestess' and 'Planet'): false for
        // any other name, which this block never sees (NULL here)
        const char *_type = name_is(self, "The Emperor") ? "Tarot" : name_is(self, "The High Priestess") ? "Planet" : NULL;
        const char *key_append = name_is(self, "The Emperor") ? "emp" : name_is(self, "The High Priestess") ? "pri" : NULL;
        Card *card = create_card(_type, G->consumeables, false, NIL, false, false, NULL, key_append);
        Card_add_to_deck(card, false);
        CardArea_emplace(G->consumeables, card, NULL, false);
        Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    }
    return true;
}
// card.lua:1416
static bool ev_judgement_soul(void *ctx) {
    Ctx *c = ctx;
    Card *self = c->self;
    play_sound("timpani", -1, -1);
    Card *card = create_card("Joker", G->jokers, name_is(self, "The Soul"), NIL, false, false, NULL,
                             name_is(self, "Judgement") ? "jud" : "sou");
    Card_add_to_deck(card, false);
    CardArea_emplace(G->jokers, card, NULL, false);
    if (name_is(self, "The Soul")) check_for_unlock(TAB("type", S("spawn_legendary")));
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    return true;
}
// card.lua:1436
static bool ev_ankh_dissolve(void *ctx) {
    Ctx *c = ctx;
    // note: _first_dissolve is an upvalue of use_consumeable that only this closure touches, and a 'before'
    // event's func runs once: a local is the same
    bool _first_dissolve = false;
    for (TIter it = titer(c->t); tnext(&it);) {
        Card *v = it.val.obj;
        if (v != c->other) {
            Card_start_dissolve(v, NULL, _first_dissolve, -1, false);
            _first_dissolve = true;
        }
    }
    return true;
}
// card.lua:1444
static bool ev_ankh_copy(void *ctx) {
    Ctx *c = ctx;
    Card *chosen_joker = c->other;
    Card *card = copy_card(chosen_joker, NULL, -1, NIL, chosen_joker->edition && tbool(chosen_joker->edition, "negative"));
    Card_start_materialize(card, NULL, false, -1);
    Card_add_to_deck(card, false);
    if (card->edition && tbool(card->edition, "negative")) Card_set_edition(card, NULL, true, false);
    CardArea_emplace(G->jokers, card, NULL, false);
    return true;
}
// card.lua:1455
static bool ev_wraith(void *ctx) {
    Ctx *c = ctx;
    play_sound("timpani", -1, -1);
    Card *card = create_card("Joker", G->jokers, false, N(0.99), false, false, NULL, "wra");
    Card_add_to_deck(card, false);
    CardArea_emplace(G->jokers, card, NULL, false);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    if (tnum(G->GAME, "dollars") != 0) ease_dollars(-tnum(G->GAME, "dollars"), true);
    return true;
}
// card.lua:1471
static bool ev_wheel_ecto_hex(void *ctx) {
    Ctx *c = ctx;
    Card *self = c->self;
    // note: the Lua's seed expression is false for any other name (an error inside pseudoseed): never reached here
    const char *seed_key = name_is(self, "The Wheel of Fortune") ? "wheel_of_fortune"
                         : name_is(self, "Ectoplasm") ? "ectoplasm"
                         : name_is(self, "Hex") ? "hex" : NULL;
    if (!seed_key) ERROR("card.lua:1473: pseudoseed(false)");
    Card *eligible_card = pseudorandom_element(c->t, N(pseudoseed(seed_key, NULL)), NULL).obj;
    Table *edition = NULL;
    if (name_is(self, "Ectoplasm")) edition = TAB("negative", B(true));
    else if (name_is(self, "Hex")) edition = TAB("polychrome", B(true));
    else if (name_is(self, "The Wheel of Fortune")) edition = poll_edition("wheel_of_fortune", -1, true, true);
    Card_set_edition(eligible_card, edition, true, false);
    if (name_is(self, "The Wheel of Fortune") || name_is(self, "Ectoplasm") || name_is(self, "Hex"))
        check_for_unlock(TAB("type", S("have_edition")));
    if (name_is(self, "Hex")) {
        bool _first_dissolve = false;
        for (TIter it = titer(G->jokers->cards); tnext(&it);) {
            Card *v = it.val.obj;
            if (v != eligible_card && !tbool(v->ability, "eternal")) {
                Card_start_dissolve(v, NULL, _first_dissolve, -1, false);
                _first_dissolve = true;
            }
        }
    }
    if (name_is(self, "Ectoplasm")) {
        if (!truthy(tget(G->GAME, "ecto_minus"))) tsetn(G->GAME, "ecto_minus", 1);
        CardArea_change_size(G->hand, -tnum(G->GAME, "ecto_minus"));
        tsetn(G->GAME, "ecto_minus", tnum(G->GAME, "ecto_minus") + 1);
    }
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    return true;
}
// card.lua:1513
static bool ev_nope_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}
// card.lua:1502
static bool ev_wheel_nope(void *ctx) {
    Ctx *c = ctx;
    bool in_pack = G->STATE == STATES_TAROT_PACK || G->STATE == STATES_SPECTRAL_PACK;
    attention_text(TAB("text", localize(S("k_nope_ex"), NULL),
                       "scale", N(1.3),
                       "hold", N(1.4),
                       "major", O(c->used_tarot),
                       "backdrop_colour", P(G->C.SECONDARY_SET.Tarot),
                       "align", S(in_pack ? "tm" : "cm"),
                       "offset", T(TAB("x", N(0), "y", N(in_pack ? -0.2 : 0))),
                       "silent", B(true)));
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"),
                                             .not_blockable = true, .not_blocking = true, .func = ev_nope_sound}), NULL, false);
    play_sound("tarot2", 1, 0.4f);
    Card_juice_up(c->used_tarot, 0.3f, N(0.5));
    return true;
}

// card.lua:1091
void Card_use_consumeable(Card *self, CardArea *area, Card *copier) {
    stop_use();
    if (!copier) set_consumeable_usage(self);
    if (self->debuff) return;
    Card *used_tarot = copier ? copier : self;
    Table *consumeable = ttab(self->ability, "consumeable");

    if (truthy(tget(consumeable, "max_highlighted"))) {
        update_hand_text(TAB("immediate", B(true), "nopulse", B(true), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "level", S(""), "handname", S("")));
    }

    if (truthy(tget(consumeable, "mod_conv")) || truthy(tget(consumeable, "suit_conv"))) {
        add_after(0.4, ev_tarot1_juice, ctx_new(self, used_tarot));
        int n = tlen(G->hand->highlighted);
        for (int i = 1; i <= n; i++) {
            Ctx *c = ctx_new(self, used_tarot);
            c->i = i;
            c->percent = 1.15 - (i - 0.999) / (tlen(G->hand->highlighted) - 0.998) * 0.3;
            add_after(0.15, ev_flip_card1, c);
        }
        delay(0.2, NULL);
        if (name_is(self, "Death")) {
            Card *rightmost = tobji(G->hand->highlighted, 1);
            for (int i = 1; i <= n; i++) {
                Card *h = tobji(G->hand->highlighted, i);
                if (h->moveable.node.T->x > rightmost->moveable.node.T->x) rightmost = h;
            }
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i, c->other = rightmost;
                add_after(0.1, ev_death, c);
            }
        } else if (name_is(self, "Strength")) {
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i;
                add_after(0.1, ev_strength, c);
            }
        } else if (truthy(tget(consumeable, "suit_conv"))) {
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i;
                add_after(0.1, ev_suit_conv, c);
            }
        } else {
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i;
                add_after(0.1, ev_mod_conv, c);
            }
        }
        for (int i = 1; i <= n; i++) {
            Ctx *c = ctx_new(self, used_tarot);
            c->i = i;
            c->percent = 0.85 + (i - 0.999) / (tlen(G->hand->highlighted) - 0.998) * 0.3;
            add_after(0.15, ev_flip_tarot2, c);
        }
        add_after(0.2, ev_unhighlight_all, NULL);
        delay(0.5, NULL);
    }
    if (name_is(self, "Black Hole")) {
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.8), "delay", N(0.3)),
                         TAB("handname", localize(S("k_all_hands"), NULL), "chips", S("..."), "mult", S("..."), "level", S("")));
        add_after(0.2, ev_black_hole_1, ctx_new(self, used_tarot));
        update_hand_text(TAB("delay", N(0)), TAB("mult", S("+"), "StatusText", B(true)));
        add_after(0.9, ev_black_hole_2, ctx_new(self, used_tarot));
        update_hand_text(TAB("delay", N(0)), TAB("chips", S("+"), "StatusText", B(true)));
        add_after(0.9, ev_black_hole_3, ctx_new(self, used_tarot));
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.9), "delay", N(0)), TAB("level", S("+1")));
        delay(1.3, NULL);
        // note: pairs(G.GAME.hands) is hash order in the Lua, insertion order here: only the order of the
        // check_for_unlock events that level_up_hand queues differs
        for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) {
            level_up_hand(self, it.key.s, true, NIL);
        }
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(1.1), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "handname", S(""), "level", S("")));
    }
    if (name_is(self, "Talisman") || name_is(self, "Deja Vu") || name_is(self, "Trance") || name_is(self, "Medium")) {
        Card *conv_card = tobji(G->hand->highlighted, 1);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_tarot1_juice, .ctx = ctx_new(self, used_tarot)}), NULL, false);

        Ctx *c = ctx_new(self, used_tarot);
        c->other = conv_card;
        add_after(0.1, ev_set_seal, c);

        delay(0.5, NULL);
        add_after(0.2, ev_unhighlight_all, NULL);
    }
    if (name_is(self, "Aura")) {
        add_after(0.4, ev_aura, ctx_new(self, used_tarot));
    }
    if (name_is(self, "Cryptid")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_cryptid, .ctx = ctx_new(self, used_tarot)}), NULL, false);
    }
    if (name_is(self, "Sigil") || name_is(self, "Ouija")) {
        add_after(0.4, ev_tarot1_juice, ctx_new(self, used_tarot));
        int n = tlen(G->hand->cards);
        for (int i = 1; i <= n; i++) {
            Ctx *c = ctx_new(self, used_tarot);
            c->i = i, c->from_cards = true;
            c->percent = 1.15 - (i - 0.999) / (tlen(G->hand->cards) - 0.998) * 0.3;
            add_after(0.15, ev_flip_card1, c);
        }
        delay(0.2, NULL);
        if (name_is(self, "Sigil")) {
            const char *_suit = pseudorandom_element(ARR(S("S"), S("H"), S("D"), S("C")), N(pseudoseed("sigil", NULL)), NULL).s;
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i, c->s = _suit;
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_sigil, .ctx = c}), NULL, false);
            }
        }
        if (name_is(self, "Ouija")) {
            const char *_rank = pseudorandom_element(ARR(S("2"), S("3"), S("4"), S("5"), S("6"), S("7"), S("8"), S("9"), S("T"),
                                                         S("J"), S("Q"), S("K"), S("A")),
                                                     N(pseudoseed("ouija", NULL)), NULL).s;
            for (int i = 1; i <= n; i++) {
                Ctx *c = ctx_new(self, used_tarot);
                c->i = i, c->s = _rank;
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_ouija, .ctx = c}), NULL, false);
            }
            CardArea_change_size(G->hand, -1);
        }
        for (int i = 1; i <= n; i++) {
            Ctx *c = ctx_new(self, used_tarot);
            c->i = i, c->from_cards = true;
            c->percent = 0.85 + (i - 0.999) / (tlen(G->hand->cards) - 0.998) * 0.3;
            add_after(0.15, ev_flip_tarot2, c);
        }
        delay(0.5, NULL);
    }
    if (truthy(tget(consumeable, "hand_type"))) {
        const char *hand_type = tstr(consumeable, "hand_type");
        Table *hand = ttab(ttab(G->GAME, "hands"), hand_type);
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.8), "delay", N(0.3)),
                         TAB("handname", localize(S(hand_type), "poker_hands"), "chips", tget(hand, "chips"),
                             "mult", tget(hand, "mult"), "level", tget(hand, "level")));
        level_up_hand(used_tarot, hand_type, false, NIL);
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(1.1), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "handname", S(""), "level", S("")));
    }
    if (truthy(tget(consumeable, "remove_card"))) {
        Table *destroyed_cards = tnew();
        if (name_is(self, "The Hanged Man")) {
            for (int i = tlen(G->hand->highlighted); i >= 1; i--) {
                tseti(destroyed_cards, tlen(destroyed_cards) + 1, tgeti(G->hand->highlighted, i));
            }
            add_after(0.4, ev_tarot1_juice, ctx_new(self, used_tarot));
            add_after(0.2, ev_hanged_man, NULL);
        } else if (name_is(self, "Familiar") || name_is(self, "Grim") || name_is(self, "Incantation")) {
            tseti(destroyed_cards, tlen(destroyed_cards) + 1,
                  pseudorandom_element(G->hand->cards, N(pseudoseed("random_destroy", NULL)), NULL));
            add_after(0.4, ev_tarot1_juice, ctx_new(self, used_tarot));
            Ctx *c = ctx_new(self, used_tarot);
            c->t = destroyed_cards;
            add_after(0.1, ev_destroy_random, c);
            add_after(0.7, ev_spectral_create, ctx_new(self, used_tarot));
        } else if (name_is(self, "Immolate")) {
            Table *temp_hand = tnew();
            for (int k = 1; tgeti(G->hand->cards, k).t != V_NIL; k++) tseti(temp_hand, tlen(temp_hand) + 1, tgeti(G->hand->cards, k));
            tsort(temp_hand, immolate_less, NULL);
            pseudoshuffle(temp_hand, N(pseudoseed("immolate", NULL)));

            num destroy = tnum(ttab(self->ability, "extra"), "destroy");
            // destroyed_cards[#destroyed_cards+1] = nil (a hand shorter than extra.destroy) adds nothing
            for (int i = 1; i <= destroy; i++) {
                Val v = tgeti(temp_hand, i);
                if (v.t != V_NIL) tseti(destroyed_cards, tlen(destroyed_cards) + 1, v);
            }

            add_after(0.4, ev_tarot1_juice, ctx_new(self, used_tarot));
            Ctx *c = ctx_new(self, used_tarot);
            c->t = destroyed_cards;
            add_after(0.1, ev_destroy_immolate, c);
            delay(0.5, NULL);
            ease_dollars(tnum(ttab(self->ability, "extra"), "dollars"), false);
        }
        delay(0.3, NULL);
        int njokers = CardArea_n(G->jokers);
        for (int i = 1; i <= njokers; i++) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("remove_playing_cards", B(true), "removed", T(destroyed_cards)));
        }
    }
    if (name_is(self, "The Fool")) {
        add_after(0.4, ev_fool, ctx_new(self, used_tarot));
        delay(0.6, NULL);
    }
    if (name_is(self, "The Hermit")) {
        add_after(0.4, ev_hermit, ctx_new(self, used_tarot));
        delay(0.6, NULL);
    }
    if (name_is(self, "Temperance")) {
        add_after(0.4, ev_temperance, ctx_new(self, used_tarot));
        delay(0.6, NULL);
    }
    if (name_is(self, "The Emperor") || name_is(self, "The High Priestess")) {
        Val tarots = tget(consumeable, "tarots");
        num count = lmin(truthy(tarots) ? tarots.n : tnum(consumeable, "planets"),
                         tnum(node_config(G->consumeables), "card_limit") - CardArea_n(G->consumeables));
        for (int i = 1; i <= count; i++) {
            add_after(0.4, ev_emperor_priestess, ctx_new(self, used_tarot));
        }
        delay(0.6, NULL);
    }
    if (name_is(self, "Judgement") || name_is(self, "The Soul")) {
        add_after(0.4, ev_judgement_soul, ctx_new(self, used_tarot));
        delay(0.6, NULL);
    }
    if (name_is(self, "Ankh")) {
        // (the Lua's comments on the edge cases: max jokers and all eternal, or exactly one not eternal)
        Table *deletable_jokers = tnew();
        for (TIter it = titer(G->jokers->cards); tnext(&it);) {
            Card *v = it.val.obj;
            if (!tbool(v->ability, "eternal")) tseti(deletable_jokers, tlen(deletable_jokers) + 1, O(v));
        }
        Card *chosen_joker = pseudorandom_element(G->jokers->cards, N(pseudoseed("ankh_choice", NULL)), NULL).obj;
        Ctx *c = ctx_new(self, used_tarot);
        c->t = deletable_jokers, c->other = chosen_joker;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.75, .func = ev_ankh_dissolve, .ctx = c}), NULL, false);
        c = ctx_new(self, used_tarot);
        c->other = chosen_joker;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.4, .func = ev_ankh_copy, .ctx = c}), NULL, false);
    }
    if (name_is(self, "Wraith")) {
        add_after(0.4, ev_wraith, ctx_new(self, used_tarot));
        delay(0.6, NULL);
    }
    if (name_is(self, "The Wheel of Fortune") || name_is(self, "Ectoplasm") || name_is(self, "Hex")) {
        // note: the eligible arrays are captured by pointer and read when the event runs, as the Lua's table
        // reference (Card_update empties and refills the same table)
        Table *temp_pool = name_is(self, "The Wheel of Fortune") && self->eligible_strength_jokers ? self->eligible_strength_jokers
                         : (name_is(self, "Ectoplasm") || name_is(self, "Hex")) && self->eligible_editionless_jokers
                               ? self->eligible_editionless_jokers
                               : tnew();
        if (name_is(self, "Ectoplasm") || name_is(self, "Hex") ||
            pseudorandom(S("wheel_of_fortune"), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / tnum(self->ability, "extra")) {
            Ctx *c = ctx_new(self, used_tarot);
            c->t = temp_pool;
            add_after(0.4, ev_wheel_ecto_hex, c);
        } else {
            add_after(0.4, ev_wheel_nope, ctx_new(self, used_tarot));
        }
        delay(0.6, NULL);
    }
}

// card.lua:1523
bool Card_can_use_consumeable(Card *self, bool any_state, bool skip_check) {
    Val STOP_USE = tget(G->GAME, "STOP_USE");
    if (!skip_check && ((G->play && CardArea_n(G->play) > 0) ||
                        G->CONTROLLER->locked ||
                        (truthy(STOP_USE) && STOP_USE.n > 0)))
        return false;
    if ((G->STATE != STATES_HAND_PLAYED && G->STATE != STATES_DRAW_TO_HAND && G->STATE != STATES_PLAY_TAROT) || any_state) {
        Table *consumeable = ttab(self->ability, "consumeable");

        if (name_is(self, "The Hermit") || truthy(tget(consumeable, "hand_type")) || name_is(self, "Temperance") ||
            name_is(self, "Black Hole")) {
            return true;
        }
        if (name_is(self, "The Wheel of Fortune")) {
            // note: next(nil) is an error in the Lua (Card:update has not run yet): false here
            if (self->eligible_strength_jokers && !tempty(self->eligible_strength_jokers)) return true;
        }
        if (name_is(self, "Ankh")) {
            // if there is at least one joker
            for (TIter it = titer(G->jokers->cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (streq(tstr(v->ability, "set"), "Joker") && tnum(node_config(G->jokers), "card_limit") > 1) {
                    return true;
                }
            }
        }
        if (name_is(self, "Aura")) {
            if (G->hand && tlen(G->hand->highlighted) == 1 && tobji(G->hand->highlighted, 1) &&
                !((Card *)tobji(G->hand->highlighted, 1))->edition)
                return true;
        }
        if (name_is(self, "Ectoplasm") || name_is(self, "Hex")) {
            if (self->eligible_editionless_jokers && !tempty(self->eligible_editionless_jokers)) return true;
        }
        if (name_is(self, "The Emperor") || name_is(self, "The High Priestess")) {
            if (CardArea_n(G->consumeables) < tnum(node_config(G->consumeables), "card_limit") || self->area == G->consumeables) return true;
        }
        if (name_is(self, "The Fool")) {
            Val last = tget(G->GAME, "last_tarot_planet");
            if ((CardArea_n(G->consumeables) < tnum(node_config(G->consumeables), "card_limit") || self->area == G->consumeables) &&
                truthy(last) && !veq(last, S("c_fool")))
                return true;
        }
        if (name_is(self, "Judgement") || name_is(self, "The Soul") || name_is(self, "Wraith")) {
            if (CardArea_n(G->jokers) < tnum(node_config(G->jokers), "card_limit") || self->area == G->jokers) {
                return true;
            } else {
                return false;
            }
        }
        if (G->STATE == STATES_SELECTING_HAND || G->STATE == STATES_TAROT_PACK || G->STATE == STATES_SPECTRAL_PACK ||
            G->STATE == STATES_PLANET_PACK) {
            if (truthy(tget(consumeable, "max_highlighted"))) {
                Val mod_num = tget(consumeable, "mod_num");
                if (mod_num.t != V_NUM) ERROR("card.lua:1566: attempt to compare number with nil (mod_num)");
                Val min_highlighted = tget(consumeable, "min_highlighted");
                if (mod_num.n >= tlen(G->hand->highlighted) &&
                    tlen(G->hand->highlighted) >= (truthy(min_highlighted) ? min_highlighted.n : 1)) {
                    return true;
                }
            }
            if ((name_is(self, "Familiar") || name_is(self, "Grim") ||
                 name_is(self, "Incantation") || name_is(self, "Immolate") ||
                 name_is(self, "Sigil") || name_is(self, "Ouija")) &&
                CardArea_n(G->hand) > 1) {
                return true;
            }
        }
    }
    return false;
}

// card.lua:1581
bool Card_check_use(Card *self) {
    if (name_is(self, "Ankh")) {
        if (CardArea_n(G->jokers) >= tnum(node_config(G->jokers), "card_limit")) {
            alert_no_space(self, G->jokers);
            return true;
        }
    }
    return false;
}

// card.lua:1626
static bool ev_sell_unlock(void *ctx) {
    Ctx *c = ctx;
    tset(G->CONTROLLER->locks, "selling_card", NIL);
    Controller_recall_cardarea_focus(G->CONTROLLER, S(c->area == G->jokers ? "jokers" : "consumeables"));
    return true;
}
// card.lua:1624
static bool ev_sell_nest_2(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_sell_unlock, .ctx = ctx}), NULL, false);
    return true;
}
// card.lua:1622
static bool ev_sell_nest_1(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_sell_nest_2, .ctx = ctx}), NULL, false);
    return true;
}
// card.lua:1617
static bool ev_sell_disable_blind(void *ctx) {
    Blind_disable(tobj(G->GAME, "blind"));
    return true;
}
// card.lua:1601
static bool ev_sell_coin(void *ctx) {
    Ctx *c = ctx;
    play_sound("coin2", -1, -1);
    Card_juice_up(c->self, 0.3f, N(0.4));
    return true;
}
// card.lua:1607
static bool ev_sell(void *ctx) {
    Ctx *c = ctx;
    Card *self = c->self;
    ease_dollars(self->sell_cost, false);
    Card_start_dissolve(self, ARR(P(G->C.GOLD)), false, -1, false);
    delay(0.3, NULL);

    inc_career_stat("c_cards_sold", 1);
    if (streq(tstr(self->ability, "set"), "Joker")) {
        inc_career_stat("c_jokers_sold", 1);
    }
    Blind *blind = tobj(G->GAME, "blind");
    if (streq(tstr(self->ability, "set"), "Joker") && blind && streq(blind->name, "Verdant Leaf")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_sell_disable_blind}), NULL, false);
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .not_blocking = true,
                                             .func = ev_sell_nest_1, .ctx = c}), NULL, false);
    return true;
}

// card.lua:1590
void Card_sell_card(Card *self) {
    tsetb(G->CONTROLLER->locks, "selling_card", true);
    stop_use();
    CardArea *area = self->area;
    Controller_save_cardarea_focus(G->CONTROLLER, area == G->jokers ? "jokers" : "consumeables");

    Table *children = self->moveable.node.children;
    if (Card_child(self, "use_button")) { CALL(Card_child(self, "use_button"), remove); tset(children, "use_button", NIL); }
    if (Card_child(self, "sell_button")) { CALL(Card_child(self, "sell_button"), remove); tset(children, "sell_button", NIL); }

    Card_calculate_joker(self, TAB("selling_self", B(true)));

    Ctx *c = ctx_new(self, NULL);
    c->area = area;
    add_after(0.2, ev_sell_coin, c);
    delay(0.2, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_sell, .ctx = c}), NULL, false);
}

// card.lua:1640
bool Card_can_sell_card(Card *self, Table *context) {
    Val STOP_USE = tget(G->GAME, "STOP_USE");
    if ((G->play && CardArea_n(G->play) > 0) ||
        G->CONTROLLER->locked ||
        (truthy(STOP_USE) && STOP_USE.n > 0)) // or G.STATE == G.STATES.BLIND_SELECT (commented out in the Lua)
        return false;
    if ((tbool(G->SETTINGS, "tutorial_complete") || !streq(tstr(ttab(G->GAME, "pseudorandom"), "seed"), "TUTORIAL") ||
         tnum(ttab(G->GAME, "round_resets"), "ante") > 1) &&
        self->area &&
        streq(tstr(node_config(self->area), "type"), "joker") &&
        !tbool(self->ability, "eternal")) {
        return true;
    }
    return false;
}

// card.lua:1655
Val Card_calculate_dollar_bonus(Card *self) {
    if (self->debuff) return NIL;
    if (streq(tstr(self->ability, "set"), "Joker")) {
        if (name_is(self, "Golden Joker")) {
            return tget(self->ability, "extra");
        }
        Val nine_tally = tget(self->ability, "nine_tally");
        if (name_is(self, "Cloud 9") && truthy(nine_tally) && nine_tally.n > 0) {
            return N(tnum(self->ability, "extra") * nine_tally.n);
        }
        if (name_is(self, "Rocket")) {
            return tget(ttab(self->ability, "extra"), "dollars");
        }
        if (name_is(self, "Satellite")) {
            num planets_used = 0;
            for (TIter it = titer(ttab(G->GAME, "consumeable_usage")); tnext(&it);) {
                if (streq(tstr(it.val.tab, "set"), "Planet")) planets_used = planets_used + 1;
            }
            if (planets_used == 0) return NIL;
            return N(tnum(self->ability, "extra") * planets_used);
        }
        Table *current_round = ttab(G->GAME, "current_round");
        if (name_is(self, "Delayed Gratification") && tnum(current_round, "discards_used") == 0 &&
            tnum(current_round, "discards_left") > 0) {
            return N(tnum(current_round, "discards_left") * tnum(self->ability, "extra"));
        }
    }
    return NIL;
}
