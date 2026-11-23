// card.lua:3409-4063: the final else of Card:calculate_joker (context.cardarea == G.jokers: before, after,
// joker_main).
#include "all.h"

#define AB (self->ability)
#define EX ttab(self->ability, "extra")  // self.ability.extra when it is a table
#define EXN tnum(self->ability, "extra") // self.ability.extra when it is a number

// localize{type='variable', key=key, vars={v}}
static Val locv(const char *key, num v) {
    return localize(T(TAB("type", S("variable"), "key", S(key), "vars", T(ARR(N(v))))), NULL);
}

// {message = localize{type='variable', key=key, vars={v}}, [mod] = v}
static Val ret_mod(const char *key, const char *mod, num v) {
    return T(TAB("message", locv(key, v), mod, N(v)));
}

// the same with colour = col
static Val ret_mod_col(const char *key, const char *mod, num v, float *col) {
    return T(TAB("message", locv(key, v), mod, N(v), "colour", P(col)));
}

// next(context.poker_hands[hand])
static bool PH(Table *context, const char *hand) {
    Table *t = hand ? ttab(ttab(context, "poker_hands"), hand) : NULL;
    if (!t) ERROR("bad argument #1 to 'next' (table expected, got nil)"); // note: the Lua's own error for an unknown hand
    return !tempty(t);
}

// card.lua:3450 (Midas Mask)
static bool ev_midas_juice(void *ctx) {
    Card_juice_up((Card *)ctx, -1, NIL);
    return true;
}

// card.lua:3473 (Vampire)
static bool ev_vampire_juice(void *ctx) {
    Card *v = ctx;
    Card_juice_up(v, -1, NIL);
    tset(v->moveable.node.object.dyn, "vampired", NIL);
    return true;
}

// card.lua:3494, 3723
static bool ev_dollar_buffer_0(void *ctx) {
    tsetn(G->GAME, "dollar_buffer", 0);
    return true;
}

// card.lua:3512 (DNA)
static bool ev_dna_materialize(void *ctx) {
    Card_start_materialize((Card *)ctx, NULL, false, -1);
    return true;
}

// card.lua:3581, 3611
static bool ev_eaten_remove(void *ctx) {
    Card *self = ctx;
    CardArea_remove_card(G->jokers, self, false);
    Card_remove(self);
    // note: the Lua's `self = nil` only clears the closure's upvalue
    return true;
}

// card.lua:3574, 3604 (Ice Cream, Seltzer: the same closure)
static bool ev_eaten(void *ctx) {
    Card *self = ctx;
    play_sound("tarot1", -1, -1);
    self->moveable.node.T->r = -0.2f;
    Card_juice_up(self, 0.3f, N(0.4));
    self->moveable.node.states.drag.is = true;
    ((Moveable *)Card_child(self, "center"))->pinch.x = true;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .not_blockable = true,
                                             .func = ev_eaten_remove, .ctx = self}), NULL, false);
    return true;
}

// card.lua:3643 (Loyalty Card)
static Val loyalty_eval(void *ctx, int argc, Val *argv) {
    Card *card = argv[0].obj;
    return B(veq(tget(card->ability, "loyalty_remaining"), N(0)));
}

// card.lua:3749, 3773, 3793 (Vagabond, Superposition, Seance)
struct ev_create_ctx { const char *type, *key_append; };
static bool ev_create_consumeable(void *ctx) {
    struct ev_create_ctx *c = ctx;
    Card *card = create_card(c->type, G->consumeables, false, NIL, false, false, NULL, c->key_append);
    Card_add_to_deck(card, false);
    CardArea_emplace(G->consumeables, card, NULL, false);
    tsetn(G->GAME, "consumeable_buffer", 0);
    return true;
}
static void add_create_event(const char *type, const char *key_append) {
    struct ev_create_ctx *c = NEWCTX(struct ev_create_ctx);
    c->type = type, c->key_append = key_append;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.0, .func = ev_create_consumeable, .ctx = c}),
          NULL, false);
}

// #G.consumeables.cards + G.GAME.consumeable_buffer < G.consumeables.config.card_limit
static bool consumeable_room(void) {
    return tlen(G->consumeables->cards) + tnum(G->GAME, "consumeable_buffer") <
           tnum(G->consumeables->moveable.node.config, "card_limit");
}

// (G.GAME.dollar_buffer or 0)
static num dollar_buffer(void) {
    Val b = tget(G->GAME, "dollar_buffer");
    return truthy(b) ? b.n : 0;
}

// G.GAME.dollars + (G.GAME.dollar_buffer or 0)
static num dollars_buffered(void) { return tnum(G->GAME, "dollars") + dollar_buffer(); }

// card.lua:3409
Val Card_calculate_joker_c(Card *self, Table *context) {
    const char *name = tstr(AB, "name");
    bool blueprint = tbool(context, "blueprint");
    Table *current_round = ttab(G->GAME, "current_round");
    // 3410
    if (tobj(context, "cardarea") != (void *)G->jokers) return NIL;
    if (tbool(context, "before")) { // 3411
        Table *scoring_hand = ttab(context, "scoring_hand");
        if (streq(name, "Spare Trousers") && (PH(context, "Two Pair") || PH(context, "Full House")) && !blueprint) { // 3412
            tsetn(AB, "mult", tnum(AB, "mult") + EXN);
            return T(TAB("message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.RED), "card", O(self)));
        }
        // 3420
        if (streq(name, "Space Joker") &&
            pseudorandom(S("space"), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / EXN) {
            return T(TAB("card", O(self), "level_up", B(true), "message", localize(S("k_level_up_ex"), NULL)));
        }
        if (streq(name, "Square Joker") && tlen(ttab(context, "full_hand")) == 4 && !blueprint) { // 3427
            tsetn(EX, "chips", tnum(EX, "chips") + tnum(EX, "chip_mod"));
            return T(TAB("message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.CHIPS), "card", O(self)));
        }
        if (streq(name, "Runner") && PH(context, "Straight") && !blueprint) { // 3435
            tsetn(EX, "chips", tnum(EX, "chips") + tnum(EX, "chip_mod"));
            return T(TAB("message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.CHIPS), "card", O(self)));
        }
        if (streq(name, "Midas Mask") && !blueprint) { // 3443
            int faces = 0; // #faces: the list itself is not used
            for (int k = 1; truthy(tgeti(scoring_hand, k)); k++) {
                Card *v = tobji(scoring_hand, k);
                if (Card_is_face(v, false)) {
                    faces++;
                    Card_set_ability(v, ttab(G->P_CENTERS, "m_gold"), false, true);
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_midas_juice, .ctx = v}), NULL, false);
                }
            }
            if (faces > 0) {
                return T(TAB("message", localize(S("k_gold"), NULL), "colour", P(G->C.MONEY), "card", O(self)));
            }
        }
        if (streq(name, "Vampire") && !blueprint) { // 3465
            int enhanced = 0; // #enhanced
            for (int k = 1; truthy(tgeti(scoring_hand, k)); k++) {
                Card *v = tobji(scoring_hand, k);
                if (ttab(v->moveable.node.config, "center") != ttab(G->P_CENTERS, "c_base") && !v->debuff &&
                    !tbool(v->moveable.node.object.dyn, "vampired")) {
                    enhanced++;
                    tsetb(v->moveable.node.object.dyn, "vampired", true);
                    Card_set_ability(v, ttab(G->P_CENTERS, "c_base"), false, true);
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_vampire_juice, .ctx = v}), NULL, false);
                }
            }
            if (enhanced > 0) {
                tsetn(AB, "x_mult", tnum(AB, "x_mult") + EXN * enhanced);
                return T(TAB("message", locv("a_xmult", tnum(AB, "x_mult")), "colour", P(G->C.MULT), "card", O(self)));
            }
        }
        if (streq(name, "To Do List") && streq(tstr(context, "scoring_name"), tstr(AB, "to_do_poker_hand"))) { // 3491
            num dollars = tnum(EX, "dollars");
            ease_dollars(dollars, false);
            tsetn(G->GAME, "dollar_buffer", dollar_buffer() + dollars);
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_dollar_buffer_0}), NULL, false);
            return T(TAB("message", S(fmt("%s%s", vstr(localize(S("$"), NULL)), number_tostring(dollars))),
                         "dollars", N(dollars), "colour", P(G->C.MONEY)));
        }
        if (streq(name, "DNA") && tnum(current_round, "hands_played") == 0) { // 3501
            Table *full_hand = ttab(context, "full_hand");
            if (tlen(full_hand) == 1) {
                G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1; 0 as nil
                Card *_card = copy_card(tobji(full_hand, 1), NULL, -1, N(G->playing_card), false);
                Card_add_to_deck(_card, false);
                Table *deck_config = G->deck->moveable.node.config;
                tsetn(deck_config, "card_limit", tnum(deck_config, "card_limit") + 1);
                tinsert(G->playing_cards, O(_card));
                CardArea_emplace(G->hand, _card, NULL, false);
                _card->moveable.node.states.visible = false; // nil
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_dna_materialize, .ctx = _card}), NULL, false);
                return T(TAB("message", localize(S("k_copied_ex"), NULL), "colour", P(G->C.CHIPS), "card", O(self),
                             "playing_cards_created", T(ARR(B(true)))));
            }
        }
        if (streq(name, "Ride the Bus") && !blueprint) { // 3525
            bool faces = false;
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                if (Card_is_face(tobji(scoring_hand, i), false)) faces = true;
            }
            if (faces) {
                num last_mult = tnum(AB, "mult");
                tsetn(AB, "mult", 0);
                if (last_mult > 0) {
                    return T(TAB("card", O(self), "message", localize(S("k_reset"), NULL)));
                }
            } else {
                tsetn(AB, "mult", tnum(AB, "mult") + EXN);
            }
        }
        if (streq(name, "Obelisk") && !blueprint) { // 3543
            bool reset = true;
            Val scoring_name = tget(context, "scoring_name");
            Table *hands = ttab(G->GAME, "hands");
            Val played = tget(ttab(hands, vstr(scoring_name)), "played");
            num play_more_than = truthy(played) ? played.n : 0;
            // note: pairs over G.GAME.hands; the result does not depend on the order
            for (TIter it = titer(hands); tnext(&it);) {
                Table *v = it.val.tab;
                if (!veq(it.key, scoring_name) && tnum(v, "played") >= play_more_than && tbool(v, "visible")) {
                    reset = false;
                }
            }
            if (reset) {
                if (tnum(AB, "x_mult") > 1) {
                    tsetn(AB, "x_mult", 1);
                    return T(TAB("card", O(self), "message", localize(S("k_reset"), NULL)));
                }
            } else {
                tsetn(AB, "x_mult", tnum(AB, "x_mult") + EXN);
            }
        }
        if (streq(name, "Green Joker") && !blueprint) { // 3563
            tsetn(AB, "mult", tnum(AB, "mult") + tnum(EX, "hand_add"));
            return T(TAB("card", O(self), "message", locv("a_mult", tnum(EX, "hand_add"))));
        }
    } else if (tbool(context, "after")) { // 3570
        if (streq(name, "Ice Cream") && !blueprint) { // 3571
            if (tnum(EX, "chips") - tnum(EX, "chip_mod") <= 0) {
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_eaten, .ctx = self}), NULL, false);
                return T(TAB("message", localize(S("k_melted_ex"), NULL), "colour", P(G->C.CHIPS)));
            } else {
                tsetn(EX, "chips", tnum(EX, "chips") - tnum(EX, "chip_mod"));
                return T(TAB("message", locv("a_chips_minus", tnum(EX, "chip_mod")), "colour", P(G->C.CHIPS)));
            }
        }
        if (streq(name, "Seltzer") && !blueprint) { // 3601
            if (EXN - 1 <= 0) {
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_eaten, .ctx = self}), NULL, false);
                return T(TAB("message", localize(S("k_drank_ex"), NULL), "colour", P(G->C.FILTER)));
            } else {
                tsetn(AB, "extra", EXN - 1);
                return T(TAB("message", S(number_tostring(EXN)), "colour", P(G->C.FILTER)));
            }
        }
    } else { // 3631
        Table *scoring_hand = ttab(context, "scoring_hand");
        const char *scoring_name = tstr(context, "scoring_name");
        if (streq(name, "Loyalty Card")) { // 3632
            num every = tnum(EX, "every");
            tsetn(AB, "loyalty_remaining",
                  lmod(every - 1 - (tnum(G->GAME, "hands_played") - tnum(AB, "hands_played_at_create")), every + 1));
            if (blueprint) {
                if (tnum(AB, "loyalty_remaining") == every) {
                    return ret_mod("a_xmult", "Xmult_mod", tnum(EX, "Xmult"));
                }
            } else {
                if (tnum(AB, "loyalty_remaining") == 0) {
                    juice_card_until(self, FN(loyalty_eval, NULL), true, -1);
                } else if (tnum(AB, "loyalty_remaining") == every) {
                    return ret_mod("a_xmult", "Xmult_mod", tnum(EX, "Xmult"));
                }
            }
        }
        // 3653
        if (!streq(name, "Seeing Double") && tnum(AB, "x_mult") > 1 &&
            (streq(tstr(AB, "type"), "") || PH(context, tstr(AB, "type")))) {
            return ret_mod_col("a_xmult", "Xmult_mod", tnum(AB, "x_mult"), G->C.RED);
        }
        if (tnum(AB, "t_mult") > 0 && PH(context, tstr(AB, "type"))) { // 3660
            return ret_mod("a_mult", "mult_mod", tnum(AB, "t_mult"));
        }
        if (tnum(AB, "t_chips") > 0 && PH(context, tstr(AB, "type"))) { // 3666
            return ret_mod("a_chips", "chip_mod", tnum(AB, "t_chips"));
        }
        if (streq(name, "Half Joker") && tlen(ttab(context, "full_hand")) <= tnum(EX, "size")) { // 3672
            return ret_mod("a_mult", "mult_mod", tnum(EX, "mult"));
        }
        if (streq(name, "Abstract Joker")) { // 3678
            num x = 0;
            for (int i = 1; i <= tlen(G->jokers->cards); i++) {
                if (streq(tstr(CardArea_card(G->jokers, i)->ability, "set"), "Joker")) x = x + 1;
            }
            return ret_mod("a_mult", "mult_mod", x * EXN);
        }
        if (streq(name, "Acrobat") && tnum(current_round, "hands_left") == 0) { // 3688
            return ret_mod("a_xmult", "Xmult_mod", EXN);
        }
        if (streq(name, "Mystic Summit") && tnum(current_round, "discards_left") == tnum(EX, "d_remaining")) { // 3694
            return ret_mod("a_mult", "mult_mod", tnum(EX, "mult"));
        }
        if (streq(name, "Misprint")) { // 3700
            num temp_Mult = pseudorandom(S("misprint"), N(tnum(EX, "min")), N(tnum(EX, "max")));
            return ret_mod("a_mult", "mult_mod", temp_Mult);
        }
        if (streq(name, "Banner") && tnum(current_round, "discards_left") > 0) { // 3707
            return ret_mod("a_chips", "chip_mod", tnum(current_round, "discards_left") * EXN);
        }
        if (streq(name, "Stuntman")) { // 3713
            return ret_mod("a_chips", "chip_mod", tnum(EX, "chip_mod"));
        }
        if (streq(name, "Matador")) { // 3719
            if (((Blind *)tobj(G->GAME, "blind"))->triggered) {
                ease_dollars(EXN, false);
                tsetn(G->GAME, "dollar_buffer", dollar_buffer() + EXN);
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_dollar_buffer_0}), NULL, false);
                return T(TAB("message", S(fmt("%s%s", vstr(localize(S("$"), NULL)), number_tostring(EXN))),
                             "dollars", N(EXN), "colour", P(G->C.MONEY)));
            }
        }
        if (streq(name, "Supernova")) { // 3731
            return ret_mod("a_mult", "mult_mod", tnum(ttab(ttab(G->GAME, "hands"), scoring_name), "played"));
        }
        if (streq(name, "Ceremonial Dagger") && tnum(AB, "mult") > 0) { // 3737
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Vagabond") && consumeable_room()) { // 3743
            if (tnum(G->GAME, "dollars") <= EXN) {
                tsetn(G->GAME, "consumeable_buffer", tnum(G->GAME, "consumeable_buffer") + 1);
                add_create_event("Tarot", "vag");
                return T(TAB("message", localize(S("k_plus_tarot"), NULL), "card", O(self)));
            }
        }
        if (streq(name, "Superposition") && consumeable_room()) { // 3762
            num aces = 0;
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                if (Card_get_id(tobji(scoring_hand, i)) == 14) aces = aces + 1;
            }
            if (aces >= 1 && PH(context, "Straight")) {
                const char *card_type = "Tarot";
                tsetn(G->GAME, "consumeable_buffer", tnum(G->GAME, "consumeable_buffer") + 1);
                add_create_event(card_type, "sup");
                return T(TAB("message", localize(S("k_plus_tarot"), NULL), "colour", P(G->C.SECONDARY_SET.Tarot),
                             "card", O(self)));
            }
        }
        if (streq(name, "Seance") && consumeable_room()) { // 3787
            if (PH(context, tstr(EX, "poker_hand"))) {
                tsetn(G->GAME, "consumeable_buffer", tnum(G->GAME, "consumeable_buffer") + 1);
                add_create_event("Spectral", "sea");
                return T(TAB("message", localize(S("k_plus_spectral"), NULL), "colour", P(G->C.SECONDARY_SET.Spectral),
                             "card", O(self)));
            }
        }
        if (streq(name, "Flower Pot")) { // 3807
            num Hearts = 0, Diamonds = 0, Spades = 0, Clubs = 0; // suits
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                Card *c = tobji(scoring_hand, i);
                if (!streq(tstr(c->ability, "name"), "Wild Card")) {
                    if (Card_is_suit(c, "Hearts", true, false) && Hearts == 0) Hearts = Hearts + 1;
                    else if (Card_is_suit(c, "Diamonds", true, false) && Diamonds == 0) Diamonds = Diamonds + 1;
                    else if (Card_is_suit(c, "Spades", true, false) && Spades == 0) Spades = Spades + 1;
                    else if (Card_is_suit(c, "Clubs", true, false) && Clubs == 0) Clubs = Clubs + 1;
                }
            }
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                Card *c = tobji(scoring_hand, i);
                if (streq(tstr(c->ability, "name"), "Wild Card")) {
                    if (Card_is_suit(c, "Hearts", false, false) && Hearts == 0) Hearts = Hearts + 1;
                    else if (Card_is_suit(c, "Diamonds", false, false) && Diamonds == 0) Diamonds = Diamonds + 1;
                    else if (Card_is_suit(c, "Spades", false, false) && Spades == 0) Spades = Spades + 1;
                    else if (Card_is_suit(c, "Clubs", false, false) && Clubs == 0) Clubs = Clubs + 1;
                }
            }
            if (Hearts > 0 && Diamonds > 0 && Spades > 0 && Clubs > 0) {
                return ret_mod("a_xmult", "Xmult_mod", EXN);
            }
        }
        if (streq(name, "Seeing Double")) { // 3840
            num Hearts = 0, Diamonds = 0, Spades = 0, Clubs = 0; // suits
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                Card *c = tobji(scoring_hand, i);
                if (!streq(tstr(c->ability, "name"), "Wild Card")) {
                    if (Card_is_suit(c, "Hearts", false, false)) Hearts = Hearts + 1;
                    if (Card_is_suit(c, "Diamonds", false, false)) Diamonds = Diamonds + 1;
                    if (Card_is_suit(c, "Spades", false, false)) Spades = Spades + 1;
                    if (Card_is_suit(c, "Clubs", false, false)) Clubs = Clubs + 1;
                }
            }
            for (int i = 1; i <= tlen(scoring_hand); i++) {
                Card *c = tobji(scoring_hand, i);
                if (streq(tstr(c->ability, "name"), "Wild Card")) {
                    if (Card_is_suit(c, "Clubs", false, false) && Clubs == 0) Clubs = Clubs + 1;
                    else if (Card_is_suit(c, "Diamonds", false, false) && Diamonds == 0) Diamonds = Diamonds + 1;
                    else if (Card_is_suit(c, "Spades", false, false) && Spades == 0) Spades = Spades + 1;
                    else if (Card_is_suit(c, "Hearts", false, false) && Hearts == 0) Hearts = Hearts + 1;
                }
            }
            if ((Hearts > 0 || Diamonds > 0 || Spades > 0) && Clubs > 0) {
                return ret_mod("a_xmult", "Xmult_mod", EXN);
            }
        }
        if (streq(name, "Wee Joker")) { // 3873
            return ret_mod_col("a_chips", "chip_mod", tnum(EX, "chips"), G->C.CHIPS);
        }
        if (streq(name, "Castle") && (tnum(EX, "chips") > 0)) { // 3880
            return ret_mod_col("a_chips", "chip_mod", tnum(EX, "chips"), G->C.CHIPS);
        }
        if (streq(name, "Blue Joker") && tlen(G->deck->cards) > 0) { // 3887
            return ret_mod_col("a_chips", "chip_mod", EXN * tlen(G->deck->cards), G->C.CHIPS);
        }
        if (streq(name, "Erosion") && (tnum(G->GAME, "starting_deck_size") - tlen(G->playing_cards)) > 0) { // 3894
            return ret_mod_col("a_mult", "mult_mod", EXN * (tnum(G->GAME, "starting_deck_size") - tlen(G->playing_cards)),
                               G->C.MULT);
        }
        if (streq(name, "Square Joker")) { // 3901
            return ret_mod_col("a_chips", "chip_mod", tnum(EX, "chips"), G->C.CHIPS);
        }
        if (streq(name, "Runner")) { // 3908
            return ret_mod_col("a_chips", "chip_mod", tnum(EX, "chips"), G->C.CHIPS);
        }
        if (streq(name, "Ice Cream")) { // 3915
            return ret_mod_col("a_chips", "chip_mod", tnum(EX, "chips"), G->C.CHIPS);
        }
        // note: stone_tally / steel_tally are nil until the first Card:update (the Lua would error): 0 here
        if (streq(name, "Stone Joker") && tnum(AB, "stone_tally") > 0) { // 3922
            return ret_mod_col("a_chips", "chip_mod", EXN * tnum(AB, "stone_tally"), G->C.CHIPS);
        }
        if (streq(name, "Steel Joker") && tnum(AB, "steel_tally") > 0) { // 3929
            return ret_mod_col("a_xmult", "Xmult_mod", 1 + EXN * tnum(AB, "steel_tally"), G->C.MULT);
        }
        if (streq(name, "Bull") && dollars_buffered() > 0) { // 3936
            return ret_mod_col("a_chips", "chip_mod", EXN * lmax(0, dollars_buffered()), G->C.CHIPS);
        }
        if (streq(name, "Driver's License")) { // 3943
            if (tnum(AB, "driver_tally") >= 16) { // (driver_tally or 0)
                return ret_mod("a_xmult", "Xmult_mod", EXN);
            }
        }
        if (streq(name, "Blackboard")) { // 3951
            num black_suits = 0, all_cards = 0;
            for (int k = 1; truthy(tgeti(G->hand->cards, k)); k++) {
                Card *v = tobji(G->hand->cards, k);
                all_cards = all_cards + 1;
                if (Card_is_suit(v, "Clubs", false, true) || Card_is_suit(v, "Spades", false, true)) {
                    black_suits = black_suits + 1;
                }
            }
            if (black_suits == all_cards) {
                return ret_mod("a_xmult", "Xmult_mod", EXN);
            }
        }
        if (streq(name, "Joker Stencil")) { // 3966
            if ((tnum(G->jokers->moveable.node.config, "card_limit") - tlen(G->jokers->cards)) > 0) {
                return ret_mod("a_xmult", "Xmult_mod", tnum(AB, "x_mult"));
            }
        }
        if (streq(name, "Swashbuckler") && tnum(AB, "mult") > 0) { // 3974
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Joker")) { // 3980
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Spare Trousers") && tnum(AB, "mult") > 0) { // 3986
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Ride the Bus") && tnum(AB, "mult") > 0) { // 3992
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Flash Card") && tnum(AB, "mult") > 0) { // 3998
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Popcorn") && tnum(AB, "mult") > 0) { // 4004
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        if (streq(name, "Green Joker") && tnum(AB, "mult") > 0) { // 4010
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        // 4016
        if (streq(name, "Fortune Teller") && truthy(tget(G->GAME, "consumeable_usage_total")) &&
            tnum(ttab(G->GAME, "consumeable_usage_total"), "tarot") > 0) {
            return ret_mod("a_mult", "mult_mod", tnum(ttab(G->GAME, "consumeable_usage_total"), "tarot"));
        }
        if (streq(name, "Gros Michel")) { // 4022
            return ret_mod("a_mult", "mult_mod", tnum(EX, "mult"));
        }
        if (streq(name, "Cavendish")) { // 4028
            return ret_mod("a_xmult", "Xmult_mod", tnum(EX, "Xmult"));
        }
        if (streq(name, "Red Card") && tnum(AB, "mult") > 0) { // 4034
            return ret_mod("a_mult", "mult_mod", tnum(AB, "mult"));
        }
        // 4040
        if (streq(name, "Card Sharp") && truthy(tget(ttab(G->GAME, "hands"), scoring_name)) &&
            tnum(ttab(ttab(G->GAME, "hands"), scoring_name), "played_this_round") > 1) {
            return ret_mod("a_xmult", "Xmult_mod", tnum(EX, "Xmult"));
        }
        if (streq(name, "Bootstraps") && floor(dollars_buffered() / tnum(EX, "dollars")) >= 1) { // 4046
            return ret_mod("a_mult", "mult_mod", tnum(EX, "mult") * floor(dollars_buffered() / tnum(EX, "dollars")));
        }
        if (streq(name, "Caino") && tnum(AB, "caino_xmult") > 1) { // 4052
            return ret_mod("a_xmult", "Xmult_mod", tnum(AB, "caino_xmult"));
        }
    }
    return NIL;
}
