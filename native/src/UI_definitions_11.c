// functions/UI_definitions.lua:6308-6436: create_UIBox_generic_options, UIBox_dyn_container, simple_text_container,
// UIBox_button.
#include "all.h"

// Lua's `a or b` on values.
static inline Val v_or(Val a, Val b) { return truthy(a) ? a : b; }

// A colour the Lua builds in place: a new 4-float block that belongs to the definition (never freed).
static float *new_colour(float r, float g, float b, float a) {
    float *c = malloc(4 * sizeof(float));
    c[0] = r, c[1] = g, c[2] = b, c[3] = a;
    return c;
}

// UI_definitions.lua:6317 the event's func; captures args
static bool ev_generic_options_infotip(void *ctx) {
    Table *args = ctx;
    if (G->OVERLAY_MENU) {
        // note: G.OVERLAY_MENU == true here is the Lua's error (a method call on a boolean)
        if (G->OVERLAY_MENU == OVERLAY_MENU_TRUE) ERROR("attempt to index field 'OVERLAY_MENU' (a boolean value)");
        UIElement *_infotip_object = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("overlay_menu_infotip"), NULL);
        if (_infotip_object) {
            Table *config = ((Node *)_infotip_object)->config;
            CALL(tobj(config, "object"), remove);
            Val infotip = tget(args, "infotip");
            tseto(config, "object", UIBox_new(TAB(
                "definition", T(overlay_infotip(infotip.t == V_TAB ? infotip.tab : NULL)),
                "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("bm"), "parent", O(_infotip_object))))));
        }
    }
    return true;
}

// UI_definitions.lua:6308
Table *create_UIBox_generic_options(Table *args) {
    if (!args) args = tnew();
    Val back_func = v_or(tget(args, "back_func"), S("exit_overlay_menu"));
    // note: the default is a single T node used as the `nodes` list itself, as in the Lua. G.C.UI.RED is nil.
    Val contents = v_or(tget(args, "contents"),
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", S(lit("generic_options", 1)), "scale", N(0.4))))));
    if (tbool(args, "infotip")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){
            .not_blocking = true,
            .not_blockable = true,
            .timer = "REAL",
            .func = ev_generic_options_infotip, .ctx = args}), NULL, false);
    }

    Val bg_colour = tget(args, "bg_colour");
    if (!truthy(bg_colour)) bg_colour = P(new_colour(G->C.GREY[0], G->C.GREY[1], G->C.GREY[2], 0.7f));
    Table *root_config = TAB("align", S("cm"), "minw", N(G->ROOM->T->w * 5), "minh", N(G->ROOM->T->h * 5),
                             "padding", N(0.1), "r", N(0.1), "colour", bg_colour);
    Table *outline_config = TAB("align", S("cm"), "minh", N(1), "r", N(0.3), "padding", N(0.07), "minw", N(1),
                                "colour", v_or(tget(args, "outline_colour"), P(G->C.JOKER_GREY)), "emboss", N(0.1));
    Table *inner_config = TAB("align", S("cm"), "minh", N(1), "r", N(0.2), "padding", N(0.2), "minw", N(1),
                              "colour", v_or(tget(args, "colour"), P(G->C.L_BLACK)));

    Table *inner_nodes = ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", v_or(tget(args, "padding"), N(0.2)),
                                             "minw", v_or(tget(args, "minw"), N(7)))),
              "nodes", contents)));
    // :6339 `not args.no_back and {...} or nil`: index 2, absent with no_back
    if (!tbool(args, "no_back")) {
        bool no_pip = tbool(args, "no_pip");
        Table *back_config = TAB(
            "id", v_or(tget(args, "back_id"), S("overlay_menu_back_button")), "align", S("cm"), "minw", N(2.5),
            "button_delay", tget(args, "back_delay"), "padding", N(0.1), "r", N(0.1), "hover", B(true),
            "colour", v_or(tget(args, "back_colour"), P(G->C.ORANGE)), "button", back_func, "shadow", B(true),
            "focus_args", T(TAB("nav", S("wide"), "button", S("b"), "snap_to", tget(args, "snap_back"))));
        Val back_label = tget(args, "back_label");
        if (!truthy(back_label)) back_label = localize(S("b_back"), NULL);
        Table *text_config = TAB(
            "id", v_or(tget(args, "back_id"), NIL), "text", back_label, "scale", N(0.5),
            "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true),
            "func", !no_pip ? S("set_button_pip") : NIL,
            "focus_args", !no_pip ? T(TAB("button", v_or(tget(args, "back_button"), S("b")))) : NIL);
        tseti(inner_nodes, 2, T(TAB("n", N(UIT_R), "config", T(back_config), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(text_config))))))))))));
    }

    // :6347 Moveable()
    Moveable *infotip_moveable = NEW(Moveable);
    Moveable_init(infotip_moveable, 0, 0, 0, 0);

    return TAB("n", N(UIT_ROOT), "config", T(root_config), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(outline_config), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(inner_config), "nodes", T(inner_nodes))))))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("id", S("overlay_menu_infotip"), "object", O(infotip_moveable))))))))))));
}

// UI_definitions.lua:6352
Table *UIBox_dyn_container(Table *inner_table, bool horizontal, const float *colour_override,
                           const float *background_override, bool flipped, float padding) {
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.03), "colour", P(G->C.UI.TRANSPARENT_DARK), "r", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(colour_override ? (float *)colour_override : G->C.DYN_UI.MAIN), "r", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S(horizontal ? "cl" : (flipped ? "bm" : "tm")),
                                                 "colour", P(background_override ? (float *)background_override : G->C.DYN_UI.BOSS_DARK),
                                                 "minw", N(horizontal ? 100 : 0), "minh", N(horizontal ? 0 : 30), "r", N(0.1),
                                                 "padding", N(padding < 0 ? 0.08 : padding))),
                  "nodes", T(inner_table))))))))));
}

// UI_definitions.lua:6360
Table *simple_text_container(const char *_loc, Table *args) {
    if (!_loc) return NULL;
    if (!args) args = tnew();
    Table *container = tnew();
    Val loc_result = localize(S(_loc), NULL);
    if (truthy(loc_result) && loc_result.t == V_TAB) {
        for (int k = 1;; k++) {
            Val v = tgeti(loc_result.tab, k);
            if (v.t == V_NIL) break;
            tseti(container, tlen(container) + 1,
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", v, "scale", v_or(tget(args, "scale"), N(0.35)),
                                                         "colour", v_or(tget(args, "colour"), P(G->C.UI.TEXT_DARK)),
                                                         "shadow", tget(args, "shadow"))))))))));
        }
        return TAB("n", N(tbool(args, "col") ? UIT_C : UIT_R),
                   "config", T(TAB("align", S("cm"), "padding", v_or(tget(args, "padding"), N(0.03)))),
                   "nodes", T(container));
    }
    return NULL;
}

// UI_definitions.lua:6376
Table *UIBox_button(Table *args) {
    if (!args) args = tnew();
    tset(args, "button", v_or(tget(args, "button"), S("exit_overlay_menu")));
    tset(args, "func", v_or(tget(args, "func"), NIL));
    tset(args, "colour", v_or(tget(args, "colour"), P(G->C.RED)));
    tset(args, "choice", v_or(tget(args, "choice"), NIL));
    tset(args, "chosen", v_or(tget(args, "chosen"), NIL));
    tset(args, "label", v_or(tget(args, "label"), T(ARR(S("LABEL")))));
    tset(args, "minw", v_or(tget(args, "minw"), N(2.7)));
    tset(args, "maxw", v_or(tget(args, "maxw"), N(tnum(args, "minw") - 0.2)));
    if (tnum(args, "minw") < tnum(args, "maxw")) tsetn(args, "maxw", tnum(args, "minw") - 0.2);
    tset(args, "minh", v_or(tget(args, "minh"), N(0.9)));
    tset(args, "scale", v_or(tget(args, "scale"), N(0.5)));
    tset(args, "focus_args", v_or(tget(args, "focus_args"), NIL));
    tset(args, "text_colour", v_or(tget(args, "text_colour"), P(G->C.UI.TEXT_LIGHT)));
    Val col = tget(args, "col");
    UIType but_UIT = (col.t == V_BOOL && col.b) ? UIT_C : UIT_R;

    Table *but_UI_label = tnew();

    const char *button_pip = NULL;
    Table *label = ttab(args, "label");
    for (int k = 1;; k++) {
        Val v = tgeti(label, k);
        if (v.t == V_NIL) break;
        if (k == tlen(label) && tbool(args, "focus_args") && tbool(ttab(args, "focus_args"), "set_button_pip")) {
            button_pip = "set_button_pip";
        }
        tinsert(but_UI_label, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "minw", tget(args, "minw"), "maxw", tget(args, "maxw"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", v, "scale", tget(args, "scale"), "colour", tget(args, "text_colour"),
                                                 "shadow", tget(args, "shadow"),
                                                 "focus_args", button_pip ? v_or(tget(args, "focus_args"), NIL) : NIL,
                                                 "func", S(button_pip), "ref_table", tget(args, "ref_table"))))))))));
    }

    Val count = tget(args, "count");
    if (truthy(count)) {
        tinsert(but_UI_label,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("scale", N(0.35),
                                                     "text", S(fmt("%s / %s", tostring(tget(count.tab, "tally")), tostring(tget(count.tab, "of")))),
                                                     "colour", P(new_colour(1, 1, 1, 0.9f)))))))))));
    }

    return TAB("n", N(but_UIT), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB(
            "align", S("cm"),
            "padding", v_or(tget(args, "padding"), N(0)),
            "r", N(0.1),
            "hover", B(true),
            "colour", tget(args, "colour"),
            "one_press", tget(args, "one_press"),
            "button", !veq(tget(args, "button"), S("nil")) ? tget(args, "button") : NIL,
            "choice", tget(args, "choice"),
            "chosen", tget(args, "chosen"),
            "focus_args", tget(args, "focus_args"),
            "minh", N(tnum(args, "minh") - 0.3 * (truthy(count) ? 1 : 0)),
            "shadow", B(true),
            "func", tget(args, "func"),
            "id", tget(args, "id"),
            "back_func", tget(args, "back_func"),
            "ref_table", tget(args, "ref_table"),
            "mid", tget(args, "mid"))),
            "nodes", T(but_UI_label))))));
}
