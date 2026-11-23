// functions/common_events.lua:1163-1771: check_for_unlock, unlock_card, fetch_achievements, unlock_achievement.
#include "all.h"

// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }
// G.PROFILES[G.SETTINGS.profile].career_stats[statname] (statname as the Val the Lua indexes with)
static num career_stat(Val statname) { return vnum(tgetv(ttab(cur_profile(), "career_stats"), statname)); }
// t.tally / t.of of a G.PROGRESS / G.DISCOVER_TALLIES entry
static num tally_ratio(Table *t, const char *key) { return tnum(ttab(t, key), "tally") / tnum(ttab(t, key), "of"); }

// common_events.lua:1611 (inner event of the chip_score unlock)
static bool ev_chip_score_unlock_2(void *ctx) {
    unlock_card((Table *)ctx);
    return true;
}
// common_events.lua:1609
static bool ev_chip_score_unlock_1(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_chip_score_unlock_2, .ctx = ctx}), NULL, false);
    return true;
}

// common_events.lua:1163
void check_for_unlock(Table *args) {
    if (tempty(args)) return;
    if (tbool(G->GAME, "seeded")) return;
    const char *type = tstr(args, "type");
    Val vtype = tget(args, "type");
    if (streq(type, "win_challenge")) {
        unlock_achievement("rule_bender");
        bool _c = true;
        for (TIter it = titer(G->CHALLENGES); tnext(&it);) {
            Table *v = it.val.tab;
            if (!truthy(tgetv(ttab(ttab(cur_profile(), "challenge_progress"), "completed"), tget(v, "id")))) {
                _c = false;
            }
        }
        if (_c) {
            unlock_achievement("rule_breaker");
        }
    }
    if (truthy(tget(G->GAME, "challenge"))) return;

    // ---- Achievements
    if (streq(type, "career_stat")) {
        Val statname = tget(args, "statname");
        if (streq(tstr(args, "statname"), "c_cards_played") && career_stat(statname) >= 2500) {
            unlock_achievement("card_player");
        }
        if (streq(tstr(args, "statname"), "c_cards_discarded") && career_stat(statname) >= 2500) {
            unlock_achievement("card_discarder");
        }
    }
    if (streq(type, "ante_up")) {
        if (tnum(args, "ante") >= 4) {
            unlock_achievement("ante_up");
        }
        if (tnum(args, "ante") >= 8) {
            unlock_achievement("ante_upper");
        }
    }
    if (streq(type, "win")) {
        unlock_achievement("heads_up");
        if (tnum(G->GAME, "round") <= 12) {
            unlock_achievement("speedrunner");
        }
        if (vnum(tpath(G->GAME, "round_scores.times_rerolled.amt")) <= 0) {
            unlock_achievement("you_get_what_you_get");
        }
    }
    if (streq(type, "win_stake")) {
        num lowest_win = 0;
        num highest_win = get_deck_win_stake(NULL, &lowest_win);
        if (highest_win >= 2) {
            unlock_achievement("low_stakes");
        }
        if (highest_win >= 4) {
            unlock_achievement("mid_stakes");
        }
        if (highest_win >= 8) {
            unlock_achievement("high_stakes");
        }
        if (G->PROGRESS && tally_ratio(G->PROGRESS, "deck_stakes") >= 1) {
            unlock_achievement("completionist_plus");
        }
        if (G->PROGRESS && tally_ratio(G->PROGRESS, "joker_stickers") >= 1) {
            unlock_achievement("completionist_plus_plus");
        }
    }
    if (streq(type, "money")) {
        if (tnum(G->GAME, "dollars") >= 400) {
            unlock_achievement("nest_egg");
        }
    }
    if (streq(type, "hand")) {
        if (streq(tstr(args, "handname"), "Flush") && truthy(tget(args, "scoring_hand"))) {
            Table *scoring_hand = ttab(args, "scoring_hand");
            int _w = 0;
            for (int k = 1; truthy(tgeti(scoring_hand, k)); k++) {
                Card *v = tobji(scoring_hand, k);
                if (streq(tstr(v->ability, "name"), "Wild Card")) {
                    _w = _w + 1;
                }
            }
            if (_w == tlen(scoring_hand)) {
                unlock_achievement("flushed");
            }
        }

        if (streq(tstr(args, "disp_text"), "Royal Flush")) {
            unlock_achievement("royale");
        }
    }
    if (streq(type, "shatter")) {
        if (tlen(ttab(args, "shattered")) >= 2) {
            unlock_achievement("shattered");
        }
    }
    if (streq(type, "run_redeem")) {
        num _v = 0;
        Val svc = tget(G->GAME, "starting_voucher_count");
        _v = _v - (truthy(svc) ? vnum(svc) : 0);
        for (TIter it = titer(ttab(G->GAME, "used_vouchers")); tnext(&it);) {
            _v = _v + 1;
        }
        if (_v >= 5 && vnum(tpath(G->GAME, "round_resets.ante")) <= 4) {
            unlock_achievement("roi");
        }
    }
    if (streq(type, "upgrade_hand")) {
        if (tnum(args, "level") >= 10) {
            unlock_achievement("retrograde");
        }
    }
    if (streq(type, "chip_score")) {
        if (tnum(args, "chips") >= 10000) {
            unlock_achievement("_10k");
        }
        if (tnum(args, "chips") >= 1000000) {
            unlock_achievement("_1000k");
        }
        if (tnum(args, "chips") >= 100000000) {
            unlock_achievement("_100000k");
        }
    }
    if (streq(type, "modify_deck")) {
        if (G->deck && tnum(G->deck->moveable.node.config, "card_limit") <= 20) {
            unlock_achievement("tiny_hands");
        }
        if (G->deck && tnum(G->deck->moveable.node.config, "card_limit") >= 80) {
            unlock_achievement("big_hands");
        }
    }
    if (streq(type, "spawn_legendary")) {
        unlock_achievement("legendary");
    }
    if (streq(type, "discover_amount")) {
        if (tally_ratio(G->DISCOVER_TALLIES, "vouchers") >= 1) {
            unlock_achievement("extreme_couponer");
        }
        if (tally_ratio(G->DISCOVER_TALLIES, "spectrals") >= 1) {
            unlock_achievement("clairvoyance");
        }
        if (tally_ratio(G->DISCOVER_TALLIES, "tarots") >= 1) {
            unlock_achievement("cartomancy");
        }
        if (tally_ratio(G->DISCOVER_TALLIES, "planets") >= 1) {
            unlock_achievement("astronomy");
        }
        if (tally_ratio(G->DISCOVER_TALLIES, "total") >= 1) {
            unlock_achievement("completionist");
        }
    }
    // ----

    int i = 1;
    while (i <= tlen(G->P_LOCKED)) {
        bool ret = false;
        Table *card = ttabi(G->P_LOCKED, i);
        Table *uc = ttab(card, "unlock_condition"); // card.unlock_condition
        Val extra = tget(uc, "extra");              // a number, a string or a table, by condition
        Table *extra_t = extra.t == V_TAB ? extra.tab : NULL;
        const char *name = tstr(card, "name");
        if (!tbool(card, "unlocked") && uc && streq(type, "career_stat")) {
            if (veq(tget(args, "statname"), tget(uc, "type")) && career_stat(tget(args, "statname")) >= vnum(extra)) {
                ret = true;
                unlock_card(card);
            }
        }

        if (!tbool(card, "unlocked") && uc && veq(tget(uc, "type"), vtype)) {
            if (streq(type, "hand") && veq(tget(args, "handname"), extra)) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "min_hand_size") && G->hand && tnum(G->hand->moveable.node.config, "card_limit") <= vnum(extra)) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "interest_streak") &&
                vnum(extra) <= tnum(ttab(cur_profile(), "career_stats"), "c_round_interest_cap_streak")) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "run_card_replays")) {
                for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
                    Card *v = tobji(G->playing_cards, k);
                    if (tnum(v->base, "times_played") >= vnum(extra)) {
                        ret = true;
                        unlock_card(card);
                        break;
                    }
                }
            }
            if (streq(type, "play_all_hearts")) {
                bool played = true;
                for (int k = 1; truthy(tgeti(G->deck->cards, k)); k++) {
                    Card *v = tobji(G->deck->cards, k);
                    if (!streq(tstr(v->ability, "name"), "Stone Card") && streq(tstr(v->base, "suit"), "Hearts")) {
                        played = false;
                    }
                }
                for (int k = 1; truthy(tgeti(G->hand->cards, k)); k++) {
                    Card *v = tobji(G->hand->cards, k);
                    if (!streq(tstr(v->ability, "name"), "Stone Card") && streq(tstr(v->base, "suit"), "Hearts")) {
                        played = false;
                    }
                }
                if (played) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "run_redeem")) {
                num vouchers_redeemed = 0;
                for (TIter it = titer(ttab(G->GAME, "used_vouchers")); tnext(&it);) {
                    vouchers_redeemed = vouchers_redeemed + 1;
                }
                if (vouchers_redeemed >= vnum(extra)) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "have_edition")) {
                num shiny_jokers = 0;
                for (int k = 1; truthy(tgeti(G->jokers->cards, k)); k++) {
                    Card *v = tobji(G->jokers->cards, k);
                    if (v->edition) shiny_jokers = shiny_jokers + 1;
                }
                if (shiny_jokers >= vnum(extra)) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "double_gold")) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "continue_game")) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "blank_redeems")) {
                // ret is not set here, as in the Lua: the entry stays in G.P_LOCKED
                Table *v_blank = ttab(ttab(cur_profile(), "voucher_usage"), "v_blank");
                if (v_blank && tnum(v_blank, "count") >= vnum(extra)) {
                    unlock_card(card);
                }
            }
            if (streq(type, "modify_deck")) {
                if (truthy(extra) && truthy(tget(extra_t, "suit"))) {
                    num count = 0;
                    for (TIter it = titer(G->playing_cards); tnext(&it);) {
                        Card *v = it.val.obj;
                        if (veq(tget(v->base, "suit"), tget(extra_t, "suit"))) count = count + 1;
                    }
                    if (count >= tnum(extra_t, "count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (truthy(extra) && truthy(tget(extra_t, "enhancement"))) {
                    num count = 0;
                    for (TIter it = titer(G->playing_cards); tnext(&it);) {
                        Card *v = it.val.obj;
                        if (veq(tget(v->ability, "name"), tget(extra_t, "enhancement"))) count = count + 1;
                    }
                    if (count >= tnum(extra_t, "count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (truthy(extra) && truthy(tget(extra_t, "tally"))) {
                    num count = 0;
                    for (TIter it = titer(G->playing_cards); tnext(&it);) {
                        Card *v = it.val.obj;
                        if (streq(tstr(v->ability, "set"), "Enhanced")) count = count + 1;
                    }
                    if (count >= tnum(extra_t, "count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "discover_amount")) {
                if (truthy(tget(uc, "amount"))) {
                    if (tnum(uc, "amount") <= tnum(args, "amount")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (truthy(tget(uc, "tarot_count"))) {
                    if (tnum(uc, "tarot_count") <= tnum(args, "tarot_count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (truthy(tget(uc, "planet_count"))) {
                    if (tnum(uc, "planet_count") <= tnum(args, "planet_count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "win_deck")) {
                if (truthy(tget(uc, "deck"))) {
                    if (get_deck_win_stake(tstr(uc, "deck"), NULL) > 0) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "win_stake")) {
                if (truthy(tget(uc, "stake"))) {
                    if (get_deck_win_stake(NULL, NULL) >= tnum(uc, "stake")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "discover_planets")) {
                num count = 0;
                for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
                    Table *v = it.val.tab;
                    if (streq(tstr(v, "set"), "Planet") && tbool(v, "discovered")) count = count + 1;
                }
                if (count >= 9) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "blind_discoveries")) {
                num discovered_blinds = 0;
                for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
                    Table *v = it.val.tab;
                    if (tbool(v, "discovered")) {
                        discovered_blinds = discovered_blinds + 1;
                    }
                }
                if (discovered_blinds >= vnum(extra)) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "modify_jokers") && G->jokers) {
                if (truthy(tget(extra_t, "count"))) {
                    num count = 0;
                    for (TIter it = titer(G->jokers->cards); tnext(&it);) {
                        Card *v = it.val.obj;
                        if (streq(tstr(v->ability, "set"), "Joker") && v->edition && tbool(v->edition, "polychrome") &&
                            tbool(extra_t, "polychrome"))
                            count = count + 1;
                    }
                    if (count >= tnum(extra_t, "count")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "money")) {
                if (vnum(extra) <= tnum(G->GAME, "dollars")) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "round_win")) {
                Blind *blind = tobj(G->GAME, "blind");
                if (streq(name, "Matador")) {
                    if (vnum(tpath(G->GAME, "current_round.hands_played")) == 1 &&
                        veq(tpath(G->GAME, "current_round.discards_left"), tpath(G->GAME, "round_resets.discards")) &&
                        streq(Blind_get_type(blind), "Boss")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (streq(name, "Troubadour")) {
                    if (tnum(ttab(cur_profile(), "career_stats"), "c_single_hand_round_streak") >= vnum(extra)) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (streq(name, "Hanging Chad")) {
                    if (veq(tget(G->GAME, "last_hand_played"), extra) && streq(Blind_get_type(blind), "Boss")) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "ante_up")) {
                if (truthy(tget(uc, "ante"))) {
                    if (veq(tget(args, "ante"), tget(uc, "ante"))) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "hand_contents")) {
                Table *cards = ttab(args, "cards");
                if (streq(name, "Seeing Double")) {
                    num tally = 0;
                    for (int j = 1; j <= tlen(cards); j++) {
                        Card *c = tobji(cards, j);
                        if (Card_get_id(c) == 7 && Card_is_suit(c, "Clubs", false, false)) {
                            tally = tally + 1;
                        }
                    }
                    if (tally >= 4) {
                        ret = true;
                        unlock_card(card);
                    }
                }

                if (streq(name, "Golden Ticket")) {
                    num tally = 0;
                    for (int j = 1; j <= tlen(cards); j++) {
                        Card *c = tobji(cards, j);
                        if (streq(tstr(c->ability, "name"), "Gold Card")) {
                            tally = tally + 1;
                        }
                    }
                    if (tally >= 5) {
                        ret = true;
                        unlock_card(card);
                    }
                }
            }
            if (streq(type, "discard_custom")) {
                Table *cards = ttab(args, "cards");
                if (streq(name, "Hit the Road")) {
                    num tally = 0;
                    for (int j = 1; j <= tlen(cards); j++) {
                        if (Card_get_id(tobji(cards, j)) == 11) {
                            tally = tally + 1;
                        }
                    }
                    if (tally >= 5) {
                        ret = true;
                        unlock_card(card);
                    }
                }
                if (streq(name, "Brainstorm")) {
                    Table *eval = evaluate_poker_hand(cards);
                    if (!tempty(ttab(eval, "Straight Flush"))) {
                        num min = 10;
                        for (int j = 1; j <= tlen(cards); j++) {
                            // two get_id calls, as the Lua (a Stone Card's id is a new random number each call)
                            if (Card_get_id(tobji(cards, j)) < min) min = Card_get_id(tobji(cards, j));
                        }
                        if (min == 10) {
                            ret = true;
                            unlock_card(card);
                        }
                    }
                }
            }
            Val no_hand = streq(type, "win_no_hand") ? tgetv(ttab(G->GAME, "hands"), extra) : NIL;
            if (streq(type, "win_no_hand") && no_hand.t != V_TAB) ERROR("common_events.lua:1584: G.GAME.hands[extra] is nil");
            if (streq(type, "win_no_hand") && tnum(no_hand.tab, "played") == 0) {
                ret = true;
                unlock_card(card);
            }
            if (streq(type, "win_custom")) {
                if (streq(name, "Invisible Joker") && tnum(G->GAME, "max_jokers") <= 4) {
                    ret = true;
                    unlock_card(card);
                }
                if (streq(name, "Blueprint")) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "win")) {
                if (tnum(uc, "n_rounds") >= tnum(G->GAME, "round")) {
                    ret = true;
                    unlock_card(card);
                }
            }
            if (streq(type, "chip_score")) {
                if (tnum(uc, "chips") <= tnum(args, "chips")) {
                    ret = true;
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_chip_score_unlock_1, .ctx = card}), NULL, false);
                }
            }
        }
        if (ret == true) {
            tremove(G->P_LOCKED, i);
        } else {
            i = i + 1;
        }
    }
}

// common_events.lua:1635 the comparator of the Back pool's sort
static bool back_pool_less(Val a, Val b, void *ctx) {
    return (tnum(a.tab, "order") - (tbool(a.tab, "unlocked") ? 100 : 0)) < (tnum(b.tab, "order") - (tbool(b.tab, "unlocked") ? 100 : 0));
}

// common_events.lua:1628
void unlock_card(Table *card) {
    Val unlocked = tget(card, "unlocked");
    if (unlocked.t == V_BOOL && !unlocked.b) { // == false exactly: nil is not locked
        if (tbool(G->GAME, "seeded") || truthy(tget(G->GAME, "challenge"))) return;
        if (tbool(card, "unlocked") || tbool(card, "wip")) return;
        Game_save_notify(G, card);
        tsetb(card, "unlocked", true);
        if (streq(tstr(card, "set"), "Back")) discover_card(card);
        tsort(ttab(G->P_CENTER_POOLS, "Back"), back_pool_less, NULL);
        Game_save_progress(G);
        tsetb(G->FILE_HANDLER, "force", true);
        notify_alert(tstr(card, "key"), tstr(card, "set"));
    }
}

// common_events.lua:1642
void fetch_achievements(void) {
    if (!G->ACHIEVEMENTS) {
        // common_events.lua:1644-1674: order = position, steamid = "BAL_<order>"; inserted in the order of the source
        static const struct { const char *name; int tier; } A[] = {
            {"ante_up", 3},      {"ante_upper", 3},           {"heads_up", 2},        {"low_stakes", 2},
            {"mid_stakes", 2},   {"high_stakes", 2},          {"card_player", 3},     {"card_discarder", 3},
            {"nest_egg", 2},     {"flushed", 3},              {"speedrunner", 2},     {"roi", 3},
            {"shattered", 3},    {"royale", 3},               {"retrograde", 2},      {"_10k", 3},
            {"_1000k", 2},       {"_100000k", 1},             {"tiny_hands", 2},      {"big_hands", 2},
            {"you_get_what_you_get", 3}, {"rule_bender", 3},  {"rule_breaker", 1},    {"legendary", 3},
            {"astronomy", 3},    {"cartomancy", 3},           {"clairvoyance", 2},    {"extreme_couponer", 1},
            {"completionist", 1}, {"completionist_plus", 1},  {"completionist_plus_plus", 1},
        };
        G->ACHIEVEMENTS = tnew();
        for (int i = 0; i < (int)(sizeof A / sizeof A[0]); i++) {
            tsett(G->ACHIEVEMENTS, A[i].name,
                  TAB("order", N(i + 1), "tier", N(A[i].tier), "earned", B(false), "steamid", S(fmt("BAL_%02d", i + 1))));
        }
    }

    if (G->F_NO_ACHIEVEMENTS) return;

    // from the local settings file (G.STEAM is nil)
    if (!ttab(G->SETTINGS, "ACHIEVEMENTS_EARNED")) tsett(G->SETTINGS, "ACHIEVEMENTS_EARNED", tnew());
    for (TIter it = titer(ttab(G->SETTINGS, "ACHIEVEMENTS_EARNED")); tnext(&it);) {
        Val a = tgetv(G->ACHIEVEMENTS, it.key);
        if (truthy(a)) {
            tsetb(a.tab, "earned", true);
        }
    }
    // common_events.lua:1693-1702: Steam achievements, not translated
}

// common_events.lua:1717. ctx: achievement_name (an interned string)
static bool ev_unlock_achievement(void *ctx) {
    const char *achievement_name = ctx;
    if (G->STATE != STATES_HAND_PLAYED) {
        if (tbool(cur_profile(), "all_unlocked")) return false; // the Lua returns nil: the event stays queued
        bool achievement_set = false;
        if (G->F_NO_ACHIEVEMENTS) return false; // the same

        if (!G->ACHIEVEMENTS) fetch_achievements();

        tsetb(ttab(G->SETTINGS, "ACHIEVEMENTS_EARNED"), achievement_name, true);
        Game_save_progress(G);
        Table *a = ttab(G->ACHIEVEMENTS, achievement_name);
        if (a) { // and not G.STEAM
            if (!tbool(a, "earned")) {
                // the first time this achievement is earned
                achievement_set = true;
                tsetb(G->FILE_HANDLER, "force", true);
            }
            tsetb(a, "earned", true);
        }
        // common_events.lua:1742-1757: Steam achievements, not translated

        if (achievement_set) notify_alert(achievement_name, NULL);
        return true;
    }
    return false;
}

// common_events.lua:1711
void unlock_achievement(const char *achievement_name) {
    if (tbool(cur_profile(), "all_unlocked")) return;
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.no_delete = true, .not_blockable = true, .not_blocking = true, .func = ev_unlock_achievement,
                               .ctx = (void *)str(achievement_name)}),
          "achievement", false);
}
