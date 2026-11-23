// functions/UI_definitions.lua, lines 2545-3182.
// note: where a list of the Lua holds several calls that create objects (DynaText, Sprite, events), the entries are
// built by statements in the Lua's order, because C does not fix the order in which arguments are evaluated.
#include "all.h"

// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }

// copy_table(colour): a new colour that belongs to the definition (never freed)
static float *colour_copy(const float *colour) {
    float *c = malloc(sizeof(Colour));
    memcpy(c, colour, sizeof(Colour));
    return c;
}

// math.floor(0.01+100*tally/of)
static const char *floor_percent(num tally, num of) { return number_tostring(floor(0.01 + 100 * tally / of)); }

// {n=G.UIT.O, config={object = DynaText({string = {string}, colours = {colour}, shadow = true, float = true, scale = scale})}}
static Val dyna_node(Val string, float *colour, num scale) {
    return T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(string)), "colours", T(ARR(P(colour))),
                                                                                "shadow", B(true), "float", B(true), "scale", N(scale))))))));
}

// G.ASSET_ATLAS["ui_"..(G.SETTINGS.colourblind_option and 2 or 1)]
static Atlas *ui_atlas(void) { return ASSET_ATLAS_get(tbool(G->SETTINGS, "colourblind_option") ? "ui_2" : "ui_1"); }

// {n=G.UIT.O, config={padding = 0, id = 'jimbo_spot', object = Moveable(0,0,G.CARD_W*1.1, G.CARD_H*1.1)}}
static Val jimbo_spot(void) {
    Moveable *m = NEW(Moveable);
    Moveable_init(m, 0, 0, G->CARD_W * 1.1f, G->CARD_H * 1.1f);
    return T(TAB("n", N(UIT_O), "config", T(TAB("padding", N(0), "id", S("jimbo_spot"), "object", O(m)))));
}

// The functions this chunk stores as values (UI_definitions.h: tab_definition_function gets ONE argument, its args;
// on_demand_tooltip.filler.func gets filler.args).
static Val tdf_view_deck(void *ctx, int argc, Val *argv) { return T(UIDEF_view_deck(argc > 0 && truthy(argv[0]))); }
static Val tdf_current_hands(void *ctx, int argc, Val *argv) { return T(create_UIBox_current_hands(argc > 0 && truthy(argv[0]))); }
static Val tdf_current_blinds(void *ctx, int argc, Val *argv) { return T(UIDEF_current_blinds()); }
static Val tdf_used_vouchers(void *ctx, int argc, Val *argv) { return T(UIDEF_used_vouchers()); }
static Val tdf_current_stake(void *ctx, int argc, Val *argv) { return T(UIDEF_current_stake()); }
static Val filler_hand_tip(void *ctx, int argc, Val *argv) { return T(create_UIBox_hand_tip(argc > 0 ? vstr(argv[0]) : NULL)); }

// UI_definitions.lua:2545
Table *UIDEF_custom_deck_tab(const char *_suit) {
    Table *t = tnew();

    CardArea *face_cards = NEW(CardArea);
    CardArea_init(face_cards, 0, 0, 4 * G->CARD_W, 1.4f * G->CARD_H, TAB("card_limit", N(3), "type", S("title"), "highlight_limit", N(0)));

    tinsert(t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.BLACK), "r", N(0.1), "padding", N(0.07), "no_fill", B(true))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(face_cards)))))
    )))));

    Val loc_options = localize(S(_suit), "collabs");
    Table *conv_loc_options = tnew();
    if (loc_options.t != V_TAB) ERROR("bad argument #1 to 'pairs' (table expected, got %s)", lua_typename_of(loc_options));
    for (TIter it = titer(loc_options.tab); tnext(&it);) { // :2562
        bool ok = false;
        num k = tonumber(it.key, &ok);
        if (!ok) ERROR("table index is nil");
        tsetv(conv_loc_options, N(k), it.val);
    }

    Val current_option = N(1);
    Val chosen = tget(ttab(ttab(G->SETTINGS, "CUSTOM_DECK"), "Collabs"), _suit);
    for (TIter it = titer(ttab(ttab(G->COLLABS, "options"), _suit)); tnext(&it);) { // :2569
        if (veq(chosen, it.val)) current_option = it.key;
    }

    Table *cycle = create_option_cycle(TAB("options", T(conv_loc_options), "w", N(5.5), "cycle_shoulders", B(true), "curr_suit", S(_suit),
                                           "opt_callback", S("change_collab"), "current_option", current_option, "colour", P(G->C.RED),
                                           "focus_args", T(TAB("snap_to", B(true), "nav", S("wide")))));
    tinsert(t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(T(cycle))))));
    tinsert(t, T(create_toggle(TAB("label", localize(S("b_high_contrast_cards"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("colourblind_option"),
                                   "callback", FN(FUNCS_refresh_contrast_mode_fn, NULL)))));

    const char *faces[3] = {"K", "Q", "J"};
    for (int i = 1; i <= 3; i++) {
        const char *card_code = fmt("%s_%s", str_sub(_suit, 1, 1), faces[i - 1]);
        Card *card = NEW(Card);
        Card_init(card, 0, 0, G->CARD_W * 1.2f, G->CARD_H * 1.2f, ttab(G->P_CARDS, card_code), ttab(G->P_CENTERS, "c_base"), NULL);
        card->no_ui = true;
        CardArea_emplace(face_cards, card, NULL, false);
    }

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0), "colour", P(G->C.CLEAR), "r", N(0.1), "minw", N(7), "minh", N(4.2))), "nodes", T(t));
}

// UI_definitions.lua:2591
Table *create_UIBox_high_scores(void) {
    fetch_achievements();
    set_profile_progress();
    set_discover_tallies();

    // the rows that are nil are holes of the list
    Table *scores = tnew();
    tseti(scores, 1, T(create_UIBox_high_scores_row("hand")));
    tseti(scores, 2, T(create_UIBox_high_scores_row("furthest_round")));
    tseti(scores, 3, T(create_UIBox_high_scores_row("furthest_ante")));
    tseti(scores, 4, T(create_UIBox_high_scores_row("poker_hand")));
    tseti(scores, 5, T(create_UIBox_high_scores_row("most_money")));
    tseti(scores, 6, T(create_UIBox_high_scores_row("win_streak")));
    G->focused_profile = (int)tnum(G->SETTINGS, "profile");
    Table *cheevs = tnew();

    Val col1 = T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(3), "padding", N(0.2), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(scores)))
    ))));
    Val progress_box = T(create_progress_box(NULL, false));
    Val usage_button = T(UIBox_button(TAB("button", S("usage"), "label", T(ARR(localize(S("k_card_stats"), NULL))), "minw", N(7.5), "minh", N(1),
                                          "focus_args", T(TAB("nav", S("wide"))))));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("options"), "snap_back", B(true), "contents", T(ARR(
        col1,
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            progress_box,
            usage_button
        )))),
        !G->F_NO_ACHIEVEMENTS ? T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(cheevs))) : NIL
    ))));

    return t;
}

// :2650-2660 one of the four identical `filling` lists, for the progress table `p` of its row
static Table *progress_filling(Table *p, num text_scale) {
    Val dyna = dyna_node(S(fmt("%s%%", floor_percent(tnum(p, "tally"), tnum(p, "of")))), G->C.WHITE, 0.55 * text_scale);
    return ARR(
        dyna,
        T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s/%s)", vstr(tget(p, "tally")), vstr(tget(p, "of")))), "scale", N(0.35), "colour", P(G->C.JOKER_GREY))))));
}

// UI_definitions.lua:2623
Table *create_progress_box(Table *_profile_progress, bool smaller) {
    Table *rows = tnew();
    const char *protos[4] = {"collection", "challenges", "joker_stickers", "deck_stake_wins"};
    if (!_profile_progress) _profile_progress = ttab(cur_profile(), "progress");
    Table *discovered = ttab(_profile_progress, "discovered"), *deck_stakes = ttab(_profile_progress, "deck_stakes");
    Table *joker_stickers = ttab(_profile_progress, "joker_stickers"), *challenges = ttab(_profile_progress, "challenges");

    num overall_tally = tnum(discovered, "tally") / tnum(discovered, "of") +
                        tnum(deck_stakes, "tally") / tnum(deck_stakes, "of") +
                        tnum(joker_stickers, "tally") / tnum(joker_stickers, "of") +
                        tnum(challenges, "tally") / tnum(challenges, "of");
    tsetn(_profile_progress, "overall_tally", overall_tally);
    tsetn(_profile_progress, "overall_of", 4);

    num text_scale = smaller ? 0.7 : 1;
    float *bar_colour = tbool(ttabi(G->PROFILES, G->focused_profile), "all_unlocked") ? G->C.RED : G->C.BLUE;

    for (int pi = 0; pi < 4; pi++) {
        const char *v = protos[pi];
        Table *tab = NULL;
        const char *val = NULL;
        Val max = NIL;
        if (streq(v, "collection")) {
            tab = discovered, val = "tally", max = tget(discovered, "of");
        } else if (streq(v, "deck_stake_wins")) {
            tab = deck_stakes, val = "tally", max = tget(deck_stakes, "of");
        } else if (streq(v, "joker_stickers")) {
            tab = joker_stickers, val = "tally", max = tget(joker_stickers, "of");
        } else if (streq(v, "challenges")) {
            tab = challenges, val = "tally", max = tget(challenges, "of");
        }
        // :2649 (false for another v, which protos never holds)
        Val filling = streq(v, "collection") ? T(progress_filling(discovered, text_scale)) :
                      streq(v, "deck_stake_wins") ? T(progress_filling(deck_stakes, text_scale)) :
                      streq(v, "joker_stickers") ? T(progress_filling(joker_stickers, text_scale)) :
                      streq(v, "challenges") ? T(progress_filling(challenges, text_scale)) : B(false);

        tseti(rows, tlen(rows) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(darken(G->C.JOKER_GREY, 0.1f, NULL)), "emboss", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "minw", N(3.5 * text_scale), "maxw", N(3.5 * text_scale))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S(fmt("k_%s", v)), NULL), "scale", N(0.5 * text_scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minh", N(smaller ? 0.5 : 0.8), "r", N(0.1), "minw", N(3.5 * text_scale), "colour", P(G->C.BLACK), "emboss", N(0.05),
                "progress_bar", T(TAB(
                    "max", max, "ref_table", T(tab), "ref_value", S(val), "empty_col", P(G->C.BLACK), "filled_col", P(adjust_alpha(bar_colour, 0.5f, NULL))
                )))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(3.5 * text_scale))), "nodes", filling))
            ))))
        )))));
    }

    Val overall_dyna = dyna_node(S(fmt("%s%%", floor_percent(tnum(_profile_progress, "overall_tally"), tnum(_profile_progress, "overall_of")))), G->C.WHITE, 0.55);
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "minw", N(3.5 * text_scale), "maxw", N(3.5 * text_scale))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_progress"), NULL), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minh", N(0.6), "r", N(0.1), "minw", N(3.5 * text_scale), "colour", P(G->C.BLACK), "emboss", N(0.05),
                "progress_bar", T(TAB(
                    "max", tget(_profile_progress, "overall_of"), "ref_table", T(_profile_progress), "ref_value", S("overall_tally"), "empty_col", P(G->C.BLACK), "filled_col", P(adjust_alpha(bar_colour, 0.5f, NULL))
                )))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(3.5 * text_scale))), "nodes", T(ARR(
                    overall_dyna
                ))))
            ))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(rows)))
    )));
}

// UI_definitions.lua:2694
Table *create_UIBox_high_scores_row(const char *score) {
    Table *high_scores = ttab(cur_profile(), "high_scores");
    Table *hs = ttab(high_scores, score);
    if (!truthy(tget(high_scores, score))) return NULL;
    num label_scale = 0.65 - 0.025 * lmax(str_len(tstr(hs, "label")) - 8, 0);
    num label_w = 3.5, score_w = 4, h = 0.8;
    Table *score_tab = tnew();
    Val amt = tget(hs, "amt");

    if (streq(score, "poker_hand")) {
        Val handname = localize(S("k_none"), NULL);
        num amount = 0;
        // note: pairs(hand_usage): insertion order here; the Lua's hash order decides ties
        for (TIter it = titer(ttab(cur_profile(), "hand_usage")); tnext(&it);) {
            Table *v = it.val.tab;
            if (tnum(v, "count") > amount) { handname = tget(v, "order"); amount = tnum(v, "count"); }
        }
        Val dyna = dyna_node((amount < 1 && truthy(handname)) ? handname : localize(handname, "poker_hands"), G->C.WHITE, 0.55);
        score_tab = ARR(
            dyna,
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s)", number_tostring(amount))), "scale", N(0.45), "colour", P(G->C.JOKER_GREY))))));
    } else if (streq(score, "most_money")) {
        score_tab = ARR(
            dyna_node(S(fmt("%s%s", vstr(localize(S("$"), NULL)), number_format(amt))), G->C.MONEY, score_number_scale(0.85f, amt)));
    } else if (streq(score, "win_streak")) {
        Val dyna = dyna_node(S(number_format(amt)), G->C.WHITE, score_number_scale(0.85f, amt));
        score_tab = ARR(
            dyna,
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s)", vstr(tget(ttab(high_scores, "current_streak"), "amt")))), "scale", N(0.45), "colour", P(G->C.JOKER_GREY))))));
    } else if (streq(score, "hand")) {
        Sprite *chip_sprite = NEW(Sprite);
        Sprite_init(chip_sprite, 0, 0, 0.4f, 0.4f, ui_atlas(), TAB("x", N(0), "y", N(0)));
        chip_sprite->moveable.node.states.drag.can = false;
        Val dyna = dyna_node(S(number_format(amt)), G->C.RED, lmin(0.75, score_number_scale(1.5f, amt)));
        score_tab = ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("w", N(0.4), "h", N(0.4), "object", O(chip_sprite)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                dyna
            )))));
    } else if (streq(score, "collection")) {
        Val tot = tget(hs, "tot");
        Val dyna = dyna_node(S(fmt("%%%s", floor_percent(vnum(amt), vnum(tot)))), G->C.WHITE, lmin(0.75, score_number_scale(1.5f, amt)));
        score_tab = ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                dyna,
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s/%s)", vstr(amt), vstr(tot))), "scale", N(0.45), "colour", P(G->C.JOKER_GREY)))))
            )))));
    } else {
        score_tab = ARR(
            dyna_node(S(number_format(amt)), G->C.FILTER, score_number_scale(0.85f, amt)));
    }
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(darken(G->C.JOKER_GREY, 0.1f, NULL)), "emboss", N(0.05))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "minw", N(label_w), "maxw", N(label_w))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S(score), "high_scores"), "scale", N(label_scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minh", N(h), "r", N(0.1), "minw", N(score_w), "colour", P(G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(score_w), "maxw", N(score_w))), "nodes", T(score_tab)))
        ))))
    )));
}

// UI_definitions.lua:2749
Table *create_UIBox_win(void) {
    bool show_lose_cta = false;
    const bool show_win_cta = false; // note: a Lua global that nothing assigns: always nil (UI_definitions.h)
    float *eased_green = colour_copy(G->C.GREEN);
    eased_green[3] = 0;
    ease_value_f(&eased_green[3], 0.5, false, NULL, 1, -1, NULL);

    Val you_win = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(localize(S("ph_you_win"), NULL))), "colours", T(ARR(P(G->C.EDITION))), "shadow", B(true), "float", B(true), "spacing", N(10), "rotate", B(true), "scale", N(1.5), "pop_in", N(0.4), "maxw", N(6.5))))))));

    Table *top = tnew(); // :2760
    tinsert(top, T(create_UIBox_round_scores_row("hand", NULL)));
    tinsert(top, T(create_UIBox_round_scores_row("poker_hand", NULL)));

    Table *left = tnew(); // :2765
    tinsert(left, T(create_UIBox_round_scores_row("cards_played", G->C.BLUE)));
    tinsert(left, T(create_UIBox_round_scores_row("cards_discarded", G->C.RED)));
    tinsert(left, T(create_UIBox_round_scores_row("cards_purchased", G->C.MONEY)));
    tinsert(left, T(create_UIBox_round_scores_row("times_rerolled", G->C.GREEN)));
    tinsert(left, T(create_UIBox_round_scores_row("new_collection", G->C.WHITE)));
    tinsert(left, T(create_UIBox_round_scores_row("seed", G->C.WHITE)));
    tinsert(left, T(UIBox_button(TAB("button", S("copy_seed"), "label", T(ARR(localize(S("b_copy"), NULL))), "colour", P(G->C.BLUE), "scale", N(0.3), "minw", N(2.3), "minh", N(0.4)))));

    Table *right = tnew(); // :2774 (the entries that are nil are holes at their index)
    tseti(right, 1, T(create_UIBox_round_scores_row("furthest_ante", G->C.FILTER)));
    tseti(right, 2, T(create_UIBox_round_scores_row("furthest_round", G->C.FILTER)));
    tseti(right, 3, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.4), "minw", N(0.1))), "nodes", T(tnew()))));
    if (show_win_cta) tseti(right, 4, T(UIBox_button(TAB("id", S("win_cta"), "button", S("show_main_cta"), "label", T(ARR(localize(S("b_next"), NULL))), "colour", P(G->C.GREEN), "scale", N(0.8), "minw", N(2.5), "minh", N(2.5), "focus_args", T(TAB("nav", S("wide"), "snap_to", B(true)))))));
    if (!show_win_cta) tseti(right, 5, T(UIBox_button(TAB("id", S("from_game_won"), "button", S("notify_then_setup_run"), "label", T(ARR(localize(S("b_start_new_run"), NULL))), "minw", N(2.5), "maxw", N(2.5), "minh", N(1), "focus_args", T(TAB("nav", S("wide"), "snap_to", B(true)))))));
    if (!show_win_cta) tseti(right, 6, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.2), "minw", N(0.1))), "nodes", T(tnew()))));
    if (!show_win_cta) tseti(right, 7, T(UIBox_button(TAB("button", S("go_to_menu"), "label", T(ARR(localize(S("b_main_menu"), NULL))), "minw", N(2.5), "maxw", N(2.5), "minh", N(1), "focus_args", T(TAB("nav", S("wide")))))));

    Val endless = T(UIBox_button(TAB("button", S("exit_overlay_menu"), "label", T(ARR(localize(S("b_endless"), NULL))), "minw", N(6.5), "maxw", N(5), "minh", N(1.2), "scale", N(0.7), "shadow", B(true), "colour", P(G->C.BLUE), "focus_args", T(TAB("nav", S("wide"), "button", S("x"), "set_button_pip", B(true))))));

    Table *t = create_UIBox_generic_options(TAB("padding", N(0), "bg_colour", P(eased_green), "colour", P(G->C.BLACK), "outline_colour", P(G->C.EDITION), "no_back", B(true), "no_esc", B(true), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            you_win
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.15))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.08))), "nodes", T(top))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.08))), "nodes", T(left))),
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tr"), "padding", N(0.08))), "nodes", T(right)))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.08))), "nodes", T(ARR(
                    endless
                ))))
            ))))
        ))))
    ))));
    Table *nodes = ttab(t, "nodes");
    Val spot = jimbo_spot();
    tseti(nodes, 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(2))), "nodes", T(ARR(
            spot
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(tgeti(nodes, 1)))))
    )))));
    //t.nodes[1].config.mid = true
    tsets(ttab(t, "config"), "id", "you_win_UI");
    return t;
}

// UI_definitions.lua:2802 (demo)
Table *create_UIBox_exit_CTA(void) {
    Val thanks_1 = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(localize(S("ph_demo_thanks_1"), NULL))), "colours", T(ARR(P(G->C.WHITE))), "shadow", B(true), "float", B(true), "scale", N(0.9))))))));
    Val thanks_2 = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(localize(S("ph_demo_thanks_2"), NULL))), "colours", T(ARR(P(G->C.WHITE))), "shadow", B(true), "bump", B(true), "rotate", B(true), "pop_in", N(0.2), "scale", N(1.4))))))));
    // nil when the text is not a table: a hole at index 1
    Val message = T(simple_text_container("ml_demo_thanks_message", TAB("colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.55), "shadow", B(true))));
    Val wishlist = T(UIBox_button(TAB("button", S("wishlist_steam"), "label", T(ARR(localize(S("b_wishlist"), NULL))), "minw", N(5.9), "minh", N(1.1), "scale", N(0.5), "shadow", B(true), "colour", P(G->C.GREEN), "focus_args", T(TAB("nav", S("wide"), "button", S("x"), "set_button_pip", B(true), "snap_to", B(true))))));
    Val playbalatro = T(UIBox_button(TAB("button", S("go_to_playbalatro"), "label", T(ARR(localize(S("b_playbalatro"), NULL))), "minw", N(4.9), "minh", N(0.8), "scale", N(0.4), "shadow", B(true), "colour", P(G->C.BLUE), "focus_args", T(TAB("nav", S("wide"), "button", S("y"), "set_button_pip", B(true))))));

    Table *t = create_UIBox_generic_options(TAB("back_label", S(lit("exit_cta", 1)), "back_func", S("quit"), "colour", P(G->C.BLACK), "back_colour", P(G->C.RED), "padding", N(0), "contents", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tm"), "padding", N(0.15))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                thanks_1
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                thanks_2
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("tm"), "padding", N(0.12), "emboss", N(0.1), "colour", P(G->C.L_BLACK), "r", N(0.1))), "nodes", T(ARR(
                message,
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.2))), "nodes", T(ARR(
                    wishlist,
                    playbalatro
                ))))
            ))))
        ))))
    ))));
    Table *nodes = ttab(t, "nodes");
    tseti(nodes, 2, tgeti(nodes, 1));
    Val spot = jimbo_spot();
    tseti(nodes, 1, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(2))), "nodes", T(ARR(
        spot
    )))));
    //t.nodes[1].config.mid = true
    return t;
}

// UI_definitions.lua:2829 (demo)
Table *create_UIBox_small_cta(void) {
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(4), "minh", N(3))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(localize(S("ph_demo_thanks_1"), NULL))), "colours", T(ARR(P(G->C.WHITE))), "shadow", B(true), "float", B(true), "scale", N(0.3))))))))
    )));
}

// UI_definitions.lua:2841
static bool ev_demo_video_cta(void *ctx) {
    G->exception_queue = "other";
    FUNCS_go_to_demo_cta(NULL);
    G->exception_queue = NULL;
    return true;
}

// UI_definitions.lua:2835 (demo)
Table *create_UIBox_demo_video_CTA(void) {
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 21.7,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_demo_video_cta}), "other", false);

    // note: RESTART_MUSIC() with no args is an error in the Lua as soon as a music voice exists (io.h): NULL raises it
    RESTART_MUSIC(NULL);

    // :2850-2859 love.graphics.newVideo('resources/democta.ogv'), the Sprite(0,0,11*16/9,11, ui atlas) that carries it
    // as vid_sprite.video, its volume and play(), and the generic options box {back_delay = 7, back_label =
    // localize('b_skip'), back_func = 'go_to_demo_cta', colour = G.C.BLACK, padding = 0} around it.
    TODO_PORT("UI_definitions.lua:2850 create_UIBox_demo_video_CTA: love.graphics.newVideo (videos are not played on this platform)");
}

// UI_definitions.lua:2862
Table *create_UIBox_game_over(void) {
    bool show_lose_cta = false;

    float *eased_red = colour_copy(tnum(ttab(G->GAME, "round_resets"), "ante") <= tnum(G->GAME, "win_ante") ? G->C.RED : G->C.BLUE);
    eased_red[3] = 0;
    ease_value_f(&eased_red[3], 0.8, false, NULL, 1, -1, NULL);

    Val game_over = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR(localize(S("ph_game_over"), NULL))), "colours", T(ARR(P(G->C.RED))), "shadow", B(true), "float", B(true), "scale", N(1.5), "pop_in", N(0.4), "maxw", N(6.5))))))));

    Table *top = tnew(); // :2876
    tinsert(top, T(create_UIBox_round_scores_row("hand", NULL)));
    tinsert(top, T(create_UIBox_round_scores_row("poker_hand", NULL)));

    Table *left = tnew(); // :2881
    tinsert(left, T(create_UIBox_round_scores_row("cards_played", G->C.BLUE)));
    tinsert(left, T(create_UIBox_round_scores_row("cards_discarded", G->C.RED)));
    tinsert(left, T(create_UIBox_round_scores_row("cards_purchased", G->C.MONEY)));
    tinsert(left, T(create_UIBox_round_scores_row("times_rerolled", G->C.GREEN)));
    tinsert(left, T(create_UIBox_round_scores_row("new_collection", G->C.WHITE)));
    tinsert(left, T(create_UIBox_round_scores_row("seed", G->C.WHITE)));
    tinsert(left, T(UIBox_button(TAB("button", S("copy_seed"), "label", T(ARR(localize(S("b_copy"), NULL))), "colour", P(G->C.BLUE), "scale", N(0.3), "minw", N(2.3), "minh", N(0.4), "focus_args", T(TAB("nav", S("wide")))))));

    Table *right = tnew(); // :2890
    tinsert(right, T(create_UIBox_round_scores_row("furthest_ante", G->C.FILTER)));
    tinsert(right, T(create_UIBox_round_scores_row("furthest_round", G->C.FILTER)));
    tinsert(right, T(create_UIBox_round_scores_row("defeated_by", NULL)));

    Val buttons; // :2897
    if (show_lose_cta) {
        buttons = T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("id", S("lose_cta"), "align", S("cm"), "minw", N(5), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.GREEN), "button", S("show_main_cta"), "shadow", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_next"), NULL), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT), "focus_args", T(TAB("nav", S("wide"), "snap_to", B(true)))))))
                ))))
            ))))
        ))));
    } else {
        buttons = T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("id", S("from_game_over"), "align", S("cm"), "minw", N(5), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.RED), "button", S("notify_then_setup_run"), "shadow", B(true), "focus_args", T(TAB("nav", S("wide"), "snap_to", B(true))))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true), "maxw", N(4.8))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_start_new_run"), NULL), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
                ))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(5), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.RED), "button", S("go_to_menu"), "shadow", B(true), "focus_args", T(TAB("nav", S("wide"))))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true), "maxw", N(4.8))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_main_menu"), NULL), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
                ))))
            ))))
        ))));
    }

    Table *t = create_UIBox_generic_options(TAB("bg_colour", P(eased_red), "no_back", B(true), "padding", N(0), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            game_over
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.15))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.BLACK), "emboss", N(0.05), "r", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.08))), "nodes", T(top))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.08))), "nodes", T(left))),
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tr"), "padding", N(0.08))), "nodes", T(right)))
                    ))))
                )))),
                buttons
            ))))
        ))))
    ))));
    Table *nodes = ttab(t, "nodes");
    Val spot = jimbo_spot();
    tseti(nodes, 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(2))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                spot
            ))))
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(tgeti(nodes, 1)))))
    )))));

    //t.nodes[1].config.mid = true
    return t;
}

// UI_definitions.lua:2933
Table *create_UIBox_round_scores_row(const char *score, const float *text_colour) {
    Table *round_scores = ttab(G->GAME, "round_scores");
    Val rs = tget(round_scores, score);
    Val label = truthy(rs) ? localize(S(fmt("ph_score_%s", score)), NULL) : S("");
    bool check_high_score = false;
    Table *score_tab = tnew();
    bool wide = streq(score, "hand") || streq(score, "poker_hand"); // ({hand=true,poker_hand=true})[score]
    num label_w = wide ? 3.5 : 2.9, score_w = wide ? 3.5 : 1, h = 0.5;
    float *tc = (float *)text_colour; // colours are P(float *) in tables

    if (streq(score, "furthest_ante")) {
        label_w = 1.9;
        check_high_score = true;
        label = localize(S("k_ante"), NULL);
        score_tab = ARR(
            dyna_node(S(number_format(tget(ttab(G->GAME, "round_resets"), "ante"))), tc ? tc : G->C.FILTER, 0.45));
    }
    if (streq(score, "furthest_round")) {
        label_w = 1.9;
        check_high_score = true;
        label = localize(S("k_round"), NULL);
        score_tab = ARR(
            dyna_node(S(number_format(tget(G->GAME, "round"))), tc ? tc : G->C.FILTER, 0.45));
    }
    if (streq(score, "seed")) {
        label_w = 1.9;
        score_w = 1.9;
        label = localize(S("k_seed"), NULL);
        score_tab = ARR(
            dyna_node(tget(ttab(G->GAME, "pseudorandom"), "seed"), tc ? tc : G->C.WHITE, 0.45));
    }
    if (streq(score, "defeated_by")) {
        label = localize(S("k_defeated_by"), NULL);
        Blind *blind = tobj(G->GAME, "blind");
        Table *blind_config = ttab(blind->moveable.node.config, "blind");
        Table *blind_choice = TAB("config", T(blind_config ? blind_config : ttab(G->P_BLINDS, "bl_small")));
        AnimatedSprite *animation = NEW(AnimatedSprite);
        AnimatedSprite_init(animation, 0, 0, 1.4f, 1.4f, ANIMATION_ATLAS_get("blind_chips"), ttab(ttab(blind_choice, "config"), "pos"));
        tseto(blind_choice, "animation", animation);
        Sprite_define_draw_steps((Sprite *)animation, ARR(
            T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
            T(TAB("shader", S("dissolve")))));

        // :2974 the string is localize's result itself, not a list
        Val name = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", localize(T(TAB("type", S("name_text"), "key", tget(ttab(blind_choice, "config"), "key"), "set", S("Blind"))), NULL), "colours", T(ARR(P(G->C.WHITE))), "shadow", B(true), "float", B(true), "maxw", N(2.2), "scale", N(0.45))))))));
        score_tab = ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.6))), "nodes", T(ARR(
                name
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(animation)))))
            )))));
    }

    num label_scale = 0.5;

    if (streq(score, "poker_hand")) {
        Val handname = localize(S("k_none"), NULL);
        num amount = 0;
        // note: pairs(G.GAME.hand_usage): insertion order here; the Lua's hash order decides ties
        for (TIter it = titer(ttab(G->GAME, "hand_usage")); tnext(&it);) {
            Table *v = it.val.tab;
            if (tnum(v, "count") > amount) { handname = tget(v, "order"); amount = tnum(v, "count"); }
        }
        Val dyna = T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", T(ARR((amount < 1 && truthy(handname)) ? handname : localize(handname, "poker_hands"))), "colours", T(ARR(P(tc ? tc : G->C.WHITE))), "shadow", B(true), "float", B(true), "scale", N(0.45), "maxw", N(2.5))))))));
        score_tab = ARR(
            dyna,
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s)", number_tostring(amount))), "scale", N(0.35), "colour", P(G->C.JOKER_GREY))))));
    } else if (streq(score, "hand")) {
        check_high_score = true;
        Sprite *chip_sprite = NEW(Sprite);
        Sprite_init(chip_sprite, 0, 0, 0.3f, 0.3f, ui_atlas(), TAB("x", N(0), "y", N(0)));
        chip_sprite->moveable.node.states.drag.can = false;
        Val amt = tget(rs.t == V_TAB ? rs.tab : NULL, "amt");
        Val dyna = dyna_node(S(number_format(amt)), tc ? tc : G->C.RED, lmin(0.6, score_number_scale(1.2f, amt)));
        score_tab = ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("w", N(0.3), "h", N(0.3), "object", O(chip_sprite)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                dyna
            )))));
    } else if (truthy(rs) && !truthy(tgeti(score_tab, 1))) {
        Val amt = tget(rs.t == V_TAB ? rs.tab : NULL, "amt");
        score_tab = ARR(
            dyna_node(S(number_format(amt)), tc ? tc : G->C.FILTER, score_number_scale(0.6f, amt)));
    }
    bool defeated_by = streq(score, "defeated_by");
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(darken(G->C.JOKER_GREY, 0.1f, NULL)), "emboss", N(0.05), "func", check_high_score ? S("high_score_alert") : NIL, "id", S(score))), "nodes", T(ARR(
        T(TAB("n", N(defeated_by ? UIT_R : UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.02), "minw", N(label_w), "maxw", N(label_w))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", label, "scale", N(label_scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
        )))),
        T(TAB("n", N(defeated_by ? UIT_R : UIT_C), "config", T(TAB("align", S("cr"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(h), "r", N(0.1), "minw", N(defeated_by ? label_w : score_w), "colour", P((streq(score, "seed") && tbool(G->GAME, "seeded")) ? G->C.RED : G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(score_w))), "nodes", T(score_tab)))
            ))))
        ))))
    )));
}

// UI_definitions.lua:3020
Table *create_UIBox_hand_tip(const char *handname) {
    Table *example = ttab(ttab(ttab(G->GAME, "hands"), handname), "example");
    if (!example) return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(tnew()));
    CardArea *cardarea = NEW(CardArea);
    CardArea_init(cardarea, 2, 2, 3.5f * G->CARD_W, 0.75f * G->CARD_H, TAB("card_limit", N(5), "type", S("title"), "highlight_limit", N(0)));
    for (int k = 1; truthy(tgeti(example, k)); k++) {
        Table *v = ttabi(example, k);
        bool v2 = truthy(tgeti(v, 2));
        Card *card = NEW(Card);
        Card_init(card, 0, 0, 0.5f * G->CARD_W, 0.5f * G->CARD_H, ttab(G->P_CARDS, tstri(v, 1)), ttab(G->P_CENTERS, "c_base"), NULL);
        if (v2) Card_juice_up(card, 0.3f, N(0.2));
        if (k == 1) play_sound("paper1", 0.95 + math_random() * 0.1, 0.3f);
        // note: the ease holds the address of card.T.scale (event.h, "Lifetime"): as in the Lua, the 0.2 s ease may
        // outlive a tooltip that is closed at once
        ease_value_f(&card->moveable.node.T->scale, v2 ? 0.25 : -0.15, false, "REAL", 1, 0.2, NULL);
        CardArea_emplace(cardarea, card, NULL, false);
    }

    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.WHITE), "r", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(cardarea)))))
        ))))
    )));
}

// on_demand_tooltip = {text = localize(handname, 'poker_hand_descriptions'), filler = {func = create_UIBox_hand_tip, args = handname}}
static Val hand_row_tooltip(const char *handname) {
    return T(TAB("text", localize(S(handname), "poker_hand_descriptions"), "filler", T(TAB("func", FN(filler_hand_tip, NULL), "args", S(handname)))));
}

// UI_definitions.lua:3042
Table *create_UIBox_current_hand_row(const char *handname, bool simple) {
    Table *hand = ttab(ttab(G->GAME, "hands"), handname);
    if (!tbool(hand, "visible")) return NULL;
    if (!simple) {
        // G.C.HAND_LEVELS[math.min(7, level)]: nil for an index the table does not have
        num level = lmin(7, tnum(hand, "level"));
        Val level_colour = (level >= 0 && level == floor(level)) ? P(G->C.HAND_LEVELS[(int)level]) : NIL;
        return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(darken(G->C.JOKER_GREY, 0.1f, NULL)), "emboss", N(0.05), "hover", B(true), "force_focus", B(true), "on_demand_tooltip", hand_row_tooltip(handname))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "padding", N(0), "minw", N(5))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.01), "r", N(0.1), "colour", level_colour, "minw", N(1.5), "outline", N(0.8), "outline_colour", P(G->C.WHITE))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s%s", vstr(localize(S("k_level_prefix"), NULL)), vstr(tget(hand, "level")))), "scale", N(0.5), "colour", P(G->C.UI.TEXT_DARK)))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(4.5), "maxw", N(4.5))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s", vstr(localize(S(handname), "poker_hands")))), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
                ))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.BLACK), "r", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "padding", N(0.01), "r", N(0.1), "colour", P(G->C.CHIPS), "minw", N(1.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(hand, "chips"), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT))))),
                    T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.08), "h", N(0.01)))))
                )))),
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S("X"), "scale", N(0.45), "colour", P(G->C.MULT))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "padding", N(0.01), "r", N(0.1), "colour", P(G->C.MULT), "minw", N(1.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.08), "h", N(0.01))))),
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(hand, "mult"), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT)))))
                ))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S("  #"), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.L_BLACK), "r", N(0.1), "minw", N(0.9))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(hand, "played"), "scale", N(0.45), "colour", P(G->C.FILTER), "shadow", B(true)))))
            ))))
        )));
    }
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(darken(G->C.JOKER_GREY, 0.1f, NULL)), "force_focus", B(true), "emboss", N(0.05), "hover", B(true), "on_demand_tooltip", hand_row_tooltip(handname), "focus_args", T(TAB("snap_to", B(simple && streq(handname, "Straight Flush")))))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0), "minw", N(5))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S(handname), "poker_hands"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
        ))))
    )));
}

// UI_definitions.lua:3080
Table *create_UIBox_current_hands(bool simple) {
    // the rows that are nil are holes of the list
    static const char *const names[12] = {"Flush Five", "Flush House", "Five of a Kind", "Straight Flush", "Four of a Kind", "Full House",
                                          "Flush", "Straight", "Three of a Kind", "Two Pair", "Pair", "High Card"};
    Table *hands = tnew();
    for (int i = 1; i <= 12; i++) tseti(hands, i, T(create_UIBox_current_hand_row(names[i - 1], simple)));

    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(3), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.04))), "nodes", T(hands)))
    )));

    return t;
}

// UI_definitions.lua:3105
Table *UIDEF_deck_info(bool _show_remaining) {
    Table *tabs = _show_remaining ? ARR(
        T(TAB(
            "label", localize(S("b_remaining"), NULL),
            "chosen", B(true),
            "tab_definition_function", FN(tdf_view_deck, NULL),
            "tab_definition_function_args", B(true))),
        T(TAB(
            "label", localize(S("b_full_deck"), NULL),
            "tab_definition_function", FN(tdf_view_deck, NULL)))
    ) : ARR(
        T(TAB(
            "label", localize(S("b_full_deck"), NULL),
            "chosen", B(true),
            "tab_definition_function", FN(tdf_view_deck, NULL)))
    );
    Table *tabs_node = create_tabs(TAB("tabs", T(tabs),
        "tab_h", N(8),
        "snap_to_nav", B(true)));
    return create_UIBox_generic_options(TAB("contents", T(ARR(T(tabs_node)))));
}

// UI_definitions.lua:3129
Table *UIDEF_run_info(void) {
    Table *tabs = ARR(
        T(TAB(
            "label", localize(S("b_poker_hands"), NULL),
            "chosen", B(true),
            "tab_definition_function", FN(tdf_current_hands, NULL))),
        T(TAB(
            "label", localize(S("b_blinds"), NULL),
            "tab_definition_function", FN(tdf_current_blinds, NULL))),
        T(TAB(
            "label", localize(S("b_vouchers"), NULL),
            "tab_definition_function", FN(tdf_used_vouchers, NULL))),
        tnum(G->GAME, "stake") > 1 ? T(TAB(
            "label", localize(S("b_stake"), NULL),
            "tab_definition_function", FN(tdf_current_stake, NULL))) : NIL
    );
    Table *tabs_node = create_tabs(TAB("tabs", T(tabs),
        "tab_h", N(8),
        "snap_to_nav", B(true)));
    return create_UIBox_generic_options(TAB("contents", T(ARR(T(tabs_node)))));
}

// UI_definitions.lua:3154
Table *UIDEF_current_blinds(void) {
    Table *blind_states = ttab(ttab(G->GAME, "round_resets"), "blind_states");
    static const char *const types[3] = {"Small", "Big", "Boss"};
    // the entries that are nil are holes at their index; built in order (create_UIBox_blind_choice may poll 'orbital')
    Table *nodes = tnew();
    for (int i = 1; i <= 3; i++) {
        if (!streq(tstr(blind_states, types[i - 1]), "Hide")) {
            Val choice = T(create_UIBox_blind_choice(types[i - 1], true));
            tseti(nodes, i, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tm"), "padding", N(0.1), "outline", N(2), "r", N(0.1), "line_emboss", N(0.2), "outline_colour", P(G->C.BLACK))), "nodes", T(ARR(
                choice
            )))));
        }
    }
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.2))), "nodes", T(nodes));
}

// UI_definitions.lua:3168
Table *UIDEF_deck_stake_column(const char *_deck_key) {
    Table *deck_usage = ttab(ttab(cur_profile(), "deck_usage"), _deck_key);
    Table *stake_col = tnew();
    bool valid_option = false;
    for (int i = tlen(ttab(G->P_CENTER_POOLS, "Stake")); i >= 1; i--) {
        Val w = deck_usage ? tgeti(ttab(deck_usage, "wins"), i) : NIL;
        num _wins = truthy(w) ? vnum(w) : 0;
        if ((deck_usage && truthy(tgeti(ttab(deck_usage, "wins"), i - 1))) || i == 1 || tbool(cur_profile(), "all_unlocked")) valid_option = true;
        float *stake_colour = _wins > 0 ? get_stake_col(i) : NULL;
        tseti(stake_col, tlen(stake_col) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("id", N(i), "align", S("cm"), "colour", P(_wins > 0 ? G->C.GREY : G->C.CLEAR), "outline", N(0), "outline_colour", P(G->C.WHITE), "r", N(0.1), "minh", N(0.25), "minw", N(valid_option ? 0.45 : 0.25), "func", S("RUN_SETUP_check_back_stake_highlight"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(valid_option ? 0.17 : 0.13), "minw", N(valid_option ? 0.37 : 0.13), "colour", P(stake_colour ? stake_colour : G->C.UI.TRANSPARENT_LIGHT), "r", N(0.1))), "nodes", T(tnew())))
        )))));
        if (i > 1) tseti(stake_col, tlen(stake_col) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.1), "minw", N(0.04))), "nodes", T(tnew()))));
    }
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(stake_col));
}
