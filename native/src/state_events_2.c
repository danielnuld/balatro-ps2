// functions/state_events.lua:571-1209: the scoring of a played hand, the moves of cards between the areas after it,
// and the rows of the round's payout.
#include "all.h"

#define DYN(c) ((c)->moveable.node.object.dyn)

// state_events.lua:615-616. The Lua's globals mult and hand_chips (state_events.h): first assigned in this range, so
// they are defined here.
num mult = 0;
num hand_chips = 0;

// a Val that must be a table to be indexed (the Lua would raise on anything else but nil/false is tested before)
static Table *vtab(Val v) { return v.t == V_TAB ? v.tab : NULL; }

// the Lua's `cond and number` (false when cond is false)
static Val and_num(bool cond, num n) { return cond ? N(n) : B(false); }

// localize{type='variable', key=key, vars={var}}
static Val loc_variable(const char *key, Val var) {
    return localize(T(TAB("type", S("variable"), "key", S(key), "vars", T(ARR(var)))), NULL);
}

// state_events.lua:600 function (a, b) return a.T.x < b.T.x end
static bool less_T_x(Val a, Val b, void *ctx) {
    return ((Card *)a.obj)->moveable.node.T->x < ((Card *)b.obj)->moveable.node.T->x;
}

// state_events.lua:659 and :1003 share these three lines
static void juice_blind_debuff(void) {
    UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_1"), NULL), 0.3f, N(0));
    UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_2"), NULL), 0.3f, N(0));
    Blind_juice_up((Blind *)tobj(G->GAME, "blind"), -1, NIL);
}

// state_events.lua:659
static bool ev_debuffed_card(void *ctx) {
    juice_blind_debuff();
    return true;
}

// state_events.lua:838. Captures the table effects and the index ii; the card is read when the event runs.
struct ev_index_ctx { Table *t; int i; };
static bool ev_effect_juice(void *ctx) {
    struct ev_index_ctx *c = ctx;
    void *card = tobj(ttabi(c->t, c->i), "card");
    CALL(card, juice_up, 0.7f, NIL);
    return true;
}

// state_events.lua:987. Captures cards_destroyed and i.
static bool ev_destroy_card(void *ctx) {
    struct ev_index_ctx *c = ctx;
    Card *card = (Card *)tobji(c->t, c->i);
    if (streq(tstr(card->ability, "name"), "Glass Card")) {
        Card_shatter(card);
    } else {
        Card_start_dissolve(card, NULL, false, -1, false);
    }
    return true;
}

// state_events.lua:1006
static bool ev_not_allowed_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}

// state_events.lua:1002
static bool ev_not_allowed(void *ctx) {
    juice_blind_debuff();
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"),
                                             .not_blockable = true, .not_blocking = true, .func = ev_not_allowed_sound}),
          NULL, false);
    play_sound("tarot2", 1, 0.4f);
    return true;
}

// state_events.lua:1031. Reads the globals hand_chips and mult when it runs.
static bool ev_hand_total(void *ctx) {
    update_hand_text(TAB("delay", N(0), "immediate", B(true)),
                     TAB("mult", N(0), "chips", N(0), "chip_total", N(floor(hand_chips * mult)), "level", S(""), "handname", S("")));
    play_sound("button", 0.9f, 0.6f);
    return true;
}

// state_events.lua:1041
static bool ev_chips2(void *ctx) {
    play_sound("chips2", -1, -1);
    return true;
}

// state_events.lua:1051 and :1060
static num ease_floor(num t, void *ctx) { return floor(t); }

// state_events.lua:1064
static bool ev_clear_handname(void *ctx) {
    tsets(ttab(ttab(G->GAME, "current_round"), "current_hand"), "handname", "");
    return true;
}

// state_events.lua:1079. ctx: scoring_hand.
static bool ev_debuff_played(void *ctx) {
    Table *scoring_hand = ctx;
    if (tbool(ttab(G->GAME, "modifiers"), "debuff_played_cards")) {
        Val v;
        for (int k = 1; truthy(v = tgeti(scoring_hand, k)); k++) tsetb(((Card *)v.obj)->ability, "perma_debuff", true);
    }
    return true;
}

#define HAND(text) ttab(ttab(G->GAME, "hands"), (text))
#define FULL_HAND T(G->play->cards)

// state_events.lua:571
void FUNCS_evaluate_play(UIElement *e) {
    const char *disp_text, *non_loc_disp_text;
    Table *poker_hands, *scoring_hand;
    const char *text = FUNCS_get_poker_hand_info(G->play->cards, &disp_text, &poker_hands, &scoring_hand, &non_loc_disp_text);

    tsetn(HAND(text), "played", tnum(HAND(text), "played") + 1);
    tsetn(HAND(text), "played_this_round", tnum(HAND(text), "played_this_round") + 1);
    tsets(G->GAME, "last_hand_played", text);
    set_hand_usage(text);
    tsetb(HAND(text), "visible", true);

    // Add all the pure bonus cards to the scoring hand
    Table *pures = tnew();
    for (int i = 1, n = CardArea_n(G->play); i <= n; i++) {
        if (!tempty(find_joker("Splash", false))) {
            tseti(scoring_hand, i, tgeti(G->play->cards, i));
        } else {
            if (streq(tstr(CardArea_card(G->play, i)->ability, "effect"), "Stone Card")) {
                bool inside = false;
                for (int j = 1, m = tlen(scoring_hand); j <= m; j++) {
                    if (veq(tgeti(scoring_hand, j), tgeti(G->play->cards, i))) {
                        inside = true;
                    }
                }
                if (!inside) tinsert(pures, tgeti(G->play->cards, i));
            }
        }
    }
    for (int i = 1, n = tlen(pures); i <= n; i++) {
        tinsert(scoring_hand, tgeti(pures, i));
    }
    tsort(scoring_hand, less_T_x, NULL);
    delay(0.2, NULL);
    for (int i = 1, n = tlen(scoring_hand); i <= n; i++) {
        // Highlight all the cards used in scoring and play a sound indicating highlight
        highlight_card((Card *)tobji(scoring_hand, i), (i - 0.999) / 5, "up");
    }

    // note: percent only reaches the sounds' pitch; kept a double as in the Lua and narrowed at the calls
    num percent = 0.3;
    num percent_delta = 0.08;

    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    if (!veq(tget(current_hand, "handname"), S(disp_text))) delay(0.3, NULL);
    {
        bool changed = !veq(tget(current_hand, "handname"), S(disp_text));
        update_hand_text(TAB("sound", changed ? S("button") : NIL, "volume", N(0.4), "immediate", B(true), "delay", N(changed ? 0.4 : 0)),
                         TAB("handname", S(disp_text), "level", tget(HAND(text), "level"), "mult", tget(HAND(text), "mult"),
                             "chips", tget(HAND(text), "chips")));
    }

    if (!Blind_debuff_hand((Blind *)tobj(G->GAME, "blind"), G->play->cards, poker_hands, text, false)) {
        mult = mod_mult(tnum(HAND(text), "mult"));
        hand_chips = mod_chips(tnum(HAND(text), "chips"));

        check_for_unlock(TAB("type", S("hand"), "handname", S(text), "disp_text", S(non_loc_disp_text),
                             "scoring_hand", T(scoring_hand), "full_hand", FULL_HAND));

        delay(0.4, NULL);

        {
            Val lvl = tget(G->GAME, "first_used_hand_level");
            if (truthy(lvl) && vnum(lvl) > 0) {
                level_up_hand(CardArea_card(G->deck, 1), text, false, lvl);
                tset(G->GAME, "first_used_hand_level", NIL);
            }
        }

        bool hand_text_set = false;
        for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
            // calculate the joker effects
            Table *effects = eval_card(CardArea_card(G->jokers, i),
                                       TAB("cardarea", O(G->jokers), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                           "scoring_name", S(text), "poker_hands", T(poker_hands), "before", B(true)));
            if (truthy(tget(effects, "jokers"))) {
                card_eval_status_text(CardArea_card(G->jokers, i), "jokers", 0, (float)percent, NULL, ttab(effects, "jokers"));
                percent = percent + percent_delta;
                if (tbool(ttab(effects, "jokers"), "level_up")) {
                    level_up_hand(CardArea_card(G->jokers, i), text, false, NIL);
                }
            }
        }

        mult = mod_mult(tnum(HAND(text), "mult"));
        hand_chips = mod_chips(tnum(HAND(text), "chips"));

        bool modded = false;

        {
            num new_chips = hand_chips;
            num new_mult = Blind_modify_hand((Blind *)tobj(G->GAME, "blind"), G->play->cards, poker_hands, text, mult, hand_chips,
                                             &new_chips, &modded);
            mult = new_mult, hand_chips = new_chips;
        }
        {
            num m = mod_mult(mult), c = mod_chips(hand_chips);
            mult = m, hand_chips = c;
        }
        if (modded) update_hand_text(TAB("sound", S("chips2"), "modded", B(modded)), TAB("chips", N(hand_chips), "mult", N(mult)));
        for (int i = 1, n = tlen(scoring_hand); i <= n; i++) {
            Card *sc = (Card *)tobji(scoring_hand, i);
            // add cards played to list
            if (!streq(tstr(sc->ability, "effect"), "Stone Card")) {
                Table *played = vtab(tgetv(ttab(G->GAME, "cards_played"), tget(sc->base, "value")));
                tsetn(played, "total", tnum(played, "total") + 1);
                tsetv(ttab(played, "suits"), tget(sc->base, "suit"), B(true));
            }
            // if card is debuffed
            if (sc->debuff) {
                ((Blind *)tobj(G->GAME, "blind"))->triggered = true;
                E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_debuffed_card}), NULL, false);
                card_eval_status_text(sc, "debuff", 0, -1, NULL, NULL);
            } else {
                // Check for play doubling
                Table *reps = ARR(N(1));

                // From Red seal
                Table *eval = eval_card(sc, TAB("repetition_only", B(true), "cardarea", O(G->play), "full_hand", FULL_HAND,
                                                "scoring_hand", T(scoring_hand), "scoring_name", S(text),
                                                "poker_hands", T(poker_hands), "repetition", B(true)));
                if (!tempty(eval)) {
                    // note: the Lua raises if eval has no seals here; with repetition_only it always has
                    num repetitions = tnum(ttab(eval, "seals"), "repetitions");
                    for (num h = 1; h <= repetitions; h++) {
                        tseti(reps, tlen(reps) + 1, T(eval));
                    }
                }
                // From jokers
                for (int j = 1, nj = CardArea_n(G->jokers); j <= nj; j++) {
                    // calculate the joker effects
                    Table *eval = eval_card(CardArea_card(G->jokers, j),
                                            TAB("cardarea", O(G->play), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                "scoring_name", S(text), "poker_hands", T(poker_hands), "other_card", O(sc),
                                                "repetition", B(true)));
                    if (!tempty(eval) && truthy(tget(eval, "jokers"))) {
                        num repetitions = tnum(ttab(eval, "jokers"), "repetitions");
                        for (num h = 1; h <= repetitions; h++) {
                            tseti(reps, tlen(reps) + 1, T(eval));
                        }
                    }
                }
                for (int j = 1, nr = tlen(reps); j <= nr; j++) {
                    percent = percent + percent_delta;
                    if (!veq(tgeti(reps, j), N(1))) {
                        Table *rep = ttabi(reps, j);
                        Table *r = truthy(tget(rep, "jokers")) ? ttab(rep, "jokers") : ttab(rep, "seals");
                        card_eval_status_text((Card *)tobj(r, "card"), "jokers", 0, -1, NULL, r);
                    }

                    // calculate the hand effects
                    Table *effects = ARR(T(eval_card(sc, TAB("cardarea", O(G->play), "full_hand", FULL_HAND,
                                                             "scoring_hand", T(scoring_hand), "poker_hand", S(text)))));
                    for (int k = 1, nk = CardArea_n(G->jokers); k <= nk; k++) {
                        // calculate the joker individual card effects
                        Val eval = Card_calculate_joker(CardArea_card(G->jokers, k),
                                                        TAB("cardarea", O(G->play), "full_hand", FULL_HAND,
                                                            "scoring_hand", T(scoring_hand), "scoring_name", S(text),
                                                            "poker_hands", T(poker_hands), "other_card", O(sc), "individual", B(true)));
                        if (truthy(eval)) {
                            tinsert(effects, eval);
                        }
                    }
                    tset(DYN(sc), "lucky_trigger", NIL);

                    for (int ii = 1, ne = tlen(effects); ii <= ne; ii++) {
                        Table *eff = vtab(tgeti(effects, ii));
                        // If chips added, do chip add event and add the chips to the total
                        if (truthy(tget(eff, "chips"))) {
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            hand_chips = mod_chips(hand_chips + tnum(eff, "chips"));
                            update_hand_text(TAB("delay", N(0)), TAB("chips", N(hand_chips)));
                            card_eval_status_text(sc, "chips", tnum(eff, "chips"), (float)percent, NULL, NULL);
                        }

                        // If mult added, do mult add event and add the mult to the total
                        if (truthy(tget(eff, "mult"))) {
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            mult = mod_mult(mult + tnum(eff, "mult"));
                            update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                            card_eval_status_text(sc, "mult", tnum(eff, "mult"), (float)percent, NULL, NULL);
                        }

                        // If play dollars added, add dollars to total
                        if (truthy(tget(eff, "p_dollars"))) {
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            ease_dollars(tnum(eff, "p_dollars"), false);
                            card_eval_status_text(sc, "dollars", tnum(eff, "p_dollars"), (float)percent, NULL, NULL);
                        }

                        // If dollars added, add dollars to total
                        if (truthy(tget(eff, "dollars"))) {
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            ease_dollars(tnum(eff, "dollars"), false);
                            card_eval_status_text(sc, "dollars", tnum(eff, "dollars"), (float)percent, NULL, NULL);
                        }

                        // Any extra effects
                        if (truthy(tget(eff, "extra"))) {
                            Table *extra = ttab(eff, "extra");
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            struct { bool mult, hand_chips; } extras = {false, false};
                            if (truthy(tget(extra, "mult_mod"))) { mult = mod_mult(mult + tnum(extra, "mult_mod")); extras.mult = true; }
                            if (truthy(tget(extra, "chip_mod"))) { hand_chips = mod_chips(hand_chips + tnum(extra, "chip_mod")); extras.hand_chips = true; }
                            if (truthy(tget(extra, "swap"))) {
                                num old_mult = mult;
                                mult = mod_mult(hand_chips);
                                hand_chips = mod_chips(old_mult);
                                extras.hand_chips = true; extras.mult = true;
                            }
                            if (truthy(tget(extra, "func"))) fcall0(tget(extra, "func"));
                            update_hand_text(TAB("delay", N(0)), TAB("chips", and_num(extras.hand_chips, hand_chips),
                                                                     "mult", and_num(extras.mult, mult)));
                            card_eval_status_text(sc, "extra", 0, (float)percent, NULL, extra);
                        }

                        // If x_mult added, do mult add event and mult the mult to the total
                        if (truthy(tget(eff, "x_mult"))) {
                            if (truthy(tget(eff, "card"))) juice_card((Card *)tobj(eff, "card"));
                            mult = mod_mult(mult * tnum(eff, "x_mult"));
                            update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                            card_eval_status_text(sc, "x_mult", tnum(eff, "x_mult"), (float)percent, NULL, NULL);
                        }

                        // calculate the card edition effects
                        if (truthy(tget(eff, "edition"))) {
                            Table *ed = ttab(eff, "edition");
                            Val chip_mod = tget(ed, "chip_mod"), mult_mod = tget(ed, "mult_mod"), x_mult_mod = tget(ed, "x_mult_mod");
                            hand_chips = mod_chips(hand_chips + (truthy(chip_mod) ? vnum(chip_mod) : 0));
                            mult = mult + (truthy(mult_mod) ? vnum(mult_mod) : 0);
                            mult = mod_mult(mult * (truthy(x_mult_mod) ? vnum(x_mult_mod) : 1));
                            update_hand_text(TAB("delay", N(0)),
                                             TAB("chips", truthy(chip_mod) ? N(hand_chips) : NIL,
                                                 "mult", (truthy(mult_mod) || truthy(x_mult_mod)) ? N(mult) : NIL));
                            Val message = truthy(chip_mod) ? loc_variable("a_chips", chip_mod) : B(false);
                            if (!truthy(message)) message = truthy(mult_mod) ? loc_variable("a_mult", mult_mod) : mult_mod;
                            if (!truthy(message)) message = truthy(x_mult_mod) ? loc_variable("a_xmult", x_mult_mod) : x_mult_mod;
                            card_eval_status_text(sc, "extra", 0, (float)percent, NULL,
                                                  TAB("message", message,
                                                      "chip_mod", chip_mod,
                                                      "mult_mod", mult_mod,
                                                      "x_mult_mod", x_mult_mod,
                                                      "colour", P(G->C.DARK_EDITION),
                                                      "edition", B(true)));
                        }
                    }
                }
            }
        }

        delay(0.3, NULL);
        bool mod_percent = false;
        for (int i = 1, n = CardArea_n(G->hand); i <= n; i++) {
            Card *hc = CardArea_card(G->hand, i);
            if (mod_percent) percent = percent + percent_delta;
            mod_percent = false;

            // Check for hand doubling
            Table *reps = ARR(N(1));
            int j = 1;
            while (j <= tlen(reps)) {
                if (!veq(tgeti(reps, j), N(1))) {
                    Table *rep = ttabi(reps, j);
                    Table *r = truthy(tget(rep, "jokers")) ? ttab(rep, "jokers") : ttab(rep, "seals");
                    card_eval_status_text((Card *)tobj(r, "card"), "jokers", 0, -1, NULL, r);
                    percent = percent + percent_delta;
                }

                // calculate the hand effects
                Table *effects = ARR(T(eval_card(hc, TAB("cardarea", O(G->hand), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                         "scoring_name", S(text), "poker_hands", T(poker_hands)))));

                for (int k = 1, nk = CardArea_n(G->jokers); k <= nk; k++) {
                    // calculate the joker individual card effects
                    Val eval = Card_calculate_joker(CardArea_card(G->jokers, k),
                                                    TAB("cardarea", O(G->hand), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                        "scoring_name", S(text), "poker_hands", T(poker_hands), "other_card", O(hc),
                                                        "individual", B(true)));
                    if (truthy(eval)) {
                        mod_percent = true;
                        tinsert(effects, eval);
                    }
                }

                if (veq(tgeti(reps, j), N(1))) {
                    // Check for hand doubling

                    // From Red seal
                    Table *eval = eval_card(hc, TAB("repetition_only", B(true), "cardarea", O(G->hand), "full_hand", FULL_HAND,
                                                    "scoring_hand", T(scoring_hand), "scoring_name", S(text),
                                                    "poker_hands", T(poker_hands), "repetition", B(true), "card_effects", T(effects)));
                    if (!tempty(eval) && (!tempty(vtab(tgeti(effects, 1))) || tlen(effects) > 1)) {
                        num repetitions = tnum(ttab(eval, "seals"), "repetitions");
                        for (num h = 1; h <= repetitions; h++) {
                            tseti(reps, tlen(reps) + 1, T(eval));
                        }
                    }

                    // From Joker
                    for (int jj = 1, nj = CardArea_n(G->jokers); jj <= nj; jj++) {
                        // calculate the joker effects
                        Table *eval = eval_card(CardArea_card(G->jokers, jj),
                                                TAB("cardarea", O(G->hand), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                    "scoring_name", S(text), "poker_hands", T(poker_hands), "other_card", O(hc),
                                                    "repetition", B(true), "card_effects", T(effects)));
                        if (!tempty(eval)) {
                            // note: no `and eval.jokers` guard here in the Lua (it would raise without jokers)
                            num repetitions = tnum(ttab(eval, "jokers"), "repetitions");
                            for (num h = 1; h <= repetitions; h++) {
                                tseti(reps, tlen(reps) + 1, T(eval));
                            }
                        }
                    }
                }

                for (int ii = 1, ne = tlen(effects); ii <= ne; ii++) {
                    Table *eff = vtab(tgeti(effects, ii));
                    // if this effect came from a joker
                    if (truthy(tget(eff, "card"))) {
                        mod_percent = true;
                        struct ev_index_ctx *c = NEWCTX(struct ev_index_ctx);
                        c->t = effects, c->i = ii;
                        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_effect_juice, .ctx = c}), NULL, false);
                    }

                    // If hold mult added, do hold mult add event and add the mult to the total

                    // If dollars added, add dollars to total
                    if (truthy(tget(eff, "dollars"))) {
                        ease_dollars(tnum(eff, "dollars"), false);
                        card_eval_status_text(hc, "dollars", tnum(eff, "dollars"), (float)percent, NULL, NULL);
                    }

                    if (truthy(tget(eff, "h_mult"))) {
                        mod_percent = true;
                        mult = mod_mult(mult + tnum(eff, "h_mult"));
                        update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                        card_eval_status_text(hc, "h_mult", tnum(eff, "h_mult"), (float)percent, NULL, NULL);
                    }

                    if (truthy(tget(eff, "x_mult"))) {
                        mod_percent = true;
                        mult = mod_mult(mult * tnum(eff, "x_mult"));
                        update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                        card_eval_status_text(hc, "x_mult", tnum(eff, "x_mult"), (float)percent, NULL, NULL);
                    }

                    if (truthy(tget(eff, "message"))) {
                        mod_percent = true;
                        update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                        card_eval_status_text(hc, "extra", 0, (float)percent, NULL, eff);
                    }
                }
                j = j + 1;
            }
        }
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
        // Joker Effects
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
        percent = percent + percent_delta;
        for (int i = 1, n = CardArea_n(G->jokers) + CardArea_n(G->consumeables); i <= n; i++) {
            Card *_card = CardArea_card(G->jokers, i);
            if (!_card) _card = CardArea_card(G->consumeables, i - CardArea_n(G->jokers));
            // calculate the joker edition effects
            Table *edition_effects = eval_card(_card, TAB("cardarea", O(G->jokers), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                          "scoring_name", S(text), "poker_hands", T(poker_hands), "edition", B(true)));
            if (truthy(tget(edition_effects, "jokers"))) {
                Table *ed = ttab(edition_effects, "jokers");
                tsetb(ed, "edition", true);
                if (truthy(tget(ed, "chip_mod"))) {
                    hand_chips = mod_chips(hand_chips + tnum(ed, "chip_mod"));
                    update_hand_text(TAB("delay", N(0)), TAB("chips", N(hand_chips)));
                    card_eval_status_text(_card, "jokers", 0, (float)percent, NULL,
                                          TAB("message", loc_variable("a_chips", tget(ed, "chip_mod")),
                                              "chip_mod", tget(ed, "chip_mod"),
                                              "colour", P(G->C.EDITION),
                                              "edition", B(true)));
                }
                if (truthy(tget(ed, "mult_mod"))) {
                    mult = mod_mult(mult + tnum(ed, "mult_mod"));
                    update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                    card_eval_status_text(_card, "jokers", 0, (float)percent, NULL,
                                          TAB("message", loc_variable("a_mult", tget(ed, "mult_mod")),
                                              "mult_mod", tget(ed, "mult_mod"),
                                              "colour", P(G->C.DARK_EDITION),
                                              "edition", B(true)));
                }
                percent = percent + percent_delta;
            }

            // calculate the joker effects
            Table *effects = eval_card(_card, TAB("cardarea", O(G->jokers), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                                  "scoring_name", S(text), "poker_hands", T(poker_hands), "joker_main", B(true)));

            // Any Joker effects
            if (truthy(tget(effects, "jokers"))) {
                Table *je = ttab(effects, "jokers");
                struct { bool mult, hand_chips; } extras = {false, false};
                if (truthy(tget(je, "mult_mod"))) { mult = mod_mult(mult + tnum(je, "mult_mod")); extras.mult = true; }
                if (truthy(tget(je, "chip_mod"))) { hand_chips = mod_chips(hand_chips + tnum(je, "chip_mod")); extras.hand_chips = true; }
                if (truthy(tget(je, "Xmult_mod"))) { mult = mod_mult(mult * tnum(je, "Xmult_mod")); extras.mult = true; }
                update_hand_text(TAB("delay", N(0)), TAB("chips", and_num(extras.hand_chips, hand_chips), "mult", and_num(extras.mult, mult)));
                card_eval_status_text(_card, "jokers", 0, (float)percent, NULL, je);
                percent = percent + percent_delta;
            }

            // Joker on Joker effects
            Val vv;
            for (int k = 1; truthy(vv = tgeti(G->jokers->cards, k)); k++) {
                Card *v = (Card *)vv.obj;
                Val effect_v = Card_calculate_joker(v, TAB("full_hand", FULL_HAND, "scoring_hand", T(scoring_hand), "scoring_name", S(text),
                                                           "poker_hands", T(poker_hands), "other_joker", O(_card)));
                if (truthy(effect_v)) {
                    Table *effect = vtab(effect_v);
                    struct { bool mult, hand_chips; } extras = {false, false};
                    if (truthy(tget(effect, "mult_mod"))) { mult = mod_mult(mult + tnum(effect, "mult_mod")); extras.mult = true; }
                    if (truthy(tget(effect, "chip_mod"))) { hand_chips = mod_chips(hand_chips + tnum(effect, "chip_mod")); extras.hand_chips = true; }
                    if (truthy(tget(effect, "Xmult_mod"))) { mult = mod_mult(mult * tnum(effect, "Xmult_mod")); extras.mult = true; }
                    if (extras.mult || extras.hand_chips)
                        update_hand_text(TAB("delay", N(0)), TAB("chips", and_num(extras.hand_chips, hand_chips), "mult", and_num(extras.mult, mult)));
                    if (extras.mult || extras.hand_chips) card_eval_status_text(v, "jokers", 0, (float)percent, NULL, effect);
                    percent = percent + percent_delta;
                }
            }

            if (truthy(tget(edition_effects, "jokers"))) {
                Table *ed = ttab(edition_effects, "jokers");
                if (truthy(tget(ed, "x_mult_mod"))) {
                    mult = mod_mult(mult * tnum(ed, "x_mult_mod"));
                    update_hand_text(TAB("delay", N(0)), TAB("mult", N(mult)));
                    card_eval_status_text(_card, "jokers", 0, (float)percent, NULL,
                                          TAB("message", loc_variable("a_xmult", tget(ed, "x_mult_mod")),
                                              "x_mult_mod", tget(ed, "x_mult_mod"),
                                              "colour", P(G->C.EDITION),
                                              "edition", B(true)));
                }
                percent = percent + percent_delta;
            }
        }

        Val nu_mult = NIL;
        Val nu_chip = Back_trigger_effect((Back *)tobj(G->GAME, "selected_back"),
                                          TAB("context", S("final_scoring_step"), "chips", N(hand_chips), "mult", N(mult)), &nu_mult);
        mult = mod_mult(truthy(nu_mult) ? vnum(nu_mult) : mult);
        hand_chips = mod_chips(truthy(nu_chip) ? vnum(nu_chip) : hand_chips);

        Table *cards_destroyed = tnew();
        for (int i = 1, n = tlen(scoring_hand); i <= n; i++) {
            Card *sc = (Card *)tobji(scoring_hand, i);
            Val destroyed = NIL;
            // un-highlight all cards
            highlight_card(sc, (i - 0.999) / (n - 0.998), "down");

            for (int j = 1, nj = CardArea_n(G->jokers); j <= nj; j++) {
                destroyed = Card_calculate_joker(CardArea_card(G->jokers, j), TAB("destroying_card", O(sc), "full_hand", FULL_HAND));
                if (truthy(destroyed)) break;
            }

            if (streq(tstr(sc->ability, "name"), "Glass Card") && !sc->debuff &&
                pseudorandom(S("glass"), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / tnum(sc->ability, "extra")) {
                destroyed = B(true);
            }

            if (truthy(destroyed)) {
                if (streq(tstr(sc->ability, "name"), "Glass Card")) {
                    tsetb(DYN(sc), "shattered", true);
                } else {
                    tsetb(DYN(sc), "destroyed", true);
                }
                tseti(cards_destroyed, tlen(cards_destroyed) + 1, O(sc));
            }
        }
        for (int j = 1, nj = CardArea_n(G->jokers); j <= nj; j++) {
            eval_card(CardArea_card(G->jokers, j), TAB("cardarea", O(G->jokers), "remove_playing_cards", B(true), "removed", T(cards_destroyed)));
        }

        Table *glass_shattered = tnew();
        {
            Val v;
            for (int k = 1; truthy(v = tgeti(cards_destroyed, k)); k++) {
                if (tbool(DYN((Card *)v.obj), "shattered")) tseti(glass_shattered, tlen(glass_shattered) + 1, v);
            }
        }

        check_for_unlock(TAB("type", S("shatter"), "shattered", T(glass_shattered)));

        for (int i = 1, n = tlen(cards_destroyed); i <= n; i++) {
            struct ev_index_ctx *c = NEWCTX(struct ev_index_ctx);
            c->t = cards_destroyed, c->i = i;
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_destroy_card, .ctx = c}), NULL, false);
        }
    } else {
        mult = mod_mult(0);
        hand_chips = mod_chips(0);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_not_allowed}), NULL, false);

        play_area_status_text(lit("evaluate_play", 1), false, -1); // the Lua's literal (data.h LITERALS); --localize('k_not_allowed_ex'), true)
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
        // Joker Debuff Effects
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
        for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
            // calculate the joker effects
            Table *effects = eval_card(CardArea_card(G->jokers, i),
                                       TAB("cardarea", O(G->jokers), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                           "scoring_name", S(text), "poker_hands", T(poker_hands), "debuffed_hand", B(true)));

            // Any Joker effects
            if (truthy(tget(effects, "jokers"))) {
                card_eval_status_text(CardArea_card(G->jokers, i), "jokers", 0, (float)percent, NULL, ttab(effects, "jokers"));
                percent = percent + percent_delta;
            }
        }
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.4, .func = ev_hand_total}), NULL, false);
    check_and_set_high_score("hand", hand_chips * mult);

    check_for_unlock(TAB("type", S("chip_score"), "chips", N(floor(hand_chips * mult))));

    if (hand_chips * mult > 0) {
        delay(0.8, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_chips2}), NULL, false);
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "ease",
                                             .not_blocking = true,
                                             .ref_table = G->GAME,
                                             .ref_value = "chips",
                                             .ease_to = tnum(G->GAME, "chips") + floor(hand_chips * mult),
                                             .delay = 0.5,
                                             .ease_func = ease_floor}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "ease",
                                             .ref_table = ttab(ttab(G->GAME, "current_round"), "current_hand"),
                                             .ref_value = "chip_total",
                                             .ease_to = 0,
                                             .delay = 0.5,
                                             .ease_func = ease_floor}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_clear_handname}), NULL, false);
    delay(0.3, NULL);

    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        // calculate the joker after hand played effects
        Table *effects = eval_card(CardArea_card(G->jokers, i),
                                   TAB("cardarea", O(G->jokers), "full_hand", FULL_HAND, "scoring_hand", T(scoring_hand),
                                       "scoring_name", S(text), "poker_hands", T(poker_hands), "after", B(true)));
        if (truthy(tget(effects, "jokers"))) {
            card_eval_status_text(CardArea_card(G->jokers, i), "jokers", 0, (float)percent, NULL, ttab(effects, "jokers"));
            percent = percent + percent_delta;
        }
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_debuff_played, .ctx = scoring_hand}), NULL, false);
}

// state_events.lua:1088
void FUNCS_draw_from_play_to_discard(UIElement *e) {
    int play_count = CardArea_n(G->play);
    int it = 1;
    Val vv;
    for (int k = 1; truthy(vv = tgeti(G->play->cards, k)); k++) {
        Card *v = (Card *)vv.obj;
        if (!tbool(DYN(v), "shattered") && !tbool(DYN(v), "destroyed")) {
            draw_card(G->play, G->discard, it * 100.0 / play_count, "down", false, v, -1, false, false, -1, false);
            it = it + 1;
        }
    }
}

// state_events.lua:1099
void FUNCS_draw_from_play_to_hand(Table *cards) {
    int gold_count = tlen(cards);
    for (int i = 1; i <= gold_count; i++) { // draw cards from play
        Card *c = (Card *)tobji(cards, i);
        if (!tbool(DYN(c), "shattered") && !tbool(DYN(c), "destroyed")) {
            draw_card(G->play, G->hand, i * 100.0 / gold_count, "up", true, c, -1, false, false, -1, false);
        }
    }
}

// state_events.lua:1111
static bool ev_discard_to_deck(void *ctx) {
    int discard_count = CardArea_n(G->discard);
    for (int i = 1; i <= discard_count; i++) { // draw cards from deck
        draw_card(G->discard, G->deck, i * 100.0 / discard_count, "up", false, NULL, 0.005, i % 2 == 0, false,
                  lmax((21 - i) / 20.0, 0.7), false);
    }
    return true;
}

// state_events.lua:1108
void FUNCS_draw_from_discard_to_deck(UIElement *e) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_discard_to_deck}), NULL, false);
}

// state_events.lua:1121
void FUNCS_draw_from_hand_to_deck(UIElement *e) {
    int hand_count = CardArea_n(G->hand);
    for (int i = 1; i <= hand_count; i++) { // draw cards from deck
        draw_card(G->hand, G->deck, i * 100.0 / hand_count, "down", false, NULL, 0.08, false, false, -1, false);
    }
}

// state_events.lua:1128
void FUNCS_draw_from_hand_to_discard(UIElement *e) {
    int hand_count = CardArea_n(G->hand);
    for (int i = 1; i <= hand_count; i++) {
        draw_card(G->hand, G->discard, i * 100.0 / hand_count, "down", false, NULL, 0.07, false, false, -1, false);
    }
}

// state_events.lua:1151
static bool ev_blind_defeat(void *ctx) {
    Blind_defeat((Blind *)tobj(G->GAME, "blind"), false);
    return true;
}

// state_events.lua:1158
static bool ev_round_eval_background(void *ctx) {
    ease_background_colour_blind(STATES_ROUND_EVAL, "");
    return true;
}

// state_events.lua:1135
void FUNCS_evaluate_round(void) {
    num pitch = 0.95;
    num dollars = 0;
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    Table *current_round = ttab(G->GAME, "current_round");
    Table *modifiers = ttab(G->GAME, "modifiers");

    if (tnum(G->GAME, "chips") - blind->chips >= 0) {
        add_round_eval_row(TAB("dollars", N(blind->dollars), "name", S("blind1"), "pitch", N(pitch)));
        pitch = pitch + 0.06;
        dollars = dollars + blind->dollars;
    } else {
        add_round_eval_row(TAB("dollars", N(0), "name", S("blind1"), "pitch", N(pitch), "saved", B(true)));
        pitch = pitch + 0.06;
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before",
                                             .delay = 1.3 * lmin(blind->dollars + 2, 7) / 2 * 0.15 + 0.5,
                                             .func = ev_blind_defeat}), NULL, false);
    delay(0.2, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_round_eval_background}), NULL, false);
    Back_trigger_effect((Back *)tobj(G->GAME, "selected_back"), TAB("context", S("eval")), NULL);

    if (tnum(current_round, "hands_left") > 0 && !tbool(modifiers, "no_extra_hand_money")) {
        Val mph = tget(modifiers, "money_per_hand");
        num money_per_hand = truthy(mph) ? vnum(mph) : 1;
        add_round_eval_row(TAB("dollars", N(tnum(current_round, "hands_left") * money_per_hand), "disp", tget(current_round, "hands_left"),
                               "bonus", B(true), "name", S("hands"), "pitch", N(pitch)));
        pitch = pitch + 0.06;
        dollars = dollars + tnum(current_round, "hands_left") * money_per_hand;
    }
    if (tnum(current_round, "discards_left") > 0 && truthy(tget(modifiers, "money_per_discard"))) {
        add_round_eval_row(TAB("dollars", N(tnum(current_round, "discards_left") * tnum(modifiers, "money_per_discard")),
                               "disp", tget(current_round, "discards_left"), "bonus", B(true), "name", S("discards"), "pitch", N(pitch)));
        pitch = pitch + 0.06;
        dollars = dollars + tnum(current_round, "discards_left") * tnum(modifiers, "money_per_discard");
    }
    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        Val ret = Card_calculate_dollar_bonus(CardArea_card(G->jokers, i));
        if (truthy(ret)) {
            add_round_eval_row(TAB("dollars", ret, "bonus", B(true), "name", S(fmt("joker%d", i)), "pitch", N(pitch),
                                   "card", O(CardArea_card(G->jokers, i))));
            pitch = pitch + 0.06;
            dollars = dollars + vnum(ret);
        }
    }
    for (int i = 1, n = tlen(ttab(G->GAME, "tags")); i <= n; i++) {
        Val retv = Tag_apply_to_run((Tag *)tobji(ttab(G->GAME, "tags"), i), TAB("type", S("eval")));
        if (truthy(retv)) {
            Table *ret = vtab(retv);
            add_round_eval_row(TAB("dollars", tget(ret, "dollars"), "bonus", B(true), "name", S(fmt("tag%d", i)), "pitch", N(pitch),
                                   "condition", tget(ret, "condition"), "pos", tget(ret, "pos"), "tag", tget(ret, "tag")));
            pitch = pitch + 0.06;
            dollars = dollars + tnum(ret, "dollars");
        }
    }
    if (tnum(G->GAME, "dollars") >= 5 && !tbool(modifiers, "no_interest")) {
        add_round_eval_row(TAB("bonus", B(true), "name", S("interest"), "pitch", N(pitch),
                               "dollars", N(tnum(G->GAME, "interest_amount") *
                                            lmin(floor(tnum(G->GAME, "dollars") / 5), tnum(G->GAME, "interest_cap") / 5))));
        pitch = pitch + 0.06;
        if (!tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
            Table *career_stats = ttab(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "career_stats");
            if (tnum(G->GAME, "interest_amount") * lmin(floor(tnum(G->GAME, "dollars") / 5), tnum(G->GAME, "interest_cap") / 5) ==
                tnum(G->GAME, "interest_amount") * tnum(G->GAME, "interest_cap") / 5) {
                tsetn(career_stats, "c_round_interest_cap_streak", tnum(career_stats, "c_round_interest_cap_streak") + 1);
            } else {
                tsetn(career_stats, "c_round_interest_cap_streak", 0);
            }
        }
        check_for_unlock(TAB("type", S("interest_streak")));
        dollars = dollars + tnum(G->GAME, "interest_amount") * lmin(floor(tnum(G->GAME, "dollars") / 5), tnum(G->GAME, "interest_cap") / 5);
    }

    pitch = pitch + 0.06;

    add_round_eval_row(TAB("name", S("bottom"), "dollars", N(dollars)));
}
