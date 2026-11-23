// functions/button_callbacks.lua:1299-1936: change_tab, the overlay menus, quit / links / language, starting runs,
// the first HUD number callbacks and scale_number.
#include "all.h"

#define CFG(e) ((e)->moveable.node.config) // e.config
#define XY(x_, y_) T(TAB("x", N(x_), "y", N(y_)))

// button_callbacks.lua:1299
void FUNCS_change_tab(UIElement *e) {
    if (!e) return;
    UIElement *_infotip_object = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("overlay_menu_infotip"), NULL);
    if (_infotip_object && truthy(tget(CFG(_infotip_object), "object"))) {
        CALL(tobj(CFG(_infotip_object), "object"), remove);
        Moveable *m = NEW(Moveable);
        Moveable_init(m, 0, 0, 0, 0); // Moveable()
        tseto(CFG(_infotip_object), "object", m);
    }

    UIElement *tab_contents = UIBox_get_UIE_by_ID(e->UIBox, S("tab_contents"), NULL);
    CALL(tobj(CFG(tab_contents), "object"), remove);
    Table *ref_table = ttab(CFG(e), "ref_table");
    Val fn_args = tget(ref_table, "tab_definition_function_args");
    Val definition = fcall(tget(ref_table, "tab_definition_function"), 1, &fn_args);
    tseto(CFG(tab_contents), "object",
          UIBox_new(TAB("definition", definition, "config", T(TAB("offset", XY(0, 0), "parent", O(tab_contents), "type", S("cm"))))));
    UIBox_recalculate(tab_contents->UIBox);
}

// ---------------------------------------------------------------- overlay menus

// button_callbacks.lua:1328
void FUNCS_overlay_menu(Table *args) {
    if (!args) return;
    // Remove any existing overlays if there is one
    if (G->OVERLAY_MENU) CALL(G->OVERLAY_MENU, remove);
    tsetb(G->CONTROLLER->locks, "frame_set", true);
    tsetb(G->CONTROLLER->locks, "frame", true);
    G->CONTROLLER->cursor_down.target = NULL;
    Controller_mod_cursor_context_layer(G->CONTROLLER, G->NO_MOD_CURSOR_STACK ? 0 : 1);

    Table *config = ttab(args, "config");
    if (!config) config = tnew();
    Val align = tget(config, "align");
    Val offset = tget(config, "offset");
    Val major = tget(config, "major");
    config = TAB("align", truthy(align) ? align : S("cm"),
                 "offset", truthy(offset) ? offset : XY(0, 10),
                 "major", truthy(major) ? major : O(G->ROOM_ATTACH),
                 "bond", S("Weak"),
                 "no_esc", tget(config, "no_esc"));
    tsett(args, "config", config);
    G->OVERLAY_MENU = OVERLAY_MENU_TRUE;
    // Generate the UIBox
    G->OVERLAY_MENU = UIBox_new(TAB("definition", tget(args, "definition"), "config", T(config)));

    // The menu overlay can be initially offset in the y direction and this ensures it slides to the middle
    G->OVERLAY_MENU->moveable.alignment.offset.y = 0;
    tsetn(G->ROOM->object.dyn, "jiggle", tnum(G->ROOM->object.dyn, "jiggle") + 1);
    Moveable_align_to_major((Moveable *)G->OVERLAY_MENU);
}

// button_callbacks.lua:1359
void FUNCS_exit_overlay_menu(void) {
    if (!G->OVERLAY_MENU) return;
    tsetb(G->CONTROLLER->locks, "frame_set", true);
    tsetb(G->CONTROLLER->locks, "frame", true);
    Controller_mod_cursor_context_layer(G->CONTROLLER, -1000);
    CALL(G->OVERLAY_MENU, remove);
    G->OVERLAY_MENU = NULL;
    G->VIEWING_DECK = false;
    tsetb(G->SETTINGS, "paused", false);

    // Save settings to file
    Game_save_settings(G);
}

// button_callbacks.lua:1374
void FUNCS_continue_unlock(void) {
    FUNCS_exit_overlay_menu();
    Controller_mod_cursor_context_layer(G->CONTROLLER, -2000);
    EventManager_update(G->E_MANAGER, 0, true);
}

// The body shared by the openers below: G.SETTINGS.paused = true; G.FUNCS.overlay_menu{definition = D}.
// The pause is set BEFORE the definition is built, as in the Lua (events made while building see it).
#define OPEN_OVERLAY(D) \
    do { \
        tsetb(G->SETTINGS, "paused", true); \
        FUNCS_overlay_menu(TAB("definition", T(D))); \
    } while (0)

// button_callbacks.lua:1380
void FUNCS_test_framework(Table *options) { OPEN_OVERLAY(create_UIBox_test_framework(options)); }
// button_callbacks.lua:1387
void FUNCS_options(UIElement *e) { OPEN_OVERLAY(create_UIBox_options()); }
// button_callbacks.lua:1394
void FUNCS_current_hands(UIElement *e, bool simple) { OPEN_OVERLAY(create_UIBox_current_hands(simple)); }
// button_callbacks.lua:1401
void FUNCS_run_info(UIElement *e) { OPEN_OVERLAY(UIDEF_run_info()); }

// button_callbacks.lua:1408
void FUNCS_deck_info(UIElement *e) {
    tsetb(G->SETTINGS, "paused", true);
    if (G->deck_preview) {
        CALL(G->deck_preview, remove);
        G->deck_preview = NULL;
    }
    FUNCS_overlay_menu(TAB("definition", T(UIDEF_deck_info(G->STATE == STATES_SELECTING_HAND || G->STATE == STATES_HAND_PLAYED ||
                                                           G->STATE == STATES_DRAW_TO_HAND))));
}

// button_callbacks.lua:1421
void FUNCS_settings(UIElement *e, bool instant) {
    tsetb(G->SETTINGS, "paused", true);
    Table *definition = create_UIBox_settings();
    FUNCS_overlay_menu(TAB("definition", T(definition), "config", T(TAB("offset", XY(0, instant ? 0 : 10)))));
}

// button_callbacks.lua:1429
void FUNCS_show_credits(UIElement *e) { OPEN_OVERLAY(UIDEF_credits()); }
// button_callbacks.lua:1436
void FUNCS_language_selection(UIElement *e) { OPEN_OVERLAY(UIDEF_language_selector()); }
// button_callbacks.lua:1443
void FUNCS_your_collection(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection()); }
// button_callbacks.lua:1450
void FUNCS_your_collection_blinds(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_blinds(NULL)); }
// button_callbacks.lua:1457
void FUNCS_your_collection_jokers(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_jokers()); }
// button_callbacks.lua:1464
void FUNCS_your_collection_tarots(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_tarots()); }
// button_callbacks.lua:1471
void FUNCS_your_collection_planets(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_planets()); }
// button_callbacks.lua:1478
void FUNCS_your_collection_spectrals(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_spectrals()); }
// button_callbacks.lua:1485
void FUNCS_your_collection_vouchers(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_vouchers(NULL)); }
// button_callbacks.lua:1492
void FUNCS_your_collection_enhancements_exit_overlay_menu(UIElement *e) {
    OPEN_OVERLAY(create_UIBox_your_collection_enhancements("exit_overlay_menu"));
}
// button_callbacks.lua:1499
void FUNCS_your_collection_enhancements(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_enhancements(NULL)); }
// button_callbacks.lua:1506
void FUNCS_your_collection_decks(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_decks()); }
// button_callbacks.lua:1513
void FUNCS_your_collection_editions(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_editions()); }
// button_callbacks.lua:1520
void FUNCS_your_collection_tags(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_tags()); }
// button_callbacks.lua:1527
void FUNCS_your_collection_seals(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_seals(NULL)); }
// button_callbacks.lua:1534
void FUNCS_your_collection_boosters(UIElement *e) { OPEN_OVERLAY(create_UIBox_your_collection_boosters()); }

// button_callbacks.lua:1541
void FUNCS_challenge_list(UIElement *e) {
    OPEN_OVERLAY(UIDEF_challenge_list(veq(tget(CFG(e), "id"), S("from_game_over"))));
    if (veq(tget(CFG(e), "id"), S("from_game_over"))) tsetb(G->OVERLAY_MENU->moveable.node.config, "no_esc", true);
}

// button_callbacks.lua:1549
void FUNCS_high_scores(UIElement *e) { OPEN_OVERLAY(create_UIBox_high_scores()); }
// button_callbacks.lua:1556
void FUNCS_customize_deck(UIElement *e) { OPEN_OVERLAY(create_UIBox_customize_deck()); }
// button_callbacks.lua:1563
void FUNCS_usage(UIElement *e) { OPEN_OVERLAY(UIDEF_usage_tabs()); }

// button_callbacks.lua:1570
void FUNCS_setup_run(UIElement *e) {
    Val id = tget(CFG(e), "id");
    bool over_or_won = veq(id, S("from_game_over")) || veq(id, S("from_game_won"));
    OPEN_OVERLAY(UIDEF_run_setup((over_or_won || veq(id, S("challenge_list"))) ? vstr(id) : NULL));
    if (over_or_won) tsetb(G->OVERLAY_MENU->moveable.node.config, "no_esc", true);
}

// button_callbacks.lua:1578
// note: G.ARGS.HIGH_SCORE_RESPONSE comes from the HTTP manager, which is not ported: the body never runs.
void FUNCS_wait_for_high_scores(UIElement *e) {
    Val resp = tget(G->ARGS, "HIGH_SCORE_RESPONSE");
    if (truthy(resp)) {
        CALL(tobj(CFG(e), "object"), remove);
        tseto(CFG(e), "object",
              UIBox_new(TAB("definition", T(create_UIBox_high_scores_filling(vstr(resp))),
                            "config", T(TAB("offset", XY(0, 0), "align", S("cm"), "parent", O(e))))));
        tset(G->ARGS, "HIGH_SCORE_RESPONSE", NIL);
    }
}

// button_callbacks.lua:1595
static bool ev_notify_unlock(void *ctx) {
    unlock_notify();
    return true;
}

// button_callbacks.lua:1603
static bool ev_notify_setup_run(void *ctx) {
    UIElement *e = ctx;
    if (tlen(ttab(G->E_MANAGER->queues, "unlock")) <= 0 && !G->OVERLAY_MENU) {
        Val id = tget(CFG(e), "id");
        bool over_or_won = veq(id, S("from_game_over")) || veq(id, S("from_game_won"));
        OPEN_OVERLAY(UIDEF_run_setup(over_or_won ? vstr(id) : NULL));
        if (over_or_won) tsetb(G->OVERLAY_MENU->moveable.node.config, "no_esc", true);
        return true;
    }
    return false;
}

// button_callbacks.lua:1589
void FUNCS_notify_then_setup_run(UIElement *e) {
    CALL(G->OVERLAY_MENU, remove);
    G->OVERLAY_MENU = NULL;

    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .func = ev_notify_unlock}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .func = ev_notify_setup_run, .ctx = e}), NULL, false);
}

// button_callbacks.lua:1616. e NULL: the made-up {config = {id = 'nil'}} of :1647.
void FUNCS_change_challenge_description(UIElement *e) {
    if (G->OVERLAY_MENU) {
        Val id = e ? tget(CFG(e), "id") : S("nil");
        UIElement *desc_area = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("challenge_area"), NULL);
        if (desc_area && !veq(tget(CFG(desc_area), "oid"), id)) {
            UIElement *old_chosen = tobj(CFG(desc_area), "old_chosen");
            if (old_chosen) tset(CFG(old_chosen), "chosen", NIL);
            if (e) tsets(CFG(e), "chosen", "vert");
            if (truthy(tget(CFG(desc_area), "object"))) CALL(tobj(CFG(desc_area), "object"), remove);
            tseto(CFG(desc_area), "object",
                  UIBox_new(TAB("definition", T(UIDEF_challenge_description(id, false, false)),
                                "config", T(TAB("offset", XY(0, 0), "align", S("cm"), "parent", O(desc_area))))));
            tset(CFG(desc_area), "oid", id);
            tset(CFG(desc_area), "old_chosen", O(e)); // e NULL: nil (the header's convention for the made-up table)
        }
    }
}

// button_callbacks.lua:1635
void FUNCS_change_challenge_list_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    if (G->OVERLAY_MENU) {
        UIElement *ch_list = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("challenge_list"), NULL);
        if (ch_list) {
            if (truthy(tget(CFG(ch_list), "object"))) CALL(tobj(CFG(ch_list), "object"), remove);
            tseto(CFG(ch_list), "object",
                  UIBox_new(TAB("definition", T(UIDEF_challenge_list_page((int)tnum(ttab(args, "cycle_config"), "current_option") - 1)),
                                "config", T(TAB("offset", XY(0, 0), "align", S("cm"), "parent", O(ch_list))))));
            FUNCS_change_challenge_description(NULL);
        }
    }
}

// button_callbacks.lua:1652. e.config.id: Table, a challenge.
void FUNCS_deck_view_challenge(UIElement *e) {
    const char *id = tstr(ttab(CFG(e), "id"), "id");
    Table *desc = UIDEF_challenge_description(N(get_challenge_int_from_id(id ? id : "")), false, true);
    FUNCS_overlay_menu(TAB("definition", T(create_UIBox_generic_options(TAB("back_func", S("deck_info"), "contents", T(ARR(T(desc))))))));
}

// button_callbacks.lua:1661
void FUNCS_profile_select(UIElement *e) {
    tsetb(G->SETTINGS, "paused", true);
    G->focused_profile = (int)tnum(G->SETTINGS, "profile");

    for (int i = 1; i <= 3; i++) {
        if (i != G->focused_profile && love_filesystem_getInfo(fmt("%d/profile.jkr", i), NULL)) Game_load_profile(G, i);
    }
    Game_load_profile(G, G->focused_profile);

    FUNCS_overlay_menu(TAB("definition", T(UIDEF_profile_select())));
}

// button_callbacks.lua:1675
void FUNCS_quit(UIElement *e) { love_event_quit(); }

// button_callbacks.lua:1697
static bool ev_quit_cta_jimbo(void *ctx) {
    if (G->OVERLAY_MENU && UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL)) {
        Card_Character *Jimbo = NEW(Card_Character);
        Card_Character_init(Jimbo, TAB("x", N(0), "y", N(5)));
        UIElement *spot = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL);
        CALL(tobj(CFG(spot), "object"), remove);
        tseto(CFG(spot), "object", Jimbo);
        tsetb(Jimbo->moveable.node.object.dyn, "ui_object_updated", true);
        Card_Character_add_speech_bubble(
            Jimbo, T(ARR(S(lit("quit_cta", 1)), T(ARR(T(TAB("text", S(lit("quit_cta", 2)), "type", S("GREEN"))))))), NULL, NULL);
        Card_Character_say_stuff(Jimbo, 5, false);
    }
    return true;
}

// button_callbacks.lua:1679 (demo only)
void FUNCS_quit_cta(UIElement *e) {
    tsetb(G->SETTINGS, "paused", true);
    tsetb(ttab(G->SETTINGS, "DEMO"), "quit_CTA_shown", true);

    Game_save_progress(G);

    Table *definition = create_UIBox_exit_CTA();
    FUNCS_overlay_menu(TAB("definition", T(definition), "config", T(TAB("no_esc", B(true)))));

    if (!G->jimboed) {
        G->jimboed = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .not_blockable = true, .delay = 2.5, .func = ev_quit_cta_jimbo}),
              NULL, false);
    }
}

// The link a G.FUNCS opens: the literal of the Lua function, LITERALS.url.<name> of data/game.dat (data.h)
static const char *url(const char *func) {
    const char *u = tstr(lits("url"), func);
    if (!u) ERROR("data/game.dat has no LITERALS.url.%s: regenerate it with tools/dump_data.sh", func);
    return u;
}

// button_callbacks.lua:1713
void FUNCS_demo_survey(UIElement *e) { love_system_openURL(url("demo_survey")); }
// button_callbacks.lua:1717
void FUNCS_louisf_insta(UIElement *e) { love_system_openURL(url("louisf_insta")); }
// button_callbacks.lua:1721
void FUNCS_wishlist_steam(UIElement *e) { love_system_openURL(url("wishlist_steam")); }
// button_callbacks.lua:1725
void FUNCS_go_to_playbalatro(UIElement *e) { love_system_openURL(url("go_to_playbalatro")); }
// button_callbacks.lua:1729
void FUNCS_go_to_discord(UIElement *e) { love_system_openURL(url("go_to_discord")); }
// button_callbacks.lua:1733
void FUNCS_go_to_discord_loc(UIElement *e) { love_system_openURL(url("go_to_discord_loc")); }
// button_callbacks.lua:1737
void FUNCS_loc_survey(UIElement *e) { love_system_openURL(url("loc_survey")); }
// button_callbacks.lua:1741
void FUNCS_go_to_twitter(UIElement *e) { love_system_openURL(url("go_to_twitter")); }

// button_callbacks.lua:1745
void FUNCS_unlock_this(UIElement *e) { unlock_achievement(tstr(CFG(e), "id")); }

// button_callbacks.lua:1749
void FUNCS_reset_achievements(UIElement *e) {
    G->ACHIEVEMENTS = NULL;
    tsett(G->SETTINGS, "ACHIEVEMENTS_EARNED", tnew());
    Game_save_progress(G);
    FUNCS_exit_overlay_menu();
}

// button_callbacks.lua:1756
void FUNCS_refresh_contrast_mode(void) {
    int n = tbool(G->SETTINGS, "colourblind_option") ? 2 : 1; // new_colour_proto = G.C["SO_"..n]
    G->C.SUITS.Hearts = C_SO_get(n, "Hearts");
    G->C.SUITS.Diamonds = C_SO_get(n, "Diamonds");
    G->C.SUITS.Spades = C_SO_get(n, "Spades");
    G->C.SUITS.Clubs = C_SO_get(n, "Clubs");
    // note: pairs order of G.I.CARD is insertion order here; the result does not depend on it.
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        // note: G.I.CARD also holds the Blind and Card_Characters, which have no config.card: skipped by is_a.
        if (!is_a(it.val.obj, &Card_class)) continue;
        Card *v = it.val.obj;
        Table *config = v->moveable.node.config;
        if (config && truthy(tget(config, "card")) && Card_child(v, "front") && !streq(tstr(v->ability, "effect"), "Stone Card")) {
            Card_set_sprites(v, NULL, ttab(config, "card"));
        }
    }
}

// G.FUNCS.refresh_contrast_mode as a function value (a toggle's callback)
Val FUNCS_refresh_contrast_mode_fn(void *ctx, int argc, Val *argv) {
    FUNCS_refresh_contrast_mode();
    return NIL;
}

// button_callbacks.lua:1780
static bool ev_warn_lang_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}

// button_callbacks.lua:1783
static bool ev_warn_lang_enable(void *ctx) {
    UIElement *e = ctx;
    tset(CFG(e), "disable_button", NIL);
    return true;
}

// button_callbacks.lua:1769
void FUNCS_warn_lang(UIElement *e) {
    UIElement *_infotip_object = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("overlay_menu_infotip"), NULL);
    Table *lang = ttab(CFG(e), "ref_table");
    if (!veq(tget(CFG(_infotip_object), "set"), tget(lang, "label")) && !G->F_NO_ACHIEVEMENTS) {
        CALL(tobj(CFG(_infotip_object), "object"), remove);
        Table *warning = ttab(lang, "warning");
        Table *text_rows = ARR(tgeti(warning, 1), tgeti(warning, 2), tgeti(warning, 3));
        tsett(text_rows, "lang", lang);
        UIBox *box = UIBox_new(TAB("definition", T(overlay_infotip(text_rows)),
                                   "config", T(TAB("offset", XY(0, 0), "align", S("bm"), "parent", O(_infotip_object)))));
        tseto(CFG(_infotip_object), "object", box);
        UIElement_juice_up(box->UIRoot, -1, NIL);
        tset(CFG(_infotip_object), "set", tget(lang, "label"));
        tsetb(CFG(e), "disable_button", true); // e.config.disable_button: in the config, as the Lua writes it
        E_add(G->E_MANAGER,
              Event_new((EventCfg){.trigger = "after", .delay = 0.06, .not_blockable = true, .not_blocking = true, .func = ev_warn_lang_sound}),
              NULL, false);

        E_add(G->E_MANAGER,
              Event_new((EventCfg){.trigger = "after", .delay = 0.35, .not_blockable = true, .not_blocking = true,
                                   .func = ev_warn_lang_enable, .ctx = e}),
              NULL, false);
        tsets(CFG(e), "button", "change_lang");
        play_sound("tarot2", 1, 0.4f);
    }
}

// button_callbacks.lua:1805
static bool ev_change_lang(void *ctx) {
    Game_delete_run(G);
    Game_init_item_prototypes(G);
    Game_main_menu(G, NULL);
    return true;
}

// button_callbacks.lua:1792
void FUNCS_change_lang(UIElement *e) {
    Table *lang = ttab(CFG(e), "ref_table");
    if (!lang || lang == G->LANG) {
        FUNCS_exit_overlay_menu();
    } else {
        tset(G->SETTINGS, "language", tget(lang, "key"));
        Game_set_language(G);
        EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
        FUNCS_wipe_on(NULL, false, -1, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .not_blocking = true, .func = ev_change_lang}), NULL, false);
        FUNCS_wipe_off();
    }
}

// button_callbacks.lua:1816
void FUNCS_copy_seed(UIElement *e) {
    const char *seed = tstr(ttab(G->GAME, "pseudorandom"), "seed");
    if (G->F_LOCAL_CLIPBOARD) {
        G->CLIPBOARD = str(seed);
    } else {
        love_system_setClipboardText(seed);
    }
}

// ---------------------------------------------------------------- starting runs

// button_callbacks.lua:1824
void FUNCS_start_setup_run(UIElement *e) {
    if (G->OVERLAY_MENU) FUNCS_exit_overlay_menu();
    Val current_setup = tget(G->SETTINGS, "current_setup");
    Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
    if (veq(current_setup, S("New Run"))) {
        if (!G->GAME || (!tbool(G->GAME, "won") && !tbool(G->GAME, "seeded"))) {
            if (G->SAVED_GAME != NULL) {
                if (!tbool(ttab(G->SAVED_GAME, "GAME"), "won")) {
                    tsetn(ttab(ttab(profile, "high_scores"), "current_streak"), "amt", 0);
                }
                Game_save_settings(G);
            }
        }
        // note: an empty setup_seed is truthy in the Lua and is passed on as the seed.
        Val setup_seed = tget(G->object.dyn, "setup_seed");
        Val _seed = (G_run_setup_seed() && truthy(setup_seed)) ? setup_seed : S(G->forced_seed);
        Val _challenge = T(G->challenge_tab);
        Val memory_stake = tget(ttab(profile, "MEMORY"), "stake");
        Val _stake = G->forced_stake != 0 ? N(G->forced_stake) : truthy(memory_stake) ? memory_stake : N(1);
        FUNCS_start_run(e, TAB("stake", _stake, "seed", _seed, "challenge", _challenge));

    } else if (veq(current_setup, S("Continue"))) {
        if (G->SAVED_GAME != NULL) {
            FUNCS_start_run(NULL, TAB("savetext", T(G->SAVED_GAME)));
        }
    }
}

// button_callbacks.lua:1847
void FUNCS_start_challenge_run(UIElement *e) {
    if (G->OVERLAY_MENU) FUNCS_exit_overlay_menu();
    FUNCS_start_run(e, TAB("stake", N(1), "challenge", tgetv(G->CHALLENGES, tget(CFG(e), "id"))));
}

// button_callbacks.lua:1852
void FUNCS_toggle_seeded_run(UIElement *e) {
    Val object = tget(CFG(e), "object");
    if (truthy(object) && !G_run_setup_seed()) {
        CALL(object.obj, remove);
        tset(CFG(e), "object", NIL);
    } else if (!truthy(object) && G_run_setup_seed()) {
        // locals: the Lua's table constructor evaluates these in this order
        Table *text = simple_text_container("ml_disabled_seed", TAB("colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.26), "shadow", B(true)));
        Val prompt_text = localize(S("k_enter_seed"), NULL);
        Table *input = create_text_input(TAB("max_length", N(8), "all_caps", B(true), "ref_table", O(G), "ref_value", S("setup_seed"),
                                             "prompt_text", prompt_text));
        Val label = localize(S("ml_paste_seed"), NULL);
        Table *paste = UIBox_button(TAB("label", label, "minw", N(1), "minh", N(0.6), "button", S("paste_seed"), "colour", P(G->C.BLUE),
                                        "scale", N(0.3), "col", B(true)));
        UIBox *box = UIBox_new(TAB(
            "definition", T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.05))), "nodes", T(ARR(
                    T(text))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.1))), "nodes", T(ARR(
                    T(input),
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.1))), "nodes", T(tnew()))),
                    T(paste))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(2.5))), "nodes", T(tnew()))))))),
            "config", T(TAB("offset", XY(0, 0), "parent", O(e), "type", S("cm")))));
        tseto(CFG(e), "object", box);
        UIBox_recalculate(box);
    }
}

// button_callbacks.lua:1877
void FUNCS_start_tutorial(UIElement *e) {
    if (G->OVERLAY_MENU) FUNCS_exit_overlay_menu();
    tsett(G->SETTINGS, "tutorial_progress",
          TAB("forced_shop", T(ARR(S("j_joker"), S("c_empress"))),
              "forced_voucher", S("v_grabber"),
              "forced_tags", T(ARR(S("tag_handy"), S("tag_garbage"))),
              "hold_parts", T(tnew()),
              "completed_parts", T(tnew())));
    tsetb(G->SETTINGS, "tutorial_complete", false);
    FUNCS_start_run(e, NULL);
}

// ---------------------------------------------------------------- the HUD's numbers

// button_callbacks.lua:1891
void FUNCS_chip_UI_set(UIElement *e) {
    const char *new_chips_text = number_format(tget(G->GAME, "chips"));
    if (!veq(tget(G->GAME, "chips_text"), S(new_chips_text))) {
        tsetn(CFG(e), "scale", lmin(0.8, scale_number(tget(G->GAME, "chips"), 1.1f, -1)));
        tsets(G->GAME, "chips_text", new_chips_text);
    }
}

// button_callbacks.lua:1899
void FUNCS_blind_chip_UI_scale(UIElement *e) {
    Blind *blind = tobj(G->GAME, "blind");
    // note: blind.chips is a num member, never nil (0 is truthy in the Lua): only the blind is tested.
    if (blind) {
        tsetn(CFG(e), "scale", scale_number(N(blind->chips), 0.7f, 100000));
    }
}

// math.log(x, 10) as LuaJIT 2.1 computes it (lib_math.c: log2(x) * (1/log2(base)), not joined into a division):
// the floor of the result at exact powers of ten depends on it (spec, parity trap 7.3).
static num lj_log10b(num x) {
    volatile num y = 1.0 / log2(10.0);
    return log2(x) * y;
}

// button_callbacks.lua:1905
float scale_number(Val number, float scale, num max) {
    if (G->E_SWITCH_POINT == 0) G->E_SWITCH_POINT = 100000000000;
    if (!truthy(number) || number.t != V_NUM) return scale;
    if (max < 0) max = 10000;
    num s = scale;
    if (number.n >= G->E_SWITCH_POINT) {
        s = s * floor(lj_log10b(max * 10)) / floor(lj_log10b(1000000 * 10));
    } else if (number.n >= max) {
        s = s * floor(lj_log10b(max * 10)) / floor(lj_log10b(number.n * 10));
    }
    return (float)s;
}

// button_callbacks.lua:1917
void FUNCS_hand_mult_UI_set(UIElement *e) {
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    Val mult = tget(current_hand, "mult");
    const char *new_mult_text = number_format(mult);
    if (!veq(S(new_mult_text), tget(current_hand, "mult_text"))) {
        tsets(current_hand, "mult_text", new_mult_text);
        DynaText *object = tobj(CFG(e), "object");
        object->scale = scale_number(mult, 0.9f, 1000);
        DynaText_update_text(object, false);
        if (!G->TAROT_INTERRUPT_PULSE) {
            FUNCS_text_super_juice(e, (float)lmax(0, floor(log10(mult.t == V_NUM ? mult.n : 1))));
        }
    }
}

// button_callbacks.lua:1927
void FUNCS_hand_chip_UI_set(UIElement *e) {
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    Val chips = tget(current_hand, "chips");
    const char *new_chip_text = number_format(chips);
    if (!veq(S(new_chip_text), tget(current_hand, "chip_text"))) {
        tsets(current_hand, "chip_text", new_chip_text);
        DynaText *object = tobj(CFG(e), "object");
        object->scale = scale_number(chips, 0.9f, 1000);
        DynaText_update_text(object, false);
        if (!G->TAROT_INTERRUPT_PULSE) {
            FUNCS_text_super_juice(e, (float)lmax(0, floor(log10(chips.t == V_NUM ? chips.n : 1))));
        }
    }
}
