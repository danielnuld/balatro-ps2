// card.lua:2874-3408: Card:calculate_joker, the context branches end_of_round, individual, repetition, other_joker.
#include "all.h"

#define RET(v) do { *ret = (v); return true; } while (0)
#define NAME(s) streq(tstr(self->ability, "name"), s)

// card.lua:2912 (also 2954, 3028): the three identical "eaten" closures; `self = nil` only clears the upvalue
static bool ev_eat_remove(void *ctx) {
    Card *self = ctx;
    CardArea_remove_card(G->jokers, self, false);
    CALL(self, remove);
    return true;
}

// card.lua:2906 (also 2948, 3022)
static bool ev_eat(void *ctx) {
    Card *self = ctx;
    play_sound("tarot1", -1, -1);
    self->moveable.node.T->r = -0.2f;
    Card_juice_up(self, 0.3f, N(0.4));
    self->moveable.node.states.drag.is = true;
    ((Moveable *)Card_child(self, "center"))->pinch.x = true;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .not_blockable = true,
        .func = ev_eat_remove, .ctx = self}), NULL, false);
    return true;
}

// card.lua:2937 local eval = function(card) return not card.REMOVED end
static Val invis_eval(void *ctx, int argc, Val *argv) {
    Card *card = argv[0].obj;
    return B(!card->moveable.node.REMOVED);
}

// card.lua:3050
static bool ev_mr_bones(void *ctx) {
    Card *self = ctx;
    UIElement_juice_up(tobj(G->hand_text_area, "blind_chips"), -1, NIL);
    UIElement_juice_up(tobj(G->hand_text_area, "game_chips"), -1, NIL);
    play_sound("tarot1", -1, -1);
    Card_start_dissolve(self, NULL, false, -1, false);
    return true;
}

// card.lua:3114
static bool ev_8ball_create(void *ctx) {
    Card *card = create_card("Tarot", G->consumeables, false, NIL, false, false, NULL, "8ba");
    Card_add_to_deck(card, false);
    CardArea_emplace(G->consumeables, card, NULL, false);
    tsetn(G->GAME, "consumeable_buffer", 0);
    return true;
}

// card.lua:3110 extra.func: the scorer calls it (fcall0) before showing the popup
static Val fn_8ball(void *ctx, int argc, Val *argv) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.0, .func = ev_8ball_create}), NULL, false);
    return NIL;
}

// card.lua:3153 (also 3179, 3227, 3313)
static bool ev_dollar_buffer_0(void *ctx) {
    tsetn(G->GAME, "dollar_buffer", 0);
    return true;
}

// card.lua:3399 note: the closure captures `context` and reads other_joker when it runs: ctx is the context table
static bool ev_baseball(void *ctx) {
    Table *context = ctx;
    CALL(tobj(context, "other_joker"), juice_up, 0.5f, N(0.5));
    return true;
}

// card.lua:3152 G.GAME.dollar_buffer = (G.GAME.dollar_buffer or 0) + amount, then the event that resets it
static void dollar_buffer_add(num amount) {
    Val db = tget(G->GAME, "dollar_buffer");
    tsetn(G->GAME, "dollar_buffer", (truthy(db) ? db.n : 0) + amount);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_dollar_buffer_0}), NULL, false);
}

// card.lua:2880 next(context.card_effects[1]) or #context.card_effects > 1
static bool mime_cond(Table *context) {
    Table *card_effects = ttab(context, "card_effects");
    return !tempty(ttabi(card_effects, 1)) || tlen(card_effects) > 1;
}

// card.lua:2874
bool Card_calculate_joker_b(Card *self, Table *context, Val *ret) {
    Table *ability = self->ability;
    Table *GAME = G->GAME;
    Val cardarea = tget(context, "cardarea");
    if (truthy(tget(context, "end_of_round"))) { // card.lua:2874
        if (truthy(tget(context, "individual"))) {
        } else if (truthy(tget(context, "repetition"))) {
            if (veq(cardarea, O(G->hand))) {
                if (NAME("Mime") && mime_cond(context)) {
                    RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
                }
            }
        } else if (!truthy(tget(context, "blueprint"))) {
            Blind *blind = tobj(GAME, "blind");
            if (NAME("Campfire") && blind->boss && tnum(ability, "x_mult") > 1) {
                tsetn(ability, "x_mult", 1);
                RET(T(TAB("message", localize(S("k_reset"), NULL), "colour", P(G->C.RED))));
            }
            if (NAME("Rocket") && blind->boss) {
                Table *extra = ttab(ability, "extra");
                tsetn(extra, "dollars", tnum(extra, "dollars") + tnum(extra, "increase"));
                RET(T(TAB("message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.MONEY))));
            }
            if (NAME("Turtle Bean") && !truthy(tget(context, "blueprint"))) {
                Table *extra = ttab(ability, "extra");
                if (tnum(extra, "h_size") - tnum(extra, "h_mod") <= 0) {
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_eat, .ctx = self}), NULL, false);
                    RET(T(TAB("message", localize(S("k_eaten_ex"), NULL), "colour", P(G->C.FILTER))));
                } else {
                    tsetn(extra, "h_size", tnum(extra, "h_size") - tnum(extra, "h_mod"));
                    CardArea_change_size(G->hand, -tnum(extra, "h_mod"));
                    RET(T(TAB("message", localize(T(TAB("type", S("variable"), "key", S("a_handsize_minus"),
                                                       "vars", T(ARR(tget(extra, "h_mod"))))), NULL),
                              "colour", P(G->C.FILTER))));
                }
            }
            if (NAME("Invisible Joker") && !truthy(tget(context, "blueprint"))) {
                tsetn(ability, "invis_rounds", tnum(ability, "invis_rounds") + 1);
                if (tnum(ability, "invis_rounds") == tnum(ability, "extra")) {
                    juice_card_until(self, FN(invis_eval, NULL), true, -1);
                }
                Val message;
                if (tnum(ability, "invis_rounds") < tnum(ability, "extra")) {
                    const char *a = str(number_tostring(tnum(ability, "invis_rounds")));
                    message = S(fmt("%s/%s", a, number_tostring(tnum(ability, "extra"))));
                } else {
                    message = localize(S("k_active_ex"), NULL);
                }
                RET(T(TAB("message", message, "colour", P(G->C.FILTER))));
            }
            if (NAME("Popcorn") && !truthy(tget(context, "blueprint"))) {
                if (tnum(ability, "mult") - tnum(ability, "extra") <= 0) {
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_eat, .ctx = self}), NULL, false);
                    RET(T(TAB("message", localize(S("k_eaten_ex"), NULL), "colour", P(G->C.RED))));
                } else {
                    tsetn(ability, "mult", tnum(ability, "mult") - tnum(ability, "extra"));
                    RET(T(TAB("message", localize(T(TAB("type", S("variable"), "key", S("a_mult_minus"),
                                                       "vars", T(ARR(tget(ability, "extra"))))), NULL),
                              "colour", P(G->C.MULT))));
                }
            }
            if (NAME("To Do List") && !truthy(tget(context, "blueprint"))) {
                Table *_poker_hands = tnew();
                // note: pairs(G.GAME.hands) is hash order in the Lua; here insertion order of G.GAME.hands, and the
                // hand picked depends on it (spec card-c.md section 4)
                for (TIter it = titer(ttab(GAME, "hands")); tnext(&it);) {
                    if (tbool(it.val.tab, "visible") && !veq(it.key, tget(ability, "to_do_poker_hand"))) tinsert(_poker_hands, it.key);
                }
                tset(ability, "to_do_poker_hand", pseudorandom_element(_poker_hands, N(pseudoseed("to_do", NULL)), NULL));
                RET(T(TAB("message", localize(S("k_reset"), NULL))));
            }
            if (NAME("Egg")) {
                tsetn(ability, "extra_value", tnum(ability, "extra_value") + tnum(ability, "extra"));
                Card_set_cost(self);
                RET(T(TAB("message", localize(S("k_val_up"), NULL), "colour", P(G->C.MONEY))));
            }
            if (NAME("Gift Card")) {
                // (v.set_cost is always there: every entry is a Card)
                for (int k = 1; CardArea_card(G->jokers, k); k++) {
                    Card *v = CardArea_card(G->jokers, k);
                    Val ev = tget(v->ability, "extra_value");
                    tsetn(v->ability, "extra_value", (truthy(ev) ? ev.n : 0) + tnum(ability, "extra"));
                    Card_set_cost(v);
                }
                for (int k = 1; CardArea_card(G->consumeables, k); k++) {
                    Card *v = CardArea_card(G->consumeables, k);
                    Val ev = tget(v->ability, "extra_value");
                    tsetn(v->ability, "extra_value", (truthy(ev) ? ev.n : 0) + tnum(ability, "extra"));
                    Card_set_cost(v);
                }
                RET(T(TAB("message", localize(S("k_val_up"), NULL), "colour", P(G->C.MONEY))));
            }
            if (NAME("Hit the Road") && tnum(ability, "x_mult") > 1) {
                tsetn(ability, "x_mult", 1);
                RET(T(TAB("message", localize(S("k_reset"), NULL), "colour", P(G->C.RED))));
            }

            if (NAME("Gros Michel") || NAME("Cavendish")) {
                if (pseudorandom(S(NAME("Cavendish") ? "cavendish" : "gros_michel"), NIL, NIL) <
                    tnum(ttab(GAME, "probabilities"), "normal") / tnum(ttab(ability, "extra"), "odds")) {
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_eat, .ctx = self}), NULL, false);
                    if (NAME("Gros Michel")) tsetb(ttab(GAME, "pool_flags"), "gros_michel_extinct", true);
                    RET(T(TAB("message", localize(S("k_extinct_ex"), NULL))));
                } else {
                    RET(T(TAB("message", localize(S("k_safe_ex"), NULL))));
                }
            }
            if (NAME("Mr. Bones") && truthy(tget(context, "game_over")) &&
                tnum(GAME, "chips") / blind->chips >= 0.25) {
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_mr_bones, .ctx = self}), NULL, false);
                RET(T(TAB("message", localize(S("k_saved_ex"), NULL), "saved", B(true), "colour", P(G->C.RED))));
            }
        }
        RET(NIL);
    } else if (truthy(tget(context, "individual"))) { // card.lua:3065
        Card *other_card = tobj(context, "other_card");
        if (veq(cardarea, O(G->play))) {
            if (NAME("Hiker")) {
                Val pb = tget(other_card->ability, "perma_bonus");
                tsetn(other_card->ability, "perma_bonus", truthy(pb) ? pb.n : 0);
                tsetn(other_card->ability, "perma_bonus", tnum(other_card->ability, "perma_bonus") + tnum(ability, "extra"));
                RET(T(TAB("extra", T(TAB("message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.CHIPS))),
                          "colour", P(G->C.CHIPS), "card", O(self))));
            }
            if (NAME("Lucky Cat") && truthy(tget(other_card->moveable.node.object.dyn, "lucky_trigger")) &&
                !truthy(tget(context, "blueprint"))) {
                tsetn(ability, "x_mult", tnum(ability, "x_mult") + tnum(ability, "extra"));
                RET(T(TAB("extra", T(TAB("focus", O(self), "message", localize(S("k_upgrade_ex"), NULL), "colour", P(G->C.MULT))),
                          "card", O(self))));
            }
            if (NAME("Wee Joker") && Card_get_id(other_card) == 2 && !truthy(tget(context, "blueprint"))) {
                Table *extra = ttab(ability, "extra");
                tsetn(extra, "chips", tnum(extra, "chips") + tnum(extra, "chip_mod"));

                RET(T(TAB("extra", T(TAB("focus", O(self), "message", localize(S("k_upgrade_ex"), NULL))),
                          "card", O(self), "colour", P(G->C.CHIPS))));
            }
            if (NAME("Photograph")) {
                Table *scoring_hand = ttab(context, "scoring_hand");
                Card *first_face = NULL;
                for (int i = 1; i <= tlen(scoring_hand); i++) {
                    if (Card_is_face(tobji(scoring_hand, i), false)) { first_face = tobji(scoring_hand, i); break; }
                }
                if (other_card == first_face) {
                    RET(T(TAB("x_mult", tget(ability, "extra"), "colour", P(G->C.RED), "card", O(self))));
                }
            }
            if (NAME("8 Ball") && CardArea_n(G->consumeables) + tnum(GAME, "consumeable_buffer") <
                                      tnum(G->consumeables->moveable.node.config, "card_limit")) {
                if ((Card_get_id(other_card) == 8) &&
                    (pseudorandom(S("8ball"), NIL, NIL) < tnum(ttab(GAME, "probabilities"), "normal") / tnum(ability, "extra"))) {
                    tsetn(GAME, "consumeable_buffer", tnum(GAME, "consumeable_buffer") + 1);
                    RET(T(TAB("extra", T(TAB("focus", O(self), "message", localize(S("k_plus_tarot"), NULL),
                                             "func", FN(fn_8ball, NULL))),
                              "colour", P(G->C.SECONDARY_SET.Tarot), "card", O(self))));
                }
            }
            if (NAME("The Idol")) {
                Table *idol_card = ttab(ttab(GAME, "current_round"), "idol_card");
                if (veq(N(Card_get_id(other_card)), tget(idol_card, "id")) &&
                    Card_is_suit(other_card, tstr(idol_card, "suit"), false, false)) {
                    RET(T(TAB("x_mult", tget(ability, "extra"), "colour", P(G->C.RED), "card", O(self))));
                }
            }
            if (NAME("Scary Face") && (Card_is_face(other_card, false))) {
                RET(T(TAB("chips", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Smiley Face") && (Card_is_face(other_card, false))) {
                RET(T(TAB("mult", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Golden Ticket") && streq(tstr(other_card->ability, "name"), "Gold Card")) {
                dollar_buffer_add(tnum(ability, "extra"));
                RET(T(TAB("dollars", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Scholar") && Card_get_id(other_card) == 14) {
                Table *extra = ttab(ability, "extra");
                RET(T(TAB("chips", tget(extra, "chips"), "mult", tget(extra, "mult"), "card", O(self))));
            }
            if (NAME("Walkie Talkie") && (Card_get_id(other_card) == 10 || Card_get_id(other_card) == 4)) {
                Table *extra = ttab(ability, "extra");
                RET(T(TAB("chips", tget(extra, "chips"), "mult", tget(extra, "mult"), "card", O(self))));
            }
            if (NAME("Business Card") && Card_is_face(other_card, false) &&
                pseudorandom(S("business"), NIL, NIL) < tnum(ttab(GAME, "probabilities"), "normal") / tnum(ability, "extra")) {
                dollar_buffer_add(2);
                RET(T(TAB("dollars", N(2), "card", O(self))));
            }
            if (NAME("Fibonacci") && (Card_get_id(other_card) == 2 || Card_get_id(other_card) == 3 ||
                                      Card_get_id(other_card) == 5 || Card_get_id(other_card) == 8 ||
                                      Card_get_id(other_card) == 14)) {
                RET(T(TAB("mult", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Even Steven") && Card_get_id(other_card) <= 10 && Card_get_id(other_card) >= 0 &&
                lmod(Card_get_id(other_card), 2) == 0) {
                RET(T(TAB("mult", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Odd Todd") && ((Card_get_id(other_card) <= 10 && Card_get_id(other_card) >= 0 &&
                                      lmod(Card_get_id(other_card), 2) == 1) ||
                                     (Card_get_id(other_card) == 14))) {
                RET(T(TAB("chips", tget(ability, "extra"), "card", O(self))));
            }
            if (streq(tstr(ability, "effect"), "Suit Mult") &&
                Card_is_suit(other_card, tstr(ttab(ability, "extra"), "suit"), false, false)) {
                RET(T(TAB("mult", tget(ttab(ability, "extra"), "s_mult"), "card", O(self))));
            }
            if (NAME("Rough Gem") && Card_is_suit(other_card, "Diamonds", false, false)) {
                dollar_buffer_add(tnum(ability, "extra"));
                RET(T(TAB("dollars", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Onyx Agate") && Card_is_suit(other_card, "Clubs", false, false)) {
                RET(T(TAB("mult", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Arrowhead") && Card_is_suit(other_card, "Spades", false, false)) {
                RET(T(TAB("chips", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Bloodstone") && Card_is_suit(other_card, "Hearts", false, false) &&
                pseudorandom(S("bloodstone"), NIL, NIL) <
                    tnum(ttab(GAME, "probabilities"), "normal") / tnum(ttab(ability, "extra"), "odds")) {
                RET(T(TAB("x_mult", tget(ttab(ability, "extra"), "Xmult"), "card", O(self))));
            }
            if (NAME("Ancient Joker") &&
                Card_is_suit(other_card, tstr(ttab(ttab(GAME, "current_round"), "ancient_card"), "suit"), false, false)) {
                RET(T(TAB("x_mult", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Triboulet") && (Card_get_id(other_card) == 12 || Card_get_id(other_card) == 13)) {
                RET(T(TAB("x_mult", tget(ability, "extra"), "colour", P(G->C.RED), "card", O(self))));
            }
        }
        if (veq(cardarea, O(G->hand))) { // card.lua:3271
            if (NAME("Shoot the Moon") && Card_get_id(other_card) == 12) {
                if (other_card->debuff) {
                    RET(T(TAB("message", localize(S("k_debuffed"), NULL), "colour", P(G->C.RED), "card", O(self))));
                } else {
                    RET(T(TAB("h_mult", N(13), "card", O(self))));
                }
            }
            if (NAME("Baron") && Card_get_id(other_card) == 13) {
                if (other_card->debuff) {
                    RET(T(TAB("message", localize(S("k_debuffed"), NULL), "colour", P(G->C.RED), "card", O(self))));
                } else {
                    RET(T(TAB("x_mult", tget(ability, "extra"), "card", O(self))));
                }
            }
            if (NAME("Reserved Parking") && Card_is_face(other_card, false) &&
                pseudorandom(S("parking"), NIL, NIL) <
                    tnum(ttab(GAME, "probabilities"), "normal") / tnum(ttab(ability, "extra"), "odds")) {
                if (other_card->debuff) {
                    RET(T(TAB("message", localize(S("k_debuffed"), NULL), "colour", P(G->C.RED), "card", O(self))));
                } else {
                    dollar_buffer_add(tnum(ttab(ability, "extra"), "dollars"));
                    RET(T(TAB("dollars", tget(ttab(ability, "extra"), "dollars"), "card", O(self))));
                }
            }
            if (NAME("Raised Fist")) {
                num temp_Mult = 15, temp_ID = 15;
                Card *raised_card = NULL;
                for (int i = 1; i <= CardArea_n(G->hand); i++) {
                    Card *c = CardArea_card(G->hand, i);
                    if (temp_ID >= tnum(c->base, "id") && !streq(tstr(c->ability, "effect"), "Stone Card")) {
                        temp_Mult = tnum(c->base, "nominal"); temp_ID = tnum(c->base, "id"); raised_card = c;
                    }
                }
                if (raised_card == other_card) {
                    if (other_card->debuff) {
                        RET(T(TAB("message", localize(S("k_debuffed"), NULL), "colour", P(G->C.RED), "card", O(self))));
                    } else {
                        RET(T(TAB("h_mult", N(2 * temp_Mult), "card", O(self))));
                    }
                }
            }
        }
        RET(NIL);
    } else if (truthy(tget(context, "repetition"))) { // card.lua:3342
        Card *other_card = tobj(context, "other_card");
        if (veq(cardarea, O(G->play))) {
            if (NAME("Sock and Buskin") && (Card_is_face(other_card, false))) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Hanging Chad") && (other_card == tobji(ttab(context, "scoring_hand"), 1))) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Dusk") && tnum(ttab(GAME, "current_round"), "hands_left") == 0) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
            }
            if (NAME("Seltzer")) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", N(1), "card", O(self))));
            }
            if (NAME("Hack") && (Card_get_id(other_card) == 2 || Card_get_id(other_card) == 3 ||
                                 Card_get_id(other_card) == 4 || Card_get_id(other_card) == 5)) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
            }
        }
        if (veq(cardarea, O(G->hand))) {
            if (NAME("Mime") && mime_cond(context)) {
                RET(T(TAB("message", localize(S("k_again_ex"), NULL), "repetitions", tget(ability, "extra"), "card", O(self))));
            }
        }
        RET(NIL);
    } else if (truthy(tget(context, "other_joker"))) { // card.lua:3396
        Card *other_joker = tobj(context, "other_joker");
        if (NAME("Baseball Card") && veq(tget(ttab(other_joker->moveable.node.config, "center"), "rarity"), N(2)) &&
            self != other_joker) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_baseball, .ctx = context}), NULL, false);
            RET(T(TAB("message", localize(T(TAB("type", S("variable"), "key", S("a_xmult"),
                                               "vars", T(ARR(tget(ability, "extra"))))), NULL),
                      "Xmult_mod", tget(ability, "extra"))));
        }
        RET(NIL);
    }
    return false;
}
