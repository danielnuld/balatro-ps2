// card.lua:645-1090: Card:remove_from_deck .. Card:get_p_dollars
#include "all.h"

// self.config (the Node's config table)
#define CFG(self) ((self)->moveable.node.config)
#define DYN(self) ((self)->moveable.node.object.dyn)
#define AREA_CFG(area) ((area)->moveable.node.config)

// card.lua:674 the Astronomer event: every card recomputes its cost
static bool ev_astronomer_set_cost(void *ctx) {
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        // note: 'if v.set_cost' is true only for Cards (G.I.CARD also holds the Blind and Card_Characters)
        if (it.val.t == V_OBJ && is_a(it.val.obj, &Card_class)) Card_set_cost((Card *)it.val.obj);
    }
    return true;
}

// card.lua:698
static bool ev_reset_blind(void *ctx) {
    Blind_set_blind((Blind *)tobj(G->GAME, "blind"), NULL, true, false);
    return true;
}

// card.lua:645
void Card_remove_from_deck(Card *self, bool from_debuff) {
    if (self->added_to_deck) {
        Table *a = self->ability;
        const char *name = tstr(a, "name");
        self->added_to_deck = false;
        if (tnum(a, "h_size") != 0) {
            CardArea_change_size(G->hand, -tnum(a, "h_size"));
        }
        if (tnum(a, "d_size") > 0) {
            Table *round_resets = ttab(G->GAME, "round_resets");
            tsetn(round_resets, "discards", tnum(round_resets, "discards") - tnum(a, "d_size"));
            ease_discard(-tnum(a, "d_size"), false, false);
        }
        if (streq(name, "Credit Card")) {
            tsetn(G->GAME, "bankrupt_at", tnum(G->GAME, "bankrupt_at") + tnum(a, "extra"));
        }
        if (streq(name, "Chaos the Clown")) {
            Table *current_round = ttab(G->GAME, "current_round");
            tsetn(current_round, "free_rerolls", tnum(current_round, "free_rerolls") - 1);
            calculate_reroll_cost(true);
        }
        if (streq(name, "Turtle Bean")) {
            CardArea_change_size(G->hand, -tnum(ttab(a, "extra"), "h_size"));
        }
        if (streq(name, "Oops! All 6s")) {
            Table *probabilities = ttab(G->GAME, "probabilities");
            for (TIter it = titer(probabilities); tnext(&it);) {
                tsetv(probabilities, it.key, N(vnum(it.val) / 2));
            }
        }
        if (streq(name, "To the Moon")) {
            tsetn(G->GAME, "interest_amount", tnum(G->GAME, "interest_amount") - tnum(a, "extra"));
        }
        if (streq(name, "Astronomer")) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_astronomer_set_cost}), NULL, false);
        }
        if (streq(name, "Troubadour")) {
            Table *round_resets = ttab(G->GAME, "round_resets");
            CardArea_change_size(G->hand, -tnum(ttab(a, "extra"), "h_size"));
            tsetn(round_resets, "hands", tnum(round_resets, "hands") - tnum(ttab(a, "extra"), "h_plays"));
        }
        if (streq(name, "Stuntman")) {
            CardArea_change_size(G->hand, tnum(ttab(a, "extra"), "h_size"));
        }
        if (self->edition && tbool(self->edition, "negative") && G->jokers) {
            if (from_debuff) {
                tsetb(a, "queue_negative_removal", true);
            } else {
                if (tbool(a, "consumeable")) {
                    Table *c = AREA_CFG(G->consumeables);
                    tsetn(c, "card_limit", tnum(c, "card_limit") - 1);
                } else {
                    Table *c = AREA_CFG(G->jokers);
                    tsetn(c, "card_limit", tnum(c, "card_limit") - 1);
                }
            }
        }
        if (truthy(tget(G->GAME, "blind"))) E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_reset_blind}), NULL, false);
    }
}

// card.lua:702
Table *Card_generate_UIBox_unlock_table(Card *self, bool hidden) {
    Table *loc_vars = TAB("no_name", B(true), "not_hidden", B(!hidden));

    return generate_card_ui(ttab(CFG(self), "center"), NULL, loc_vars, "Locked", NULL, false, NULL, NULL);
}

// ''..(G.GAME and G.GAME.probabilities.normal or 1)
static Val prob_normal_str(void) {
    Val v = G->GAME ? tget(ttab(G->GAME, "probabilities"), "normal") : NIL;
    if (!truthy(v)) v = N(1);
    return S(tostring(v));
}

// card.lua:813 / 902 the Blueprint / Brainstorm compatibility box
static Table *blueprint_main_end(Card *self) {
    return ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"), "minh", N(0.4))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("ref_table", O(self), "align", S("m"), "colour", P(G->C.JOKER_GREY), "r", N(0.05), "padding", N(0.06), "func", S("blueprint_compat"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(self->ability), "ref_value", S("blueprint_compat_ui"), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.32 * 0.8)))))
            ))))
        ))))
    );
}

// card.lua:708
Table *Card_generate_UIBox_ability_table(Card *self) {
    Table *a = self->ability;
    Table *center = ttab(CFG(self), "center");
    const char *card_type = tstr(a, "set") ? tstr(a, "set") : "None";
    bool hide_desc = false;
    Table *loc_vars = NULL;
    Table *main_start = NULL, *main_end = NULL;
    bool no_badge = false;
    const char *set = tstr(a, "set");
    const char *name = tstr(a, "name");
    Val unlocked = tget(center, "unlocked");
    bool unlocked_is_false = unlocked.t == V_BOOL && !unlocked.b;

    if (!self->bypass_lock && !unlocked_is_false &&
        (streq(set, "Joker") || streq(set, "Edition") || tbool(a, "consumeable") || streq(set, "Voucher") || streq(set, "Booster")) &&
        !tbool(center, "discovered") &&
        ((self->area != G->jokers && self->area != G->consumeables && self->area) || !self->area)) {
        card_type = "Undiscovered";
    }
    if (unlocked_is_false && !self->bypass_lock) { // For everything that is locked
        card_type = "Locked";
        if (self->area && self->area == G->shop_demo) { loc_vars = tnew(); no_badge = true; }
    } else if (streq(card_type, "Undiscovered") && !self->bypass_discovery_ui) { // Any Joker or tarot/planet/voucher that is not yet discovered
        hide_desc = true;
    } else if (self->debuff) {
        loc_vars = TAB("debuffed", B(true), "playing_card", B(truthy(tget(self->base, "colour"))), "value", tget(self->base, "value"),
                       "suit", tget(self->base, "suit"), "colour", tget(self->base, "colour"));
    } else if (streq(card_type, "Default") || streq(card_type, "Enhanced")) {
        Val perma = tget(a, "perma_bonus");
        num bonus = tnum(a, "bonus") + (truthy(perma) ? vnum(perma) : 0);
        loc_vars = TAB("playing_card", B(truthy(tget(self->base, "colour"))), "value", tget(self->base, "value"),
                       "suit", tget(self->base, "suit"), "colour", tget(self->base, "colour"),
                       "nominal_chips", tnum(self->base, "nominal") > 0 ? tget(self->base, "nominal") : NIL,
                       "bonus_chips", bonus > 0 ? N(bonus) : NIL);
    } else if (streq(set, "Joker")) { // all remaining jokers
        Val extra = tget(a, "extra");                           // self.ability.extra: a number or a table
        Table *xt = extra.t == V_TAB ? extra.tab : NULL;
        num xn = vnum(extra);
#define IS(s) streq(name, s)
#define A(k) tget(a, k)
#define X(k) tget(xt, k)
#define OR0(v) (truthy(v) ? vnum(v) : 0)
        if (IS("Joker")) loc_vars = ARR(A("mult"));
        else if (IS("Jolly Joker") || IS("Zany Joker") ||
                 IS("Mad Joker") || IS("Crazy Joker") ||
                 IS("Droll Joker")) {
            loc_vars = ARR(A("t_mult"), localize(A("type"), "poker_hands"));
        } else if (IS("Sly Joker") || IS("Wily Joker") ||
                   IS("Clever Joker") || IS("Devious Joker") ||
                   IS("Crafty Joker")) {
            loc_vars = ARR(A("t_chips"), localize(A("type"), "poker_hands"));
        } else if (IS("Half Joker")) loc_vars = ARR(X("mult"), X("size"));
        else if (IS("Fortune Teller")) {
            Table *cut = ttab(G->GAME, "consumeable_usage_total");
            Val tarot = cut ? tget(cut, "tarot") : NIL;
            loc_vars = ARR(extra, truthy(tarot) ? tarot : N(0));
        } else if (IS("Steel Joker")) loc_vars = ARR(extra, N(1 + xn * OR0(A("steel_tally"))));
        else if (IS("Chaos the Clown")) loc_vars = ARR(extra);
        else if (IS("Space Joker")) loc_vars = ARR(prob_normal_str(), extra);
        else if (IS("Stone Joker")) loc_vars = ARR(extra, N(xn * OR0(A("stone_tally"))));
        else if (IS("Drunkard")) loc_vars = ARR(A("d_size"));
        else if (IS("Green Joker")) loc_vars = ARR(X("hand_add"), X("discard_sub"), A("mult"));
        else if (IS("Credit Card")) loc_vars = ARR(extra);
        else if (IS("Greedy Joker") || IS("Lusty Joker") ||
                 IS("Wrathful Joker") || IS("Gluttonous Joker")) loc_vars = ARR(X("s_mult"), localize(X("suit"), "suits_singular"));
        else if (IS("Blue Joker")) loc_vars = ARR(extra, N(xn * ((G->deck && G->deck->cards) ? tlen(G->deck->cards) : 52)));
        else if (IS("Sixth Sense")) loc_vars = tnew();
        else if (IS("Mime")) {
        } else if (IS("Hack")) loc_vars = ARR(N(xn + 1));
        else if (IS("Pareidolia")) {
        } else if (IS("Faceless Joker")) loc_vars = ARR(X("dollars"), X("faces"));
        else if (IS("Oops! All 6s")) {
        } else if (IS("Juggler")) loc_vars = ARR(A("h_size"));
        else if (IS("Golden Joker")) loc_vars = ARR(extra);
        else if (IS("Joker Stencil")) loc_vars = ARR(A("x_mult"));
        else if (IS("Four Fingers")) {
        } else if (IS("Ceremonial Dagger")) loc_vars = ARR(A("mult"));
        else if (IS("Banner")) loc_vars = ARR(extra);
        else if (IS("Misprint")) {
            Table *r_mults = tnew();
            for (num i = vnum(X("min")); i <= vnum(X("max")); i += 1) {
                tseti(r_mults, tlen(r_mults) + 1, S(tostring(N(i))));
            }
            Val loc_mult = S(fmt(" %s ", vstr(localize(S("k_mult"), NULL))));
            // G.deck and G.deck.cards[1] and G.deck.cards[#G.deck.cards].base.<id / suit:sub(1,1)> or <11 / 'D'>
            Card *last = (G->deck && CardArea_card(G->deck, 1)) ? CardArea_card(G->deck, CardArea_n(G->deck)) : NULL;
            Val id = last ? tget(last->base, "id") : NIL;
            if (!truthy(id)) id = N(11);
            const char *su = last ? str_sub(tstr(last->base, "suit"), 1, 1) : "D";
            main_start = ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S("  +"), "colour", P(G->C.MULT), "scale", N(0.32))))),
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(r_mults), "colours", T(ARR(P(G->C.RED))), "pop_in_rate", N(9999999), "silent", B(true), "random_element", B(true), "pop_delay", N(0.5), "scale", N(0.32), "min_cycle_time", N(0)))))))),
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(
                    T(TAB("string", S("rand()"), "colour", P(G->C.JOKER_GREY))), T(TAB("string", S(fmt("#@%s%s", tostring(id), su)), "colour", P(G->C.RED))),
                    loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult, loc_mult)),
                "colours", T(ARR(P(G->C.UI.TEXT_DARK))), "pop_in_rate", N(9999999), "silent", B(true), "random_element", B(true), "pop_delay", N(0.2011), "scale", N(0.32), "min_cycle_time", N(0))))))))
            );
        } else if (IS("Mystic Summit")) loc_vars = ARR(X("mult"), X("d_remaining"));
        else if (IS("Marble Joker")) {
        } else if (IS("Loyalty Card")) {
            loc_vars = ARR(X("Xmult"), N(vnum(X("every")) + 1),
                           localize(T(TAB("type", S("variable"), "key", S(veq(A("loyalty_remaining"), N(0)) ? "loyalty_active" : "loyalty_inactive"),
                                          "vars", T(ARR(A("loyalty_remaining"))))), NULL));
        } else if (IS("8 Ball")) loc_vars = ARR(prob_normal_str(), extra);
        else if (IS("Dusk")) loc_vars = ARR(N(xn + 1));
        else if (IS("Raised Fist")) {
        } else if (IS("Fibonacci")) loc_vars = ARR(extra);
        else if (IS("Scary Face")) loc_vars = ARR(extra);
        else if (IS("Abstract Joker")) loc_vars = ARR(extra, N(((G->jokers && G->jokers->cards) ? tlen(G->jokers->cards) : 0) * xn));
        else if (IS("Delayed Gratification")) loc_vars = ARR(extra);
        else if (IS("Gros Michel")) loc_vars = ARR(X("mult"), prob_normal_str(), X("odds"));
        else if (IS("Even Steven")) loc_vars = ARR(extra);
        else if (IS("Odd Todd")) loc_vars = ARR(extra);
        else if (IS("Scholar")) loc_vars = ARR(X("mult"), X("chips"));
        else if (IS("Business Card")) loc_vars = ARR(prob_normal_str(), extra);
        else if (IS("Supernova")) {
        } else if (IS("Spare Trousers")) loc_vars = ARR(extra, localize(S("Two Pair"), "poker_hands"), A("mult"));
        else if (IS("Superposition")) loc_vars = ARR(extra);
        else if (IS("Ride the Bus")) loc_vars = ARR(extra, A("mult"));
        else if (IS("Egg")) loc_vars = ARR(extra);
        else if (IS("Burglar")) loc_vars = ARR(extra);
        else if (IS("Blackboard")) loc_vars = ARR(extra, localize(S("Spades"), "suits_plural"), localize(S("Clubs"), "suits_plural"));
        else if (IS("Runner")) loc_vars = ARR(X("chips"), X("chip_mod"));
        else if (IS("Ice Cream")) loc_vars = ARR(X("chips"), X("chip_mod"));
        else if (IS("DNA")) loc_vars = ARR(extra);
        else if (IS("Splash")) {
        } else if (IS("Constellation")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Hiker")) loc_vars = ARR(extra);
        else if (IS("To Do List")) loc_vars = ARR(X("dollars"), localize(A("to_do_poker_hand"), "poker_hands"));
        else if (IS("Smeared Joker")) {
        } else if (IS("Blueprint")) {
            if (!truthy(A("blueprint_compat_ui"))) tsets(a, "blueprint_compat_ui", "");
            tset(a, "blueprint_compat_check", NIL);
            main_end = (self->area && self->area == G->jokers) ? blueprint_main_end(self) : NULL;
        } else if (IS("Cartomancer")) {
        } else if (IS("Astronomer")) loc_vars = ARR(extra);

        else if (IS("Golden Ticket")) loc_vars = ARR(extra);
        else if (IS("Mr. Bones")) {
        } else if (IS("Acrobat")) loc_vars = ARR(extra);
        else if (IS("Sock and Buskin")) loc_vars = ARR(N(xn + 1));
        else if (IS("Swashbuckler")) loc_vars = ARR(A("mult"));
        else if (IS("Troubadour")) loc_vars = ARR(X("h_size"), N(-vnum(X("h_plays"))));
        else if (IS("Certificate")) loc_vars = ARR(extra);
        else if (IS("Throwback")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Hanging Chad")) loc_vars = ARR(extra);
        else if (IS("Rough Gem")) loc_vars = ARR(extra);
        else if (IS("Bloodstone")) loc_vars = ARR(prob_normal_str(), X("odds"), X("Xmult"));
        else if (IS("Arrowhead")) loc_vars = ARR(extra);
        else if (IS("Onyx Agate")) loc_vars = ARR(extra);
        else if (IS("Glass Joker")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Showman")) {
        } else if (IS("Flower Pot")) loc_vars = ARR(extra);
        else if (IS("Wee Joker")) loc_vars = ARR(X("chips"), X("chip_mod"));
        else if (IS("Merry Andy")) loc_vars = ARR(A("d_size"), A("h_size"));
        else if (IS("The Idol")) {
            Table *idol_card = ttab(ttab(G->GAME, "current_round"), "idol_card");
            loc_vars = ARR(extra, localize(tget(idol_card, "rank"), "ranks"), localize(tget(idol_card, "suit"), "suits_plural"));
            tsett(loc_vars, "colours", ARR(P(C_SUITS_get(tstr(idol_card, "suit")))));
        } else if (IS("Seeing Double")) loc_vars = ARR(extra);
        else if (IS("Matador")) loc_vars = ARR(extra);
        else if (IS("Hit the Road")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("The Duo") || IS("The Trio")
                 || IS("The Family") || IS("The Order") || IS("The Tribe")) loc_vars = ARR(A("x_mult"), localize(A("type"), "poker_hands"));

        else if (IS("Cavendish")) loc_vars = ARR(X("Xmult"), prob_normal_str(), X("odds"));
        else if (IS("Card Sharp")) loc_vars = ARR(X("Xmult"));
        else if (IS("Red Card")) loc_vars = ARR(extra, A("mult"));
        else if (IS("Madness")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Square Joker")) loc_vars = ARR(X("chips"), X("chip_mod"));
        else if (IS("Seance")) loc_vars = ARR(localize(X("poker_hand"), "poker_hands"));
        else if (IS("Riff-raff")) loc_vars = ARR(extra);
        else if (IS("Vampire")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Shortcut")) {
        } else if (IS("Hologram")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Vagabond")) loc_vars = ARR(extra);
        else if (IS("Baron")) loc_vars = ARR(extra);
        else if (IS("Cloud 9")) loc_vars = ARR(extra, N(xn * OR0(A("nine_tally"))));
        else if (IS("Rocket")) loc_vars = ARR(X("dollars"), X("increase"));
        else if (IS("Obelisk")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Midas Mask")) {
        } else if (IS("Luchador")) {
            bool has_message = (G->GAME && self->area && (self->area == G->jokers));
            if (has_message) {
                Blind *blind = (Blind *)tobj(G->GAME, "blind");
                bool disableable = blind && ((!blind->disabled) && streq(Blind_get_type(blind), "Boss"));
                main_end = ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"), "minh", N(0.4))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_C), "config", T(TAB("ref_table", O(self), "align", S("m"), "colour", P(disableable ? G->C.GREEN : G->C.RED), "r", N(0.05), "padding", N(0.06))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s ", vstr(localize(S(disableable ? "k_active" : "ph_no_boss_active"), NULL)))), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.32 * 0.9)))))
                        ))))
                    ))))
                );
            }
        } else if (IS("Photograph")) loc_vars = ARR(extra);
        else if (IS("Gift Card")) loc_vars = ARR(extra);
        else if (IS("Turtle Bean")) loc_vars = ARR(X("h_size"), X("h_mod"));
        else if (IS("Erosion")) {
            num diff = G->playing_cards ? (tnum(G->GAME, "starting_deck_size") - tlen(G->playing_cards)) : 0;
            loc_vars = ARR(extra, N(lmax(0, xn * diff)), tget(G->GAME, "starting_deck_size"));
        } else if (IS("Reserved Parking")) loc_vars = ARR(X("dollars"), prob_normal_str(), X("odds"));
        else if (IS("Mail-In Rebate")) loc_vars = ARR(extra, localize(tget(ttab(ttab(G->GAME, "current_round"), "mail_card"), "rank"), "ranks"));
        else if (IS("To the Moon")) loc_vars = ARR(extra);
        else if (IS("Hallucination")) loc_vars = ARR(tget(ttab(G->GAME, "probabilities"), "normal"), extra);
        else if (IS("Lucky Cat")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Baseball Card")) loc_vars = ARR(extra);
        else if (IS("Bull")) loc_vars = ARR(extra, N(xn * lmax(0, tnum(G->GAME, "dollars"))));
        else if (IS("Diet Cola")) loc_vars = ARR(localize(T(TAB("type", S("name_text"), "set", S("Tag"), "key", S("tag_double"), "nodes", T(tnew()))), NULL));
        else if (IS("Trading Card")) loc_vars = ARR(extra);
        else if (IS("Flash Card")) loc_vars = ARR(extra, A("mult"));
        else if (IS("Popcorn")) loc_vars = ARR(A("mult"), extra);
        else if (IS("Ramen")) loc_vars = ARR(A("x_mult"), extra);
        else if (IS("Ancient Joker")) {
            Table *ancient_card = ttab(ttab(G->GAME, "current_round"), "ancient_card");
            loc_vars = ARR(extra, localize(tget(ancient_card, "suit"), "suits_singular"));
            tsett(loc_vars, "colours", ARR(P(C_SUITS_get(tstr(ancient_card, "suit")))));
        } else if (IS("Walkie Talkie")) loc_vars = ARR(X("chips"), X("mult"));
        else if (IS("Seltzer")) loc_vars = ARR(extra);
        else if (IS("Castle")) {
            Table *castle_card = ttab(ttab(G->GAME, "current_round"), "castle_card");
            loc_vars = ARR(X("chip_mod"), localize(tget(castle_card, "suit"), "suits_singular"), X("chips"));
            tsett(loc_vars, "colours", ARR(P(C_SUITS_get(tstr(castle_card, "suit")))));
        } else if (IS("Smiley Face")) loc_vars = ARR(extra);
        else if (IS("Campfire")) loc_vars = ARR(extra, A("x_mult"));
        else if (IS("Stuntman")) loc_vars = ARR(X("chip_mod"), X("h_size"));
        else if (IS("Invisible Joker")) loc_vars = ARR(extra, A("invis_rounds"));
        else if (IS("Brainstorm")) {
            if (!truthy(A("blueprint_compat_ui"))) tsets(a, "blueprint_compat_ui", "");
            tset(a, "blueprint_compat_check", NIL);
            main_end = (self->area && self->area == G->jokers) ? blueprint_main_end(self) : NULL;
        } else if (IS("Satellite")) {
            num planets_used = 0;
            for (TIter it = titer(ttab(G->GAME, "consumeable_usage")); tnext(&it);) {
                if (it.val.t == V_TAB && streq(tstr(it.val.tab, "set"), "Planet")) planets_used = planets_used + 1;
            }
            loc_vars = ARR(extra, N(planets_used * xn));
        } else if (IS("Shoot the Moon")) loc_vars = ARR(extra);
        else if (IS("Driver's License")) loc_vars = ARR(extra, truthy(A("driver_tally")) ? A("driver_tally") : S("0"));
        else if (IS("Burnt Joker")) {
        } else if (IS("Bootstraps")) {
            loc_vars = ARR(X("mult"), X("dollars"),
                           N(vnum(X("mult")) * floor((tnum(G->GAME, "dollars") + OR0(tget(G->GAME, "dollar_buffer"))) / vnum(X("dollars")))));
        } else if (IS("Caino")) loc_vars = ARR(extra, A("caino_xmult"));
        else if (IS("Triboulet")) loc_vars = ARR(extra);
        else if (IS("Yorick")) loc_vars = ARR(X("xmult"), X("discards"), A("yorick_discards"), A("x_mult"));
        else if (IS("Chicot")) {
        } else if (IS("Perkeo")) loc_vars = ARR(extra);
#undef IS
#undef A
#undef X
#undef OR0
    }
    Table *badges = tnew();
    if ((!streq(card_type, "Locked") && !streq(card_type, "Undiscovered") && !streq(card_type, "Default")) || self->debuff) {
        tsets(badges, "card_type", card_type);
    }
    if (streq(set, "Joker") && self->bypass_discovery_ui && (!no_badge)) {
        tsetb(badges, "force_rarity", true);
    }
    if (self->edition) {
        const char *type = tstr(self->edition, "type");
        if (streq(type, "negative") && tbool(a, "consumeable")) {
            tseti(badges, tlen(badges) + 1, S("negative_consumable"));
        } else {
            tseti(badges, tlen(badges) + 1, streq(type, "holo") ? S("holographic") : tget(self->edition, "type"));
        }
    }
    if (self->seal) tseti(badges, tlen(badges) + 1, S(fmt("%s_seal", str_lower(self->seal))));
    if (tbool(a, "eternal")) tseti(badges, tlen(badges) + 1, S("eternal"));
    if (tbool(a, "perishable")) {
        if (!loc_vars) loc_vars = tnew();
        tset(loc_vars, "perish_tally", tget(a, "perish_tally"));
        tseti(badges, tlen(badges) + 1, S("perishable"));
    }
    if (tbool(a, "rental")) tseti(badges, tlen(badges) + 1, S("rental"));
    if (self->pinned) tseti(badges, tlen(badges) + 1, S("pinned_left"));
    if (self->sticker || ((self->sticker_run && !streq(self->sticker_run, "NONE")) && tbool(G->SETTINGS, "run_stake_stickers"))) {
        if (!loc_vars) loc_vars = tnew();
        tsets(loc_vars, "sticker", self->sticker ? self->sticker : self->sticker_run);
    }

    return generate_card_ui(center, NULL, loc_vars, card_type, badges, hide_desc, main_start, main_end);
}

// card.lua:950
num Card_get_nominal(Card *self, const char *mod) {
    num mult = 1;
    if (streq(mod, "suit")) mult = 1000;
    if (streq(tstr(self->ability, "effect"), "Stone Card")) mult = -1000;
    Val sno = tget(self->base, "suit_nominal_original");
    return tnum(self->base, "nominal") + tnum(self->base, "suit_nominal") * mult + (truthy(sno) ? vnum(sno) : 0) * 0.0001 * mult +
           tnum(self->base, "face_nominal") + 0.000001 * self->unique_val;
}

// card.lua:957
num Card_get_id(Card *self) {
    if (streq(tstr(self->ability, "effect"), "Stone Card") && !tbool(DYN(self), "vampired")) {
        return -math_random_range(100, 1000000);
    }
    return tnum(self->base, "id");
}

// card.lua:964
bool Card_is_face(Card *self, bool from_boss) {
    if (self->debuff && !from_boss) return false;
    num id = Card_get_id(self);
    if (id == 11 || id == 12 || id == 13 || !tempty(find_joker("Pareidolia", false))) {
        return true;
    }
    return false;
}

// card.lua:972
const char *Card_get_original_rank(Card *self) {
    return tstr(self->base, "original_value");
}

// card.lua:976
num Card_get_chip_bonus(Card *self) {
    if (self->debuff) return 0;
    Val perma = tget(self->ability, "perma_bonus");
    if (streq(tstr(self->ability, "effect"), "Stone Card")) {
        return tnum(self->ability, "bonus") + (truthy(perma) ? vnum(perma) : 0);
    }
    return tnum(self->base, "nominal") + tnum(self->ability, "bonus") + (truthy(perma) ? vnum(perma) : 0);
}

// card.lua:984
num Card_get_chip_mult(Card *self) {
    if (self->debuff) return 0;
    if (streq(tstr(self->ability, "set"), "Joker")) return 0;
    if (streq(tstr(self->ability, "effect"), "Lucky Card")) {
        if (pseudorandom(S("lucky_mult"), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / 5) {
            tsetb(DYN(self), "lucky_trigger", true);
            return tnum(self->ability, "mult");
        } else {
            return 0;
        }
    } else {
        return tnum(self->ability, "mult");
    }
}

// card.lua:999
num Card_get_chip_x_mult(Card *self, Table *context) {
    if (self->debuff) return 0;
    if (streq(tstr(self->ability, "set"), "Joker")) return 0;
    if (tnum(self->ability, "x_mult") <= 1) return 0;
    return tnum(self->ability, "x_mult");
}

// card.lua:1006
num Card_get_chip_h_mult(Card *self) {
    if (self->debuff) return 0;
    return tnum(self->ability, "h_mult");
}

// card.lua:1011
num Card_get_chip_h_x_mult(Card *self) {
    if (self->debuff) return 0;
    return tnum(self->ability, "h_x_mult");
}

// card.lua:1016
Table *Card_get_edition(Card *self) {
    if (self->debuff) return NULL;
    if (self->edition) {
        Table *ret = TAB("card", O(self));
        if (truthy(tget(self->edition, "x_mult"))) {
            tset(ret, "x_mult_mod", tget(self->edition, "x_mult"));
        }
        if (truthy(tget(self->edition, "mult"))) {
            tset(ret, "mult_mod", tget(self->edition, "mult"));
        }
        if (truthy(tget(self->edition, "chips"))) {
            tset(ret, "chip_mod", tget(self->edition, "chips"));
        }
        return ret;
    }
    return NULL;
}

// card.lua:1046 the Blue seal's planet (captures only card_type = 'Planet', a constant)
static bool ev_blue_seal_planet(void *ctx) {
    const char *card_type = "Planet";
    if (truthy(tget(G->GAME, "last_hand_played"))) {
        // note: the Lua starts with _planet = 0 and would pass that 0 as forced_key when no planet matches (which
        // cannot happen: every hand has a planet); here that case is NULL, no forced key.
        const char *_planet = NULL;
        for (TIter it = titer(ttab(G->P_CENTER_POOLS, "Planet")); tnext(&it);) {
            Table *v = it.val.t == V_TAB ? it.val.tab : NULL;
            if (veq(tget(ttab(v, "config"), "hand_type"), tget(G->GAME, "last_hand_played"))) {
                _planet = tstr(v, "key");
            }
        }
        Card *card = create_card(card_type, G->consumeables, false, NIL, false, false, _planet, "blusl");
        Card_add_to_deck(card, false);
        CardArea_emplace(G->consumeables, card, NULL, false);
        tsetn(G->GAME, "consumeable_buffer", 0);
    }
    return true;
}

// card.lua:1033
Table *Card_get_end_of_round_effect(Card *self, Table *context) {
    if (self->debuff) return tnew();
    Table *ret = tnew();
    if (tnum(self->ability, "h_dollars") > 0) {
        tset(ret, "h_dollars", tget(self->ability, "h_dollars"));
        tseto(ret, "card", self);
    }
    if (streq(self->seal, "Blue") &&
        CardArea_n(G->consumeables) + tnum(G->GAME, "consumeable_buffer") < tnum(AREA_CFG(G->consumeables), "card_limit")) {
        tsetn(G->GAME, "consumeable_buffer", tnum(G->GAME, "consumeable_buffer") + 1);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.0, .func = ev_blue_seal_planet}), NULL, false);
        card_eval_status_text(self, "extra", 0, -1, NULL,
                              TAB("message", localize(S("k_plus_planet"), NULL), "colour", P(G->C.SECONDARY_SET.Planet)));
        tsetb(ret, "effect", true);
    }
    return ret;
}

// card.lua:1086
static bool ev_clear_dollar_buffer(void *ctx) {
    tsetn(G->GAME, "dollar_buffer", 0);
    return true;
}

// card.lua:1068
num Card_get_p_dollars(Card *self) {
    if (self->debuff) return 0;
    num ret = 0;
    if (streq(self->seal, "Gold")) {
        ret = ret + 3;
    }
    if (tnum(self->ability, "p_dollars") > 0) {
        if (streq(tstr(self->ability, "effect"), "Lucky Card")) {
            if (pseudorandom(S("lucky_money"), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / 15) {
                tsetb(DYN(self), "lucky_trigger", true);
                ret = ret + tnum(self->ability, "p_dollars");
            }
        } else {
            ret = ret + tnum(self->ability, "p_dollars");
        }
    }
    if (ret > 0) {
        Val buf = tget(G->GAME, "dollar_buffer");
        tsetn(G->GAME, "dollar_buffer", (truthy(buf) ? vnum(buf) : 0) + ret);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_clear_dollar_buffer}), NULL, false);
    }
    return ret;
}
