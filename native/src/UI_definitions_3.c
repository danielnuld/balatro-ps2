// functions/UI_definitions.lua:1274-1902: the HUD, the blind select screen, the round evaluation frame, the booster
// pack screens, the card alert and the slider.
#include "all.h"

// A definition node {n=, config=, nodes=} as a Val (exactly the Lua's table); UI0: a node without `nodes`.
#define UI(t, cfg, nodes) T(TAB("n", N(t), "config", T(cfg), "nodes", T(nodes)))
#define UI0(t, cfg) T(TAB("n", N(t), "config", T(cfg)))
// note: C does not fix the order in which a call's arguments are evaluated, so every object a definition creates
// (DynaText, Sprite, Moveable, UIBox, CardArea) is made in a local first, in the Lua's order.

// args.k = args.k or d
static void dflt(Table *t, const char *k, Val d) {
    if (!truthy(tget(t, k))) tset(t, k, d);
}

// A colour literal of the Lua ({0,0,0,0.12}): a new block that belongs to the definition.
static float *new_colour(float r, float g, float b, float a) {
    float *c = malloc(4 * sizeof(float));
    c[0] = r, c[1] = g, c[2] = b, c[3] = a;
    return c;
}

// Moveable(0,0,0,0)
static Moveable *new_moveable0(void) {
    Moveable *m = NEW(Moveable);
    Moveable_init(m, 0, 0, 0, 0);
    return m;
}

// {{ref_table = t, ref_value = key}}: the string list of a DynaText bound to a table's field
static Val dyna_ref(Table *t, const char *key) {
    return T(ARR(T(TAB("ref_table", T(t), "ref_value", S(key)))));
}

// UI_definitions.lua:1274
Table *create_UIBox_HUD(void) {
    double scale = 0.4;
    Val stake = tget(G->GAME, "stake");
    Sprite *stake_sprite = get_stake_sprite(truthy(stake) ? (int)stake.n : 1, 0.5f);

    double spacing = 0.13;
    float *temp_col = G->C.DYN_UI.BOSS_MAIN;
    float *temp_col2 = G->C.DYN_UI.BOSS_DARK;
    Table *current_round = ttab(G->GAME, "current_round");
    Table *current_hand = ttab(current_round, "current_hand");
    Val en_font = T(ttab(ttab(G->LANGUAGES, "en-us"), "font"));

    // contents.round (:1283)
    DynaText *dt_hands = DynaText_new(TAB("string", dyna_ref(current_round, "hands_left"), "font", en_font,
        "colours", T(ARR(P(G->C.BLUE))), "shadow", B(true), "rotate", B(true), "scale", N(2 * scale)));
    DynaText *dt_discards = DynaText_new(TAB("string", dyna_ref(current_round, "discards_left"), "font", en_font,
        "colours", T(ARR(P(G->C.RED))), "shadow", B(true), "rotate", B(true), "scale", N(2 * scale)));
    DynaText *dt_dollars = DynaText_new(TAB(
        "string", T(ARR(T(TAB("ref_table", T(G->GAME), "ref_value", S("dollars"), "prefix", localize(S("$"), NULL))))),
        "maxw", N(1.35), "colours", T(ARR(P(G->C.MONEY))), "font", en_font, "shadow", B(true), "spacing", N(2),
        "bump", B(true), "scale", N(2.2 * scale)));
    DynaText *dt_ante = DynaText_new(TAB("string", dyna_ref(ttab(G->GAME, "round_resets"), "ante"),
        "colours", T(ARR(P(G->C.IMPORTANT))), "shadow", B(true), "font", en_font, "scale", N(2 * scale)));
    DynaText *dt_round = DynaText_new(TAB("string", dyna_ref(G->GAME, "round"),
        "colours", T(ARR(P(G->C.IMPORTANT))), "shadow", B(true), "scale", N(2 * scale)));

    Table *contents_round = ARR(
        UI(UIT_R, TAB("align", S("cm")), ARR(
            UI(UIT_C, TAB("id", S("hud_hands"), "align", S("cm"), "padding", N(0.05), "minw", N(1.45), "colour", P(temp_col), "emboss", N(0.05), "r", N(0.1)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "minh", N(0.33), "maxw", N(1.35)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("k_hud_hands"), NULL), "scale", N(0.85 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(1.2), "colour", P(temp_col2)), ARR(
                    UI0(UIT_O, TAB("object", O(dt_hands), "id", S("hand_UI_count"))))))),
            UI(UIT_C, TAB("minw", N(spacing)), tnew()),
            UI(UIT_C, TAB("align", S("cm"), "padding", N(0.05), "minw", N(1.45), "colour", P(temp_col), "emboss", N(0.05), "r", N(0.1)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "minh", N(0.33), "maxw", N(1.35)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("k_hud_discards"), NULL), "scale", N(0.85 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm")), ARR(
                    UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(1.2), "colour", P(temp_col2)), ARR(
                        UI0(UIT_O, TAB("object", O(dt_discards), "id", S("discard_UI_count"))))))))))),
        UI(UIT_R, TAB("minh", N(spacing)), tnew()),
        UI(UIT_R, TAB("align", S("cm")), ARR(
            UI(UIT_C, TAB("align", S("cm"), "padding", N(0.05), "minw", N(1.45 * 2 + spacing), "minh", N(1.15), "colour", P(temp_col), "emboss", N(0.05), "r", N(0.1)), ARR(
                UI(UIT_R, TAB("align", S("cm")), ARR(
                    UI(UIT_C, TAB("align", S("cm"), "r", N(0.1), "minw", N(1.28 * 2 + spacing), "minh", N(1), "colour", P(temp_col2)), ARR(
                        UI0(UIT_O, TAB("object", O(dt_dollars), "id", S("dollar_text_UI"))))))))))),
        UI(UIT_R, TAB("minh", N(spacing)), tnew()),
        UI(UIT_R, TAB("align", S("cm")), ARR(
            UI(UIT_C, TAB("id", S("hud_ante"), "align", S("cm"), "padding", N(0.05), "minw", N(1.45), "minh", N(1), "colour", P(temp_col), "emboss", N(0.05), "r", N(0.1)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "minh", N(0.33), "maxw", N(1.35)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("k_ante"), NULL), "scale", N(0.85 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(1.2), "colour", P(temp_col2)), ARR(
                    UI0(UIT_O, TAB("object", O(dt_ante), "id", S("ante_UI_count"))),
                    UI0(UIT_T, TAB("text", S(" "), "scale", N(0.3 * scale))),
                    UI0(UIT_T, TAB("text", S("/ "), "scale", N(0.7 * scale), "colour", P(G->C.WHITE), "shadow", B(true))),
                    UI0(UIT_T, TAB("ref_table", T(G->GAME), "ref_value", S("win_ante"), "scale", N(scale), "colour", P(G->C.WHITE), "shadow", B(true))))))),
            UI(UIT_C, TAB("minw", N(spacing)), tnew()),
            UI(UIT_C, TAB("align", S("cm"), "padding", N(0.05), "minw", N(1.45), "minh", N(1), "colour", P(temp_col), "emboss", N(0.05), "r", N(0.1)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "maxw", N(1.35)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("k_round"), NULL), "minh", N(0.33), "scale", N(0.85 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(1.2), "colour", P(temp_col2), "id", S("row_round_text")), ARR(
                    UI0(UIT_O, TAB("object", O(dt_round), "id", S("round_UI_count"))))))))));

    // contents.hand (:1340)
    float *hand_colour = darken(G->C.BLACK, 0.1f, NULL);
    DynaText *dt_handname = DynaText_new(TAB("string", dyna_ref(current_hand, "handname_text"),
        "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "shadow", B(true), "float", B(true), "scale", N(scale * 1.4)));
    DynaText *dt_chip_total = DynaText_new(TAB("string", dyna_ref(current_hand, "chip_total_text"),
        "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "shadow", B(true), "float", B(true), "scale", N(scale * 1.4)));
    Moveable *flame_chips = new_moveable0();
    DynaText *dt_chips = DynaText_new(TAB("string", dyna_ref(current_hand, "chip_text"),
        "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "font", en_font, "shadow", B(true), "float", B(true), "scale", N(scale * 2.3)));
    Moveable *flame_mult = new_moveable0();
    DynaText *dt_mult = DynaText_new(TAB("string", dyna_ref(current_hand, "mult_text"),
        "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "font", en_font, "shadow", B(true), "float", B(true), "scale", N(scale * 2.3)));

    Val contents_hand =
        UI(UIT_R, TAB("align", S("cm"), "id", S("hand_text_area"), "colour", P(hand_colour), "r", N(0.1), "emboss", N(0.05), "padding", N(0.03)), ARR(
            UI(UIT_C, TAB("align", S("cm")), ARR(
                UI(UIT_R, TAB("align", S("cm"), "minh", N(1.1)), ARR(
                    UI0(UIT_O, TAB("id", S("hand_name"), "func", S("hand_text_UI_set"), "object", O(dt_handname))),
                    UI0(UIT_O, TAB("id", S("hand_chip_total"), "func", S("hand_chip_total_UI_set"), "object", O(dt_chip_total))),
                    UI0(UIT_T, TAB("ref_table", T(current_hand), "ref_value", S("hand_level"), "scale", N(scale), "colour", P(G->C.UI.TEXT_LIGHT), "id", S("hand_level"), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm"), "minh", N(1), "padding", N(0.1)), ARR(
                    UI(UIT_C, TAB("align", S("cr"), "minw", N(2), "minh", N(1), "r", N(0.1), "colour", P(G->C.UI_CHIPS), "id", S("hand_chip_area"), "emboss", N(0.05)), ARR(
                        UI0(UIT_O, TAB("func", S("flame_handler"), "no_role", B(true), "id", S("flame_chips"), "object", O(flame_chips), "w", N(0), "h", N(0))),
                        UI0(UIT_O, TAB("id", S("hand_chips"), "func", S("hand_chip_UI_set"), "object", O(dt_chips))),
                        UI0(UIT_B, TAB("w", N(0.1), "h", N(0.1))))),
                    UI(UIT_C, TAB("align", S("cm")), ARR(
                        UI0(UIT_T, TAB("text", S("X"), "lang", T(ttab(G->LANGUAGES, "en-us")), "scale", N(scale * 2), "colour", P(G->C.UI_MULT), "shadow", B(true))))),
                    UI(UIT_C, TAB("align", S("cl"), "minw", N(2), "minh", N(1), "r", N(0.1), "colour", P(G->C.UI_MULT), "id", S("hand_mult_area"), "emboss", N(0.05)), ARR(
                        UI0(UIT_O, TAB("func", S("flame_handler"), "no_role", B(true), "id", S("flame_mult"), "object", O(flame_mult), "w", N(0), "h", N(0))),
                        UI0(UIT_B, TAB("w", N(0.1), "h", N(0.1))),
                        UI0(UIT_O, TAB("id", S("hand_mult"), "func", S("hand_mult_UI_set"), "object", O(dt_mult)))))))))));

    // contents.dollars_chips (:1365)
    Val contents_dollars_chips =
        UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "padding", N(0), "colour", P(G->C.DYN_UI.BOSS_MAIN), "emboss", N(0.05), "id", S("row_dollars_chips")), ARR(
            UI(UIT_C, TAB("align", S("cm"), "padding", N(0.1)), ARR(
                UI(UIT_C, TAB("align", S("cm"), "minw", N(1.3)), ARR(
                    UI(UIT_R, TAB("align", S("cm"), "padding", N(0), "maxw", N(1.3)), ARR(
                        UI0(UIT_T, TAB("text", localize(S("k_round"), NULL), "scale", N(0.42), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                    UI(UIT_R, TAB("align", S("cm"), "padding", N(0), "maxw", N(1.3)), ARR(
                        UI0(UIT_T, TAB("text", localize(S("k_lower_score"), NULL), "scale", N(0.42), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))),
                UI(UIT_C, TAB("align", S("cm"), "minw", N(3.3), "minh", N(0.7), "r", N(0.1), "colour", P(G->C.DYN_UI.BOSS_DARK)), ARR(
                    UI0(UIT_O, TAB("w", N(0.5), "h", N(0.5), "object", O(stake_sprite), "hover", B(true), "can_collide", B(false))),
                    UI0(UIT_B, TAB("w", N(0.1), "h", N(0.1))),
                    UI0(UIT_T, TAB("ref_table", T(G->GAME), "ref_value", S("chips_text"), "lang", T(ttab(G->LANGUAGES, "en-us")), "scale", N(0.85), "colour", P(G->C.WHITE), "id", S("chip_UI_count"), "func", S("chip_UI_set"), "shadow", B(true)))))))));

    // contents.buttons (:1383)
    Table *contents_buttons = ARR(
        UI(UIT_C, TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.CLEAR), "shadow", B(true), "id", S("button_area"), "padding", N(0.2)), ARR(
            UI(UIT_R, TAB("id", S("run_info_button"), "align", S("cm"), "minh", N(1.75), "minw", N(1.5), "padding", N(0.05), "r", N(0.1), "hover", B(true), "colour", P(G->C.RED), "button", S("run_info"), "shadow", B(true)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "padding", N(0), "maxw", N(1.4)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("b_run_info_1"), NULL), "scale", N(1.2 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))),
                UI(UIT_R, TAB("align", S("cm"), "padding", N(0), "maxw", N(1.4)), ARR(
                    UI0(UIT_T, TAB("text", localize(S("b_run_info_2"), NULL), "scale", N(1 * scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true),
                        "focus_args", T(TAB("button", S(G->F_GUIDE ? "guide" : "back"), "orientation", S("bm"))), "func", S("set_button_pip"))))))),
            UI(UIT_R, TAB("align", S("cm"), "minh", N(1.75), "minw", N(1.5), "padding", N(0.05), "r", N(0.1), "hover", B(true), "colour", P(G->C.ORANGE), "button", S("options"), "shadow", B(true)), ARR(
                UI(UIT_C, TAB("align", S("cm"), "maxw", N(1.4), "focus_args", T(TAB("button", S("start"), "orientation", S("bm"))), "func", S("set_button_pip")), ARR(
                    UI0(UIT_T, TAB("text", localize(S("b_options"), NULL), "scale", N(scale), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))))));

    // :1401
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.03), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
        UI(UIT_R, TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.DYN_UI.MAIN), "r", N(0.1)), ARR(
            UI(UIT_R, TAB("align", S("cm"), "colour", P(G->C.DYN_UI.BOSS_DARK), "r", N(0.1), "minh", N(30), "padding", N(0.08)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "minh", N(0.3)), tnew()),
                UI(UIT_R, TAB("align", S("cm"), "id", S("row_blind"), "minw", N(1), "minh", N(3.75)), tnew()),
                contents_dollars_chips,
                contents_hand,
                UI(UIT_R, TAB("align", S("cm"), "id", S("row_round")), ARR(
                    UI(UIT_C, TAB("align", S("cm")), contents_buttons),
                    UI(UIT_C, TAB("align", S("cm")), contents_round))))))))));
}

// UI_definitions.lua:1434
static bool ev_blind_prompt_box_offset(void *ctx) {
    G->blind_prompt_box->moveable.alignment.offset.y = 0;
    return true;
}

// {definition = ROOT{UIBox_dyn_container(...)}, config = {align="bmi", offset = {x=0,y=0}}} (:1447-1449)
static UIBox *blind_select_opt_box(Table *container) {
    return UIBox_new(TAB(
        "definition", UI(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(T(container))),
        "config", T(TAB("align", S("bmi"), "offset", T(TAB("x", N(0), "y", N(0)))))));
}

// UI_definitions.lua:1417
Table *create_UIBox_blind_select(void) {
    Table *used_vouchers = ttab(G->GAME, "used_vouchers");
    DynaText *prompt1 = DynaText_new(TAB("string", localize(S("ph_choose_blind_1"), NULL), "colours", T(ARR(P(G->C.WHITE))),
        "shadow", B(true), "bump", B(true), "scale", N(0.6), "pop_in", N(0.5), "maxw", N(5)));
    DynaText *prompt2 = DynaText_new(TAB("string", localize(S("ph_choose_blind_2"), NULL), "colours", T(ARR(P(G->C.WHITE))),
        "shadow", B(true), "bump", B(true), "scale", N(0.7), "pop_in", N(0.5), "maxw", N(5), "silent", B(true)));
    Table *prompt_nodes = ARR(
        UI(UIT_R, TAB("align", S("cm")), ARR(
            UI0(UIT_O, TAB("object", O(prompt1), "id", S("prompt_dynatext1"))))),
        UI(UIT_R, TAB("align", S("cm")), ARR(
            UI0(UIT_O, TAB("object", O(prompt2), "id", S("prompt_dynatext2"))))));
    if (truthy(tget(used_vouchers, "v_retcon")) || truthy(tget(used_vouchers, "v_directors_cut"))) {
        tseti(prompt_nodes, 3, T(UIBox_button(TAB(
            "label", T(ARR(localize(S("b_reroll_boss"), NULL), S(fmt("%s10", vstr(localize(S("$"), NULL)))))),
            "button", S("reroll_boss"), "func", S("reroll_boss_button")))));
    }
    UIElement *row_blind = UIBox_get_UIE_by_ID(G->HUD, S("row_blind"), NULL);
    G->blind_prompt_box = UIBox_new(TAB(
        "definition", UI(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.2)), prompt_nodes),
        "config", T(TAB("align", S("cm"), "offset", T(TAB("x", N(0), "y", N(-15))), "major", O(row_blind), "bond", S("Weak")))));
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_blind_prompt_box_offset}), NULL, false);

    float width = G->hand->moveable.node.T->w;
    Table *blind_states = ttab(ttab(G->GAME, "round_resets"), "blind_states");
    const char *small = tstr(blind_states, "Small"), *big = tstr(blind_states, "Big");
    tsets(G->GAME, "blind_on_deck",
        !(streq(small, "Defeated") || streq(small, "Skipped") || streq(small, "Hide")) ? "Small" :
        !(streq(big, "Defeated") || streq(big, "Skipped") || streq(big, "Hide")) ? "Big" :
        "Boss");

    // note: blind_states is re-read for each test, as the Lua does (create_UIBox_blind_choice writes 'Select' in it)
    G->blind_select_opts = tnew();
    if (!streq(tstr(blind_states, "Small"), "Hide")) {
        Table *choice = create_UIBox_blind_choice("Small", false);
        tseto(G->blind_select_opts, "small", blind_select_opt_box(
            UIBox_dyn_container(ARR(T(choice)), false, get_blind_main_colour("Small"), NULL, false, -1)));
    }
    if (!streq(tstr(blind_states, "Big"), "Hide")) {
        Table *choice = create_UIBox_blind_choice("Big", false);
        tseto(G->blind_select_opts, "big", blind_select_opt_box(
            UIBox_dyn_container(ARR(T(choice)), false, get_blind_main_colour("Big"), NULL, false, -1)));
    }
    if (!streq(tstr(blind_states, "Boss"), "Hide")) {
        Table *choice = create_UIBox_blind_choice("Boss", false);
        float *main_colour = get_blind_main_colour("Boss");
        float *background = mix_colours(G->C.BLACK, get_blind_main_colour("Boss"), 0.8f, NULL);
        tseto(G->blind_select_opts, "boss", blind_select_opt_box(
            UIBox_dyn_container(ARR(T(choice)), false, main_colour, background, false, -1)));
    }

    // :1451 (the hidden ones are holes of the list)
    Table *opts = tnew();
    if (!streq(tstr(blind_states, "Small"), "Hide"))
        tseti(opts, 1, UI0(UIT_O, TAB("align", S("cm"), "object", tget(G->blind_select_opts, "small"))));
    if (!streq(tstr(blind_states, "Big"), "Hide"))
        tseti(opts, 2, UI0(UIT_O, TAB("align", S("cm"), "object", tget(G->blind_select_opts, "big"))));
    if (!streq(tstr(blind_states, "Boss"), "Hide"))
        tseti(opts, 3, UI0(UIT_O, TAB("align", S("cm"), "object", tget(G->blind_select_opts, "boss"))));
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "minw", N(width), "r", N(0.15), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        UI(UIT_R, TAB("align", S("cm"), "padding", N(0.5)), opts))));
    return t;
}

// UI_definitions.lua:1461
Table *create_UIBox_blind_tag(const char *blind_choice, bool run_info) {
    Table *round_resets = ttab(G->GAME, "round_resets");
    if (!truthy(tget(round_resets, "blind_tags"))) tsett(round_resets, "blind_tags", tnew());
    Table *blind_tags = ttab(round_resets, "blind_tags");
    if (!truthy(tget(blind_tags, blind_choice))) return NULL;
    Tag *_tag = NEW(Tag);
    Tag_init(_tag, tstr(blind_tags, blind_choice), false, blind_choice);
    Sprite *_tag_sprite = NULL;
    Table *_tag_ui = Tag_generate_UI(_tag, -1, &_tag_sprite);
    _tag_sprite->moveable.node.states.collide.can = run_info;
    const bool disabled = false; // the Lua reads a global nothing assigns: nil
    return TAB("n", N(UIT_R), "config", T(TAB("id", S("tag_container"), "ref_table", O(_tag), "align", S("cm"))), "nodes", T(ARR(
        UI(UIT_R, TAB("align", S("tm"), "minh", N(0.65)), ARR(
            UI0(UIT_T, TAB("text", localize(S("k_or"), NULL), "scale", N(0.55), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled))))),
        UI(UIT_R, TAB("id", S(fmt("tag_%s", blind_choice)), "align", S("cm"), "r", N(0.1), "padding", N(0.1), "minw", N(1), "can_collide", B(true), "ref_table", O(_tag_sprite)), ARR(
            UI(UIT_C, TAB("id", S("tag_desc"), "align", S("cm"), "minh", N(1)), ARR(
                T(_tag_ui))),
            !run_info ? UI(UIT_C, TAB("align", S("cm"), "colour", P(G->C.UI.BACKGROUND_INACTIVE), "minh", N(0.6), "minw", N(2), "maxw", N(2), "padding", N(0.07), "r", N(0.1), "shadow", B(true), "hover", B(true), "one_press", B(true), "button", S("skip_blind"), "func", S("hover_tag_proxy"), "ref_table", O(_tag)), ARR(
                UI0(UIT_T, TAB("text", localize(S("b_skip_blind"), NULL), "scale", N(0.4), "colour", P(G->C.UI.TEXT_INACTIVE)))))
            : UI(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "emboss", N(0.05), "colour", P(mix_colours(G->C.BLUE, G->C.BLACK, 0.4f, NULL)), "r", N(0.1), "maxw", N(2)), ARR(
                UI0(UIT_T, TAB("text", localize(S("b_skip_reward"), NULL), "scale", N(0.35), "colour", P(G->C.WHITE))))))))));
}

// UI_definitions.lua:1485
Table *create_UIBox_blind_choice(const char *type, bool run_info) {
    if (!truthy(tget(G->GAME, "blind_on_deck"))) {
        tsets(G->GAME, "blind_on_deck", "Small");
    }
    Table *round_resets = ttab(G->GAME, "round_resets");
    if (!run_info) tsets(ttab(round_resets, "blind_states"), tstr(G->GAME, "blind_on_deck"), "Select");

    const bool disabled = false;
    if (!type) type = "Small";

    Table *blind_choice_config = ttab(G->P_BLINDS, tstr(ttab(round_resets, "blind_choices"), type));

    AnimatedSprite *animation = NEW(AnimatedSprite);
    AnimatedSprite_init(animation, 0, 0, 1.4f, 1.4f, ANIMATION_ATLAS_get("blind_chips"), ttab(blind_choice_config, "pos"));
    Sprite_define_draw_steps((Sprite *)animation, ARR(
        T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
        T(TAB("shader", S("dissolve")))));
    Table *extras = NULL;
    Val stake = tget(G->GAME, "stake");
    Sprite *stake_sprite = get_stake_sprite(truthy(stake) ? (int)stake.n : 1, 0.5f);

    if (!truthy(tget(G->GAME, "orbital_choices"))) tsett(G->GAME, "orbital_choices", tnew());
    Table *orbital_choices = ttab(G->GAME, "orbital_choices");
    Val ante = tget(round_resets, "ante");
    if (!truthy(tgetv(orbital_choices, ante))) tsetv(orbital_choices, ante, T(tnew()));
    Table *ante_choices = tgetv(orbital_choices, ante).tab;

    if (!truthy(tget(ante_choices, type))) {
        Table *_poker_hands = tnew();
        // note: pairs(G.GAME.hands) is insertion order here; pseudorandom_element sorts its candidates by key
        for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) {
            if (truthy(tget(it.val.tab, "visible"))) tinsert(_poker_hands, it.key);
        }

        tset(ante_choices, type, pseudorandom_element(_poker_hands, N(pseudoseed("orbital", NULL)), NULL));
    }

    if (streq(type, "Small")) {
        extras = create_UIBox_blind_tag(type, run_info);
    } else if (streq(type, "Big")) {
        extras = create_UIBox_blind_tag(type, run_info);
    } else if (!run_info) {
        DynaText *dt1 = DynaText_new(TAB("string", T(ARR(T(TAB("string", localize(S("ph_up_ante_1"), NULL), "colour", P(G->C.FILTER))))),
            "colours", T(ARR(P(G->C.BLACK))), "scale", N(0.55), "silent", B(true), "pop_delay", N(4.5), "shadow", B(true), "bump", B(true), "maxw", N(3)));
        DynaText *dt2 = DynaText_new(TAB("string", T(ARR(T(TAB("string", localize(S("ph_up_ante_2"), NULL), "colour", P(G->C.WHITE))))),
            "colours", T(ARR(P(G->C.CHANCE))), "scale", N(0.35), "silent", B(true), "pop_delay", N(4.5), "shadow", B(true), "maxw", N(3)));
        DynaText *dt3 = DynaText_new(TAB("string", T(ARR(T(TAB("string", localize(S("ph_up_ante_3"), NULL), "colour", P(G->C.WHITE))))),
            "colours", T(ARR(P(G->C.CHANCE))), "scale", N(0.35), "silent", B(true), "pop_delay", N(4.5), "shadow", B(true), "maxw", N(3)));
        extras = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            UI(UIT_R, TAB("align", S("cm"), "padding", N(0.07), "r", N(0.1), "colour", P(new_colour(0, 0, 0, 0.12f)), "minw", N(2.9)), ARR(
                UI(UIT_R, TAB("align", S("cm")), ARR(
                    UI0(UIT_O, TAB("object", O(dt1))))),
                UI(UIT_R, TAB("align", S("cm")), ARR(
                    UI0(UIT_O, TAB("object", O(dt2))))),
                UI(UIT_R, TAB("align", S("cm")), ARR(
                    UI0(UIT_O, TAB("object", O(dt3))))))))));
    }
    if (!truthy(tget(round_resets, "blind_ante"))) tset(round_resets, "blind_ante", tget(round_resets, "ante"));
    Val loc_target = localize(T(TAB("type", S("raw_descriptions"), "key", tget(blind_choice_config, "key"), "set", S("Blind"),
        "vars", T(ARR(localize(tget(ttab(G->GAME, "current_round"), "most_played_poker_hand"), "poker_hands"))))), NULL);
    Val loc_name = localize(T(TAB("type", S("name_text"), "key", tget(blind_choice_config, "key"), "set", S("Blind"))), NULL);
    Table *text_table = loc_target.tab;
    float *blind_col = get_blind_main_colour(type);
    num blind_amt = get_blind_amount(tnum(round_resets, "blind_ante")) * tnum(blind_choice_config, "mult") *
                    tnum(ttab(G->GAME, "starting_params"), "ante_scaling");

    const char *blind_state = tstr(ttab(round_resets, "blind_states"), type);
    bool _reward = true;
    Table *no_blind_reward = ttab(ttab(G->GAME, "modifiers"), "no_blind_reward");
    if (no_blind_reward && truthy(tget(no_blind_reward, type))) _reward = false;
    if (streq(blind_state, "Select")) blind_state = "Current";
    // (false when not run_info: only the run_info button reads it)
    float *run_info_colour = !run_info ? NULL :
        streq(blind_state, "Defeated") ? G->C.GREY : streq(blind_state, "Skipped") ? G->C.BLUE :
        streq(blind_state, "Upcoming") ? G->C.ORANGE : streq(blind_state, "Current") ? G->C.RED : G->C.GOLD;

    DynaText *dt_name = DynaText_new(TAB("string", loc_name, "colours", T(ARR(P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE))),
        "shadow", B(!disabled), "float", B(!disabled), "y_offset", N(-4), "scale", N(0.45), "maxw", N(2.8)));

    // :1577 the debuff text rows (holes where a line is missing)
    Table *desc_rows = ARR(
        UI(UIT_R, TAB("align", S("cm"), "minh", N(1.5)), ARR(
            UI0(UIT_O, TAB("object", O(animation))))));
    if (truthy(tgeti(text_table, 1))) {
        Table *lines = tnew();
        if (truthy(tgeti(text_table, 1))) {
            tseti(lines, 1, UI(UIT_R, TAB("align", S("cm"), "maxw", N(2.8)), ARR(
                UI0(UIT_T, TAB("id", tget(blind_choice_config, "key"), "ref_table", T(TAB("val", S(""))), "ref_value", S("val"), "scale", N(0.32), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled), "func", S("HUD_blind_debuff_prefix"))),
                UI0(UIT_T, TAB("text", truthy(tgeti(text_table, 1)) ? tgeti(text_table, 1) : S("-"), "scale", N(0.32), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled))))));
        }
        if (truthy(tgeti(text_table, 2))) {
            tseti(lines, 2, UI(UIT_R, TAB("align", S("cm"), "maxw", N(2.8)), ARR(
                UI0(UIT_T, TAB("text", truthy(tgeti(text_table, 2)) ? tgeti(text_table, 2) : S("-"), "scale", N(0.32), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled))))));
        }
        tseti(desc_rows, 2, UI(UIT_R, TAB("align", S("cm"), "minh", N(0.7), "padding", N(0.05), "minw", N(2.9)), lines));
    }

    // :1587 the score and reward box
    Table *score_rows = ARR(
        UI(UIT_R, TAB("align", S("cm"), "maxw", N(3)), ARR(
            UI0(UIT_T, TAB("text", localize(S("ph_blind_score_at_least"), NULL), "scale", N(0.3), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled))))),
        UI(UIT_R, TAB("align", S("cm"), "minh", N(0.6)), ARR(
            UI0(UIT_O, TAB("w", N(0.5), "h", N(0.5), "colour", P(G->C.BLUE), "object", O(stake_sprite), "hover", B(true), "can_collide", B(false))),
            UI0(UIT_B, TAB("h", N(0.1), "w", N(0.1))),
            UI0(UIT_T, TAB("text", S(number_format(N(blind_amt))), "scale", N(score_number_scale(0.9f, N(blind_amt))), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.RED), "shadow", B(!disabled))))));
    if (_reward) {
        tseti(score_rows, 3, UI(UIT_R, TAB("align", S("cm")), ARR(
            UI0(UIT_T, TAB("text", localize(S("ph_blind_reward"), NULL), "scale", N(0.35), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.WHITE), "shadow", B(!disabled))),
            UI0(UIT_T, TAB("text", S(fmt("%s+", str_rep(vstr(localize(S("$"), NULL)), (int)tnum(blind_choice_config, "dollars")))), "scale", N(0.35), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.MONEY), "shadow", B(!disabled))))));
    }

    Table *extras_nodes = tnew();
    if (extras) tseti(extras_nodes, 1, T(extras));

    // :1555 (ref_table = {deck = nil, run_info = run_info})
    Table *t =
        TAB("n", N(UIT_R), "config", T(TAB("id", S(type), "align", S("tm"), "func", S("blind_choice_handler"), "minh", !run_info ? N(10) : NIL, "ref_table", T(TAB("run_info", run_info ? B(true) : NIL)), "r", N(0.1), "padding", N(0.05))), "nodes", T(ARR(
            UI(UIT_R, TAB("align", S("cm"), "colour", P(mix_colours(G->C.BLACK, G->C.L_BLACK, 0.5f, NULL)), "r", N(0.1), "outline", N(1), "outline_colour", P(G->C.L_BLACK)), ARR(
                UI(UIT_R, TAB("align", S("cm"), "padding", N(0.2)), ARR(
                    !run_info ? UI(UIT_R, TAB("id", S("select_blind_button"), "align", S("cm"), "ref_table", T(blind_choice_config), "colour", P(disabled ? G->C.UI.BACKGROUND_INACTIVE : G->C.ORANGE), "minh", N(0.6), "minw", N(2.7), "padding", N(0.07), "r", N(0.1), "shadow", B(true), "hover", B(true), "one_press", B(true), "button", S("select_blind")), ARR(
                        UI0(UIT_T, TAB("ref_table", T(ttab(round_resets, "loc_blind_states")), "ref_value", S(type), "scale", N(0.45), "colour", P(disabled ? G->C.UI.TEXT_INACTIVE : G->C.UI.TEXT_LIGHT), "shadow", B(!disabled)))))
                    : UI(UIT_R, TAB("id", S("select_blind_button"), "align", S("cm"), "ref_table", T(blind_choice_config), "colour", P(run_info_colour), "minh", N(0.6), "minw", N(2.7), "padding", N(0.07), "r", N(0.1), "emboss", N(0.08)), ARR(
                        UI0(UIT_T, TAB("text", localize(S(blind_state), "blind_states"), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))),
                UI(UIT_R, TAB("id", S("blind_name"), "align", S("cm"), "padding", N(0.07)), ARR(
                    UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "outline", N(1), "outline_colour", P(blind_col), "colour", P(darken(blind_col, 0.3f, NULL)), "minw", N(2.9), "emboss", N(0.1), "padding", N(0.07), "line_emboss", N(1)), ARR(
                        UI0(UIT_O, TAB("object", O(dt_name))))))),
                UI(UIT_R, TAB("align", S("cm"), "padding", N(0.05)), ARR(
                    UI(UIT_R, TAB("id", S("blind_desc"), "align", S("cm"), "padding", N(0.05)), ARR(
                        UI(UIT_R, TAB("align", S("cm")), desc_rows),
                        UI(UIT_R, TAB("align", S("cm"), "r", N(0.1), "padding", N(0.05), "minw", N(3.1), "colour", P(G->C.BLACK), "emboss", N(0.05)), score_rows))))))),
            UI(UIT_R, TAB("id", S("blind_extras"), "align", S("cm")), extras_nodes))));
    return t;
}

// UI_definitions.lua:1612
Table *create_UIBox_round_evaluation(void) {
    float width = G->hand->moveable.node.T->w - 2;
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(UIBox_dyn_container(
            ARR(
                UI(UIT_R, TAB("align", S("tm"), "minw", N(width), "minh", N(3), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05)), ARR(
                    UI(UIT_R, TAB("align", S("cm"), "minw", N(width), "minh", N(1.4)), tnew()),
                    UI(UIT_R, TAB("align", S("cm"), "minw", N(width), "id", S("base_round_eval")), tnew()),
                    UI(UIT_R, TAB("align", S("cm"), "minw", N(width), "id", S("bonus_round_eval")), tnew()))),
                UI(UIT_R, TAB("align", S("cm"), "minh", N(0.05)), tnew()),
                UI(UIT_R, TAB("align", S("cm"), "minw", N(width), "id", S("eval_bottom")), tnew())),
            false, NULL, NULL, false, -1)))));
    return t;
}

// UI_definitions.lua:1629, :1675, :1721, :1767, :1813
// note: the five pack functions are the same text except the area's width and the title's key; one body serves
// them. width_mult and width_add: the W of G.pack_cards is _size*G.CARD_W*width_mult + width_add.
static Table *pack_ui(float width_mult, float width_add, const char *title_key) {
    Val _size = tget(G->GAME, "pack_size");
    G->pack_cards = NEW(CardArea);
    CardArea_init(G->pack_cards,
        G->ROOM->T->x + 9 + G->hand->moveable.node.T->x, G->hand->moveable.node.T->y,
        (float)(vnum(_size) * G->CARD_W * width_mult + width_add),
        1.05f * G->CARD_H,
        TAB("card_limit", _size, "type", S("consumeable"), "highlight_limit", N(1)));

    DynaText *dt_title = DynaText_new(TAB("string", localize(S(title_key), NULL), "colours", T(ARR(P(G->C.WHITE))),
        "shadow", B(true), "rotate", B(true), "bump", B(true), "spacing", N(2), "scale", N(0.7), "maxw", N(4), "pop_in", N(0.5)));
    DynaText *dt_choose = DynaText_new(TAB("string", T(ARR(S(fmt("%s ", vstr(localize(S("k_choose"), NULL)))))), "colours", T(ARR(P(G->C.WHITE))),
        "shadow", B(true), "rotate", B(true), "bump", B(true), "spacing", N(2), "scale", N(0.5), "pop_in", N(0.7)));
    DynaText *dt_choices = DynaText_new(TAB("string", dyna_ref(G->GAME, "pack_choices"), "colours", T(ARR(P(G->C.WHITE))),
        "shadow", B(true), "rotate", B(true), "bump", B(true), "spacing", N(2), "scale", N(0.5), "pop_in", N(0.7)));
    Table *title_box = UIBox_dyn_container(ARR(
        UI(UIT_C, TAB("align", S("cm"), "padding", N(0.05), "minw", N(4)), ARR(
            UI(UIT_R, TAB("align", S("bm"), "padding", N(0.05)), ARR(
                UI0(UIT_O, TAB("object", O(dt_title))))),
            UI(UIT_R, TAB("align", S("bm"), "padding", N(0.05)), ARR(
                UI0(UIT_O, TAB("object", O(dt_choose))),
                UI0(UIT_O, TAB("object", O(dt_choices)))))))),
        false, NULL, NULL, false, -1);

    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("tm"), "r", N(0.15), "colour", P(G->C.CLEAR), "padding", N(0.15))), "nodes", T(ARR(
        UI(UIT_R, TAB("align", S("cl"), "colour", P(G->C.CLEAR), "r", N(0.15), "padding", N(0.1), "minh", N(2), "shadow", B(true)), ARR(
            UI(UIT_R, TAB("align", S("cm")), ARR(
                UI(UIT_C, TAB("align", S("cm"), "padding", N(0.1)), ARR(
                    UI(UIT_C, TAB("align", S("cm"), "r", N(0.2), "colour", P(G->C.CLEAR), "shadow", B(true)), ARR(
                        UI0(UIT_O, TAB("object", O(G->pack_cards))))))))),
            UI(UIT_R, TAB("align", S("cm")), tnew()),
            UI(UIT_R, TAB("align", S("tm")), ARR(
                UI(UIT_C, TAB("align", S("tm"), "padding", N(0.05), "minw", N(2.4)), tnew()),
                UI(UIT_C, TAB("align", S("tm"), "padding", N(0.05)), ARR(
                    T(title_box))),
                UI(UIT_C, TAB("align", S("tm"), "padding", N(0.05), "minw", N(2.4)), ARR(
                    UI(UIT_R, TAB("minh", N(0.2)), tnew()),
                    UI(UIT_R, TAB("align", S("tm"), "padding", N(0.2), "minh", N(1.2), "minw", N(1.8), "r", N(0.15), "colour", P(G->C.GREY), "one_press", B(true), "button", S("skip_booster"), "hover", B(true), "shadow", B(true), "func", S("can_skip_booster")), ARR(
                        UI0(UIT_T, TAB("text", localize(S("b_skip"), NULL), "scale", N(0.5), "colour", P(G->C.WHITE), "shadow", B(true), "focus_args", T(TAB("button", S("y"), "orientation", S("bm"))), "func", S("set_button_pip"))))))))))))));
    return t;
}

// UI_definitions.lua:1629
Table *create_UIBox_arcana_pack(void) { return pack_ui(1, 0, "k_arcana_pack"); }
// UI_definitions.lua:1675
Table *create_UIBox_spectral_pack(void) { return pack_ui(1, 0, "k_spectral_pack"); }
// UI_definitions.lua:1721
Table *create_UIBox_standard_pack(void) { return pack_ui(1.1f, 0, "k_standard_pack"); }
// UI_definitions.lua:1767
Table *create_UIBox_buffoon_pack(void) { return pack_ui(1.1f, 0, "k_buffoon_pack"); }
// UI_definitions.lua:1813
Table *create_UIBox_celestial_pack(void) { return pack_ui(1.1f, 0.5f, "k_celestial_pack"); }

// UI_definitions.lua:1859
Table *create_UIBox_card_alert(Table *args) {
    if (!args) args = tnew();
    Val text = tget(args, "text"), scale = tget(args, "scale");
    Val y_offset = tget(args, "y_offset"), bump_amount = tget(args, "bump_amount"), text_rot = tget(args, "text_rot");
    float *colour = truthy(tget(args, "no_bg")) ? G->C.CLEAR :
        truthy(tget(args, "bg_col")) ? UI_colour(args, "bg_col") :
        (truthy(tget(args, "red_bad")) ? darken(G->C.RED, 0.1f, NULL) : G->C.RED);
    DynaText *dt = DynaText_new(TAB("string", truthy(text) ? text : S("!"), "colours", T(ARR(P(G->C.WHITE))), "shadow", B(true), "rotate", B(true),
        "H_offset", truthy(y_offset) ? y_offset : N(0), "bump_rate", truthy(text) ? N(3) : N(7), "bump_amount", truthy(bump_amount) ? bump_amount : N(3),
        "bump", B(true), "maxw", tget(args, "maxw"), "text_rot", truthy(text_rot) ? text_rot : N(0.2),
        "spacing", N(3 * (truthy(scale) ? vnum(scale) : 1)), "scale", truthy(scale) ? scale : N(0.48)));
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR), "refresh_movement", B(true))), "nodes", T(ARR(
        UI(UIT_R, TAB("align", S("cm"), "r", N(0.15), "minw", N(0.42), "minh", N(0.42), "colour", P(colour), "draw_layer", N(1), "emboss", N(0.05), "refresh_movement", B(true)), ARR(
            UI0(UIT_O, TAB("object", O(dt))))))));
}

// UI_definitions.lua:1868
Table *create_slider(Table *args) {
    if (!args) args = tnew();
    dflt(args, "colour", P(G->C.RED));
    dflt(args, "w", N(1));
    dflt(args, "h", N(0.5));
    dflt(args, "label_scale", N(0.5));
    dflt(args, "text_scale", N(0.3));
    dflt(args, "min", N(0));
    dflt(args, "max", N(1));
    dflt(args, "decimal_places", N(0));
    // note: ref_table is a table or an object (ui.h): ui_ref_get reads either
    num value = vnum(ui_ref_get(tget(args, "ref_table"), tget(args, "ref_value")));
    tsets(args, "text", fmt("%.*f", (int)tnum(args, "decimal_places"), value));
    num startval = tnum(args, "w") * (value - tnum(args, "min")) / (tnum(args, "max") - tnum(args, "min"));

    // (min_h is the Lua's spelling: the engine ignores it)
    Table *t =
        TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", tget(args, "w"), "min_h", tget(args, "h"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR), "focus_args", T(TAB("type", S("slider"))))), "nodes", T(ARR(
            UI(UIT_C, TAB("align", S("cl"), "minw", tget(args, "w"), "r", N(0.1), "min_h", tget(args, "h"), "collideable", B(true), "hover", B(true), "colour", P(G->C.BLACK), "emboss", N(0.05), "func", S("slider"), "refresh_movement", B(true)), ARR(
                UI0(UIT_B, TAB("w", N(startval), "h", tget(args, "h"), "r", N(0.1), "colour", tget(args, "colour"), "ref_table", T(args), "refresh_movement", B(true))))),
            UI(UIT_C, TAB("align", S("cm"), "minh", tget(args, "h"), "r", N(0.1), "minw", N(0.8), "colour", tget(args, "colour"), "shadow", B(true)), ARR(
                UI0(UIT_T, TAB("ref_table", T(args), "ref_value", S("text"), "scale", tget(args, "text_scale"), "colour", P(G->C.UI.TEXT_LIGHT), "decimal_places", tget(args, "decimal_places"))))))));
    if (truthy(tget(args, "label"))) {
        t = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(1), "minw", N(1), "padding", N(0.1 * tnum(args, "label_scale")), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            UI(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                UI0(UIT_T, TAB("text", tget(args, "label"), "scale", tget(args, "label_scale"), "colour", P(G->C.UI.TEXT_LIGHT))))),
            UI(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                T(t))))));
    }
    return t;
}
