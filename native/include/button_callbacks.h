// functions/button_callbacks.lua: the G.FUNCS callbacks of the UI (what buttons do, what UI nodes run every frame,
// the option cycles, the text input, the overlay menus, the shop / blind select / booster actions, the screen wipe)
// and the registry that finds a callback by its name.
// The file defines no class: no structs here. Everything it touches is a UI node's config (Table), G.GAME /
// G.SETTINGS / G.PROFILES (tables) or objects of other headers.
// The file has no file-level locals: all its state is in fields of G (game.h) and in G.ARGS (see the end).
#ifndef BUTTON_CALLBACKS_H
#define BUTTON_CALLBACKS_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- the registry (G.FUNCS[name])
// The UI refers to callbacks by string, in two shapes, which are two tables here:
//  - config.button (click) and config.func (every update), called with the UI element: G.FUNCS[name](e)
//    (engine/ui.lua:139, 463, 950, 978). C: funcs_lookup(name)(e).
//  - opt_callback of an option cycle, called with a new table of arguments (button_callbacks.lua:572), and callback
//    of a slider, called with the slider's args table (button_callbacks.lua:508). C: funcs_lookup_args(name)(args).
// No name of the game is used in both shapes. What the Lua callback returns is ignored by both.
typedef void (*FuncsFn)(UIElement *e);
typedef void (*FuncsArgsFn)(Table *args);
// G.FUNCS.name = fn. Registering a name again replaces the function (as the Lua's assignment). name is copied.
void funcs_register(const char *name, FuncsFn fn);
void funcs_register_args(const char *name, FuncsArgsFn fn);
// G.FUNCS[name]: NULL if no function has that name in that table (the Lua would fail calling nil) or name is NULL.
FuncsFn funcs_lookup(const char *name);
FuncsArgsFn funcs_lookup_args(const char *name);
// Registers every callback below under its Lua name. Call it once at start, before any UIBox is made (the Lua
// defines them when main.lua requires the files). Implemented in the last part of src/button_callbacks.c, with the
// four functions above. The tag at the start of each prototype's comment says how it is registered:
//   (no tag)      funcs_register(name, FUNCS_name): the signature is already FuncsFn
//   [args]        funcs_register_args(name, FUNCS_name)
//   [wrap: ...]   funcs_register(name, a static wrapper (UIElement *e) that calls it with the arguments shown): what
//                 the Lua function gets when the UI calls it with the element
//   [direct]      not registered: nothing can reach it by string; it is only called by name from C
// G.FUNCS of the other files:
//   functions/state_events.lua (state_events.h): play_cards_from_highlighted (no tag) and
//     discard_cards_from_highlighted [wrap: (e, false)] are registered here too (state_events.c may also do it: the
//     same function, harmless). draw_from_deck_to_hand, get_poker_hand_info, evaluate_play,
//     draw_from_play_to_discard, draw_from_play_to_hand, draw_from_discard_to_deck, draw_from_hand_to_deck,
//     draw_from_hand_to_discard, evaluate_round, tutorial_controller, tutorial_part: [direct].
//   functions/UI_definitions.lua: false_ret (5303, assigned each time G.UIDEF.run_setup runs; nothing refers to it):
//     not registered here. DT_add_money, DT_add_round, DT_add_ante, DT_add_hand, DT_add_discard, DT_reroll_boss,
//     DT_toggle_background, DT_add_chips, DT_add_mult, DT_x_chips, DT_x_mult, DT_chip_mult_reset, DT_win_game,
//     DT_lose_game, DT_jimbo_toggle, DT_jimbo_talk (7-59, assigned when create_UIBox_debug_tools runs): debug only,
//     not translated.
//   game.lua: col1change, col2change, edition_change, do_time (opt_callback), cb (slider callback), pulseme,
//     spawn_joker, rem_joker (1315-1331, assigned when create_UIBox_sandbox_controls runs inside Game:sandbox):
//     not registered here; if game.c translates the sandbox it registers them itself at that point, as the Lua.
void funcs_register_all(void);

// ---------------------------------------------------------------- conventions of this file
// e: the UI element that called the function. "e unused" means the function does not look at it: direct callers
// pass NULL. A UI node's config is e->config (Table); config.ref_table is a Table or an object (O(card)...) as each
// comment says; config.colour and the like hold a colour as the UI stores it (ui.h).
// Some call sites of the Lua pass a made-up table instead of an element ({config = {...}}): the prototype's comment
// says how that call is written in C (NULL plus an argument). Do not build a fake UIElement.
// args of the [args] callbacks, as option_cycle builds it: {from_val, to_val (the option: a string or a number),
// from_key, to_key (its 1-based index: numbers), cycle_config (Table: the cycle's args, e.config.ref_table of the
// arrow)}. A direct caller may pass only some keys.

// ---------------------------------------------------------------- G.FUNCS, in file order
// button_callbacks.lua:5. e unused: NULL from engine/ui.lua:976. The tutorial's next step.
void FUNCS_tut_next(UIElement *e);
// button_callbacks.lua:20. e.config.ref_table: O(Card), the joker.
void FUNCS_blueprint_compat(UIElement *e);
// button_callbacks.lua:36. e unused.
void FUNCS_sort_hand_suit(UIElement *e);
// button_callbacks.lua:45. e unused.
void FUNCS_sort_hand_value(UIElement *e);
// button_callbacks.lua:55. e.config.ref_table: O(Card). Sets e.config.button to 'buy_from_shop' or nil.
void FUNCS_can_buy(UIElement *e);
// button_callbacks.lua:77. e.config.ref_table: O(Card).
void FUNCS_can_buy_and_use(UIElement *e);
// button_callbacks.lua:96. e.config.ref_table: O(Card), a voucher.
void FUNCS_can_redeem(UIElement *e);
// button_callbacks.lua:111. e.config.ref_table: O(Card), a booster.
void FUNCS_can_open(UIElement *e);
// button_callbacks.lua:125. e unused.
void FUNCS_HUD_blind_visible(UIElement *e);
// button_callbacks.lua:138. e.config.ref_value: the index (number) into G.GAME.blind.loc_debuff_lines.
void FUNCS_HUD_blind_debuff(UIElement *e);
// button_callbacks.lua:160. e.config.ref_table: Table whose 'val' it writes (a string).
void FUNCS_HUD_blind_debuff_prefix(UIElement *e);
// button_callbacks.lua:171.
void FUNCS_HUD_blind_reward(UIElement *e);
// button_callbacks.lua:193. [wrap: (e), result dropped]. Returns _can_continue: true, or false as the Lua's nil
// (also when e.config.func is nil and nothing runs). e NULL: the made-up {config = {func = true}} of
// UI_definitions.lua:5302 (the body runs; the writes to e.config are skipped).
bool FUNCS_can_continue(UIElement *e);
// button_callbacks.lua:217.
void FUNCS_can_load_profile(UIElement *e);
// button_callbacks.lua:227. [wrap: (true)]: as a button the Lua gets the element as delete_prof_data, which is
// truthy. Direct call: button_callbacks.lua:298 passes true. false as nil (no caller passes it).
void FUNCS_load_profile(bool delete_prof_data);
// button_callbacks.lua:261.
void FUNCS_can_delete_profile(UIElement *e);
// button_callbacks.lua:273.
void FUNCS_delete_profile(UIElement *e);
// button_callbacks.lua:306.
void FUNCS_can_unlock_all(UIElement *e);
// button_callbacks.lua:316.
void FUNCS_unlock_all(UIElement *e);
// button_callbacks.lua:372. e.config.id: a key of G.GAME.round_scores.
void FUNCS_high_score_alert(UIElement *e);
// button_callbacks.lua:388. e.config.ref_table: Table, a language of G.LANGUAGES, or nil.
void FUNCS_beta_lang_alert(UIElement *e);
// button_callbacks.lua:408. (engine/ui.lua:463 compares config.func with this name.)
void FUNCS_set_button_pip(UIElement *e);
// button_callbacks.lua:429. The text input's cursor.
void FUNCS_flash(UIElement *e);
// button_callbacks.lua:447. e.config.ref_table: O(DynaText).
void FUNCS_pip_dynatext(UIElement *e);
// button_callbacks.lua:463. e.config.ref_table: Table, the toggle's args; e.config.toggle_callback: a V_FN called
// with one argument (the new boolean), or nil.
void FUNCS_toggle_button(UIElement *e);
// button_callbacks.lua:474. e.config.ref_table: Table, the toggle's args.
void FUNCS_toggle(UIElement *e);
// button_callbacks.lua:496. e: the slider's bar; its first child's config.ref_table: Table, the slider's args.
// Calls funcs_lookup_args(rt.callback)(rt) when rt.callback is set.
void FUNCS_slider(UIElement *e);
// button_callbacks.lua:518. [direct] (engine/controller.lua:1352-1365). per: the fraction of the range to add;
// every caller passes a number (the Lua's "if per" is always true).
void FUNCS_slider_descreet(UIElement *e, num per);
// button_callbacks.lua:537. e: an arrow of the cycle; e.config.ref_table: Table, the cycle's args; e.config.ref_value
// 'l' or 'r'. Calls funcs_lookup_args(opt_callback)(a new table: see "args" above) when opt_callback is set.
void FUNCS_option_cycle(UIElement *e);

// ---- cycle callbacks
// button_callbacks.lua:591. [args]. args NULL as nil ({}).
void FUNCS_test_framework_cycle_callback(Table *args);
// button_callbacks.lua:602. [args]. args NULL, or without cycle_config: returns.
void FUNCS_your_collection_joker_page(Table *args);
// button_callbacks.lua:627. [args]. args NULL, or without cycle_config: returns. (Tarots and planets.)
void FUNCS_your_collection_tarot_page(Table *args);
// button_callbacks.lua:649. [args]. args NULL, or without cycle_config: returns.
void FUNCS_your_collection_spectral_page(Table *args);
// button_callbacks.lua:675. [args]. args NULL, or without cycle_config: returns.
void FUNCS_your_collection_booster_page(Table *args);
// button_callbacks.lua:701. [args]. args NULL, or without cycle_config: returns.
void FUNCS_your_collection_voucher_page(Table *args);
// button_callbacks.lua:726. [args]. Reads to_key. (No opt_callback uses it in 1.0.1o.)
void FUNCS_change_selected_back(Table *args);
// button_callbacks.lua:734. [args]. Reads to_key, to_val.
void FUNCS_change_viewed_back(Table *args);
// button_callbacks.lua:743. [args]. Reads to_key. Direct call: UI_definitions.lua:5992 with {to_key = G.viewed_stake}.
void FUNCS_change_stake(Table *args);
// button_callbacks.lua:752. [args]. Reads to_key. Sets G.SETTINGS.QUEUED_CHANGE.vsync to 1, 0 or nil.
void FUNCS_change_vsync(Table *args);
// button_callbacks.lua:763. [args]. Reads to_key.
void FUNCS_change_screen_resolution(Table *args);
// button_callbacks.lua:776. [args]. Reads to_key.
void FUNCS_change_screenmode(Table *args);
// button_callbacks.lua:787. [args]. Reads to_key.
void FUNCS_change_display(Table *args);
// button_callbacks.lua:793. [direct]. Rebuilds the resolution cycle of the open settings menu.
void FUNCS_change_window_cycle_UI(void);
// button_callbacks.lua:817. [args]. Reads to_val (a number).
void FUNCS_change_gamespeed(Table *args);
// button_callbacks.lua:826. [args]. Reads to_key.
void FUNCS_change_play_discard_position(Table *args);
// button_callbacks.lua:842. [args]. Reads to_key. Direct calls: game.lua:1533, 1724 with {to_key = 1 or 2}.
void FUNCS_change_shadows(Table *args);
// button_callbacks.lua:852. [args]. Reads to_key.
void FUNCS_change_pixel_smoothing(Table *args);
// button_callbacks.lua:863. [args]. Reads to_key.
void FUNCS_change_crt_bloom(Table *args);
// button_callbacks.lua:868. [args]. Reads to_key, cycle_config.curr_suit.
void FUNCS_change_collab(Table *args);

// ---- text entry
// button_callbacks.lua:887. e.config.ref_table: Table {key = string}.
void FUNCS_key_button(UIElement *e);
// button_callbacks.lua:899. e.config.ref_table: Table, the text input's args.
void FUNCS_text_input(UIElement *e);
// button_callbacks.lua:932.
void FUNCS_paste_seed(UIElement *e);
// button_callbacks.lua:953. Hooks e.children[1].children[1] as G.CONTROLLER.text_input_hook.
void FUNCS_select_text_input(UIElement *e);
// button_callbacks.lua:966. [direct] (engine/controller.lua:779, and paste_seed). args: {key = string, caps = bool};
// NULL as nil ({}). The function WRITES args.key and args.caps: pass a table of your own.
void FUNCS_text_input_key(Table *args);
// button_callbacks.lua:1048. The text of the hooked input, from its letter nodes ('' if none).
const char *GET_TEXT_FROM_INPUT(void);
// button_callbacks.lua:1066. args: {letter = string, text_table = Table, pos = number, delete = bool}; NULL as {}.
// Recursive.
void MODIFY_TEXT_INPUT(Table *args);
// button_callbacks.lua:1099. amount: -1, 0 or 1 (letters to move the cursor). note: with 0 the Lua's dir is 0/0, a
// NaN that is never used because the loop does not run.
void TRANSPOSE_TEXT_INPUT(int amount);
// button_callbacks.lua:1131.
void FUNCS_can_apply_window_changes(UIElement *e);
// button_callbacks.lua:1158. [wrap: (false)]: as a button the Lua gets the element as _initial, which is not `true`.
// Direct call: game.lua:1139 passes true. false as nil.
void FUNCS_apply_window_changes(bool _initial);
// button_callbacks.lua:1197. update_alert of every card of G.your_collection (UI_definitions.lua calls it too).
void INIT_COLLECTION_CARD_ALERTS(void);

// ---- run setup
// button_callbacks.lua:1214. e.config.object: the UIBox it replaces; e.config.id: the back's name.
void FUNCS_RUN_SETUP_check_back(UIElement *e);
// button_callbacks.lua:1227.
void FUNCS_RUN_SETUP_check_back_name(UIElement *e);
// button_callbacks.lua:1242.
void FUNCS_RUN_SETUP_check_stake(UIElement *e);
// button_callbacks.lua:1253. e.config.id: a stake (number) here.
void FUNCS_RUN_SETUP_check_stake2(UIElement *e);
// button_callbacks.lua:1264. [args]. Reads to_val.
void FUNCS_change_viewed_collab(Table *args);
// button_callbacks.lua:1268.
void FUNCS_CREDITS_check_collab(UIElement *e);
// button_callbacks.lua:1279.
void FUNCS_RUN_SETUP_check_back_stake_column(UIElement *e);
// button_callbacks.lua:1291. e.config.id: a stake (number).
void FUNCS_RUN_SETUP_check_back_stake_highlight(UIElement *e);
// button_callbacks.lua:1299. e: a tab's button, NULL as nil: returns (get_UIE_by_ID may find none at :301, :364,
// :1191). e.config.ref_table: Table {tab_definition_function = V_FN, tab_definition_function_args = any}.
void FUNCS_change_tab(UIElement *e);

// ---- overlay menus
// button_callbacks.lua:1328. [direct]. args: {definition = Table (the UIBox definition), config = Table or nil
// {align, offset, major, no_esc}}; NULL as nil: returns. Replaces args.config by a new table. Sets G.OVERLAY_MENU.
void FUNCS_overlay_menu(Table *args);
// button_callbacks.lua:1359. [wrap: ()]. Removes G.OVERLAY_MENU, unpauses, saves the settings.
void FUNCS_exit_overlay_menu(void);
// button_callbacks.lua:1374. [wrap: ()].
void FUNCS_continue_unlock(void);
// button_callbacks.lua:1380. [direct]. options: passed on to create_UIBox_test_framework. (No caller in 1.0.1o.)
void FUNCS_test_framework(Table *options);
// button_callbacks.lua:1387. e unused.
void FUNCS_options(UIElement *e);
// button_callbacks.lua:1394. [wrap: (e, false)]. e unused. simple: false as nil. (No caller in 1.0.1o.)
void FUNCS_current_hands(UIElement *e, bool simple);
// button_callbacks.lua:1401. e unused.
void FUNCS_run_info(UIElement *e);
// button_callbacks.lua:1408. e unused: NULL from card.lua:4621, cardarea.lua:615.
void FUNCS_deck_info(UIElement *e);
// button_callbacks.lua:1421. [wrap: (e, false)]. e unused. instant: false as nil (the menu slides in from y = 10).
void FUNCS_settings(UIElement *e, bool instant);
// button_callbacks.lua:1429. e unused.
void FUNCS_show_credits(UIElement *e);
// button_callbacks.lua:1436. e unused.
void FUNCS_language_selection(UIElement *e);
// button_callbacks.lua:1443. e unused.
void FUNCS_your_collection(UIElement *e);
// button_callbacks.lua:1450. e unused.
void FUNCS_your_collection_blinds(UIElement *e);
// button_callbacks.lua:1457. e unused.
void FUNCS_your_collection_jokers(UIElement *e);
// button_callbacks.lua:1464. e unused.
void FUNCS_your_collection_tarots(UIElement *e);
// button_callbacks.lua:1471. e unused.
void FUNCS_your_collection_planets(UIElement *e);
// button_callbacks.lua:1478. e unused.
void FUNCS_your_collection_spectrals(UIElement *e);
// button_callbacks.lua:1485. e unused.
void FUNCS_your_collection_vouchers(UIElement *e);
// button_callbacks.lua:1492. e unused.
void FUNCS_your_collection_enhancements_exit_overlay_menu(UIElement *e);
// button_callbacks.lua:1499. e unused.
void FUNCS_your_collection_enhancements(UIElement *e);
// button_callbacks.lua:1506. e unused.
void FUNCS_your_collection_decks(UIElement *e);
// button_callbacks.lua:1513. e unused.
void FUNCS_your_collection_editions(UIElement *e);
// button_callbacks.lua:1520. e unused.
void FUNCS_your_collection_tags(UIElement *e);
// button_callbacks.lua:1527. e unused.
void FUNCS_your_collection_seals(UIElement *e);
// button_callbacks.lua:1534. e unused.
void FUNCS_your_collection_boosters(UIElement *e);
// button_callbacks.lua:1541. Reads e.config.id ('from_game_over' or other): e never NULL.
void FUNCS_challenge_list(UIElement *e);
// button_callbacks.lua:1549. e unused.
void FUNCS_high_scores(UIElement *e);
// button_callbacks.lua:1556. e unused.
void FUNCS_customize_deck(UIElement *e);
// button_callbacks.lua:1563. e unused.
void FUNCS_usage(UIElement *e);
// button_callbacks.lua:1570. Reads e.config.id ('from_game_over', 'from_game_won', 'challenge_list' or other): e
// never NULL.
void FUNCS_setup_run(UIElement *e);
// button_callbacks.lua:1578. (High scores come from the HTTP manager: G.ARGS.HIGH_SCORE_RESPONSE is never set here.)
void FUNCS_wait_for_high_scores(UIElement *e);
// button_callbacks.lua:1589. The second event reads e.config.id when it runs: capture e.
void FUNCS_notify_then_setup_run(UIElement *e);
// button_callbacks.lua:1616. e.config.id: the challenge's index. e NULL: the made-up {config = {id = 'nil'}} of
// :1647: the id is the string 'nil', e.config.chosen is not written and desc_area.config.old_chosen becomes nil
// (the Lua stores the made-up table there, which nothing else sees).
void FUNCS_change_challenge_description(UIElement *e);
// button_callbacks.lua:1635. [args]. args NULL, or without cycle_config: returns. Direct call:
// UI_definitions.lua:5509 with {cycle_config = {current_option = 1}}.
void FUNCS_change_challenge_list_page(Table *args);
// button_callbacks.lua:1652. e.config.id: Table, a challenge (its 'id' is read).
void FUNCS_deck_view_challenge(UIElement *e);
// button_callbacks.lua:1661. e unused.
void FUNCS_profile_select(UIElement *e);
// button_callbacks.lua:1675. e unused. love.event.quit().
void FUNCS_quit(UIElement *e);
// button_callbacks.lua:1679. e unused. (Demo only.)
void FUNCS_quit_cta(UIElement *e);
// button_callbacks.lua:1713. e unused. love.system.openURL, as the next seven.
void FUNCS_demo_survey(UIElement *e);
// button_callbacks.lua:1717. e unused.
void FUNCS_louisf_insta(UIElement *e);
// button_callbacks.lua:1721. e unused.
void FUNCS_wishlist_steam(UIElement *e);
// button_callbacks.lua:1725. e unused.
void FUNCS_go_to_playbalatro(UIElement *e);
// button_callbacks.lua:1729. e unused.
void FUNCS_go_to_discord(UIElement *e);
// button_callbacks.lua:1733. e unused.
void FUNCS_go_to_discord_loc(UIElement *e);
// button_callbacks.lua:1737. e unused.
void FUNCS_loc_survey(UIElement *e);
// button_callbacks.lua:1741. e unused.
void FUNCS_go_to_twitter(UIElement *e);
// button_callbacks.lua:1745. e.config.id: the achievement's name.
void FUNCS_unlock_this(UIElement *e);
// button_callbacks.lua:1749. e unused.
void FUNCS_reset_achievements(UIElement *e);
// button_callbacks.lua:1756. [direct]. Sets G.C.SUITS from G.C.SO_1 / SO_2 and rebuilds the cards' sprites.
void FUNCS_refresh_contrast_mode(void);
// The same as a function value: UI_definitions.lua:2307, 2578 pass G.FUNCS.refresh_contrast_mode itself as the
// callback of a toggle (create_toggle{callback = ...}, which FUNCS_toggle_button calls with the new value). C:
// FN(FUNCS_refresh_contrast_mode_fn, NULL). Ignores its arguments, returns NIL.
Val FUNCS_refresh_contrast_mode_fn(void *ctx, int argc, Val *argv);
// button_callbacks.lua:1769. e.config.ref_table: Table, a language.
void FUNCS_warn_lang(UIElement *e);
// button_callbacks.lua:1792. e.config.ref_table: Table, a language, or nil.
void FUNCS_change_lang(UIElement *e);
// button_callbacks.lua:1816. e unused.
void FUNCS_copy_seed(UIElement *e);
// button_callbacks.lua:1824. e: NULL from engine/controller.lua:911; passed on to FUNCS_start_run.
void FUNCS_start_setup_run(UIElement *e);
// button_callbacks.lua:1847. e.config.id: the challenge's index in G.CHALLENGES.
void FUNCS_start_challenge_run(UIElement *e);
// button_callbacks.lua:1852.
void FUNCS_toggle_seeded_run(UIElement *e);
// button_callbacks.lua:1877. e: passed on to FUNCS_start_run.
void FUNCS_start_tutorial(UIElement *e);

// ---- the HUD's numbers
// button_callbacks.lua:1891.
void FUNCS_chip_UI_set(UIElement *e);
// button_callbacks.lua:1899. Also called directly with a real element (game.lua:2435).
void FUNCS_blind_chip_UI_scale(UIElement *e);
// button_callbacks.lua:1905. number: NIL or not a number: returns scale unchanged (pass the Val as it is in the
// table). max < 0: nil, the default 10000. Sets G.E_SWITCH_POINT (1e11) if unset. Also common_events.lua:982.
float scale_number(Val number, float scale, num max);
// button_callbacks.lua:1917. e.config.object: O(DynaText).
void FUNCS_hand_mult_UI_set(UIElement *e);
// button_callbacks.lua:1927. e.config.object: O(DynaText).
void FUNCS_hand_chip_UI_set(UIElement *e);
// button_callbacks.lua:1937. e.config.object: O(DynaText).
void FUNCS_hand_chip_total_UI_set(UIElement *e);
// button_callbacks.lua:1955. [direct]. e.config.object: O(DynaText). _amount: never nil.
void FUNCS_text_super_juice(UIElement *e, float _amount);
// button_callbacks.lua:1963. e.config.id 'flame_chips' or 'flame_mult'; e.config.object: replaced by a Sprite with
// the 'flame' shader. Keeps G.ARGS.flame_handler, G.ARGS.chip_flames, G.ARGS.mult_flames; reads
// G.ARGS.score_intensity.
void FUNCS_flame_handler(UIElement *e);
// button_callbacks.lua:2036. e.config.object: O(DynaText).
void FUNCS_hand_text_UI_set(UIElement *e);

// ---- the run's buttons
// button_callbacks.lua:2048. Sets e.config.button to 'play_cards_from_highlighted' or nil.
void FUNCS_can_play(UIElement *e);
// button_callbacks.lua:2058.
void FUNCS_can_start_run(UIElement *e);
// button_callbacks.lua:2068.
void FUNCS_visible_blind(UIElement *e);
// button_callbacks.lua:2076.
void FUNCS_can_reroll(UIElement *e);
// button_callbacks.lua:2092. Sets e.config.button to 'discard_cards_from_highlighted' or nil.
void FUNCS_can_discard(UIElement *e);
// button_callbacks.lua:2102. e.config.ref_table: O(Card).
void FUNCS_can_use_consumeable(UIElement *e);
// button_callbacks.lua:2112. e.config.ref_table: O(Card).
void FUNCS_can_select_card(UIElement *e);
// button_callbacks.lua:2122. e.config.ref_table: O(Card).
void FUNCS_can_sell_card(UIElement *e);
// button_callbacks.lua:2132.
void FUNCS_can_skip_booster(UIElement *e);
// button_callbacks.lua:2143. e.config.ref_table: Table, the nodes of the tip; set to nil once shown.
void FUNCS_show_infotip(UIElement *e);
// button_callbacks.lua:2155. [wrap: (e, NULL, false, false)]. Uses, opens, redeems or takes the card.
// card NULL: the card is e.config.ref_table (the button; buy_from_shop at :2473 calls (e, NULL, true, false)).
// e NULL with card set: the Lua's made-up {config = {ref_table = card}} of tag.lua:216-276 and game.lua:2205, which
// all pass no mute and no nosave: FUNCS_use_card(NULL, card, false, false). With e NULL every write to e
// (e.config.button, e.disable_button, also in the queued event of :2164) is skipped.
// mute, nosave: false as nil.
void FUNCS_use_card(UIElement *e, Card *card, bool mute, bool nosave);
// button_callbacks.lua:2318. e.config.ref_table: O(Card).
void FUNCS_sell_card(UIElement *e);
// button_callbacks.lua:2328.
void FUNCS_can_confirm_contest_name(UIElement *e);
// button_callbacks.lua:2338. e unused.
void FUNCS_confirm_contest_name(UIElement *e);
// button_callbacks.lua:2347. e unused.
void FUNCS_change_contest_name(UIElement *e);
// button_callbacks.lua:2354. e unused.
void FUNCS_skip_tutorial_section(UIElement *e);
// button_callbacks.lua:2382.
void FUNCS_shop_voucher_empty(UIElement *e);
// button_callbacks.lua:2392. [direct] (cardarea.lua:137 and buy_from_shop). card: never NULL. Returns false (after
// alert_no_space) when there is no room for the card, true otherwise.
bool FUNCS_check_for_buy_space(Card *card);
// button_callbacks.lua:2404. e.config.ref_table: O(Card) (anything else: nothing happens); e.config.id may be
// 'buy_and_use'. (The Lua returns false when there is no space: nobody reads it.)
void FUNCS_buy_from_shop(UIElement *e);
// button_callbacks.lua:2481. e unused.
void FUNCS_toggle_shop(UIElement *e);
// button_callbacks.lua:2513. e.config.ref_table: Table, the blind's prototype (of G.P_BLINDS), read when the queued
// event runs: capture e.
void FUNCS_select_blind(UIElement *e);
// button_callbacks.lua:2558. e: passed on to FUNCS_end_consumeable (unused there).
void FUNCS_skip_booster(UIElement *e);
// button_callbacks.lua:2565. [wrap: (e, -1)]. e unused: NULL from :2283. delayfac < 0: nil, the default 1.
void FUNCS_end_consumeable(UIElement *e, num delayfac);
// button_callbacks.lua:2631. e.config.id 'Small', 'Big' or 'Boss'; e.config.ref_table: Table {run_info, deck}.
void FUNCS_blind_choice_handler(UIElement *e);
// button_callbacks.lua:2718. e.config.ref_table: O(Tag).
void FUNCS_hover_tag_proxy(UIElement *e);
// button_callbacks.lua:2740. The 'tag_container' node's config.ref_table: O(Tag).
void FUNCS_skip_blind(UIElement *e);
// button_callbacks.lua:2784.
void FUNCS_reroll_boss_button(UIElement *e);
// button_callbacks.lua:2800. e unused: NULL from tag.lua:289.
void FUNCS_reroll_boss(UIElement *e);
// button_callbacks.lua:2855. e unused.
void FUNCS_reroll_shop(UIElement *e);
// button_callbacks.lua:2912. Writes e.config.button when G.round_eval exists: e never NULL.
void FUNCS_cash_out(UIElement *e);
// button_callbacks.lua:2958. [wrap: (e, NULL)] (the main menu's 'start_run' button, UI_definitions.lua:6216).
// e NULL as nil (only its config.id == 'restart_button' is tested). args: Game:start_run's {stake, seed, challenge,
// savetext}, NULL as nil; read when the queued event runs: capture it.
void FUNCS_start_run(UIElement *e, Table *args);
// button_callbacks.lua:2981. e unused.
void FUNCS_go_to_menu(UIElement *e);
// button_callbacks.lua:3004. e unused: NULL from UI_definitions.lua:2843. (Demo only.)
void FUNCS_go_to_demo_cta(UIElement *e);
// button_callbacks.lua:3040. e NULL as nil (its config.id 'lose_cta' / 'win_cta' is read otherwise). (Demo only.)
void FUNCS_show_main_cta(UIElement *e);

// ---- the screen wipe
// button_callbacks.lua:3061. [direct]. message: array of strings, NULL as nil (no text). no_card: false as nil (the
// flipping card G.screenwipecard is made). timefac < 0: nil, the default 1. alt_colour: the particles' colour, NULL
// as nil: G.C.BACKGROUND.C. Does nothing if G.screenwipe exists. Callers of the game: () is (NULL, false, -1, NULL);
// :3011 is (NULL, true, -1, G.C.WHITE).
void FUNCS_wipe_on(Table *message, bool no_card, num timefac, const float *alt_colour);
// button_callbacks.lua:3129. [direct]. Queues the events that fade and remove G.screenwipe.
void FUNCS_wipe_off(void);

// ---------------------------------------------------------------- state this file keeps outside its functions
// Not members of anything declared here; where they live is game.h's decision (a member of Game if it declares one,
// G->dyn otherwise): G.OVERLAY_MENU, G.screenwipe, G.screenwipecard, G.SAVED_GAME, G.CHECK_PROFILE_DATA,
// G.focused_profile, G.viewed_stake, G.viewed_collab, G.TAROT_INTERRUPT, G.E_SWITCH_POINT, G.CLIPBOARD,
// G.from_boss_tag, G.jimboed, G.video_organ, G.normal_music_speed, G.playing_card, G.VIEWING_DECK,
// G.STAGE_OBJECT_INTERRUPT, G.VIBRATION, G.DISCOVER_TALLIES, G.PROGRESS, G.ACHIEVEMENTS.
// In G.ARGS (a Table): screenmode_vals, HIGH_SCORE_RESPONSE, hand_chip_total_UI_set, flame_handler, chip_flames,
// mult_flames, score_intensity.
// Fields this file sets on objects of other headers, in dyn unless their header declares a member:
//   UIBox: alignment.offset.py (the y saved while a card is used: nil when not saved; Moveable has no py member),
//          colours (G.screenwipe.colours = {black = colour, white = colour}, eased by wipe_off through ref_table),
//          parent (G.blind_select_opts.boss.parent, :2837: Node.parent)
//   UIElement: disable_button (e.disable_button, :2165, :2409: on the element, not in its config)
//   Card: sticker, playing_card, from_tag, sprite_facing, facing (see card.h)
//   Card_Character: ui_object_updated
#endif
