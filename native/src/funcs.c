// The G.FUNCS registry (button_callbacks.h): name -> callback, in the two shapes the UI calls them, and
// funcs_register_all. Generated from the tags of include/button_callbacks.h (plain / [args] / [wrap: ...]).
#include "all.h"

// ponytail: open addressing, fixed size (about 200 names of 1024 slots); grow it if the game ever registers more.
#define FUNCS_SLOTS 1024
typedef struct { const char *name; void *fn; } FuncsSlot;
static FuncsSlot funcs_e[FUNCS_SLOTS], funcs_a[FUNCS_SLOTS];

static FuncsSlot *funcs_slot(FuncsSlot *tab, const char *name) {
    unsigned h = 5381;
    for (const char *p = name; *p; p++) h = h * 33 + (unsigned char)*p;
    for (unsigned i = 0; i < FUNCS_SLOTS; i++) {
        FuncsSlot *s = &tab[(h + i) % FUNCS_SLOTS];
        if (!s->name || strcmp(s->name, name) == 0) return s;
    }
    ERROR("funcs: registry full");
    return NULL;
}
static void funcs_put(FuncsSlot *tab, const char *name, void *fn) {
    FuncsSlot *s = funcs_slot(tab, name);
    if (!s->name) s->name = strdup(name);
    s->fn = fn;
}
void funcs_register(const char *name, FuncsFn fn) { funcs_put(funcs_e, name, (void *)fn); }
void funcs_register_args(const char *name, FuncsArgsFn fn) { funcs_put(funcs_a, name, (void *)fn); }
FuncsFn funcs_lookup(const char *name) { return name ? (FuncsFn)funcs_slot(funcs_e, name)->fn : NULL; }
FuncsArgsFn funcs_lookup_args(const char *name) { return name ? (FuncsArgsFn)funcs_slot(funcs_a, name)->fn : NULL; }

// [wrap: ...]: what the Lua function gets when the UI calls it with the element
static void w_can_continue(UIElement *e) { FUNCS_can_continue(e); }
static void w_load_profile(UIElement *e) { FUNCS_load_profile(true); }
static void w_apply_window_changes(UIElement *e) { FUNCS_apply_window_changes(false); }
static void w_exit_overlay_menu(UIElement *e) { FUNCS_exit_overlay_menu(); }
static void w_continue_unlock(UIElement *e) { FUNCS_continue_unlock(); }
static void w_current_hands(UIElement *e) { FUNCS_current_hands(e, false); }
static void w_settings(UIElement *e) { FUNCS_settings(e, false); }
static void w_use_card(UIElement *e) { FUNCS_use_card(e, NULL, false, false); }
static void w_end_consumeable(UIElement *e) { FUNCS_end_consumeable(e, -1); }
static void w_start_run(UIElement *e) { FUNCS_start_run(e, NULL); }
static void w_discard_cards_from_highlighted(UIElement *e) { FUNCS_discard_cards_from_highlighted(e, false); }

void funcs_register_all(void) {
    funcs_register("tut_next", FUNCS_tut_next);
    funcs_register("blueprint_compat", FUNCS_blueprint_compat);
    funcs_register("sort_hand_suit", FUNCS_sort_hand_suit);
    funcs_register("sort_hand_value", FUNCS_sort_hand_value);
    funcs_register("can_buy", FUNCS_can_buy);
    funcs_register("can_buy_and_use", FUNCS_can_buy_and_use);
    funcs_register("can_redeem", FUNCS_can_redeem);
    funcs_register("can_open", FUNCS_can_open);
    funcs_register("HUD_blind_visible", FUNCS_HUD_blind_visible);
    funcs_register("HUD_blind_debuff", FUNCS_HUD_blind_debuff);
    funcs_register("HUD_blind_debuff_prefix", FUNCS_HUD_blind_debuff_prefix);
    funcs_register("HUD_blind_reward", FUNCS_HUD_blind_reward);
    funcs_register("can_continue", w_can_continue);
    funcs_register("can_load_profile", FUNCS_can_load_profile);
    funcs_register("load_profile", w_load_profile);
    funcs_register("can_delete_profile", FUNCS_can_delete_profile);
    funcs_register("delete_profile", FUNCS_delete_profile);
    funcs_register("can_unlock_all", FUNCS_can_unlock_all);
    funcs_register("unlock_all", FUNCS_unlock_all);
    funcs_register("high_score_alert", FUNCS_high_score_alert);
    funcs_register("beta_lang_alert", FUNCS_beta_lang_alert);
    funcs_register("set_button_pip", FUNCS_set_button_pip);
    funcs_register("flash", FUNCS_flash);
    funcs_register("pip_dynatext", FUNCS_pip_dynatext);
    funcs_register("toggle_button", FUNCS_toggle_button);
    funcs_register("toggle", FUNCS_toggle);
    funcs_register("slider", FUNCS_slider);
    funcs_register("option_cycle", FUNCS_option_cycle);
    funcs_register_args("test_framework_cycle_callback", FUNCS_test_framework_cycle_callback);
    funcs_register_args("your_collection_joker_page", FUNCS_your_collection_joker_page);
    funcs_register_args("your_collection_tarot_page", FUNCS_your_collection_tarot_page);
    funcs_register_args("your_collection_spectral_page", FUNCS_your_collection_spectral_page);
    funcs_register_args("your_collection_booster_page", FUNCS_your_collection_booster_page);
    funcs_register_args("your_collection_voucher_page", FUNCS_your_collection_voucher_page);
    funcs_register_args("change_selected_back", FUNCS_change_selected_back);
    funcs_register_args("change_viewed_back", FUNCS_change_viewed_back);
    funcs_register_args("change_stake", FUNCS_change_stake);
    funcs_register_args("change_vsync", FUNCS_change_vsync);
    funcs_register_args("change_screen_resolution", FUNCS_change_screen_resolution);
    funcs_register_args("change_screenmode", FUNCS_change_screenmode);
    funcs_register_args("change_display", FUNCS_change_display);
    funcs_register_args("change_gamespeed", FUNCS_change_gamespeed);
    funcs_register_args("change_play_discard_position", FUNCS_change_play_discard_position);
    funcs_register_args("change_shadows", FUNCS_change_shadows);
    funcs_register_args("change_pixel_smoothing", FUNCS_change_pixel_smoothing);
    funcs_register_args("change_crt_bloom", FUNCS_change_crt_bloom);
    funcs_register_args("change_collab", FUNCS_change_collab);
    funcs_register("key_button", FUNCS_key_button);
    funcs_register("text_input", FUNCS_text_input);
    funcs_register("paste_seed", FUNCS_paste_seed);
    funcs_register("select_text_input", FUNCS_select_text_input);
    funcs_register("can_apply_window_changes", FUNCS_can_apply_window_changes);
    funcs_register("apply_window_changes", w_apply_window_changes);
    funcs_register("RUN_SETUP_check_back", FUNCS_RUN_SETUP_check_back);
    funcs_register("RUN_SETUP_check_back_name", FUNCS_RUN_SETUP_check_back_name);
    funcs_register("RUN_SETUP_check_stake", FUNCS_RUN_SETUP_check_stake);
    funcs_register("RUN_SETUP_check_stake2", FUNCS_RUN_SETUP_check_stake2);
    funcs_register_args("change_viewed_collab", FUNCS_change_viewed_collab);
    funcs_register("CREDITS_check_collab", FUNCS_CREDITS_check_collab);
    funcs_register("RUN_SETUP_check_back_stake_column", FUNCS_RUN_SETUP_check_back_stake_column);
    funcs_register("RUN_SETUP_check_back_stake_highlight", FUNCS_RUN_SETUP_check_back_stake_highlight);
    funcs_register("change_tab", FUNCS_change_tab);
    funcs_register("exit_overlay_menu", w_exit_overlay_menu);
    funcs_register("continue_unlock", w_continue_unlock);
    funcs_register("options", FUNCS_options);
    funcs_register("current_hands", w_current_hands);
    funcs_register("run_info", FUNCS_run_info);
    funcs_register("deck_info", FUNCS_deck_info);
    funcs_register("settings", w_settings);
    funcs_register("show_credits", FUNCS_show_credits);
    funcs_register("language_selection", FUNCS_language_selection);
    funcs_register("your_collection", FUNCS_your_collection);
    funcs_register("your_collection_blinds", FUNCS_your_collection_blinds);
    funcs_register("your_collection_jokers", FUNCS_your_collection_jokers);
    funcs_register("your_collection_tarots", FUNCS_your_collection_tarots);
    funcs_register("your_collection_planets", FUNCS_your_collection_planets);
    funcs_register("your_collection_spectrals", FUNCS_your_collection_spectrals);
    funcs_register("your_collection_vouchers", FUNCS_your_collection_vouchers);
    funcs_register("your_collection_enhancements_exit_overlay_menu", FUNCS_your_collection_enhancements_exit_overlay_menu);
    funcs_register("your_collection_enhancements", FUNCS_your_collection_enhancements);
    funcs_register("your_collection_decks", FUNCS_your_collection_decks);
    funcs_register("your_collection_editions", FUNCS_your_collection_editions);
    funcs_register("your_collection_tags", FUNCS_your_collection_tags);
    funcs_register("your_collection_seals", FUNCS_your_collection_seals);
    funcs_register("your_collection_boosters", FUNCS_your_collection_boosters);
    funcs_register("challenge_list", FUNCS_challenge_list);
    funcs_register("high_scores", FUNCS_high_scores);
    funcs_register("customize_deck", FUNCS_customize_deck);
    funcs_register("usage", FUNCS_usage);
    funcs_register("setup_run", FUNCS_setup_run);
    funcs_register("wait_for_high_scores", FUNCS_wait_for_high_scores);
    funcs_register("notify_then_setup_run", FUNCS_notify_then_setup_run);
    funcs_register("change_challenge_description", FUNCS_change_challenge_description);
    funcs_register_args("change_challenge_list_page", FUNCS_change_challenge_list_page);
    funcs_register("deck_view_challenge", FUNCS_deck_view_challenge);
    funcs_register("profile_select", FUNCS_profile_select);
    funcs_register("quit", FUNCS_quit);
    funcs_register("quit_cta", FUNCS_quit_cta);
    funcs_register("demo_survey", FUNCS_demo_survey);
    funcs_register("louisf_insta", FUNCS_louisf_insta);
    funcs_register("wishlist_steam", FUNCS_wishlist_steam);
    funcs_register("go_to_playbalatro", FUNCS_go_to_playbalatro);
    funcs_register("go_to_discord", FUNCS_go_to_discord);
    funcs_register("go_to_discord_loc", FUNCS_go_to_discord_loc);
    funcs_register("loc_survey", FUNCS_loc_survey);
    funcs_register("go_to_twitter", FUNCS_go_to_twitter);
    funcs_register("unlock_this", FUNCS_unlock_this);
    funcs_register("reset_achievements", FUNCS_reset_achievements);
    funcs_register("warn_lang", FUNCS_warn_lang);
    funcs_register("change_lang", FUNCS_change_lang);
    funcs_register("copy_seed", FUNCS_copy_seed);
    funcs_register("start_setup_run", FUNCS_start_setup_run);
    funcs_register("start_challenge_run", FUNCS_start_challenge_run);
    funcs_register("toggle_seeded_run", FUNCS_toggle_seeded_run);
    funcs_register("start_tutorial", FUNCS_start_tutorial);
    funcs_register("chip_UI_set", FUNCS_chip_UI_set);
    funcs_register("blind_chip_UI_scale", FUNCS_blind_chip_UI_scale);
    funcs_register("hand_mult_UI_set", FUNCS_hand_mult_UI_set);
    funcs_register("hand_chip_UI_set", FUNCS_hand_chip_UI_set);
    funcs_register("hand_chip_total_UI_set", FUNCS_hand_chip_total_UI_set);
    funcs_register("flame_handler", FUNCS_flame_handler);
    funcs_register("hand_text_UI_set", FUNCS_hand_text_UI_set);
    funcs_register("can_play", FUNCS_can_play);
    funcs_register("can_start_run", FUNCS_can_start_run);
    funcs_register("visible_blind", FUNCS_visible_blind);
    funcs_register("can_reroll", FUNCS_can_reroll);
    funcs_register("can_discard", FUNCS_can_discard);
    funcs_register("can_use_consumeable", FUNCS_can_use_consumeable);
    funcs_register("can_select_card", FUNCS_can_select_card);
    funcs_register("can_sell_card", FUNCS_can_sell_card);
    funcs_register("can_skip_booster", FUNCS_can_skip_booster);
    funcs_register("show_infotip", FUNCS_show_infotip);
    funcs_register("use_card", w_use_card);
    funcs_register("sell_card", FUNCS_sell_card);
    funcs_register("can_confirm_contest_name", FUNCS_can_confirm_contest_name);
    funcs_register("confirm_contest_name", FUNCS_confirm_contest_name);
    funcs_register("change_contest_name", FUNCS_change_contest_name);
    funcs_register("skip_tutorial_section", FUNCS_skip_tutorial_section);
    funcs_register("shop_voucher_empty", FUNCS_shop_voucher_empty);
    funcs_register("buy_from_shop", FUNCS_buy_from_shop);
    funcs_register("toggle_shop", FUNCS_toggle_shop);
    funcs_register("select_blind", FUNCS_select_blind);
    funcs_register("skip_booster", FUNCS_skip_booster);
    funcs_register("end_consumeable", w_end_consumeable);
    funcs_register("blind_choice_handler", FUNCS_blind_choice_handler);
    funcs_register("hover_tag_proxy", FUNCS_hover_tag_proxy);
    funcs_register("skip_blind", FUNCS_skip_blind);
    funcs_register("reroll_boss_button", FUNCS_reroll_boss_button);
    funcs_register("reroll_boss", FUNCS_reroll_boss);
    funcs_register("reroll_shop", FUNCS_reroll_shop);
    funcs_register("cash_out", FUNCS_cash_out);
    funcs_register("start_run", w_start_run);
    funcs_register("go_to_menu", FUNCS_go_to_menu);
    funcs_register("go_to_demo_cta", FUNCS_go_to_demo_cta);
    funcs_register("show_main_cta", FUNCS_show_main_cta);
    // functions/state_events.lua
    funcs_register("play_cards_from_highlighted", FUNCS_play_cards_from_highlighted);
    funcs_register("discard_cards_from_highlighted", w_discard_cards_from_highlighted);
}
