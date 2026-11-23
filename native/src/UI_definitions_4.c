// functions/UI_definitions.lua:1903-2544: the toggle, option cycle, tabs, text input, keyboard and pips widgets, the
// options and settings menus, the test framework box, the usage statistics and the customize-deck menu.
#include "all.h"

// The Lua globals `_type` and `_set` (UI_definitions.lua:2438, assigned without `local`).
const char *UI_global_type = NULL, *UI_global_set = NULL;

// `t.k or d` for a number
static num num_or(Table *t, const char *key, num d) {
    Val v = tget(t, key);
    return truthy(v) ? vnum(v) : d;
}
// `t.k = t.k or d` for a number
static num default_num(Table *t, const char *key, num d) {
    num n = num_or(t, key, d);
    tsetn(t, key, n);
    return n;
}
// `t.k = t.k or colour`
static void default_colour(Table *t, const char *key, float *colour) {
    if (!truthy(tget(t, key))) tset(t, key, P(colour));
}
// copy_table of a colour / a colour literal: a new 4-float block, never freed (it belongs to the definition)
static float *colour_new(float r, float g, float b, float a) {
    float *c = malloc(4 * sizeof(float));
    c[0] = r, c[1] = g, c[2] = b, c[3] = a;
    return c;
}
static float *colour_copy(const float *c) { return c ? colour_new(c[0], c[1], c[2], c[3]) : NULL; }
// T(t), NIL for NULL (a `cond and {...} or nil` entry)
#define TN(t) T(t)

// The shoulder pip columns of create_option_cycle (:2025, :2027) and create_tabs (:2091, :2093)
static Table *shoulder_pip(const char *button, num offset_x) {
    return TAB("n", N(UIT_C),
               "config", T(TAB("minw", N(0.7), "align", S("cm"), "colour", P(G->C.CLEAR), "func", S("set_button_pip"),
                               "focus_args", T(TAB("button", S(button), "type", S("none"), "orientation", S("cm"), "scale", N(0.7),
                                                   "offset", T(TAB("x", N(offset_x), "y", N(0))))))),
               "nodes", T(tnew()));
}

// UI_definitions.lua:1920-1928 (and :1977-1985): the info lines under a toggle / option cycle; NULL as nil
static Table *info_rows(Table *args, num scale) {
    Table *info = NULL;
    if (truthy(tget(args, "info"))) {
        Table *list = ttab(args, "info");
        info = tnew();
        for (int k = 1; truthy(tgeti(list, k)) || tgeti(list, k).t == V_BOOL; k++) { // ipairs: up to the first nil
            Val v = tgeti(list, k);
            tinsert(info, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05))),
                                "nodes", T(ARR(T(TAB("n", N(UIT_T), "config", T(TAB("text", v, "scale", N(scale), "colour", P(G->C.UI.TEXT_LIGHT))))))))));
        }
        info = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05))), "nodes", T(info));
    }
    return info;
}

// UI_definitions.lua:1903
Table *create_toggle(Table *args) {
    if (!args) args = tnew();
    default_colour(args, "active_colour", G->C.RED);
    default_colour(args, "inactive_colour", G->C.BLACK);
    num w = default_num(args, "w", 3);
    default_num(args, "h", 0.5);
    num scale = default_num(args, "scale", 1);
    if (!truthy(tget(args, "label"))) tsets(args, "label", "TEST?");
    default_num(args, "label_scale", 0.4);
    if (!truthy(tget(args, "ref_table"))) tsett(args, "ref_table", tnew());
    if (!truthy(tget(args, "ref_value"))) tsets(args, "ref_value", "test");

    Sprite *check = NEW(Sprite);
    Sprite_init(check, 0, 0, 0.5 * scale, 0.5 * scale, ASSET_ATLAS_get("icons"), TAB("x", N(1), "y", N(0)));
    check->moveable.node.states.drag.can = false;
    check->moveable.node.states.visible = false;

    Table *info = info_rows(args, 0.25);

    UIType wrap = tbool(args, "col") ? UIT_C : UIT_R;
    Table *t =
        TAB("n", N(wrap), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR), "focus_args", T(TAB("funnel_from", B(true))))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "minw", N(w))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(args, "label"), "scale", tget(args, "label_scale"), "colour", P(G->C.UI.TEXT_LIGHT))))),
                T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.1))))))))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minw", N(0.3 * w))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.BLACK))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.03), "minw", N(0.4 * scale), "minh", N(0.4 * scale), "outline_colour", P(G->C.WHITE), "outline", N(1.2 * scale), "line_emboss", N(0.5 * scale), "ref_table", T(args),
                                                     "colour", tget(args, "inactive_colour"),
                                                     "button", S("toggle_button"), "button_dist", N(0.2), "hover", B(true), "toggle_callback", tget(args, "callback"), "func", S("toggle"), "focus_args", T(TAB("funnel_to", B(true))))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(check))))))))))))))))))));
    if (truthy(tget(args, "info"))) {
        t = TAB("n", N(wrap), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(t),
            TN(info))));
    }
    return t;
}

// UI_definitions.lua:1955
Table *create_option_cycle(Table *args) {
    if (!args) args = tnew();
    default_colour(args, "colour", G->C.RED);
    if (!truthy(tget(args, "options"))) tsett(args, "options", ARR(S("Option 1"), S("Option 2")));
    if (!truthy(tget(args, "current_option"))) tsetn(args, "current_option", 1);
    Table *options = ttab(args, "options");
    tset(args, "current_option_val", tgetv(options, tget(args, "current_option")));
    if (!truthy(tget(args, "opt_callback"))) tset(args, "opt_callback", NIL);
    num scale = default_num(args, "scale", 1);
    if (!truthy(tget(args, "ref_table"))) tset(args, "ref_table", NIL);
    if (!truthy(tget(args, "ref_value"))) tset(args, "ref_value", NIL);
    num w = num_or(args, "w", 2.5) * scale;
    tsetn(args, "w", w);
    num h = num_or(args, "h", 0.8) * scale;
    tsetn(args, "h", h);
    num text_scale = num_or(args, "text_scale", 0.5) * scale;
    tsetn(args, "text_scale", text_scale);
    tsets(args, "l", "<");
    tsets(args, "r", ">");
    if (!truthy(tget(args, "focus_args"))) tsett(args, "focus_args", tnew());
    tsets(ttab(args, "focus_args"), "type", "cycle");

    Table *info = info_rows(args, 0.3 * scale);

    int noptions = tlen(options);
    bool disabled = noptions < 2;
    Table *pips = tnew();
    for (int i = 1; i <= noptions; i++) {
        tseti(pips, tlen(pips) + 1, T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1 * scale), "h", N(0.1 * scale), "r", N(0.05), "id", S(fmt("pip_%d", i)), "colour", P(veq(tget(args, "current_option"), N(i)) ? G->C.WHITE : G->C.BLACK))))));
    }

    Table *choice_pips = !truthy(tget(args, "no_pips")) ? TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N((0.05 - (noptions > 15 ? 0.03 : 0)) * scale))), "nodes", T(pips)) : NULL;
    Val shown_pips = (!disabled && choice_pips) ? T(choice_pips) : NIL; // not disabled and choice_pips or nil

    Table *main_node;
    if (truthy(tget(args, "mid"))) {
        main_node = TAB("n", N(UIT_C), "config", T(TAB("id", S("cycle_main"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05))), "nodes", T(ARR(
                tget(args, "mid"))))),
            shown_pips)));
    } else {
        DynaText *dt = DynaText_new(TAB("string", T(ARR(T(TAB("ref_table", T(args), "ref_value", S("current_option_val"))))), "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "pop_in", N(0), "pop_in_rate", N(8), "reset_pop_in", B(true), "shadow", B(true), "float", B(true), "silent", B(true), "bump", B(true), "scale", N(text_scale), "non_recalc", B(true)));
        main_node = TAB("n", N(UIT_C), "config", T(TAB("id", S("cycle_main"), "align", S("cm"), "minw", N(w), "minh", N(h), "r", N(0.1), "padding", N(0.05), "colour", tget(args, "colour"), "emboss", N(0.1), "hover", B(true), "can_collide", B(true), "on_demand_tooltip", tget(args, "on_demand_tooltip"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(dt))))))))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05))), "nodes", T(tnew()))),
                shown_pips)))))));
    }

    Val arrow_colour = !disabled ? tget(args, "colour") : P(G->C.BLACK);
    Val arrow_button = !disabled ? S("option_cycle") : NIL;
    float *arrow_text_colour = !disabled ? G->C.UI.TEXT_LIGHT : G->C.UI.TEXT_INACTIVE;
    Val id = (truthy(tget(args, "id")) && !truthy(tget(args, "label"))) ? tget(args, "id") : NIL;
    Table *t =
        TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR), "id", id, "focus_args", tget(args, "focus_args"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "minw", N(0.6 * scale), "hover", B(!disabled), "colour", arrow_colour, "shadow", B(!disabled), "button", arrow_button, "ref_table", T(args), "ref_value", S("l"), "focus_args", T(TAB("type", S("none"))))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(args), "ref_value", S("l"), "scale", N(text_scale), "colour", P(arrow_text_colour))))))))),
            T(main_node),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "minw", N(0.6 * scale), "hover", B(!disabled), "colour", arrow_colour, "shadow", B(!disabled), "button", arrow_button, "ref_table", T(args), "ref_value", S("r"), "focus_args", T(TAB("type", S("none"))))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(args), "ref_value", S("r"), "scale", N(text_scale), "colour", P(arrow_text_colour))))))))))));

    if (truthy(tget(args, "cycle_shoulders"))) {
        t = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(shoulder_pip("leftshoulder", -0.1)),
            T(TAB("n", N(UIT_C), "config", T(TAB("id", S("cycle_shoulders"), "padding", N(0.1))), "nodes", T(ARR(T(t))))),
            T(shoulder_pip("rightshoulder", 0.1)))));
    } else {
        t = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.0))), "nodes", T(ARR(
            T(t))));
    }
    if (truthy(tget(args, "label")) || truthy(tget(args, "info"))) {
        Val label = truthy(tget(args, "label")) ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(args, "label"), "scale", N(0.5 * scale), "colour", P(G->C.UI.TEXT_LIGHT))))))))) : NIL;
        t = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "id", truthy(tget(args, "id")) ? tget(args, "id") : NIL)), "nodes", T(ARR(
            label,
            T(t),
            TN(info))));
    }
    return t;
}

// UI_definitions.lua:2061, :2068, :2075 the sample tabs' tab_definition_function; ctx: the letter shown
static Val tabs_sample_fn(void *ctx, int argc, Val *argv) {
    return T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
        T(TAB("n", N(UIT_T), "config", T(TAB("text", S((const char *)ctx), "scale", N(1), "colour", P(G->C.UI.TEXT_LIGHT)))))))));
}

// UI_definitions.lua:2047
Table *create_tabs(Table *args) {
    if (!args) args = tnew();
    default_colour(args, "colour", G->C.RED);
    if (!truthy(tget(args, "tab_alignment"))) tsets(args, "tab_alignment", "cm");
    if (!truthy(tget(args, "opt_callback"))) tset(args, "opt_callback", NIL);
    num scale = default_num(args, "scale", 1);
    default_num(args, "tab_w", 0);
    default_num(args, "tab_h", 0);
    num text_scale = default_num(args, "text_scale", 0.5);
    if (!truthy(tget(args, "tabs"))) {
        // (func = nil in the first one: not set)
        tsett(args, "tabs", ARR(
            T(TAB("label", S("tab 1"), "chosen", B(true), "tab_definition_function", FN(tabs_sample_fn, "A"))),
            T(TAB("label", S("tab 2"), "chosen", B(false), "tab_definition_function", FN(tabs_sample_fn, "B"))),
            T(TAB("label", S("tab 3"), "chosen", B(false), "tab_definition_function", FN(tabs_sample_fn, "C")))));
    }
    Table *tabs = ttab(args, "tabs");

    Table *tab_buttons = tnew();

    for (int k = 1; tgeti(tabs, k).t != V_NIL; k++) { // ipairs
        Table *v = ttabi(tabs, k);
        if (truthy(tget(v, "chosen"))) tsett(args, "current", TAB("k", N(k), "v", T(v)));
        Val label = tget(v, "label");
        tseti(tab_buttons, tlen(tab_buttons) + 1, T(UIBox_button(TAB("id", S(fmt("tab_but_%s", truthy(label) ? vstr(label) : "")), "ref_table", T(v), "button", S("change_tab"), "label", T(ARR(label)), "minh", N(0.8 * scale), "minw", N(2.5 * scale), "col", B(true), "choice", B(true), "scale", N(text_scale), "chosen", tget(v, "chosen"), "func", tget(v, "func"), "focus_args", T(TAB("type", S("none")))))));
    }

    int ntabs = tlen(tabs);
    bool no_shoulders = truthy(tget(args, "no_shoulders"));
    bool shoulders = ntabs > 1 && !no_shoulders;

    // :2096 the chosen tab's box (the only part of the constructor with side effects)
    Table *current = ttab(args, "current");
    if (!current) ERROR("create_tabs: attempt to index field 'current' (a nil value)");
    Table *current_v = ttab(current, "v");
    Val fn_arg = tget(current_v, "tab_definition_function_args");
    Val definition = fcall(tget(current_v, "tab_definition_function"), 1, &fn_arg);
    UIBox *tab_contents = UIBox_new(TAB("definition", definition, "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));

    Table *t =
        TAB("n", N(UIT_R), "config", T(TAB("padding", N(0.0), "align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                shoulders ? T(shoulder_pip("leftshoulder", -0.1)) : NIL,
                T(TAB("n", N(UIT_C), "config", T(TAB("id", S(no_shoulders ? "no_shoulders" : "tab_shoulders"), "ref_table", T(args), "align", S("cm"), "padding", N(0.15), "group", N(1), "collideable", B(true),
                                                 "focus_args", ntabs > 1 ? T(TAB("type", S("tab"), "nav", S("wide"), "snap_to", tget(args, "snap_to_nav"), "no_loop", tget(args, "no_loop"))) : NIL)), "nodes", T(tab_buttons))),
                shoulders ? T(shoulder_pip("rightshoulder", 0.1)) : NIL)))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", tget(args, "tab_alignment"), "padding", N(num_or(args, "padding", 0.1)), "no_fill", B(true), "minh", tget(args, "tab_h"), "minw", tget(args, "tab_w"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("id", S("tab_contents"), "object", O(tab_contents))))))))))));

    return t;
}

// UI_definitions.lua:2103
Table *create_text_input(Table *args) {
    if (!args) args = tnew();
    // copy_table(args.colour) or copy_table(G.C.BLUE)
    float *colour = colour_copy(UI_colour(args, "colour"));
    if (!colour) colour = colour_copy(G->C.BLUE);
    tset(args, "colour", P(colour));
    float *hooked_colour = colour_copy(UI_colour(args, "hooked_colour"));
    if (!hooked_colour) hooked_colour = darken(colour_copy(G->C.BLUE), 0.3f, NULL);
    tset(args, "hooked_colour", P(hooked_colour));
    default_num(args, "w", 2.5);
    default_num(args, "h", 0.7);
    num text_scale = default_num(args, "text_scale", 0.4);
    int max_length = (int)default_num(args, "max_length", 16);
    if (!truthy(tget(args, "all_caps"))) tsetb(args, "all_caps", false);
    if (!truthy(tget(args, "prompt_text"))) tset(args, "prompt_text", localize(S("k_enter_text"), NULL));
    tsets(args, "current_prompt_text", "");

    // args.ref_table[args.ref_value]: ref_table is a table or an object (G for the run's seed)
    Val value = ui_ref_get(tget(args, "ref_table"), tget(args, "ref_value"));
    const char *value_s = vstr(value);
    if (!value_s) ERROR("create_text_input: bad argument #1 to 'len' (string expected)");
    Table *letters = tnew();
    Table *text = TAB("ref_table", tget(args, "ref_table"), "ref_value", tget(args, "ref_value"), "letters", T(letters), "current_position", N(str_len(value_s)));
    Table *ui_letters = tnew();
    for (int i = 1; i <= max_length; i++) {
        const char *sub = truthy(value) ? str_sub(value_s, i, i) : NULL;
        tseti(letters, i, S(sub ? sub : ""));
        tseti(ui_letters, i, T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(letters), "ref_value", N(i), "scale", N(text_scale), "colour", P(G->C.UI.TEXT_LIGHT), "id", S(fmt("letter_%d", i)))))));
    }
    tsett(args, "text", text);

    float *position_text_colour = lighten(colour_copy(G->C.BLUE), 0.4f, NULL);

    tseti(ui_letters, tlen(ui_letters) + 1, T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(args), "ref_value", S("current_prompt_text"), "scale", N(text_scale), "colour", P(lighten(colour_copy(colour), 0.4f, NULL)), "id", S("prompt"))))));
    tseti(ui_letters, tlen(ui_letters) + 1, T(TAB("n", N(UIT_B), "config", T(TAB("r", N(0.03), "w", N(0.1), "h", N(0.4), "colour", P(position_text_colour), "id", S("position"), "func", S("flash"))))));

    // (min_h, not minh: as the Lua)
    Table *t =
        TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "draw_layer", N(1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("id", S("text_input"), "align", S("cm"), "padding", N(0.05), "r", N(0.1), "draw_layer", N(2), "hover", B(true), "colour", P(colour), "minw", tget(args, "w"), "min_h", tget(args, "h"), "button", S("select_text_input"), "shadow", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("ref_table", T(args), "padding", N(0.05), "align", S("cm"), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("ref_table", T(args), "align", S("cm"), "r", N(0.1), "colour", P(G->C.CLEAR), "func", S("text_input"))), "nodes",
                          T(ui_letters))))))))))))));
    return t;
}

// UI_definitions.lua:2141
Table *create_keyboard_input(Table *args) {
    const char *keyboard_rows[5] = {
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM",
        truthy(tget(args, "space_key")) ? " " : NULL};
    Table *keyboard_button_rows[5] = {tnew(), tnew(), tnew(), tnew(), tnew()};
    for (int k = 0; k < 5 && keyboard_rows[k]; k++) { // ipairs
        const char *v = keyboard_rows[k];
        for (int i = 1; i <= str_len(v); i++) {
            const char *c = str_sub(v, i, i);
            tseti(keyboard_button_rows[k], tlen(keyboard_button_rows[k]) + 1, T(create_keyboard_button(c, streq(c, " ") ? "y" : NULL)));
        }
    }
    bool backspace_key = truthy(tget(args, "backspace_key")), return_key = truthy(tget(args, "return_key"));
    Val side = NIL;
    if (backspace_key || return_key) {
        Table *backspace = backspace_key ? create_keyboard_button("backspace", "x") : NULL;
        Table *ret = return_key ? create_keyboard_button("return", "start") : NULL;
        Table *back = create_keyboard_button("back", "b");
        side = T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            backspace ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(T(backspace))))) : NIL,
            ret ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(T(ret))))) : NIL,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(T(back)))))))));
    }
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(15), "r", N(0.1), "colour", P(colour_new(G->C.GREY[0], G->C.GREY[1], G->C.GREY[2], 0.7f)))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.BLACK), "emboss", N(0.05), "r", N(0.1), "mid", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "colour", P(G->C.CLEAR))), "nodes", T(keyboard_button_rows[0]))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "colour", P(G->C.CLEAR))), "nodes", T(keyboard_button_rows[1]))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "colour", P(G->C.CLEAR))), "nodes", T(keyboard_button_rows[2]))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "colour", P(G->C.CLEAR))), "nodes", T(keyboard_button_rows[3]))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "colour", P(G->C.CLEAR))), "nodes", T(keyboard_button_rows[4]))))))),
                    side)))))))))))))));
}

// UI_definitions.lua:2181
Table *create_keyboard_button(const char *key, const char *binding) {
    const char *key_label = streq(key, "backspace") ? "Backspace" : streq(key, " ") ? "Space" : streq(key, "back") ? "Back" : streq(key, "return") ? "Enter" : key;
    return UIBox_button(TAB("label", T(ARR(S(key_label))), "button", S("key_button"), "ref_table", T(TAB("key", S(streq(key, "back") ? "return" : key))),
                            "minw", N(streq(key, " ") ? 6 : streq(key, "return") ? 2.5 : streq(key, "backspace") ? 2.5 : streq(key, "back") ? 2.5 : 0.8),
                            "minh", N(streq(key, "return") ? 1.5 : streq(key, "backspace") ? 1.5 : streq(key, "back") ? 0.8 : 0.7),
                            "col", B(true), "colour", P(G->C.GREY), "focus_args", binding ? T(TAB("button", S(binding), "orientation", S((streq(key, " ") || streq(key, "back")) ? "cr" : "bm"), "set_button_pip", B(true))) : NIL));
}

// UI_definitions.lua:2189
Table *create_dynatext_pips(Table *args) {
    if (!args) args = tnew();

    float *active_colour = colour_copy(UI_colour(args, "colour"));
    if (!active_colour) active_colour = G->C.RED;
    tset(args, "active_colour", P(active_colour));
    float *inactive_colour = colour_copy(UI_colour(args, "inactive_colour"));
    if (!inactive_colour) inactive_colour = colour_new(0, 0, 0, 0.08f);
    tset(args, "inactive_colour", P(inactive_colour));
    default_num(args, "w", 0.07);
    default_num(args, "h", 0.07);

    DynaText *dynatext = truthy(tget(args, "dynatext")) ? (DynaText *)tobj(args, "dynatext") : NULL;
    if (!dynatext || !(dynatext->nstrings > 1)) return NULL;

    Table *pips = tnew();

    for (int i = 1; i <= dynatext->nstrings; i++) {
        tseti(pips, i, T(TAB("n", N(UIT_C), "config", T(TAB("ref_table", O(dynatext), "minw", tget(args, "w"), "minh", tget(args, "h"), "colour", P(G->C.UI.TEXT_INACTIVE), "r", N(0.1), "id", S(fmt("pip_%d", i)), "pipcol1", P(active_colour), "pipcol2", P(inactive_colour), "func", S("pip_dynatext"))))));
    }

    return TAB("n", N(UIT_R), "config", T(TAB("padding", N(0.05), "align", S("cm"))), "nodes", T(pips));
}

// UI_definitions.lua:2218
static bool ev_options_refresh_alerts(void *ctx) {
    G->REFRESH_ALERTS = true;
    return true;
}

// UI_definitions.lua:2208
Table *create_UIBox_options(void) {
    Table *current_seed = NULL;
    Table *restart = NULL;
    Table *main_menu = NULL;
    Table *your_collection = NULL;
    Table *credits = NULL;
    Table *customize = NULL;

    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .func = ev_options_refresh_alerts}), NULL, false);

    if (G->STAGE == STAGES_RUN) {
        restart = UIBox_button(TAB("id", S("restart_button"), "label", T(ARR(localize(S("b_start_new_run"), NULL))), "button", S("setup_run"), "minw", N(5)));
        main_menu = UIBox_button(TAB("label", T(ARR(localize(S("b_main_menu"), NULL))), "button", S("go_to_menu"), "minw", N(5)));
        your_collection = UIBox_button(TAB("label", T(ARR(localize(S("b_collection"), NULL))), "button", S("your_collection"), "minw", N(5), "id", S("your_collection")));
        Table *copy_button = UIBox_button(TAB("col", B(true), "button", S("copy_seed"), "label", T(ARR(localize(S("b_copy"), NULL))), "colour", P(G->C.BLUE), "scale", N(0.3), "minw", N(1.3), "minh", N(0.5)));
        current_seed = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s: ", vstr(localize(S("b_seed"), NULL)))), "scale", N(0.4), "colour", P(G->C.WHITE))))))))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0), "minh", N(0.8))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0), "minh", N(0.8))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(tbool(G->GAME, "seeded") ? G->C.RED : G->C.BLACK), "minw", N(1.8), "minh", N(0.5), "padding", N(0.1), "emboss", N(0.05))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(tget(ttab(G->GAME, "pseudorandom"), "seed"))), "scale", N(0.43), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))))))))))))))))),
            T(copy_button))));
    }
    if (G->STAGE == STAGES_MAIN_MENU) {
        credits = UIBox_button(TAB("label", T(ARR(localize(S("b_credits"), NULL))), "button", S("show_credits"), "minw", N(5)));
    }

    Table *settings = UIBox_button(TAB("button", S("settings"), "label", T(ARR(localize(S("b_settings"), NULL))), "minw", N(5), "focus_args", T(TAB("snap_to", B(true)))));
    Table *high_scores = UIBox_button(TAB("label", T(ARR(localize(S("b_stats"), NULL))), "button", S("high_scores"), "minw", N(5)));
    customize = UIBox_button(TAB("label", T(ARR(localize(S("b_customize_deck"), NULL))), "button", S("customize_deck"), "minw", N(5)));

    // the nil entries are holes: the later ones keep their indices
    Table *t = create_UIBox_generic_options(TAB("contents", T(ARR(
        T(settings),
        tbool(G->GAME, "seeded") ? TN(current_seed) : NIL,
        TN(restart),
        TN(main_menu),
        T(high_scores),
        TN(your_collection),
        T(customize),
        TN(credits)))));
    return t;
}

// tab_definition_function = G.UIDEF.settings_tab
static Val settings_tab_fn(void *ctx, int argc, Val *argv) {
    return T(UIDEF_settings_tab(argc > 0 && argv[0].t == V_STR ? argv[0].s : NULL));
}

// UI_definitions.lua:2265
Table *create_UIBox_settings(void) {
    Table *tabs = tnew();
    tseti(tabs, tlen(tabs) + 1, T(TAB(
        "label", localize(S("b_set_game"), NULL),
        "chosen", B(true),
        "tab_definition_function", FN(settings_tab_fn, NULL),
        "tab_definition_function_args", S("Game"))));
    if (G->F_VIDEO_SETTINGS) {
        tseti(tabs, tlen(tabs) + 1, T(TAB(
            "label", localize(S("b_set_video"), NULL),
            "tab_definition_function", FN(settings_tab_fn, NULL),
            "tab_definition_function_args", S("Video"))));
    }
    tseti(tabs, tlen(tabs) + 1, T(TAB(
        "label", localize(S("b_set_graphics"), NULL),
        "tab_definition_function", FN(settings_tab_fn, NULL),
        "tab_definition_function_args", S("Graphics"))));
    tseti(tabs, tlen(tabs) + 1, T(TAB(
        "label", localize(S("b_set_audio"), NULL),
        "tab_definition_function", FN(settings_tab_fn, NULL),
        "tab_definition_function_args", S("Audio"))));

    Table *tabs_node = create_tabs(TAB(
        "tabs", T(tabs),
        "tab_h", N(7.05),
        "tab_alignment", S("tm"),
        "snap_to_nav", B(true)));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("options"), "contents", T(ARR(T(tabs_node)))));
    return t;
}

// UI_definitions.lua:2299
// note: the children are built one statement each, in the Lua's order (they create sprites and texts: C does not
// order the arguments of a call).
Table *UIDEF_settings_tab(const char *tab) {
    if (streq(tab, "Game")) {
        num GAMESPEED = tnum(G->SETTINGS, "GAMESPEED");
        Table *n1 = create_option_cycle(TAB("label", localize(S("b_set_gamespeed"), NULL), "scale", N(0.8), "options", T(ARR(N(0.5), N(1), N(2), N(4))), "opt_callback", S("change_gamespeed"), "current_option", N(GAMESPEED == 0.5 ? 1 : GAMESPEED == 4 ? 4 : GAMESPEED + 1)));
        Table *n2 = create_option_cycle(TAB("w", N(5), "label", localize(S("b_set_play_discard_pos"), NULL), "scale", N(0.8), "options", localize(S("ml_play_discard_pos_opt"), NULL), "opt_callback", S("change_play_discard_position"), "current_option", tget(G->SETTINGS, "play_button_pos")));
        Table *n3 = G->F_RUMBLE ? create_toggle(TAB("label", localize(S("b_set_rumble"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("rumble"))) : NULL;
        Table *n4 = create_slider(TAB("label", localize(S("b_set_screenshake"), NULL), "w", N(4), "h", N(0.4), "ref_table", T(G->SETTINGS), "ref_value", S("screenshake"), "min", N(0), "max", N(100)));
        Table *n5 = create_toggle(TAB("label", localize(S("ph_display_stickers"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("run_stake_stickers")));
        Table *n6 = create_toggle(TAB("label", localize(S("b_high_contrast_cards"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("colourblind_option"), "callback", FN(FUNCS_refresh_contrast_mode_fn, NULL)));
        Table *n7 = create_toggle(TAB("label", localize(S("b_reduced_motion"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("reduced_motion")));
        Table *n8 = G->F_CRASH_REPORTS ? create_toggle(TAB("label", localize(S("b_set_crash_reports"), NULL), "ref_table", T(G->SETTINGS), "ref_value", S("crashreports"), "info", localize(S("ml_crash_report_info"), NULL))) : NULL;
        return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(n1), T(n2), TN(n3), T(n4), T(n5), T(n6), T(n7), TN(n8))));
    } else if (streq(tab, "Video")) {
        // Reset the queue so there are no pending changes
        tsett(G->SETTINGS, "QUEUED_CHANGE", tnew());

        // Refresh the display information for all displays based on the screenmode selected
        Table *WINDOW = ttab(G->SETTINGS, "WINDOW");
        int res_option = GET_DISPLAYINFO(tstr(WINDOW, "screenmode"), (int)tnum(WINDOW, "selected_display"));

        const char *screenmode = tstr(WINDOW, "screenmode");
        Val display = tgetv(ttab(WINDOW, "DISPLAYS"), tget(WINDOW, "selected_display"));
        if (display.t != V_TAB) ERROR("settings_tab: attempt to index a nil value (G.SETTINGS.WINDOW.DISPLAYS[selected_display])");
        Table *n1 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "label", localize(S("b_set_monitor"), NULL), "options", tget(WINDOW, "display_names"), "opt_callback", S("change_display"), "current_option", tget(WINDOW, "selected_display")));
        Table *n2 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "label", localize(S("b_set_windowmode"), NULL), "options", localize(S("ml_windowmode_opt"), NULL), "opt_callback", S("change_screenmode"), "current_option", N(streq(screenmode, "Windowed") ? 1 : streq(screenmode, "Fullscreen") ? 2 : streq(screenmode, "Borderless") ? 3 : 1)));
        // note: res_option or 1: the C function returns a 1-based index, 0 standing for no value
        Table *n3 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "options", tget(ttab(display.tab, "screen_resolutions"), "strings"), "opt_callback", S("change_screen_resolution"), "current_option", N(res_option ? res_option : 1)));
        Table *n4 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "options", localize(S("ml_vsync_opt"), NULL), "opt_callback", S("change_vsync"), "current_option", N(veq(tget(WINDOW, "vsync"), N(0)) ? 2 : 1)));
        Table *n5 = UIBox_button(TAB("button", S("apply_window_changes"), "label", T(ARR(localize(S("b_set_apply"), NULL))), "minw", N(3), "func", S("can_apply_window_changes")));
        return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(n1),
            T(n2),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "id", S("resolution_cycle"))), "nodes", T(ARR(T(n3))))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(T(n4))))),
            T(n5))));
    } else if (streq(tab, "Audio")) {
        Table *SOUND = ttab(G->SETTINGS, "SOUND");
        Table *n1 = create_slider(TAB("label", localize(S("b_set_master_vol"), NULL), "w", N(5), "h", N(0.4), "ref_table", T(SOUND), "ref_value", S("volume"), "min", N(0), "max", N(100)));
        Table *n2 = create_slider(TAB("label", localize(S("b_set_music_vol"), NULL), "w", N(5), "h", N(0.4), "ref_table", T(SOUND), "ref_value", S("music_volume"), "min", N(0), "max", N(100)));
        Table *n3 = create_slider(TAB("label", localize(S("b_set_game_vol"), NULL), "w", N(5), "h", N(0.4), "ref_table", T(SOUND), "ref_value", S("game_sounds_volume"), "min", N(0), "max", N(100)));
        return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(n1), T(n2), T(n3))));
    } else if (streq(tab, "Graphics")) {
        Table *GRAPHICS = ttab(G->SETTINGS, "GRAPHICS");
        Table *n1 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "label", localize(S("b_set_shadows"), NULL), "options", localize(S("ml_shadow_opt"), NULL), "opt_callback", S("change_shadows"), "current_option", N(streq(tstr(GRAPHICS, "shadows"), "On") ? 1 : 2)));
        Table *n2 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "label", localize(S("b_set_pixel_smoothing"), NULL), "options", localize(S("ml_smoothing_opt"), NULL), "opt_callback", S("change_pixel_smoothing"), "current_option", tget(GRAPHICS, "texture_scaling")));
        Table *n3 = create_slider(TAB("label", localize(S("b_set_CRT"), NULL), "w", N(4), "h", N(0.4), "ref_table", T(GRAPHICS), "ref_value", S("crt"), "min", N(0), "max", N(100)));
        Table *n4 = create_option_cycle(TAB("w", N(4), "scale", N(0.8), "label", localize(S("b_set_CRT_bloom"), NULL), "options", localize(S("ml_bloom_opt"), NULL), "opt_callback", S("change_crt_bloom"), "current_option", tget(GRAPHICS, "bloom")));
        return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(n1), T(n2), T(n3), T(n4))));
    }

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR), "minh", N(5), "minw", N(5))), "nodes", T(tnew()));
}

// UI_definitions.lua:2344
Table *create_UIBox_test_framework(Table *variables) {
    if (!variables) variables = tnew();
    Table *nodes = tnew();
    for (TIter it = titer(variables); tnext(&it);) { // pairs: insertion order here
        Table *v = it.val.tab;
        const char *type = tstr(v, "type");
        if (streq(type, "cycle")) {
            Table *cycle = create_option_cycle(TAB("options", tget(v, "options"), "opt_callback", S("test_framework_cycle_callback"), "ref_table", tget(v, "ref_table"), "ref_value", tget(v, "ref_value")));
            tinsert(nodes,
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", truthy(tget(v, "label")) ? tget(v, "label") : S(""), "scale", N(0.5), "colour", P(G->C.UI.TEXT_DARK))))))))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                            T(cycle))))))))));
        } else if (streq(type, "slider")) {
            Table *slider = create_slider(TAB("w", N(5), "h", N(0.4), "ref_table", tget(v, "ref_table"), "ref_value", tget(v, "ref_value"), "min", N(num_or(v, "min", 0)), "max", N(num_or(v, "max", 1))));
            tinsert(nodes,
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(1), "minw", N(1), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(v, "label"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_DARK))))))))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                            T(slider))))))))));
        } else if (streq(type, "text_input")) {
            Table *input = create_text_input(ttab(v, "options"));
            tinsert(nodes,
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(1), "minw", N(1), "padding", N(0.05), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(v, "label"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_DARK))))))))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                            T(input))))))))));
        }
    }
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(G->ROOM->T->w * 5), "minh", N(G->ROOM->T->h * 5), "padding", N(0.15), "r", N(0.1), "colour", P(colour_new(G->C.BLACK[0], G->C.BLACK[1], G->C.BLACK[2], 0.6f)))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(1), "r", N(0.3), "padding", N(0.1), "minw", N(1), "colour", P(G->C.WHITE), "shadow", B(true))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(1), "r", N(0.2), "padding", N(0.2), "minw", N(1), "colour", P(G->C.CLEAR), "outline", N(1), "outline_colour", P(G->C.BLACK))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes",
                      T(nodes))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.ORANGE), "button", S("exit_overlay_menu"), "shadow", B(true))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", S("Back"), "scale", N(0.5), "colour", P(G->C.UI.TEXT_LIGHT))))))))))))))))))))))));
    return t;
}

// tab_definition_function = create_UIBox_usage
static Val usage_fn(void *ctx, int argc, Val *argv) {
    return T(create_UIBox_usage(argc > 0 && argv[0].t == V_TAB ? argv[0].tab : NULL));
}

// UI_definitions.lua:2397
Table *UIDEF_usage_tabs(void) {
    Table *tabs_node = create_tabs(TAB(
        "tabs", T(ARR(
            T(TAB(
                "label", localize(S("b_stat_jokers"), NULL),
                "chosen", B(true),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("joker_usage"))))),
            T(TAB(
                "label", localize(S("b_stat_consumables"), NULL),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("consumeable_usage"))))),
            T(TAB(
                "label", localize(S("b_stat_tarots"), NULL),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("consumeable_usage"), S("Tarot"))))),
            T(TAB(
                "label", localize(S("b_stat_planets"), NULL),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("consumeable_usage"), S("Planet"))))),
            T(TAB(
                "label", localize(S("b_stat_spectrals"), NULL),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("consumeable_usage"), S("Spectral"))))),
            T(TAB(
                "label", localize(S("b_stat_vouchers"), NULL),
                "tab_definition_function", FN(usage_fn, NULL),
                "tab_definition_function_args", T(ARR(S("voucher_usage"), S("Voucher"))))))),
        "tab_h", N(8),
        "snap_to_nav", B(true)));
    return create_UIBox_generic_options(TAB("back_func", S("high_scores"), "contents", T(ARR(T(tabs_node)))));
}

// UI_definitions.lua:2450
static bool usage_sort(Val a, Val b, void *ctx) { return tnum(a.tab, "count") > tnum(b.tab, "count"); }

// UI_definitions.lua:2436
Table *create_UIBox_usage(Table *args) {
    if (!args) args = tnew();
    UI_global_type = tstri(args, 1), UI_global_set = tstri(args, 2);
    const char *_type = UI_global_type, *_set = UI_global_set;
    Table *used_cards = tnew();
    num max_amt = 0;
    Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
    Table *usage = _type ? ttab(profile, _type) : NULL;
    if (!usage) ERROR("create_UIBox_usage: bad argument #1 to 'pairs' (table expected, got nil)");
    for (TIter it = titer(usage); tnext(&it);) { // note: pairs: insertion order here (ties of the sort below)
        Val k = it.key;
        Table *v = it.val.tab;
        Val center = tgetv(G->P_CENTERS, k);
        if (truthy(center) && (!_set || streq(tstr(center.tab, "set"), _set)) && tbool(center.tab, "discovered")) {
            tseti(used_cards, tlen(used_cards) + 1, T(TAB("count", tget(v, "count"), "key", k)));
            if (tnum(v, "count") > max_amt) max_amt = tnum(v, "count");
        }
    }

    float *_col = C_SECONDARY_SET_get(_set); // (unused, as in the Lua)
    if (!_col) _col = G->C.RED;

    tsort(used_cards, usage_sort, NULL);

    Table *histograms = tnew();

    for (int i = 1; i <= 10; i++) {
        Table *v = ttabi(used_cards, i);
        if (v) {
            Table *center = tgetv(G->P_CENTERS, tget(v, "key")).tab;
            Card *card = NEW(Card);
            Card_init(card, 0, 0, 0.7f * G->CARD_W, 0.7f * G->CARD_H, NULL, center, NULL);
            card->ambient_tilt = 0.8f;
            CardArea *cardarea = NEW(CardArea);
            CardArea_init(cardarea,
                          G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h,
                          G->CARD_W * 0.7f,
                          G->CARD_H * 0.7f,
                          TAB("card_limit", N(2), "type", S("title"), "highlight_limit", N(0)));
            CardArea_emplace(cardarea, card, NULL, false);

            float *bar_colour = C_SECONDARY_SET_get(tstr(center, "set"));
            if (!bar_colour) bar_colour = G->C.RED;
            // note: 0/0 when every count is 0 is NaN in the Lua; the spec (ui-defs-a.md trap 35) asks for a bar of height 0
            num bar_h = max_amt != 0 ? tnum(v, "count") / max_amt * 3.6 : 0;
            tseti(histograms, tlen(histograms) + 1,
                  T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"), "minh", N(6.2), "colour", P(G->C.UI.TRANSPARENT_DARK), "r", N(0.1))), "nodes", T(ARR(

                      T(TAB("n", N(UIT_R), "config", T(TAB("align", S("bm"))), "nodes", T(ARR(
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.7 * G->CARD_H + 0.1))), "nodes", T(ARR(
                              T(TAB("n", N(UIT_O), "config", T(TAB("object", O(cardarea))))))))),
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                              T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(v, "count"), "scale", N(0.35), "colour", P(mix_colours(G->C.FILTER, G->C.WHITE, 0.8f, NULL)), "shadow", B(true))))))))),
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                              T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(bar_h), "minw", N(0.8), "colour", P(bar_colour), "res", N(0.15), "r", N(0.001))), "nodes", T(tnew()))))))))))))))));
        } else {
            tseti(histograms, tlen(histograms) + 1,
                  T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"), "minh", N(6.2), "colour", P(G->C.UI.TRANSPARENT_DARK), "r", N(0.1))), "nodes", T(ARR(
                      T(TAB("n", N(UIT_R), "config", T(TAB("align", S("bm"))), "nodes", T(ARR(
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.7 * G->CARD_H + 0.1), "minw", N(0.7 * G->CARD_W))), "nodes", T(tnew()))),
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                              T(TAB("n", N(UIT_T), "config", T(TAB("text", S("-"), "scale", N(0.35), "colour", P(mix_colours(G->C.FILTER, G->C.WHITE, 0.8f, NULL)), "shadow", B(true))))))))),
                          T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                              T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.2), "minw", N(0.8), "colour", P(G->C.UI.TRANSPARENT_LIGHT), "res", N(0.15), "r", N(0.001))), "nodes", T(tnew()))))))))))))))));
        }
    }

    // _type == 'joker_usage' and ... or ... : false when it is none of the three
    Val title = streq(_type, "joker_usage") ? localize(S("ph_stat_joker"), NULL)
              : streq(_type, "consumeable_usage") ? localize(S("ph_stat_consumable"), NULL)
              : streq(_type, "voucher_usage") ? localize(S("ph_stat_voucher"), NULL)
              : B(false);
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(3), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.2), "h", N(0.2), "r", N(0.1), "colour", P(G->C.FILTER))))),
            T(TAB("n", N(UIT_T), "config", T(TAB("text", title,
                                                 "scale", N(0.35), "colour", P(G->C.WHITE))))))))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(histograms))))));

    return t;
}

// tab_definition_function = G.UIDEF.custom_deck_tab
static Val custom_deck_tab_fn(void *ctx, int argc, Val *argv) {
    return T(UIDEF_custom_deck_tab(argc > 0 && argv[0].t == V_STR ? argv[0].s : NULL));
}

// UI_definitions.lua:2513
Table *create_UIBox_customize_deck(void) {
    Table *tabs_node = create_tabs(TAB(
        "tabs", T(ARR(
            T(TAB(
                "label", localize(S("Spades"), "suits_plural"),
                "chosen", B(true),
                "tab_definition_function", FN(custom_deck_tab_fn, NULL),
                "tab_definition_function_args", S("Spades"))),
            T(TAB(
                "label", localize(S("Hearts"), "suits_plural"),
                "tab_definition_function", FN(custom_deck_tab_fn, NULL),
                "tab_definition_function_args", S("Hearts"))),
            T(TAB(
                "label", localize(S("Clubs"), "suits_plural"),
                "tab_definition_function", FN(custom_deck_tab_fn, NULL),
                "tab_definition_function_args", S("Clubs"))),
            T(TAB(
                "label", localize(S("Diamonds"), "suits_plural"),
                "tab_definition_function", FN(custom_deck_tab_fn, NULL),
                "tab_definition_function_args", S("Diamonds"))))),
        "snap_to_nav", B(true), "no_shoulders", B(true)));
    // (snap_back = nil: not set)
    Table *t = create_UIBox_generic_options(TAB("back_func", S("options"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(tabs_node)))))))));
    return t;
}
