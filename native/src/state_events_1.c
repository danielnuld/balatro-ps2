// functions/state_events.lua:1-570: win_game, end_round, new_round, and the G.FUNCS draw_from_deck_to_hand,
// discard_cards_from_highlighted, play_cards_from_highlighted, get_poker_hand_info.
// note: 'play_cards_from_highlighted' and 'discard_cards_from_highlighted' are registered by name by
// funcs_register_all (button_callbacks.h); this file has no start-up function to register them from.
#include "all.h"

// a table value or NULL (the Lua would fail indexing anything else)
static Table *vtab(Val v) { return v.t == V_TAB ? v.tab : NULL; }
// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }
// function(a,b) return a.T.x < b.T.x end (state_events.lua:392, :463)
static bool less_T_x(Val a, Val b, void *ctx) {
    return ((Card *)a.obj)->moveable.node.T->x < ((Card *)b.obj)->moveable.node.T->x;
}

// ---------------------------------------------------------------- win_game

// state_events.lua:57
static bool ev_win_wishlist(void *ctx) {
    Card_Character *Jimbo = ctx;
    Card_Character_add_button(Jimbo, localize_s("b_wishlist", NULL), "wishlist_steam", G->C.DARK_EDITION, NULL, true, 1.6f);
    return true;
}

// state_events.lua:46
static bool ev_win_jimbo(void *ctx) {
    if (G->OVERLAY_MENU && UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL)) {
        // note: the upvalue Jimbo is assigned here and read only by the event queued below: passed by value.
        Card_Character *Jimbo = NEW(Card_Character);
        Card_Character_init(Jimbo, TAB("x", N(0), "y", N(5)));
        UIElement *spot = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL);
        Table *spot_config = spot->moveable.node.config;
        CALL(tobj(spot_config, "object"), remove);
        tseto(spot_config, "object", Jimbo);
        tsetb(Jimbo->moveable.node.object.dyn, "ui_object_updated", true);
        Card_Character_add_speech_bubble(Jimbo, S(fmt("wq_%d", (int)math_random_range(1, 7))), NULL, TAB("quip", B(true)));
        Card_Character_say_stuff(Jimbo, 5, false);
        if (G->F_JAN_CTA) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_win_wishlist, .ctx = Jimbo}), NULL, false);
        }
    }
    return true;
}

// state_events.lua:28
static bool ev_win_overlay(void *ctx) {
    // note: pairs(G.I.CARD); the order does not matter. The list also holds the Blind and Card_Characters, where
    // sticker_run would be a dyn field.
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        if (is_a(it.val.obj, &Card_class)) ((Card *)it.val.obj)->sticker_run = NULL;
        else tset(((Object *)it.val.obj)->dyn, "sticker_run", NIL);
    }

    play_sound("win", -1, -1);
    tsetb(G->SETTINGS, "paused", true);

    FUNCS_overlay_menu(TAB("definition", T(create_UIBox_win()), "config", T(TAB("no_esc", B(true)))));

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 2.5, .not_blocking = true, .func = ev_win_jimbo}), NULL,
          false);

    return true;
}

// state_events.lua:78
static bool ev_win_endless(void *ctx) {
    if (!tbool(G->SETTINGS, "paused")) {
        tsets(ttab(G->GAME, "current_round"), "round_text", lit("win_game", 1)); // the Lua's literal (data.h LITERALS)
        return true;
    }
    return false;
}

// state_events.lua:1
void win_game(void) {
    if (!tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
        set_joker_win();
        set_deck_win();

        check_and_set_high_score("win_streak", tnum(ttab(ttab(cur_profile(), "high_scores"), "current_streak"), "amt") + 1);
        check_and_set_high_score("current_streak", tnum(ttab(ttab(cur_profile(), "high_scores"), "current_streak"), "amt") + 1);
        check_for_unlock(TAB("type", S("win_no_hand")));
        check_for_unlock(TAB("type", S("win_no")));
        check_for_unlock(TAB("type", S("win_custom")));
        check_for_unlock(TAB("type", S("win_deck")));
        check_for_unlock(TAB("type", S("win_stake")));
        check_for_unlock(TAB("type", S("win")));
        inc_career_stat("c_wins", 1);
    }

    set_profile_progress();

    if (tbool(G->GAME, "challenge")) {
        tsetv(ttab(ttab(cur_profile(), "challenge_progress"), "completed"), tget(G->GAME, "challenge"), B(true));
        set_challenge_unlock();
        check_for_unlock(TAB("type", S("win_challenge")));
        Game_save_settings(G);
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_win_overlay}), NULL, false);

    if (!tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
        Table *profile = cur_profile();
        Val p_stake = tget(profile, "stake");
        Val g_stake = tget(G->GAME, "stake");
        tsetn(profile, "stake", lmax(truthy(p_stake) ? p_stake.n : 1, (truthy(g_stake) ? g_stake.n : 1) + 1));
    }
    Game_save_progress(G);
    tsetb(G->FILE_HANDLER, "force", true);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_win_endless}), NULL, false);
}

// ---------------------------------------------------------------- end_round

// state_events.lua:162
static bool ev_end_round_win(void *ctx) {
    if (G->STATE == STATES_ROUND_EVAL) {
        win_game();
        tsetb(G->GAME, "won", true);
        return true;
    }
    return false;
}

// state_events.lua:216: reads effects[ii].card when it runs, as the Lua's closure
struct ev_end_round_juice_ctx { Table *effects; int ii; };
static bool ev_end_round_juice(void *ctx) {
    struct ev_end_round_juice_ctx *c = ctx;
    CALL(tobj(ttabi(c->effects, c->ii), "card"), juice_up, 0.7f, NIL);
    return true;
}

// state_events.lua:254
static bool ev_end_round_eval(void *ctx) {
    G->STATE = STATES_ROUND_EVAL;
    G->STATE_COMPLETE = false;

    Table *round_resets = ttab(G->GAME, "round_resets");
    if (ttab(round_resets, "blind") == ttab(G->P_BLINDS, "bl_small")) {
        tsets(ttab(round_resets, "blind_states"), "Small", "Defeated");
    } else if (ttab(round_resets, "blind") == ttab(G->P_BLINDS, "bl_big")) {
        tsets(ttab(round_resets, "blind_states"), "Big", "Defeated");
    } else {
        tsets(ttab(G->GAME, "current_round"), "voucher", get_next_voucher_key(false));
        tsets(ttab(round_resets, "blind_states"), "Boss", "Defeated");
        for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
            Card *v = tobji(G->playing_cards, k);
            tset(v->ability, "played_this_ante", NIL);
        }
    }

    if (tbool(round_resets, "temp_handsize")) {
        CardArea_change_size(G->hand, -tnum(round_resets, "temp_handsize"));
        tset(round_resets, "temp_handsize", NIL);
    }
    if (tbool(round_resets, "temp_reroll_cost")) {
        tset(round_resets, "temp_reroll_cost", NIL);
        calculate_reroll_cost(true);
    }

    reset_idol_card();
    reset_mail_rank();
    reset_ancient_card();
    reset_castle_card();
    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        tset(v->ability, "discarded", NIL);
        tset(v->ability, "forced_selection", NIL);
    }
    return true;
}

// state_events.lua:91
static bool ev_end_round(void *ctx) {
    bool game_over = true;
    bool game_won = false;
    G->RESET_BLIND_STATES = true;
    G->RESET_JIGGLES = true;
    Blind *blind = tobj(G->GAME, "blind");
    if (tnum(G->GAME, "chips") - blind->chips >= 0) {
        game_over = false;
    }
    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        Val eval = NIL;
        eval = Card_calculate_joker(CardArea_card(G->jokers, i), TAB("end_of_round", B(true), "game_over", B(game_over)));
        if (truthy(eval)) {
            if (tbool(vtab(eval), "saved")) {
                game_over = false;
            }
            card_eval_status_text(CardArea_card(G->jokers, i), "jokers", 0, -1, NULL, vtab(eval));
        }
        Card_calculate_rental(CardArea_card(G->jokers, i));
        Card_calculate_perishable(CardArea_card(G->jokers, i));
    }
    Table *round_resets = ttab(G->GAME, "round_resets");
    if (veq(tget(round_resets, "ante"), tget(G->GAME, "win_ante")) && streq(Blind_get_type(blind), "Boss")) {
        game_won = true;
        tsetb(G->GAME, "won", true);
    }
    if (game_over) {
        G->STATE = STATES_GAME_OVER;
        if (!tbool(G->GAME, "won") && !tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
            tsetn(ttab(ttab(cur_profile(), "high_scores"), "current_streak"), "amt", 0);
        }
        Game_save_settings(G);
        tsetb(G->FILE_HANDLER, "force", true);
        G->STATE_COMPLETE = false;
    } else {
        Table *current_round = ttab(G->GAME, "current_round");
        Val unused_discards = tget(G->GAME, "unused_discards");
        tsetn(G->GAME, "unused_discards", (truthy(unused_discards) ? unused_discards.n : 0) + tnum(current_round, "discards_left"));
        if (blind && truthy(tget(blind->moveable.node.config, "blind"))) {
            discover_card(ttab(blind->moveable.node.config, "blind"));
        }

        if (streq(Blind_get_type(blind), "Boss")) {
            const char *_handname = "High Card";
            num _played = -1, _order = 100;
            // note: pairs(G.GAME.hands) is hash order in the Lua; here insertion order. _order is never updated, so
            // on a tie in `played` the last tied hand visited wins: ties may pick another hand than the PC game.
            for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) {
                Table *v = it.val.tab;
                if (tnum(v, "played") > _played || (tnum(v, "played") == _played && _order > tnum(v, "order"))) {
                    _played = tnum(v, "played");
                    _handname = it.key.s;
                }
            }
            tsets(current_round, "most_played_poker_hand", _handname);
        }

        if (streq(Blind_get_type(blind), "Boss") && !tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
            tsetn(G->GAME, "current_boss_streak", tnum(G->GAME, "current_boss_streak") + 1);
            check_and_set_high_score("boss_streak", tnum(G->GAME, "current_boss_streak"));
        }

        if (tnum(current_round, "hands_played") == 1) {
            inc_career_stat("c_single_hand_round_streak", 1);
        } else {
            if (!tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
                tsetn(ttab(cur_profile(), "career_stats"), "c_single_hand_round_streak", 0);
                Game_save_settings(G);
            }
        }

        check_for_unlock(TAB("type", S("round_win")));
        set_joker_usage();
        if (game_won && !tbool(G->GAME, "win_notified")) {
            tsetb(G->GAME, "win_notified", true);
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.trigger = "immediate", .not_blocking = true, .not_blockable = true, .func = ev_end_round_win}),
                  NULL, false);
        }
        for (int i = 1, n = CardArea_n(G->hand); i <= n; i++) {
            // Check for hand doubling
            Table *reps = ARR(N(1));
            int j = 1;
            while (j <= tlen(reps)) {
                float percent = (i - 0.999) / (CardArea_n(G->hand) - 0.998) + (j - 1) * 0.1;
                Val rep = tgeti(reps, j);
                bool rep_is_1 = rep.t == V_NUM && rep.n == 1;
                if (!rep_is_1) {
                    Val r_jokers = tget(rep.tab, "jokers");
                    Table *r = truthy(r_jokers) ? vtab(r_jokers) : ttab(rep.tab, "seals");
                    card_eval_status_text(tobj(r, "card"), "jokers", 0, -1, NULL, r);
                }

                // calculate the hand effects
                Table *effects = ARR(T(Card_get_end_of_round_effect(CardArea_card(G->hand, i), NULL)));
                for (int k = 1, nk = CardArea_n(G->jokers); k <= nk; k++) {
                    // calculate the joker individual card effects
                    Val eval = Card_calculate_joker(CardArea_card(G->jokers, k),
                                                    TAB("cardarea", O(G->hand), "other_card", O(CardArea_card(G->hand, i)),
                                                        "individual", B(true), "end_of_round", B(true)));
                    if (truthy(eval)) {
                        tinsert(effects, eval);
                    }
                }

                if (rep_is_1) {
                    // Check for hand doubling
                    // From Red seal
                    Table *eval = eval_card(CardArea_card(G->hand, i), TAB("end_of_round", B(true), "cardarea", O(G->hand),
                                                                           "repetition", B(true), "repetition_only", B(true)));
                    if (!tempty(eval) && (!tempty(ttabi(effects, 1)) || tlen(effects) > 1)) {
                        num repetitions = tnum(ttab(eval, "seals"), "repetitions");
                        for (num h = 1; h <= repetitions; h++) {
                            tseti(reps, tlen(reps) + 1, T(eval));
                        }
                    }

                    // from Jokers
                    for (int jj = 1, nj = CardArea_n(G->jokers); jj <= nj; jj++) {
                        // calculate the joker effects
                        Table *eval = eval_card(CardArea_card(G->jokers, jj),
                                                TAB("cardarea", O(G->hand), "other_card", O(CardArea_card(G->hand, i)), "repetition",
                                                    B(true), "end_of_round", B(true), "card_effects", T(effects)));
                        if (!tempty(eval)) {
                            num repetitions = tnum(ttab(eval, "jokers"), "repetitions");
                            for (num h = 1; h <= repetitions; h++) {
                                tseti(reps, tlen(reps) + 1, T(eval));
                            }
                        }
                    }
                }

                for (int ii = 1, ne = tlen(effects); ii <= ne; ii++) {
                    Table *effect = vtab(tgeti(effects, ii));
                    // if this effect came from a joker
                    if (truthy(tget(effect, "card"))) {
                        struct ev_end_round_juice_ctx *c = NEWCTX(struct ev_end_round_juice_ctx);
                        c->effects = effects;
                        c->ii = ii;
                        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_end_round_juice, .ctx = c}), NULL,
                              false);
                    }

                    // If dollars
                    Val h_dollars = tget(effect, "h_dollars");
                    if (truthy(h_dollars)) {
                        ease_dollars(vnum(h_dollars), false);
                        card_eval_status_text(CardArea_card(G->hand, i), "dollars", vnum(h_dollars), percent, NULL, NULL);
                    }

                    // Any extras
                    if (truthy(tget(effect, "extra"))) {
                        card_eval_status_text(CardArea_card(G->hand, i), "extra", 0, percent, NULL, ttab(effect, "extra"));
                    }
                }
                j = j + 1;
            }
        }
        delay(0.3, NULL);

        FUNCS_draw_from_hand_to_discard(NULL);
        if (streq(Blind_get_type(blind), "Boss")) {
            tset(G->GAME, "voucher_restock", NIL);
            Table *modifiers = ttab(G->GAME, "modifiers");
            if (truthy(tget(modifiers, "set_eternal_ante")) && veq(tget(round_resets, "ante"), tget(modifiers, "set_eternal_ante"))) {
                for (int k = 1; truthy(tgeti(G->jokers->cards, k)); k++) {
                    Card_set_eternal(CardArea_card(G->jokers, k), true);
                }
            }
            if (truthy(tget(modifiers, "set_joker_slots_ante")) &&
                veq(tget(round_resets, "ante"), tget(modifiers, "set_joker_slots_ante"))) {
                tsetn(G->jokers->moveable.node.config, "card_limit", 0);
            }
            delay(0.4, NULL);
            ease_ante(1);
            delay(0.4, NULL);
            check_for_unlock(TAB("type", S("ante_up"), "ante", N(tnum(round_resets, "ante") + 1)));
        }
        FUNCS_draw_from_discard_to_deck(NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .func = ev_end_round_eval}), NULL, false);
    }
    return true;
}

// state_events.lua:87
void end_round(void) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_end_round}), NULL, false);
}

// ---------------------------------------------------------------- new_round

// state_events.lua:342
static bool ev_new_round_draw(void *ctx) {
    G->STATE = STATES_DRAW_TO_HAND;
    CardArea_shuffle(G->deck, fmt("nr%s", vstr(tget(ttab(G->GAME, "round_resets"), "ante"))));
    CardArea_hard_set_T(G->deck, NAN, NAN, NAN, NAN);
    G->STATE_COMPLETE = false;
    return true;
}

// state_events.lua:295
static bool ev_new_round(void *ctx) {
    Table *current_round = ttab(G->GAME, "current_round");
    Table *round_resets = ttab(G->GAME, "round_resets");
    Table *round_bonus = ttab(G->GAME, "round_bonus");
    tsetn(current_round, "discards_left", lmax(0, tnum(round_resets, "discards") + tnum(round_bonus, "discards")));
    tsetn(current_round, "hands_left", lmax(1, tnum(round_resets, "hands") + tnum(round_bonus, "next_hands")));
    tsetn(current_round, "hands_played", 0);
    tsetn(current_round, "discards_used", 0);
    tsetn(current_round, "reroll_cost_increase", 0);
    tsett(current_round, "used_packs", tnew());

    // note: pairs order does not matter in these two loops (plain resets)
    for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) {
        tsetn(it.val.tab, "played_this_round", 0);
    }

    for (TIter it = titer(G->playing_cards); tnext(&it);) {
        tset(((Card *)it.val.obj)->ability, "wheel_flipped", NIL);
    }

    Table *chaos = find_joker("Chaos the Clown", false);
    tsetn(current_round, "free_rerolls", tlen(chaos));
    calculate_reroll_cost(true);

    tsetn(round_bonus, "next_hands", 0);
    tsetn(round_bonus, "discards", 0);

    const char *blhash = "";
    if (ttab(round_resets, "blind") == ttab(G->P_BLINDS, "bl_small")) {
        tsets(ttab(round_resets, "blind_states"), "Small", "Current");
        tsetn(G->GAME, "current_boss_streak", 0);
        blhash = "S";
    } else if (ttab(round_resets, "blind") == ttab(G->P_BLINDS, "bl_big")) {
        tsets(ttab(round_resets, "blind_states"), "Big", "Current");
        tsetn(G->GAME, "current_boss_streak", 0);
        blhash = "B";
    } else {
        tsets(ttab(round_resets, "blind_states"), "Boss", "Current");
        blhash = "L";
    }
    tsets(G->GAME, "subhash", fmt("%s%s", vstr(tget(round_resets, "ante")), blhash));

    Blind_set_blind(tobj(G->GAME, "blind"), ttab(round_resets, "blind"), false, false);

    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("setting_blind", B(true), "blind", T(ttab(round_resets, "blind"))));
    }
    delay(0.4, NULL);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_new_round_draw}), NULL, false);
    return true;
}

// state_events.lua:290
void new_round(void) {
    G->RESET_JIGGLES = false;
    delay(0.4, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_new_round}), NULL, false);
}

// ---------------------------------------------------------------- G.FUNCS

// state_events.lua:355
bool FUNCS_draw_from_deck_to_hand(Val e) {
    if (!(G->STATE == STATES_TAROT_PACK || G->STATE == STATES_SPECTRAL_PACK) &&
        tnum(G->hand->moveable.node.config, "card_limit") <= 0 && CardArea_n(G->hand) == 0) {
        G->STATE = STATES_GAME_OVER;
        G->STATE_COMPLETE = false;
        return true;
    }

    num hand_space =
        truthy(e) ? vnum(e) : lmin(CardArea_n(G->deck), tnum(G->hand->moveable.node.config, "card_limit") - CardArea_n(G->hand));
    Blind *blind = tobj(G->GAME, "blind");
    Table *current_round = ttab(G->GAME, "current_round");
    if (streq(blind->name, "The Serpent") && !blind->disabled &&
        (tnum(current_round, "hands_played") > 0 || tnum(current_round, "discards_used") > 0)) {
        hand_space = lmin(CardArea_n(G->deck), 3);
    }
    delay(0.3, NULL);
    for (num i = 1; i <= hand_space; i++) { // draw cards from deckL
        if (G->STATE == STATES_TAROT_PACK || G->STATE == STATES_SPECTRAL_PACK) {
            draw_card(G->deck, G->hand, i * 100 / hand_space, "up", true, NULL, -1, false, false, -1, false);
        } else {
            draw_card(G->deck, G->hand, i * 100 / hand_space, "up", true, NULL, -1, false, false, -1, false);
        }
    }
    return false;
}

// state_events.lua:441
static bool ev_discard_state_complete(void *ctx) {
    G->STATE_COMPLETE = false;
    return true;
}

// state_events.lua:379
void FUNCS_discard_cards_from_highlighted(UIElement *e, bool hook) {
    stop_use();
    G->CONTROLLER->interrupt.focus = true;
    Controller_save_cardarea_focus(G->CONTROLLER, "hand");

    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        tset(v->ability, "forced_selection", NIL);
    }

    Node *target = G->CONTROLLER->focused.target;
    if (target) {
        // note: target.area / target.rank are Card members; on any other node they would be dyn fields
        if (is_a(target, &Card_class)) {
            Card *tc = (Card *)target;
            if (tc->area == G->hand) G->card_area_focus_reset = TAB("area", O(G->hand), "rank", tc->rank ? N(tc->rank) : NIL);
        } else if (tobj(target->object.dyn, "area") == (void *)G->hand) {
            G->card_area_focus_reset = TAB("area", O(G->hand), "rank", tget(target->object.dyn, "rank"));
        }
    }
    num highlighted_count =
        lmin(CardArea_n_highlighted(G->hand), tnum(G->discard->moveable.node.config, "card_limit") - CardArea_n(G->play));
    if (highlighted_count > 0) {
        update_hand_text(TAB("immediate", B(true), "nopulse", B(true), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "level", S(""), "handname", S("")));
        tsort(G->hand->highlighted, less_T_x, NULL);
        inc_career_stat("c_cards_discarded", highlighted_count);
        for (int j = 1, n = CardArea_n(G->jokers); j <= n; j++) {
            Table *context = TAB("pre_discard", B(true), "full_hand", T(G->hand->highlighted));
            if (hook) tsetb(context, "hook", true); // state_events.h: the key only when hook is true
            Card_calculate_joker(CardArea_card(G->jokers, j), context);
        }
        Table *cards = tnew();
        Table *destroyed_cards = tnew();
        for (num i = 1; i <= highlighted_count; i++) {
            Card_calculate_seal(CardArea_highlighted(G->hand, (int)i), TAB("discard", B(true)));
            bool removed = false;
            for (int j = 1, n = CardArea_n(G->jokers); j <= n; j++) {
                Val eval = NIL;
                eval = Card_calculate_joker(CardArea_card(G->jokers, j),
                                            TAB("discard", B(true), "other_card", O(CardArea_highlighted(G->hand, (int)i)), "full_hand",
                                                T(G->hand->highlighted)));
                if (truthy(eval)) {
                    if (tbool(vtab(eval), "remove")) removed = true;
                    card_eval_status_text(CardArea_card(G->jokers, j), "jokers", 0, 1, NULL, vtab(eval));
                }
            }
            tinsert(cards, O(CardArea_highlighted(G->hand, (int)i)));
            if (removed) {
                tseti(destroyed_cards, tlen(destroyed_cards) + 1, O(CardArea_highlighted(G->hand, (int)i)));
                if (streq(tstr(CardArea_highlighted(G->hand, (int)i)->ability, "name"), "Glass Card")) {
                    Card_shatter(CardArea_highlighted(G->hand, (int)i));
                } else {
                    Card_start_dissolve(CardArea_highlighted(G->hand, (int)i), NULL, false, -1, false);
                }
            } else {
                tsetb(CardArea_highlighted(G->hand, (int)i)->ability, "discarded", true);
                draw_card(G->hand, G->discard, i * 100 / highlighted_count, "down", false, CardArea_highlighted(G->hand, (int)i), -1,
                          false, false, -1, false);
            }
        }

        if (truthy(tgeti(destroyed_cards, 1))) {
            for (int j = 1, n = CardArea_n(G->jokers); j <= n; j++) {
                eval_card(CardArea_card(G->jokers, j),
                          TAB("cardarea", O(G->jokers), "remove_playing_cards", B(true), "removed", T(destroyed_cards)));
            }
        }

        Table *cards_discarded = ttab(ttab(G->GAME, "round_scores"), "cards_discarded");
        tsetn(cards_discarded, "amt", tnum(cards_discarded, "amt") + tlen(cards));
        check_for_unlock(TAB("type", S("discard_custom"), "cards", T(cards)));
        if (!hook) {
            Val discard_cost = tget(ttab(G->GAME, "modifiers"), "discard_cost");
            if (truthy(discard_cost)) {
                ease_dollars(-vnum(discard_cost), false);
            }
            ease_discard(-1, false, false);
            Table *current_round = ttab(G->GAME, "current_round");
            tsetn(current_round, "discards_used", tnum(current_round, "discards_used") + 1);
            G->STATE = STATES_DRAW_TO_HAND;
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_discard_state_complete}), NULL, false);
        }
    }
}

// state_events.lua:467
static bool ev_play_hand_played(void *ctx) {
    G->STATE = STATES_HAND_PLAYED;
    G->STATE_COMPLETE = true;
    return true;
}

// state_events.lua:495
static bool ev_play_blind_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}

// state_events.lua:491
static bool ev_play_blind_juice(void *ctx) {
    UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_1"), NULL), 0.3f, N(0));
    UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_2"), NULL), 0.3f, N(0));
    Blind_juice_up(tobj(G->GAME, "blind"), -1, NIL);
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after",
                               .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"),
                               .not_blockable = true,
                               .not_blocking = true,
                               .func = ev_play_blind_sound}),
          NULL, false);
    play_sound("tarot2", 1, 0.4f);
    return true;
}

// state_events.lua:511
static bool ev_play_evaluate(void *ctx) {
    FUNCS_evaluate_play(NULL);
    return true;
}

// state_events.lua:520
static bool ev_play_to_discard(void *ctx) {
    check_for_unlock(TAB("type", S("play_all_hearts")));
    FUNCS_draw_from_play_to_discard(NULL);
    tsetn(G->GAME, "hands_played", tnum(G->GAME, "hands_played") + 1);
    Table *current_round = ttab(G->GAME, "current_round");
    tsetn(current_round, "hands_played", tnum(current_round, "hands_played") + 1);
    return true;
}

// state_events.lua:530
static bool ev_play_state_complete(void *ctx) {
    G->STATE_COMPLETE = false;
    return true;
}

// state_events.lua:506
static bool ev_play_score(void *ctx) {
    check_for_unlock(TAB("type", S("hand_contents"), "cards", T(G->play->cards)));

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_play_evaluate}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_play_to_discard}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_play_state_complete}), NULL, false);
    return true;
}

// state_events.lua:450
void FUNCS_play_cards_from_highlighted(UIElement *e) {
    if (G->play && truthy(tgeti(G->play->cards, 1))) return;
    // check the hand first

    stop_use();
    Blind *blind = tobj(G->GAME, "blind");
    blind->triggered = false;
    G->CONTROLLER->interrupt.focus = true;
    Controller_save_cardarea_focus(G->CONTROLLER, "hand");

    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        tset(v->ability, "forced_selection", NIL);
    }

    tsort(G->hand->highlighted, less_T_x, NULL);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_play_hand_played}), NULL, false);
    inc_career_stat("c_cards_played", CardArea_n_highlighted(G->hand));
    inc_career_stat("c_hands_played", 1);
    ease_hands_played(-1, false);
    delay(0.4, NULL);

    for (int i = 1, n = CardArea_n_highlighted(G->hand); i <= n; i++) {
        if (Card_is_face(CardArea_highlighted(G->hand, i), false)) inc_career_stat("c_face_cards_played", 1);
        Table *base = CardArea_highlighted(G->hand, i)->base;
        tsetn(base, "times_played", tnum(base, "times_played") + 1);
        tsetb(CardArea_highlighted(G->hand, i)->ability, "played_this_ante", true);
        Table *cards_played = ttab(ttab(G->GAME, "round_scores"), "cards_played");
        tsetn(cards_played, "amt", tnum(cards_played, "amt") + 1);
        draw_card(G->hand, G->play, i * 100.0 / CardArea_n_highlighted(G->hand), "up", false, CardArea_highlighted(G->hand, i), -1,
                  false, false, -1, false);
    }

    check_for_unlock(TAB("type", S("run_card_replays")));

    if (Blind_press_play(blind)) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_play_blind_juice}), NULL, false);
        delay(0.4, NULL);
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_play_score}), NULL, false);
}

// state_events.lua:540
const char *FUNCS_get_poker_hand_info(Table *_cards, const char **loc_disp_text_out, Table **poker_hands_out, Table **scoring_hand_out,
                                      const char **disp_text_out) {
    Table *poker_hands = evaluate_poker_hand(_cards);
    Table *scoring_hand = tnew();
    const char *text = "NULL", *disp_text = "NULL", *loc_disp_text = "NULL";
    static const char *const order[12] = {"Flush Five", "Flush House",     "Five of a Kind", "Straight Flush",
                                          "Four of a Kind", "Full House",  "Flush",          "Straight",
                                          "Three of a Kind", "Two Pair",   "Pair",           "High Card"};
    // the Lua's if / elseif chain (:544-555), in its order
    for (int k = 0; k < 12; k++) {
        if (!tempty(ttab(poker_hands, order[k]))) {
            text = order[k];
            scoring_hand = ttabi(ttab(poker_hands, order[k]), 1);
            break;
        }
    }

    disp_text = text;
    if (streq(text, "Straight Flush")) {
        num min = 10;
        for (int j = 1, n = tlen(scoring_hand); j <= n; j++) {
            if (Card_get_id(tobji(scoring_hand, j)) < min) min = Card_get_id(tobji(scoring_hand, j));
        }
        if (min >= 10) {
            disp_text = "Royal Flush";
        }
    }
    loc_disp_text = localize_s(disp_text, "poker_hands");
    if (loc_disp_text_out) *loc_disp_text_out = loc_disp_text;
    if (poker_hands_out) *poker_hands_out = poker_hands;
    if (scoring_hand_out) *scoring_hand_out = scoring_hand;
    if (disp_text_out) *disp_text_out = disp_text;
    return text;
}
