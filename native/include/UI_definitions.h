// functions/UI_definitions.lua: every screen, panel, popup and widget of the game as a UI definition (the nested
// tables UIBox{definition = ...} takes), plus a few helpers that live in the same file (the shop's cards, the tags'
// HUD boxes, attention_text). Implemented in src/UI_definitions*.c.
//
// The file defines no class: no structs here. G.UIDEF is only a namespace: G.UIDEF.name(...) is UIDEF_name(...).
//
// Conventions used by every prototype below
// - A "definition" / "node" is the Lua's table {n = N(UIT_x), config = T({...}), nodes = T(ARR(...))} exactly as
//   ui.h keeps it (read the top of ui.h: colours are P(float *), objects O(obj), callbacks S(name of a G.FUNCS),
//   ref_table T(table) or O(object), nil holes in 'nodes' are kept: a conditional entry `cond and {...} or nil` is
//   tseti(nodes, i, NIL), i.e. simply not set, at ITS index: the later entries keep their indices).
// - Functions return Table * (a node; a ROOT node unless said otherwise) or UIBox * exactly as the Lua; "NULL as nil"
//   is said where the Lua can return nil.
// - A table of options (`args`) stays a Table * with the Lua's keys. Where the Lua WRITES its defaults into args
//   (args.x = args.x or d) the C does the same: the widgets keep args as their ref_table and the callbacks of
//   button_callbacks.lua read those keys later. "args NULL: {}" means the callee makes a new table.
// - A colour argument is a const float * to 4 floats (node.h), NULL as nil; inside a table it is P(float *).
//   A colour the Lua builds in place ({0,0,0,0.12}, copy_table(G.C.X), {G.C.GREY[1], ..., 0.7}) is a NEW 4-float
//   block that is never freed (it belongs to the definition).
// - bool arguments are the Lua's truthiness (nil and false both false). A float documented "< 0: nil" takes any
//   negative value as nil and the callee applies the default.
// - Functions kept as VALUES in a table are V_FN values (core.h FN(f, ctx)); the wrappers are static functions of
//   the .c that needs them (each just unpacks argv and calls the function declared here):
//     tabs[i].tab_definition_function  called with ONE argument, tabs[i].tab_definition_function_args (NIL when
//                                      absent), by create_tabs (:2096) and G.FUNCS.change_tab
//                                      (button_callbacks.lua:1310); returns T(the ROOT definition).
//                                      The argument is a string (settings_tab, custom_deck_tab, run_setup_option), a
//                                      number (profile_option), a boolean or nil (view_deck, challenges,
//                                      create_UIBox_current_hands) or a table (create_UIBox_usage: {type, set};
//                                      challenge_description_tab: {_id=, _tab=}); the three functions that take no
//                                      argument ignore it.
//     on_demand_tooltip.filler.func    called with ONE argument, filler.args (create_popup_UIBox_tooltip, :1202);
//                                      returns T(a node). The game only stores create_UIBox_hand_tip there (:3045).
//     create_toggle's args.callback    stored as config.toggle_callback; called with ONE argument, the new value of
//                                      ref_table[ref_value] (button_callbacks.lua:466). G.FUNCS.refresh_contrast_mode
//                                      is stored there AS A FUNCTION (:2307, :2578), not by name.
//     create_text_input's args.callback  called with NO argument (button_callbacks.lua:1023; :5389 is a closure).
//   Every other callback (button, func, opt_callback, back_func, update_func) is the NAME of a G.FUNCS entry: S().
// - The closures passed to table.sort and to Event are static functions of the .c.
//
// Methods the Lua replaces on ONE object (as tag.h does for a tag's sprite)
// - temp_blind.hover / temp_blind.stop_hover (:4003, :4021 the collection's blinds; :5834, :5847 a challenge's banned
//   blinds): the AnimatedSprite gets a class of its own, UIDEF_blind_sprite_class (super AnimatedSprite_class, hover
//   = UIDEF_blind_sprite_hover, stop_hover = UIDEF_blind_sprite_stop_hover, the rest inherited): make it with
//   NEW(AnimatedSprite) + AnimatedSprite_init, then set object.cls = &UIDEF_blind_sprite_class (class_init it first
//   if it is not ready). The class object and its two functions are DEFINED in the .c that holds
//   create_UIBox_your_collection_blinds (:3967); the .c that holds UIDEF_challenge_description_tab only uses them.
//   The closures' upvalues are read back from the sprite: `v` (the blind's prototype) is ttab(config, "blind");
//   `discovered` is the bool "discovered" in the sprite's dyn, which the builder sets (v.discovered at :3980; true
//   at :5841). The second closure has no alert branch: its sprite never gets children.alert, so one hover serves
//   both. Fields hung on that sprite, all in ITS dyn: float (true), hovering (bool), hover_tilt (number),
//   discovered (bool, the upvalue above). Its config is replaced by {blind = T(v), force_focus = true}, to which
//   hover adds h_popup and h_popup_config; children.alert is the collection's alert box.
// - base_background.set_alignment (:325, the UIBox G.UIDEF.card_focus_ui returns): Moveable_set_alignment is not a
//   Class slot, so the replacement is the plain function UIDEF_card_focus_ui_set_alignment(card); the only caller
//   of focused_ui:set_alignment() with no arguments is Card:align (card.lua:4110), which calls that function.
//
// Lua GLOBALS this file assigns by accident (no `local`): they are file-level state here, see the externs below.
// Globals it READS that nothing in the game ever assigns, so they are always nil: `disabled` in
// create_UIBox_blind_tag (:1470; in create_UIBox_blind_choice it is a local, false), `badges` (:1170), `maxw` (:1040),
// `show_win_cta` (:2778: the three `not show_win_cta` entries are always built, the win_cta button never).
//
// Fields of G this file creates or replaces (they are where game.h puts them; listed so that nobody looks for them
// here): shop_jokers, shop_vouchers, shop_booster (CardArea), SHOP_SIGN, blind_prompt_box (UIBox), blind_select_opts
// (table {small=, big=, boss=} of O(UIBox), entries absent when hidden), pack_cards (CardArea), HUD_tags (array of
// O(UIBox)), your_collection (an ARRAY of O(CardArea) in the paged collections, but ONE CardArea in
// create_UIBox_your_collection_seals :3785 and create_UIBox_card_unlock :4144), sticker_card (Card), viewed_stake,
// viewed_collab, collab_credits, focused_profile, run_setup_seed, setup_seed, SAVED_GAME, CHALLENGE_PAGE_SIZE,
// CHECK_PROFILE_DATA, VIEWING_DECK, REFRESH_ALERTS, BADGE_COL, exception_queue, GAME.viewed_back, GAME.blind_on_deck,
// GAME.orbital_choices, GAME.round_resets.blind_tags / blind_ante / blind_states, SETTINGS.current_setup,
// SETTINGS.QUEUED_CHANGE, and (debug) debug_tool_config, DT_jimbo.
//
// Not translated (docs/conventions.md): create_UIBox_debug_tools and its G.FUNCS.DT_* (debug), create_UIBox_online_high_scores
// and create_UIBox_high_scores_filling (the HTTP manager). Their prototypes exist so that the file is complete; their
// bodies are TODO_PORT. Nothing translated calls them.
#ifndef UI_DEFINITIONS_H
#define UI_DEFINITIONS_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- file-level state
// The Lua globals `_type` and `_set`, assigned by create_UIBox_usage (:2438) and never declared local. `_type` is
// READ by UIDEF_viewed_stake_option (:5485: `if _type ~= 'Continue'`), so it must persist between calls. NULL as nil
// (their value until the first usage tab is built). Defined in the .c that holds create_UIBox_usage.
extern const char *UI_global_type, *UI_global_set;
// The Lua global `saved_game`, assigned by UIDEF_run_setup_option('Continue') (:5976) to G.SAVED_GAME and read by
// the same function's definition (:6044...). NULL as nil. Defined in the .c that holds UIDEF_run_setup_option.
extern Table *saved_game;
// The class of the blind chips whose hover is replaced (see the top of this file).
extern Class UIDEF_blind_sprite_class;

// ---------------------------------------------------------------- debug, alerts, online scores
// UI_definitions.lua:5 Debug only: not translated (TODO_PORT). It also defines G.FUNCS.DT_add_money, DT_add_round,
// DT_add_ante, DT_add_hand, DT_add_discard, DT_reroll_boss, DT_toggle_background, DT_add_chips, DT_add_mult,
// DT_x_chips, DT_x_mult, DT_chip_mult_reset, DT_win_game, DT_lose_game, DT_jimbo_toggle, DT_jimbo_talk: none exists in C.
Table *create_UIBox_debug_tools(void);
// UI_definitions.lua:145 _achievement: a key of G.P_CENTERS, or an achievement's name, or 'b_challenge'. _type:
// 'achievement', 'Joker', 'Voucher', 'Back' (anything else shows 'ERROR'); never NULL (the caller defaults it).
Table *create_UIBox_notify_alert(const char *_achievement, const char *_type);
// UI_definitions.lua:196 HTTP manager: not translated (TODO_PORT).
Table *create_UIBox_online_high_scores(void);
// UI_definitions.lua:206 HTTP manager (loadstring of the response): not translated (TODO_PORT).
Table *create_UIBox_high_scores_filling(const char *_resp);

// ---------------------------------------------------------------- a card's buttons and popups
// UI_definitions.lua:239 The use / sell (or select) buttons under a highlighted card.
Table *UIDEF_use_and_sell_buttons(Card *card);
// UI_definitions.lua:299 The controller's focus frame around a card, with its buy / redeem / use / sell boxes hung
// by name on the 'ATTACH_TO_ME' element's children (buy_and_use, buy, redeem, use, sell: O(UIBox)). The new box's
// config.parent is the card. playing_card_colour is a NEW colour that an ease_value fades.
UIBox *UIDEF_card_focus_ui(Card *card);
// UI_definitions.lua:325 The closure the Lua stores as base_background.set_alignment (it ignores its arguments):
// re-offsets card.children.focused_ui from the card's place in the room. Called by Card_align (card.lua:4110).
void UIDEF_card_focus_ui_set_alignment(Card *card);
// UI_definitions.lua:382 args: {card = O(Card), parent = O(UIElement), type = 'sell' | 'buy' | 'select' | 'redeem' |
// 'use' | 'buy_and_use', func = S, button = S, card_width = number (nil: 0), buy_and_use = bool}. args NULL: returns
// NULL (the Lua returns nothing).
UIBox *UIDEF_card_focus_button(Table *args);
// UI_definitions.lua:444 text_key: the type varies, it is handed to localize as `key`: S(a key of the tutorial /
// quips texts) or T(a table of lines, :61 and button_callbacks.lua:1704); NIL as nil: 'lq_1' with loc_vars.quip,
// else 'sb_1'. loc_vars: NULL as nil ({}); loc_vars.quip picks the 'quips' texts.
Table *UIDEF_speech_bubble(Val text_key, Table *loc_vars);
// UI_definitions.lua:463 (and again, identical, at :6302: the second definition is the one in force). rect: any
// node; only T.w and T.h are read.
Table *create_UIBox_highlight(Node *rect);
// UI_definitions.lua:469 The deck's summary while the deck is hovered. (The Lua's `args` is never passed nor read.)
// Sorts G.playing_cards in place.
Table *UIDEF_deck_preview(void);
// UI_definitions.lua:621 args: {button = S(the label; nil: "NONE"), func = S(the G.FUNCS of the click) or nil,
// colour = P or nil (G.C.RED), update_func = S or nil, maxw = number or nil}. args is required.
Table *create_UIBox_character_button(Table *args);

// ---------------------------------------------------------------- the shop
// UI_definitions.lua:637 Creates G.shop_jokers, G.shop_vouchers, G.shop_booster and G.SHOP_SIGN, queues the event
// that drops the sign, and returns the shop's definition.
Table *UIDEF_shop(void);
// UI_definitions.lua:742 The next card of the shop's joker row: the tutorial's forced card, a tag's card, or a card
// of a type polled with pseudoseed('cdt' .. ante) (and the 'illusion' polls). NULL as nil: the Lua falls off the
// end when no rate interval holds the polled number.
Card *create_card_for_shop(CardArea *area);
// UI_definitions.lua:802 Queues the event that hangs the price tag and the buy / redeem / open (and buy-and-use)
// boxes on the card (children price, buy_button, buy_and_use_button). type and area are never read: pass what the
// Lua's call passes, NULL as nil.
void create_shop_card_ui(Card *card, const char *type, CardArea *area);

// ---------------------------------------------------------------- attention_text and the play buttons
// UI_definitions.lua:883 The pop-up text of the scoring and of the alerts. args NULL: {}. Keys read: text (S, or
// whatever DynaText takes as string; nil: 'test'), scale (nil: 1), colour (P; nil: G.C.WHITE; COPIED into a new
// colour), hold (seconds; nil: 0; 0.1 * G.SPEEDFACTOR is added), pos ({x=, y=}; nil: {0, 0}), align (nil: 'cm'),
// emboss, cover (O(a Moveable) to cover, or nil), cover_colour (P; nil: G.C.RED; copied), cover_align, cover_padding,
// offset ({x=, y=}), major (O), maxw, noisy (bool), rotate, backdrop_colour (P; copied by the first event),
// backdrop_scale. Keys WRITTEN into args and read back by the two events it queues (so args must outlive the call):
// fade, cover_colour_l, cover_colour_d, uibox_config, AT (O(UIBox); the box gets dyn attention_text = true),
// text (replaced by O(the DynaText)), start_time (G.TIMERS.TOTAL, a num).
void attention_text(Table *args);
// UI_definitions.lua:986 The play / sort / discard row (G.buttons).
Table *create_UIBox_buttons(void);

// ---------------------------------------------------------------- tooltip rows
// UI_definitions.lua:1027 An R node. desc_nodes: array of arrays of nodes (one per line). empty: false as nil.
// maxw: the rows' config.maxw; NIL as nil (only back.lua:79 gives one).
Table *desc_from_rows(Table *desc_nodes, bool empty, Val maxw);
// UI_definitions.lua:1037 An R node. (Its rows' maxw is the unassigned global `maxw`: nil.)
Table *transparent_multiline_text(Table *desc_nodes);
// UI_definitions.lua:1045 An R node. name: the title's text; NULL as nil.
Table *info_tip_from_rows(Table *desc_nodes, const char *name);
// UI_definitions.lua:1056 text_rows: an array of lines with an optional key `lang` (a language table, for the
// rows' config.lang); NULL as "not a table" (what localize gave was not a table): {"ERROR"}.
Table *overlay_infotip(Table *text_rows);
// UI_definitions.lua:1067 An R node, or NULL as nil when name_nodes is nil, not a table (the type varies: it is
// full_UI_table.name) or an empty table. background_colour: NULL as nil (no colour, no emboss).
Table *name_from_rows(Val name_nodes, const float *background_colour);
// UI_definitions.lua:1072 The hover popup of a card from its ability_UIBox_table (in the object's dyn); NULL as nil
// when it has none. card: a Card, or the Sprite of a tag (tag.lua:525, button_callbacks.lua:2724), which is why it is
// a Node *: read debuff, config and ability from the Card's members after is_a(card, &Card_class), else from the
// node's config / dyn as the Lua's field access would. Makes a fresh Moveable for the 'show_infotip' node.
Table *UIDEF_card_h_popup(Node *card);
// UI_definitions.lua:1135 key: 'eternal', 'foil', 'gold_seal'... Creates G.BADGE_COL the first time. Returns the
// G.C colour, or a NEW {1, 0, 0, 1} for an unknown key.
float *get_badge_colour(const char *key);
// UI_definitions.lua:1153 An R node. _string: NULL as nil: 'ERROR'. _badge_col: NULL as nil: G.C.GREEN. _text_col:
// NULL as nil: G.C.WHITE. scaling < 0: nil: 1.
Table *create_badge(const char *_string, const float *_badge_col, const float *_text_col, float scaling);
// UI_definitions.lua:1164 _center: a prototype with a `set` (config.detailed_tooltip of an element).
Table *create_UIBox_detailed_tooltip(Table *_center);
// UI_definitions.lua:1180 tooltip: {title = S or nil, text = array of S or of {ref_table=, ref_value=} (nil: {}),
// filler = {func = V_FN, args = any} or nil}.
Table *create_popup_UIBox_tooltip(Table *tooltip);

// ---------------------------------------------------------------- the HUD and the blind select
// UI_definitions.lua:1210 Also resizes G.GAME.blind (change_dim(1.5, 1.5)).
Table *create_UIBox_HUD_blind(void);
// UI_definitions.lua:1252 Makes the tag's HUD box (appended to G.HUD_tags, creating the array if nil), discovers
// its prototype, tells the run's other tags ('tag_add'), appends it to G.GAME.tags and sets _tag.HUD_tag.
void add_tag(Tag *_tag);
// UI_definitions.lua:1274
Table *create_UIBox_HUD(void);
// UI_definitions.lua:1417 Creates G.blind_prompt_box and G.blind_select_opts, sets G.GAME.blind_on_deck.
Table *create_UIBox_blind_select(void);
// UI_definitions.lua:1461 An R node, or NULL as nil when the blind has no tag. blind_choice: 'Small' or 'Big'.
// run_info: false as nil. Creates G.GAME.round_resets.blind_tags if nil and a new Tag.
Table *create_UIBox_blind_tag(const char *blind_choice, bool run_info);
// UI_definitions.lua:1485 An R node. type: 'Small', 'Big', 'Boss'; NULL as nil: 'Small' (applied AFTER the first
// lines, as in the Lua). run_info: false as nil. Polls pseudoseed('orbital') when the ante has no orbital choice
// for this type yet (the hands are collected with pairs(G.GAME.hands): insertion order here).
Table *create_UIBox_blind_choice(const char *type, bool run_info);
// UI_definitions.lua:1612
Table *create_UIBox_round_evaluation(void);

// ---------------------------------------------------------------- booster packs (each replaces G.pack_cards)
// UI_definitions.lua:1629
Table *create_UIBox_arcana_pack(void);
// UI_definitions.lua:1675
Table *create_UIBox_spectral_pack(void);
// UI_definitions.lua:1721
Table *create_UIBox_standard_pack(void);
// UI_definitions.lua:1767
Table *create_UIBox_buffoon_pack(void);
// UI_definitions.lua:1813
Table *create_UIBox_celestial_pack(void);

// ---------------------------------------------------------------- widgets
// UI_definitions.lua:1859 args NULL: {}. Keys: no_bg, bg_col (P), red_bad, text (S; nil: '!'), y_offset, bump_amount,
// maxw, text_rot, scale.
Table *create_UIBox_card_alert(Table *args);
// UI_definitions.lua:1868 A C node (an R node when args.label). args NULL: {} (which the Lua cannot survive:
// ref_table is required). Keys: ref_table, ref_value (the number it edits), colour (P; G.C.RED), w (1), h (0.5),
// label_scale (0.5), text_scale (0.3), min (0), max (1), decimal_places (0), label (S or nil). Defaults are written
// into args, and args.text (S, the formatted value): args is the ref_table of the bar and of the text.
Table *create_slider(Table *args);
// UI_definitions.lua:1903 An R node (a C node when args.col). args NULL: {}. Keys: ref_table ({}), ref_value
// ('test'), active_colour (P; G.C.RED), inactive_colour (P; G.C.BLACK), w (3), h (0.5), scale (1), label
// ('TEST?'), label_scale (0.4), info (array of S or nil), col (bool), callback (V_FN or nil: see the top). Defaults
// are written into args: args is the button's ref_table.
Table *create_toggle(Table *args);
// UI_definitions.lua:1955 An R node. args NULL: {}. Keys: options (array of S or numbers; nil: {'Option 1',
// 'Option 2'}), current_option (1), opt_callback (S or nil), colour (P; G.C.RED), scale (1), w (2.5), h (0.8),
// text_scale (0.5), ref_table, ref_value, focus_args (table; {}), info (array of S), no_pips, mid (a node shown
// instead of the text), cycle_shoulders, label (S), id, on_demand_tooltip, and whatever the caller adds for its
// callback (curr_suit...). Written into args: the defaults, w / h / text_scale MULTIPLIED by scale,
// current_option_val (the option's raw Val), l = '<', r = '>', focus_args.type = 'cycle'. args is the ref_table.
Table *create_option_cycle(Table *args);
// UI_definitions.lua:2047 An R node. args NULL: {}. Keys: tabs (array of {label = S or number, chosen = bool,
// func = S or nil, tab_definition_function = V_FN, tab_definition_function_args = any}; may have nil holes at its
// END only: it is walked with ipairs and measured with #; nil: three sample tabs), colour, tab_alignment ('cm'),
// opt_callback, scale (1), tab_w (0), tab_h (0), text_scale (0.5), padding (0.1), no_shoulders, snap_to_nav,
// no_loop. Written into args: the defaults and current = {k = index, v = the chosen tab} (a tab must be chosen).
// Builds the chosen tab's UIBox at once.
Table *create_tabs(Table *args);
// UI_definitions.lua:2103 A C node. args NULL: {} (not survivable: ref_table[ref_value] must be a string). Keys:
// ref_table, ref_value, colour (P; COPIED; nil: a copy of G.C.BLUE), hooked_colour (P; copied; nil: G.C.BLUE
// darkened by 0.3), w (2.5), h (0.7), text_scale (0.4), max_length (16), all_caps (false), prompt_text
// (localize('k_enter_text')), extended_corpus, keyboard_offset, callback (V_FN). Written into args: the defaults,
// current_prompt_text = '', text = {ref_table, ref_value, letters = array of max_length one-character S,
// current_position = the string's length}.
Table *create_text_input(Table *args);
// UI_definitions.lua:2141 The on-screen keyboard. args (required): {space_key, backspace_key, return_key} bools.
Table *create_keyboard_input(Table *args);
// UI_definitions.lua:2181 key: one character, or 'backspace', 'return', 'back'. binding: a gamepad button name
// for its pip; NULL as nil.
Table *create_keyboard_button(const char *key, const char *binding);
// UI_definitions.lua:2189 An R node, or NULL as nil when args.dynatext is nil or has fewer than 2 strings
// (DynaText.nstrings). args NULL: {}. Keys: dynatext (O(DynaText)), colour (P; copied), inactive_colour (P; copied;
// nil: a new {0, 0, 0, 0.08}), w (0.07), h (0.07). Written into args: active_colour, inactive_colour, w, h.
Table *create_dynatext_pips(Table *args);

// ---------------------------------------------------------------- options, settings, statistics
// UI_definitions.lua:2208 Queues the event that sets G.REFRESH_ALERTS.
Table *create_UIBox_options(void);
// UI_definitions.lua:2265
Table *create_UIBox_settings(void);
// UI_definitions.lua:2299 tab: 'Game', 'Video', 'Graphics', 'Audio'; anything else (NULL too) gives an empty box.
Table *UIDEF_settings_tab(const char *tab);
// UI_definitions.lua:2344 variables: a table (walked with pairs) of {type = 'cycle' | 'slider' | 'text_input',
// label, options, ref_table, ref_value, min, max}; NULL as nil: {}.
Table *create_UIBox_test_framework(Table *variables);
// UI_definitions.lua:2397
Table *UIDEF_usage_tabs(void);
// UI_definitions.lua:2436 args: {S(type), S(set) or nil} with type 'joker_usage', 'consumeable_usage',
// 'voucher_usage' (a key of the profile); NULL as nil: {} (not survivable). Assigns UI_global_type and
// UI_global_set. The usage table is walked with pairs before the sort by count: insertion order here (ties).
Table *create_UIBox_usage(Table *args);
// UI_definitions.lua:2513
Table *create_UIBox_customize_deck(void);
// UI_definitions.lua:2545 _suit: 'Spades', 'Hearts', 'Clubs', 'Diamonds'.
Table *UIDEF_custom_deck_tab(const char *_suit);
// UI_definitions.lua:2591 Runs fetch_achievements, set_profile_progress, set_discover_tallies; sets G.focused_profile.
Table *create_UIBox_high_scores(void);
// UI_definitions.lua:2623 An R node. _profile_progress: NULL as nil: the current profile's progress; its
// overall_tally and overall_of are written. smaller: false as nil (the Lua's caller passes 0.5: true).
Table *create_progress_box(Table *_profile_progress, bool smaller);
// UI_definitions.lua:2694 An R node, or NULL as nil when the profile has no such high score. score: 'hand',
// 'furthest_round', 'furthest_ante', 'poker_hand', 'most_money', 'win_streak', 'collection'.
Table *create_UIBox_high_scores_row(const char *score);

// ---------------------------------------------------------------- end of run
// UI_definitions.lua:2749
Table *create_UIBox_win(void);
// UI_definitions.lua:2802 (demo)
Table *create_UIBox_exit_CTA(void);
// UI_definitions.lua:2829 (demo)
Table *create_UIBox_small_cta(void);
// UI_definitions.lua:2835 (demo) Plays a video (love.graphics.newVideo): videos are not played on this platform.
Table *create_UIBox_demo_video_CTA(void);
// UI_definitions.lua:2862
Table *create_UIBox_game_over(void);
// UI_definitions.lua:2933 An R node. score: 'hand', 'poker_hand', 'cards_played', 'cards_discarded',
// 'cards_purchased', 'times_rerolled', 'new_collection', 'seed', 'furthest_ante', 'furthest_round', 'defeated_by'.
// text_colour: NULL as nil (each row's own default).
Table *create_UIBox_round_scores_row(const char *score, const float *text_colour);

// ---------------------------------------------------------------- run info
// UI_definitions.lua:3020 An R node: the example cards of a poker hand (empty when the hand has no example). Makes a
// CardArea and its cards, plays 'paper1' with math.random.
Table *create_UIBox_hand_tip(const char *handname);
// UI_definitions.lua:3042 An R node, or NULL as nil when the hand is not visible. simple: false as nil.
Table *create_UIBox_current_hand_row(const char *handname, bool simple);
// UI_definitions.lua:3080 simple: false as nil. The rows that are NULL are holes of the list (tseti with NIL).
Table *create_UIBox_current_hands(bool simple);
// UI_definitions.lua:3105 _show_remaining: false as nil.
Table *UIDEF_deck_info(bool _show_remaining);
// UI_definitions.lua:3129
Table *UIDEF_run_info(void);
// UI_definitions.lua:3154
Table *UIDEF_current_blinds(void);
// UI_definitions.lua:3168 _deck_key: the deck's center key ('b_red'...).
Table *UIDEF_deck_stake_column(const char *_deck_key);
// UI_definitions.lua:3183
Table *UIDEF_current_stake(void);
// UI_definitions.lua:3233 unplayed_only: false as nil. Sets G.VIEWING_DECK, sorts G.playing_cards in place, makes
// one CardArea per suit filled with COPIES of the cards.
Table *UIDEF_view_deck(bool unplayed_only);
// UI_definitions.lua:3404 A C node. pos: {x=, y=} of the 'ui_1' / 'ui_2' atlas (absent coordinates: 0). value: the
// type varies: T({{string=, colour=}, {string=, colour=}}) (shown as a DynaText unless both strings are equal, then
// as plain text in the first's colour), or S / N for plain text; NIL as nil: 'NIL'. tooltip: array of S.
Table *tally_sprite(Table *pos, Val value, Table *tooltip);
// UI_definitions.lua:3426
Table *UIDEF_used_vouchers(void);

// ---------------------------------------------------------------- the collection (each replaces G.your_collection)
// UI_definitions.lua:3497 Runs set_discover_tallies; queues the event that sets G.REFRESH_ALERTS.
Table *create_UIBox_your_collection(void);
// UI_definitions.lua:3535
Table *create_UIBox_your_collection_jokers(void);
// UI_definitions.lua:3577
Table *create_UIBox_your_collection_tarots(void);
// UI_definitions.lua:3619
Table *create_UIBox_your_collection_boosters(void);
// UI_definitions.lua:3661
Table *create_UIBox_your_collection_planets(void);
// UI_definitions.lua:3696
Table *create_UIBox_your_collection_spectrals(void);
// UI_definitions.lua:3739 exit: the back button's G.FUNCS name; NULL as nil: 'your_collection'.
Table *create_UIBox_your_collection_vouchers(const char *exit);
// UI_definitions.lua:3782 exit: as above. G.your_collection is ONE CardArea here.
Table *create_UIBox_your_collection_seals(const char *exit);
// UI_definitions.lua:3809 exit: as above.
Table *create_UIBox_your_collection_enhancements(const char *exit);
// UI_definitions.lua:3840
Table *create_UIBox_your_collection_editions(void);
// UI_definitions.lua:3869 Sets G.GAME.viewed_back to a new Back(b_red) and G.sticker_card; 52 pseudorandom_element
// calls WITHOUT a seed.
Table *create_UIBox_your_collection_decks(void);
// UI_definitions.lua:3912 G.P_TAGS is collected with pairs and sorted by order.
Table *create_UIBox_your_collection_tags(void);
// UI_definitions.lua:3967 exit: as above. G.P_BLINDS is collected with pairs and sorted by order. Its blind chips
// are of UIDEF_blind_sprite_class.
Table *create_UIBox_your_collection_blinds(const char *exit);
// UI_definitions.lua:4003 and :5834 The hover of a blind chip of UIDEF_blind_sprite_class (see the top of this
// file): tilts it, plays 'chips1' with math.random, builds the popup and clears the collection's alert.
void UIDEF_blind_sprite_hover(AnimatedSprite *temp_blind);
// UI_definitions.lua:4021 and :5847 Its stop_hover.
void UIDEF_blind_sprite_stop_hover(AnimatedSprite *temp_blind);
// UI_definitions.lua:4093 blind: a G.P_BLINDS prototype. discovered: false as nil. vars: NULL as nil: blind.vars.
Table *create_UIBox_blind_popup(Table *blind, bool discovered, Table *vars);

// ---------------------------------------------------------------- unlocks
// UI_definitions.lua:4143 card_center: the unlocked Joker's or Voucher's prototype. G.your_collection is ONE
// CardArea here; queues the reveal's events, which compare G.OVERLAY_MENU's dyn joker_unlock_table with card.ID.
Table *create_UIBox_card_unlock(Table *card_center);
// UI_definitions.lua:4224 deck_center: the unlocked deck's prototype. Sets G.GAME.viewed_back; sets the dyn field
// named deck_center.key (true) on each of the 52 cards.
Table *create_UIBox_deck_unlock(Table *deck_center);

// ---------------------------------------------------------------- credits
// UI_definitions.lua:4273 Returns an ARRAY of R nodes (not a node): one per line of _lines (array of S).
Table *UIDEF_multiline_credit_text(Table *_lines);
// UI_definitions.lua:4284 (The Lua's `_new_option` is never passed nor read.) Reads G.viewed_collab (nil: 'The
// Binding of Isaac', written back) and G.collab_credits, which the credits' "Collabs" tab creates (:4804).
Table *UIDEF_viewed_collab_option(void);
// UI_definitions.lua:4328 Its tabs' tab_definition_function are closures of this function (static in the .c); the
// "Collabs" one creates G.collab_credits.
Table *UIDEF_credits(void);

// ---------------------------------------------------------------- run setup, profiles, challenges
// UI_definitions.lua:5206 from_game_over: false as nil.
Table *UIDEF_challenges(bool from_game_over);
// UI_definitions.lua:5277 An R node (dailies: G.F_DAILIES is off). Uses math.random.
Table *UIDEF_daily_overview(void);
// UI_definitions.lua:5297 from_game_over: the id of the button that opened it: 'from_game_over', 'from_game_won',
// 'challenge_list'; NULL as false / nil. Registers G.FUNCS.false_ret. Calls G.FUNCS.can_continue with a plain table
// {config = {func = true}} in place of an element and USES ITS RETURN VALUE (:5302).
Table *UIDEF_run_setup(const char *from_game_over);
// UI_definitions.lua:5303 G.FUNCS.false_ret (the Lua returns false; nothing in the game refers to it).
void FUNCS_false_ret(UIElement *e);
// UI_definitions.lua:5339
Table *UIDEF_profile_select(void);
// UI_definitions.lua:5371 _profile: 1..3. Sets G.focused_profile and the profile's name, reads its file.
Table *UIDEF_profile_option(int _profile);
// UI_definitions.lua:5442 A C node. _stake: 1..8, an index of G.P_CENTER_POOLS.Stake.
Table *UIDEF_stake_description(int _stake);
// UI_definitions.lua:5460 _type: G.SETTINGS.current_setup: 'New Run' or 'Continue'; NULL as nil.
Table *UIDEF_stake_option(const char *_type);
// UI_definitions.lua:5479 Clamps G.viewed_stake; its `_type` is the GLOBAL (UI_global_type), not an argument.
Table *UIDEF_viewed_stake_option(void);
// UI_definitions.lua:5502 from_game_over: false as nil (its only use, back_id, is overwritten by 'challenge_list':
// the table constructor gives back_id twice and the last one wins). Sets G.CHALLENGE_PAGE_SIZE; queues the event
// that calls G.FUNCS.change_challenge_list_page with a plain table {cycle_config = {current_option = 1}}.
Table *UIDEF_challenge_list(bool from_game_over);
// UI_definitions.lua:5539 _page: 0-based (the Lua's nil is 0).
Table *UIDEF_challenge_list_page(int _page);
// UI_definitions.lua:5567 _id: the type varies: N(index into G.CHALLENGES), or S('nil') (button_callbacks.lua:1647),
// which like any unknown id gives the "select a challenge" box. daily: never passed nor read. is_row: false as nil
// (true: an R node instead of a ROOT, without the play button).
Table *UIDEF_challenge_description(Val _id, bool daily, bool is_row);
// UI_definitions.lua:5680 args: {_id = N(index into G.CHALLENGES), _tab = 'Rules' | 'Restrictions' | 'Deck'};
// NULL as nil: {}. NULL as nil is also RETURNED for any other _tab. The 'Deck' tab walks G.P_CARDS with pairs when
// the challenge gives no card list (insertion order here; the suits are sorted afterwards). Banned blinds are of
// UIDEF_blind_sprite_class with dyn discovered = true.
Table *UIDEF_challenge_description_tab(Table *args);
// UI_definitions.lua:5961 type: 'New Run' or 'Continue'. Loads G.SAVED_GAME if nil, assigns saved_game (Continue),
// sets G.SETTINGS.current_setup, G.GAME.viewed_back, G.viewed_stake, G.sticker_card, G.setup_seed = '', and calls
// G.FUNCS.change_stake with a plain table {to_key = G.viewed_stake} (:5992). 10 unseeded pseudorandom_element calls.
Table *UIDEF_run_setup_option(const char *type);

// ---------------------------------------------------------------- main menu and generic pieces
// UI_definitions.lua:6126 args (required): {button = S(a gamepad button name: 'a', 'x', 'leftshoulder'...),
// scale = number or nil (0.45)}.
Table *create_button_binding_pip(Table *args);
// UI_definitions.lua:6158 Names the profile "P<n>" if it has no name.
Table *create_UIBox_profile_button(void);
// UI_definitions.lua:6194
Table *create_UIBox_main_menu_buttons(void);
// UI_definitions.lua:6245 (the Lua's spelling) Sets G.SETTINGS.COMP.name = ''.
Table *create_UIBox_main_menu_competittion_name(void);
// UI_definitions.lua:6262 G.LANGUAGES is collected with pairs and sorted by label.
Table *UIDEF_language_selector(void);
// (UI_definitions.lua:6302 create_UIBox_highlight again: declared at :463.)
// UI_definitions.lua:6308 The frame of every overlay menu. args NULL: {}. Keys: contents (an ARRAY of nodes; nil:
// a single T node, which the Lua then uses as the `nodes` list itself: keep that), back_func (S; 'exit_overlay_menu'),
// back_id, back_label (S; localize('b_back')), back_colour (P; G.C.ORANGE), back_delay, back_button (S; 'b'),
// no_back, no_esc (read by the caller's overlay), no_pip, snap_back, bg_colour (P; nil: a new grey with alpha 0.7),
// outline_colour (P; G.C.JOKER_GREY), colour (P; G.C.L_BLACK), padding (0.2), minw (7), infotip (array of lines, or
// whatever localize gave: handed to overlay_infotip by the event it queues, so args must outlive the call).
Table *create_UIBox_generic_options(Table *args);
// UI_definitions.lua:6352 An R node. inner_table: the ARRAY of nodes it wraps. horizontal, flipped: false as nil.
// colour_override: NULL as nil: G.C.DYN_UI.MAIN. background_override: NULL as nil: G.C.DYN_UI.BOSS_DARK.
// padding < 0: nil: 0.08.
Table *UIBox_dyn_container(Table *inner_table, bool horizontal, const float *colour_override,
                           const float *background_override, bool flipped, float padding);
// UI_definitions.lua:6360 An R node (a C node when args.col), or NULL as nil when _loc is NULL or localize(_loc) is
// not a table. args: NULL as nil: {}; keys scale (0.35), colour (P; G.C.UI.TEXT_DARK), shadow, col, padding (0.03).
Table *simple_text_container(const char *_loc, Table *args);
// UI_definitions.lua:6376 An R node (a C node when args.col == true) holding the button. args NULL: {}. Keys:
// button (S; 'exit_overlay_menu'; the STRING 'nil' means no button), func (S), colour (P; G.C.RED), choice, chosen
// (bool or 'vert'), label (array of S or numbers; {'LABEL'}), minw (2.7), maxw (minw - 0.2, also when minw < maxw),
// minh (0.9), scale (0.5), focus_args (table; set_button_pip in it puts the pip on the last label line),
// text_colour (P; G.C.UI.TEXT_LIGHT), col, shadow, count ({tally=, of=}), padding (0), one_press, id (S or N),
// back_func, ref_table, mid. The defaults are written into args.
Table *UIBox_button(Table *args);
#endif
