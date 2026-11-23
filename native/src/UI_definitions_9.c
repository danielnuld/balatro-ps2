// functions/UI_definitions.lua:5206-5679: the challenges tab, run setup, profiles, stakes, the challenge list and
// a challenge's description.
#include "all.h"

#define LOC(key) localize(S(key), NULL)
// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }
// Moveable()
static Moveable *new_moveable(void) {
    Moveable *m = NEW(Moveable);
    Moveable_init_args(m, NULL);
    return m;
}

// The tab_definition_function values (UI_definitions.h: one argument, returns T(root)).
static Val tabfn_challenges(void *ctx, int argc, Val *argv) { return T(UIDEF_challenges(argc > 0 && truthy(argv[0]))); }
static Val tabfn_run_setup_option(void *ctx, int argc, Val *argv) {
    return T(UIDEF_run_setup_option(argc > 0 ? vstr(argv[0]) : NULL));
}
static Val tabfn_profile_option(void *ctx, int argc, Val *argv) { return T(UIDEF_profile_option((int)vnum(argv[0]))); }
static Val tabfn_challenge_description_tab(void *ctx, int argc, Val *argv) {
    return T(UIDEF_challenge_description_tab(argc > 0 && argv[0].t == V_TAB ? argv[0].tab : NULL));
}

// UI_definitions.lua:5206
Table *UIDEF_challenges(bool from_game_over) {
    if (tbool(cur_profile(), "all_unlocked")) tsetn(cur_profile(), "challenges_unlocked", tlen(G->CHALLENGES));

    if (!truthy(tget(cur_profile(), "challenges_unlocked"))) {
        num deck_wins = 0;
        // note: pairs(deck_usage) only counts: the order does not matter
        for (TIter it = titer(ttab(cur_profile(), "deck_usage")); tnext(&it);) {
            Table *v = it.val.t == V_TAB ? it.val.tab : NULL;
            if (truthy(tget(v, "wins")) && truthy(tgeti(ttab(v, "wins"), 1))) deck_wins = deck_wins + 1;
        }
        Table *loc_nodes = tnew();
        localize(T(TAB("type", S("descriptions"), "key", S("challenge_locked"), "set", S("Other"), "nodes", T(loc_nodes),
                       "vars", T(ARR(N(G->CHALLENGE_WINS), N(deck_wins))), "default_col", P(G->C.WHITE))),
                 NULL);

        return TAB("n", N(UIT_ROOT),
                   "config", T(TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.CLEAR), "minh", N(8.02), "minw", N(7))),
                   "nodes", T(ARR(T(transparent_multiline_text(loc_nodes)))));
    }

    tset(G->object.dyn, "run_setup_seed", NIL);
    if (G->OVERLAY_MENU) {
        UIElement *seed_toggle = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("run_setup_seed"), NULL);
        if (seed_toggle) ((Node *)seed_toggle)->states.visible = false;
    }

    num _ch_comp = 0, _ch_tot = tlen(G->CHALLENGES);
    Table *completed = ttab(ttab(cur_profile(), "challenge_progress"), "completed");
    for (int k = 1; truthy(tgeti(G->CHALLENGES, k)); k++) {
        Table *v = ttabi(G->CHALLENGES, k);
        Val id = tget(v, "id");
        if (truthy(id) && truthy(tgetv(completed, id))) _ch_comp = _ch_comp + 1;
    }

    Table *_ch_tab = TAB("comp", N(_ch_comp), "unlocked", tget(cur_profile(), "challenges_unlocked"));

    Table *nodes = ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_challenge_mode"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(8.5), "minh", N(1.5), "padding", N(0.2))), "nodes", T(ARR(
                T(UIBox_button(TAB("id", from_game_over ? S("from_game_over") : NIL, "label", T(ARR(LOC("b_new_challenge"))),
                                   "button", S("challenge_list"), "minw", N(4), "scale", N(0.4), "minh", N(0.6))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.8), "r", N(0.1), "minw", N(4.5), "colour", P(G->C.L_BLACK), "emboss", N(0.05),
                "progress_bar", T(TAB(
                    "max", N(_ch_tot), "ref_table", T(_ch_tab), "ref_value", S("unlocked"), "empty_col", P(G->C.L_BLACK), "filled_col", P(G->C.FILTER)
                )))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(4.5))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(T(TAB("type", S("variable"), "key", S("unlocked"), "vars", T(ARR(tget(_ch_tab, "unlocked"), N(_ch_tot))))), NULL),
                                                         "scale", N(0.3), "colour", P(G->C.WHITE), "shadow", B(true)))))
                ))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.8), "r", N(0.1), "minw", N(4.5), "colour", P(G->C.L_BLACK), "emboss", N(0.05),
                "progress_bar", T(TAB(
                    "max", N(_ch_tot), "ref_table", T(_ch_tab), "ref_value", S("comp"), "empty_col", P(G->C.L_BLACK), "filled_col", P(adjust_alpha(G->C.GREEN, 0.5f, NULL))
                )))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "minw", N(4.5))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(T(TAB("type", S("variable"), "key", S("completed"), "vars", T(ARR(N(_ch_comp), N(_ch_tot))))), NULL),
                                                         "scale", N(0.3), "colour", P(G->C.WHITE), "shadow", B(true)))))
                ))))
            ))))
        )))));
    // :5266 G.F_DAILIES and {...} or nil
    if (G->F_DAILIES)
        tseti(nodes, 2, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_daily_run"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "minw", N(8.5), "minh", N(4))), "nodes", T(ARR(
                T(UIDEF_daily_overview())
            ))))
        )))));

    return TAB("n", N(UIT_ROOT),
               "config", T(TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.CLEAR), "minh", N(8), "minw", N(7))),
               "nodes", T(nodes));
}

// UI_definitions.lua:5277
Table *UIDEF_daily_overview(void) {
    num hist_height = 3, hist_width = 3;

    // daily_results.score (me = {val = 20000, percentile = 75} is never read)
    static const num hist[] = {0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                               0.05, 0.05, 0.15, 0.1,  0.1,  0.05, 0.05, 0.05, 0.05, 0.05};
    int nhist = (int)(sizeof(hist) / sizeof(hist[0]));
    Table *score_hist = tnew();
    num max_score_hist = 0;
    for (int k = 0; k < nhist; k++)
        if (max_score_hist < hist[k]) max_score_hist = hist[k];
    for (int k = 0; k < nhist; k++) {
        num v = hist[k];
        tinsert(score_hist, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("colour", P(G->C.BLUE), "minw", N(hist_width / nhist),
                                                 "minh", N((v + 0.05 * math_random()) / max_score_hist * hist_height))))))))));
    }

    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(score_hist));
}

// UI_definitions.lua:5303 G.FUNCS.false_ret
void FUNCS_false_ret(UIElement *e) {
    // returns false in the Lua; nothing reads it
}

// UI_definitions.lua:5297
Table *UIDEF_run_setup(const char *from_game_over) {
    tset(G->object.dyn, "run_setup_seed", NIL);
    bool _challenge_chosen = streq(from_game_over, "challenge_list");
    // from_game_over and not (from_game_over == 'challenge_list'): nil stays nil, else a boolean
    Val fgo = from_game_over ? B(!streq(from_game_over, "challenge_list")) : NIL;

    // note: nil / false of the Lua's `and` are both false here (create_tabs only tests `chosen`)
    bool _can_continue = G->MAIN_MENU_UI && FUNCS_can_continue(NULL); // NULL: {config = {func = true}}
    funcs_register("false_ret", FUNCS_false_ret);

    Table *tabs = ARR(
        T(TAB(
            "label", LOC("b_new_run"),
            "chosen", B(!_challenge_chosen && !_can_continue),
            "tab_definition_function", FN(tabfn_run_setup_option, NULL),
            "tab_definition_function_args", S("New Run"))),
        G->STAGE == STAGES_MAIN_MENU ? T(TAB(
            "label", LOC("b_continue"),
            "chosen", B(!_challenge_chosen && _can_continue),
            "tab_definition_function", FN(tabfn_run_setup_option, NULL),
            "tab_definition_function_args", S("Continue"),
            "func", S("can_continue")))
        : T(TAB(
            "label", LOC("b_challenges"),
            "tab_definition_function", FN(tabfn_challenges, NULL),
            "tab_definition_function_args", fgo,
            "chosen", B(_challenge_chosen))),
        G->STAGE == STAGES_MAIN_MENU ? T(TAB(
            "label", LOC("b_challenges"),
            "tab_definition_function", FN(tabfn_challenges, NULL),
            "tab_definition_function_args", fgo,
            "chosen", B(_challenge_chosen)))
        : NIL);

    Table *t = create_UIBox_generic_options(TAB("no_back", fgo, "no_esc", fgo, "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "draw_layer", N(1))), "nodes", T(ARR(
            T(create_tabs(TAB("tabs", T(tabs), "snap_to_nav", B(true))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:5339
Table *UIDEF_profile_select(void) {
    if (!G->focused_profile) {
        Val p = tget(G->SETTINGS, "profile");
        G->focused_profile = truthy(p) ? (int)vnum(p) : 1;
    }

    Table *t = create_UIBox_generic_options(TAB("padding", N(0), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "draw_layer", N(1), "minw", N(4))), "nodes", T(ARR(
            T(create_tabs(TAB("tabs", T(ARR(
                T(TAB(
                    "label", N(1),
                    "chosen", B(G->focused_profile == 1),
                    "tab_definition_function", FN(tabfn_profile_option, NULL),
                    "tab_definition_function_args", N(1))),
                T(TAB(
                    "label", N(2),
                    "chosen", B(G->focused_profile == 2),
                    "tab_definition_function", FN(tabfn_profile_option, NULL),
                    "tab_definition_function_args", N(2))),
                T(TAB(
                    "label", N(3),
                    "chosen", B(G->focused_profile == 3),
                    "tab_definition_function", FN(tabfn_profile_option, NULL),
                    "tab_definition_function_args", N(3)))
            )),
            "snap_to_nav", B(true))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:5389 the text input's callback (called with no argument)
static Val profile_name_callback(void *ctx, int argc, Val *argv) {
    Game_save_settings(G);
    tsetb(G->FILE_HANDLER, "force", true);
    return NIL;
}

// UI_definitions.lua:5371
Table *UIDEF_profile_option(int _profile) {
    set_discover_tallies();
    G->focused_profile = _profile;
    const char *packed = get_compressed(fmt("%d/profile.jkr", G->focused_profile));
    Table *profile_data = NULL;
    if (packed != NULL) {
        profile_data = STR_UNPACK(packed);
        if (!truthy(tget(profile_data, "name"))) tsets(profile_data, "name", fmt("P%d", _profile));
    }
    Table *prof = ttabi(G->PROFILES, _profile); // G.PROFILES[_profile]
    tset(prof, "name", profile_data ? tget(profile_data, "name") : S(""));

    num lwidth = 1, rwidth = 1, scale = 1;
    G->CHECK_PROFILE_DATA = false; // nil
    bool is_current = veq(N(_profile), tget(G->SETTINGS, "profile")); // _profile == G.SETTINGS.profile

    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minh", N(0.8))), "nodes", T(ARR(
            (is_current || !profile_data) ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(create_text_input(TAB(
                    "w", N(4), "max_length", N(16), "prompt_text", LOC("k_enter_name"),
                    "ref_table", T(prof), "ref_value", S("name"), "extended_corpus", B(true), "keyboard_offset", N(1),
                    "callback", FN(profile_name_callback, NULL))))
            )))) : T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minw", N(4), "r", N(0.1), "colour", P(G->C.BLACK), "minh", N(0.6))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(prof, "name"), "scale", N(0.45), "colour", P(G->C.WHITE)))))
            ))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(6))), "nodes", T(ARR(
                (truthy(tget(prof, "progress")) && truthy(tget(ttab(prof, "progress"), "discovered"))) ? T(create_progress_box(ttab(prof, "progress"), true)) : // (0.5: true)
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(4), "minw", N(5.2), "colour", P(G->C.BLACK), "r", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_empty_caps"), "scale", N(0.5), "colour", P(G->C.UI.TRANSPARENT_LIGHT)))))
                ))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(1))), "nodes", T(ARR(
                    profile_data ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(lwidth))), "nodes", T(ARR(T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_wins"), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(scale * 0.7))))))))),
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(T(TAB("n", N(UIT_T), "config", T(TAB("text", S(": "), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(scale * 0.7))))))))),
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minw", N(rwidth))), "nodes", T(ARR(T(TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(tpath(profile_data, "career_stats.c_wins"))), "colour", P(G->C.RED), "shadow", B(true), "scale", N(1 * scale)))))))))
                    )))) : NIL
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.2))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(4), "maxw", N(4), "minh", N(0.8), "padding", N(0.2), "r", N(0.1), "hover", B(true), "colour", P(G->C.BLUE), "func", S("can_load_profile"), "button", S("load_profile"), "shadow", B(true), "focus_args", T(TAB("nav", S("wide"))))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", is_current ? LOC("b_current_profile") : profile_data ? LOC("b_load_profile") : LOC("b_create_profile"), "ref_value", S("load_button_text"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
                        ))))
                    )))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "minh", N(0.7))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(3), "maxw", N(4), "minh", N(0.6), "padding", N(0.2), "r", N(0.1), "hover", B(true), "colour", P(G->C.RED), "func", S("can_delete_profile"), "button", S("delete_profile"), "shadow", B(true), "focus_args", T(TAB("nav", S("wide"))))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", is_current ? LOC("b_reset_profile") : LOC("b_delete_profile"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT)))))
                        ))))
                    )))),
                    (is_current && !tbool(cur_profile(), "all_unlocked")) ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "minh", N(0.7))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(3), "maxw", N(4), "minh", N(0.6), "padding", N(0.2), "r", N(0.1), "hover", B(true), "colour", P(G->C.ORANGE), "func", S("can_unlock_all"), "button", S("unlock_all"), "shadow", B(true), "focus_args", T(TAB("nav", S("wide"))))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("b_unlock_all"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT)))))
                        ))))
                    )))) : T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(3), "maxw", N(4), "minh", N(0.7))), "nodes", T(ARR(
                        (tbool(prof, "all_unlocked") && !G->F_NO_ACHIEVEMENTS) ? T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC(G->F_TROPHIES ? "k_trophies_disabled" : "k_achievements_disabled"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT)))))
                            : NIL
                    ))))
                ))))
            ))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("id", S("warning_text"), "text", LOC("ph_click_confirm"), "scale", N(0.4), "colour", P(G->C.CLEAR)))))
        ))))
    )));
    return t;
}

// UI_definitions.lua:5442
Table *UIDEF_stake_description(int _stake) {
    Table *_stake_center = ttabi(ttab(G->P_CENTER_POOLS, "Stake"), _stake);
    Table *ret_nodes = tnew();
    if (_stake_center)
        localize(T(TAB("type", S("descriptions"), "key", tget(_stake_center, "key"), "set", tget(_stake_center, "set"), "nodes", T(ret_nodes))), NULL);

    Table *desc_t = tnew();
    for (int k = 1; truthy(tgeti(ret_nodes, k)); k++) {
        Val v = tgeti(ret_nodes, k);
        tinsert(desc_t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(5.3))), "nodes", v)));
    }

    if (!_stake_center) ERROR("attempt to index local '_stake_center' (a nil value)"); // :5454
    return TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.L_BLACK))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(T(TAB("type", S("name_text"), "key", tget(_stake_center, "key"), "set", tget(_stake_center, "set"))), NULL), "scale", N(0.35), "colour", P(G->C.WHITE)))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.03), "colour", P(G->C.WHITE), "r", N(0.1), "minh", N(1), "minw", N(5.5))), "nodes", T(desc_t)))
    )));
}

// G.GAME.viewed_back.effect.center.key
static const char *viewed_back_key(void) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    return tstr(ttab(viewed_back->effect, "center"), "key");
}

// UI_definitions.lua:5460
Table *UIDEF_stake_option(const char *_type) {
    Table *middle = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(1.7), "minw", N(7.3))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_stake2"), "object", O(new_moveable())))))
    )));

    num max_stake = get_deck_win_stake(viewed_back_key(), NULL);
    if (tbool(cur_profile(), "all_unlocked")) max_stake = 8;
    Table *stake_options = tnew();
    for (int i = 1; i <= lmin(max_stake + 1, 8); i++) tseti(stake_options, i, N(i));

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "colour", P(G->C.CLEAR), "minh", N(2.03), "minw", N(8.3))), "nodes", T(ARR(
        streq(_type, "Continue") ? T(middle) : T(create_option_cycle(TAB("options", T(stake_options),
            "opt_callback", S("change_stake"), "current_option", G->viewed_stake ? N(G->viewed_stake) : NIL, "colour", P(G->C.RED), "w", N(6), "mid", T(middle))))
    )));
}

// UI_definitions.lua:5479
Table *UIDEF_viewed_stake_option(void) {
    if (!G->viewed_stake) G->viewed_stake = 1;
    num max_stake = get_deck_win_stake(viewed_back_key(), NULL);
    if (tbool(cur_profile(), "all_unlocked")) max_stake = 8;

    G->viewed_stake = (int)lmin(max_stake + 1, G->viewed_stake);
    // `_type` is the Lua GLOBAL here (UI_definitions.h)
    if (!streq(UI_global_type, "Continue")) tsetn(ttab(cur_profile(), "MEMORY"), "stake", G->viewed_stake);

    Sprite *stake_sprite = get_stake_sprite(G->viewed_stake, 0);

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.BLACK), "r", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_stake"), "scale", N(0.4), "colour", P(G->C.L_BLACK), "vert", B(true)))))
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("colour", P(G->C.BLUE), "object", O(stake_sprite), "hover", B(true), "can_collide", B(false)))))
            )))),
            T(UIDEF_stake_description(G->viewed_stake))
        ))))
    )));
}

// UI_definitions.lua:5508
static bool ev_challenge_list_page(void *ctx) {
    FUNCS_change_challenge_list_page(TAB("cycle_config", T(TAB("current_option", N(1)))));
    return true;
}

// UI_definitions.lua:5502
Table *UIDEF_challenge_list(bool from_game_over) {
    G->CHALLENGE_PAGE_SIZE = 10;
    Table *challenge_pages = tnew();
    int npages = (int)ceil((num)tlen(G->CHALLENGES) / G->CHALLENGE_PAGE_SIZE);
    for (int i = 1; i <= npages; i++)
        tinsert(challenge_pages, S(fmt("%s %d/%d", vstr(LOC("k_page")), i, npages)));
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_challenge_list_page}), NULL, false);

    num _ch_comp = 0, _ch_tot = tlen(G->CHALLENGES);
    Table *completed = ttab(ttab(cur_profile(), "challenge_progress"), "completed");
    for (int k = 1; truthy(tgeti(G->CHALLENGES, k)); k++) {
        Table *v = ttabi(G->CHALLENGES, k);
        Val id = tget(v, "id");
        if (truthy(id) && truthy(tgetv(completed, id))) _ch_comp = _ch_comp + 1;
    }

    // note: the Lua's constructor gives back_id twice; the last one ('challenge_list') wins, from_game_over is unused
    Table *t = create_UIBox_generic_options(TAB("back_func", S("setup_run"), "back_id", S("challenge_list"), "contents", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minh", N(7), "minw", N(4.2))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("id", S("challenge_list"), "object", O(new_moveable())))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(create_option_cycle(TAB("id", S("challenge_page"), "scale", N(0.9), "h", N(0.3), "w", N(3.5), "options", T(challenge_pages), "cycle_shoulders", B(true), "opt_callback", S("change_challenge_list_page"), "current_option", N(1), "colour", P(G->C.RED), "no_pips", B(true), "focus_args", T(TAB("snap_to", B(true))))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(T(TAB("type", S("variable"), "key", S("challenges_completed"), "vars", T(ARR(N(_ch_comp), N(_ch_tot))))), NULL), "scale", N(0.4), "colour", P(G->C.WHITE)))))
            ))))
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(9), "minw", N(11.5))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("id", S("challenge_area"), "object", O(new_moveable())))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:5539
Table *UIDEF_challenge_list_page(int _page) {
    bool snapped = false;
    Table *challenge_list = tnew();
    for (int k = 1; truthy(tgeti(G->CHALLENGES, k)); k++) {
        Table *v = ttabi(G->CHALLENGES, k);
        if (k > G->CHALLENGE_PAGE_SIZE * _page && k <= G->CHALLENGE_PAGE_SIZE * (_page + 1)) {
            Node *target = G->CONTROLLER->focused.target;
            if (target && veq(tget(target->config, "id"), S("challenge_page"))) snapped = true;
            Val id = tget(v, "id");
            Val challenge_completed = tgetv(ttab(ttab(cur_profile(), "challenge_progress"), "completed"), truthy(id) ? id : S(""));
            Val unl = tget(cur_profile(), "challenges_unlocked");
            bool challenge_unlocked = truthy(unl) && vnum(unl) >= k;

            Table *sprite_nodes = tnew();
            if (truthy(challenge_completed)) {
                Sprite *s = NEW(Sprite);
                Sprite_init(s, 0, 0, 0.4f, 0.4f, ASSET_ATLAS_get("icons"), TAB("x", N(1), "y", N(0)));
                tseti(sprite_nodes, 1, T(TAB("n", N(UIT_O), "config", T(TAB("object", O(s))))));
            }

            tinsert(challenge_list,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minw", N(0.8))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%d", k)), "scale", N(0.4), "colour", P(G->C.WHITE)))))
                )))),
                T(UIBox_button(TAB("id", N(k), "col", B(true), "label", T(ARR(challenge_unlocked ? localize(id, "challenge_names") : LOC("k_locked"))), "button", S(challenge_unlocked ? "change_challenge_description" : "nil"), "colour", P(challenge_unlocked ? G->C.RED : G->C.GREY), "minw", N(4), "scale", N(0.4), "minh", N(0.6), "focus_args", T(TAB("snap_to", B(!snapped)))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "minw", N(0.6))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("minh", N(0.4), "minw", N(0.4), "emboss", N(0.05), "r", N(0.1), "colour", P(truthy(challenge_completed) ? G->C.GREEN : G->C.BLACK))), "nodes", T(sprite_nodes)))
                ))))
            )))));
            snapped = true;
        }
    }

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(challenge_list));
}

// :5580, :5603, :5625 the small card of a challenge's joker / consumeable / voucher
static Card *challenge_card(Table *v, float joker_size) {
    Card *card = NEW(Card);
    Card_init(card, 0, 0, G->CARD_W * joker_size, G->CARD_H * joker_size, NULL, ttab(G->P_CENTERS, tstr(v, "id")),
              TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true), "bypass_lock", B(true)));
    if (truthy(tget(v, "edition"))) Card_set_edition(card, TAB(tstr(v, "edition"), B(true)), true, true);
    if (tbool(v, "eternal")) Card_set_eternal(card, true);
    return card;
}

// UI_definitions.lua:5567
Table *UIDEF_challenge_description(Val _id, bool daily, bool is_row) {
    Val chv = tgetv(G->CHALLENGES, _id);
    Table *challenge = chv.t == V_TAB ? chv.tab : NULL;
    if (!challenge)
        return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.BLACK), "minh", N(8.82), "minw", N(11.5), "r", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("ph_select_challenge"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT))))))));

    float joker_size = 0.6f;
    Val rule = get_challenge_rule(challenge, "modifiers", "joker_limit");
    CardArea *jokers = NEW(CardArea);
    CardArea_init(jokers, 0, 0,
        10 * joker_size,
        0.6f * G->CARD_H,
        TAB("card_limit", truthy(rule) ? rule : N(5),
            "card_w", N(joker_size * G->CARD_W), "type", S("title_2"), "highlight_limit", N(0)));

    if (truthy(tget(challenge, "jokers"))) {
        Table *list = ttab(challenge, "jokers");
        for (int k = 1; truthy(tgeti(list, k)); k++) {
            Table *v = ttabi(list, k);
            Card *card = challenge_card(v, joker_size);
            if (tbool(v, "pinned")) card->pinned = true;
            CardArea_emplace(jokers, card, NULL, false);
        }
    }

    Table *joker_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.L_BLACK), "r", N(0.1), "maxh", N(1.8))), "nodes", T(ARR(
        T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_jokers_cap"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT), "vert", B(true), "shadow", B(true))))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(0.6f * G->CARD_H), "minw", N(5), "r", N(0.1), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
            jokers ? T(TAB("n", N(UIT_O), "config", T(TAB("object", O(jokers))))) : T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_none"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
        ))))
    )));

    rule = get_challenge_rule(challenge, "modifiers", "consumable_limit");
    CardArea *consumeables = NEW(CardArea);
    CardArea_init(consumeables, 0, 0,
        3 * joker_size,
        0.6f * G->CARD_H,
        TAB("card_limit", truthy(rule) ? rule : N(2),
            "card_w", N(joker_size * G->CARD_W), "type", S("title_2"), "spread", B(true), "highlight_limit", N(0)));

    if (truthy(tget(challenge, "consumeables"))) {
        Table *list = ttab(challenge, "consumeables");
        for (int k = 1; truthy(tgeti(list, k)); k++) {
            Card *card = challenge_card(ttabi(list, k), joker_size);
            CardArea_emplace(consumeables, card, NULL, false);
        }
    }

    Table *consumable_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.L_BLACK), "r", N(0.1), "maxh", N(1.8))), "nodes", T(ARR(
        T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_cap_consumables"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT), "vert", B(true), "shadow", B(true))))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(0.6f * G->CARD_H), "r", N(0.1), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
            consumeables ? T(TAB("n", N(UIT_O), "config", T(TAB("object", O(consumeables))))) : T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_none"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
        ))))
    )));

    CardArea *vouchers = NEW(CardArea);
    CardArea_init(vouchers, 0, 0,
        3 * joker_size,
        0.6f * G->CARD_H,
        TAB("card_w", N(joker_size * G->CARD_W), "type", S("title_2"), "spread", B(true), "highlight_limit", N(0))); // card_limit = nil

    if (truthy(tget(challenge, "vouchers"))) {
        Table *list = ttab(challenge, "vouchers");
        for (int k = 1; truthy(tgeti(list, k)); k++) {
            Card *card = challenge_card(ttabi(list, k), joker_size);
            CardArea_emplace(vouchers, card, NULL, false);
        }
    }

    Table *voucher_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.L_BLACK), "r", N(0.1), "maxh", N(1.8))), "nodes", T(ARR(
        T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_vouchers_cap"), "scale", N(0.33), "colour", P(G->C.UI.TEXT_LIGHT), "vert", B(true), "shadow", B(true))))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(0.6f * G->CARD_H), "r", N(0.1), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
            vouchers ? T(TAB("n", N(UIT_O), "config", T(TAB("object", O(vouchers))))) : T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_none"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT)))))
        ))))
    )));

    return TAB("n", N(is_row ? UIT_R : UIT_ROOT), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.BLACK))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(joker_col), T(consumable_col), T(voucher_col)
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(create_tabs(TAB("tabs", T(ARR(
                T(TAB(
                    "label", LOC("b_rules"),
                    "chosen", B(true),
                    "tab_definition_function", FN(tabfn_challenge_description_tab, NULL),
                    "tab_definition_function_args", T(TAB("_id", _id, "_tab", S("Rules"))))),
                T(TAB(
                    "label", LOC("b_restrictions"),
                    "tab_definition_function", FN(tabfn_challenge_description_tab, NULL),
                    "tab_definition_function_args", T(TAB("_id", _id, "_tab", S("Restrictions"))))),
                T(TAB(
                    "label", LOC("b_deck"),
                    "tab_definition_function", FN(tabfn_challenge_description_tab, NULL),
                    "tab_definition_function_args", T(TAB("_id", _id, "_tab", S("Deck")))))
            )),
            "tab_h", N(5),
            "padding", N(0),
            "text_scale", N(0.36),
            "scale", N(0.85),
            "no_shoulders", B(true),
            "no_loop", B(true))))
        )))),
        !is_row ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.9))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minh", N(0.7), "minw", N(9), "r", N(0.1), "hover", B(true), "colour", P(G->C.BLUE), "button", S("start_challenge_run"), "shadow", B(true), "id", _id)), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("b_play_cap"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT), "func", S("set_button_pip"), "focus_args", T(TAB("button", S("x"), "set_button_pip", B(true)))))))
            ))))
        )))) : NIL
    )));
}
