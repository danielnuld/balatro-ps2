// functions/UI_definitions.lua, lines 1-636.
#include "all.h"

// UI_definitions.lua:3 G.UIDEF = {}: only a namespace (UIDEF_x).

// UI_definitions.lua:5 Debug only (and its G.FUNCS.DT_*): not translated.
Table *create_UIBox_debug_tools(void) {
    TODO_PORT("UI_definitions.lua:5 create_UIBox_debug_tools: debug tools are not translated");
}

// UI_definitions.lua:145
Table *create_UIBox_notify_alert(const char *_achievement, const char *_type) {
    Table *_c = ttab(G->P_CENTERS, _achievement);
    Atlas *_atlas = streq(_type, "Joker") ? ASSET_ATLAS_get("Joker") :
                    streq(_type, "Voucher") ? ASSET_ATLAS_get("Voucher") :
                    streq(_type, "Back") ? ASSET_ATLAS_get("centers") :
                    ASSET_ATLAS_get("icons");

    Sprite *t_s = NEW(Sprite);
    Sprite_init(t_s, 0, 0, 1.5f * (_atlas->px / _atlas->py), 1.5f, _atlas,
                (_c && ttab(_c, "pos")) ? ttab(_c, "pos") : TAB("x", N(3), "y", N(0)));
    t_s->moveable.node.states.drag.can = false;
    t_s->moveable.node.states.hover.can = false;
    t_s->moveable.node.states.collide.can = false;

    Val subtext;
    if (streq(_type, "achievement")) subtext = localize(S(G->F_TROPHIES ? "k_trophy" : "k_achievement"), NULL);
    else if (streq(_type, "Joker")) subtext = localize(S("k_joker"), NULL);
    else if (streq(_type, "Voucher")) subtext = localize(S("k_voucher"), NULL);
    else if (streq(_type, "Back")) subtext = localize(S("k_deck"), NULL);
    else subtext = S("ERROR");

    if (streq(_achievement, "b_challenge")) subtext = localize(S("k_challenges"), NULL);
    Val name = streq(_type, "achievement") ? localize(S(_achievement), "achievement_names") : S("ERROR");

    Table *inner;
    if (!streq(_type, "achievement")) {
        inner = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.04))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(3.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", subtext, "scale", N(0.5), "colour", P(G->C.FILTER), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(3.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_unlocked_ex"), NULL), "scale", N(0.35), "colour", P(G->C.FILTER), "shadow", B(true)))))
            ))))
        )));
    } else {
        inner = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.04))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(3.4), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", name, "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(3.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", subtext, "scale", N(0.3), "colour", P(G->C.FILTER), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(3.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_unlocked_ex"), NULL), "scale", N(0.35), "colour", P(G->C.FILTER), "shadow", B(true)))))
            ))))
        )));
    }

    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cl"), "r", N(0.1), "padding", N(0.06), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "padding", N(0.2), "minw", N(20), "r", N(0.1), "colour", P(G->C.BLACK), "outline", N(1.5), "outline_colour", P(G->C.GREY))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(t_s)))))
                )))),
                T(inner)
            ))))
        ))))
    )));
    return t;
}

// UI_definitions.lua:196 HTTP manager: not translated.
Table *create_UIBox_online_high_scores(void) {
    TODO_PORT("UI_definitions.lua:196 create_UIBox_online_high_scores: the HTTP manager is not translated");
}

// UI_definitions.lua:206 HTTP manager (loadstring of the response): not translated.
Table *create_UIBox_high_scores_filling(const char *_resp) {
    TODO_PORT("UI_definitions.lua:206 create_UIBox_high_scores_filling: the HTTP manager is not translated");
}

// UI_definitions.lua:239
Table *UIDEF_use_and_sell_buttons(Card *card) {
    Table *sell = NULL;
    Table *use = NULL;
    float card_w = card->moveable.node.T->w, card_h = card->moveable.node.T->h;
    if (card->area && streq(tstr(card->area->moveable.node.config, "type"), "joker")) {
        sell = TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("ref_table", O(card), "align", S("cr"), "padding", N(0.1), "r", N(0.08), "minw", N(1.25), "hover", B(true), "shadow", B(true), "colour", P(G->C.UI.BACKGROUND_INACTIVE), "one_press", B(true), "button", S("sell_card"), "func", S("can_sell_card"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.6))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(1.25))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_sell"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.4), "shadow", B(true)))))
                    )))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("$"), NULL), "colour", P(G->C.WHITE), "scale", N(0.4), "shadow", B(true))))),
                        T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", O(card), "ref_value", S("sell_cost_label"), "colour", P(G->C.WHITE), "scale", N(0.55), "shadow", B(true)))))
                    ))))
                ))))
            ))))
        )));
    }
    if (tbool(card->ability, "consumeable")) {
        if (card->area == G->pack_cards && G->pack_cards) {
            return TAB("n", N(UIT_ROOT), "config", T(TAB("padding", N(0), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("mid", B(true))), "nodes", T(tnew()))),
                T(TAB("n", N(UIT_R), "config", T(TAB("ref_table", O(card), "r", N(0.08), "padding", N(0.1), "align", S("bm"), "minw", N(0.5f * card_w - 0.15f), "minh", N(0.8f * card_h), "maxw", N(0.7f * card_w - 0.15f), "hover", B(true), "shadow", B(true), "colour", P(G->C.UI.BACKGROUND_INACTIVE), "one_press", B(true), "button", S("use_card"), "func", S("can_use_consumeable"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_use"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.55), "shadow", B(true)))))
                ))))
            )));
        }
        use = TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("ref_table", O(card), "align", S("cr"), "maxw", N(1.25), "padding", N(0.1), "r", N(0.08), "minw", N(1.25), "minh", N((card->area && streq(tstr(card->area->moveable.node.config, "type"), "joker")) ? 0 : 1), "hover", B(true), "shadow", B(true), "colour", P(G->C.UI.BACKGROUND_INACTIVE), "one_press", B(true), "button", S("use_card"), "func", S("can_use_consumeable"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.6))))),
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_use"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.55), "shadow", B(true)))))
            ))))
        )));
    } else if (card->area && card->area == G->pack_cards) {
        return TAB("n", N(UIT_ROOT), "config", T(TAB("padding", N(0), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("ref_table", O(card), "r", N(0.08), "padding", N(0.1), "align", S("bm"), "minw", N(0.5f * card_w - 0.15f), "maxw", N(0.9f * card_w - 0.15f), "minh", N(0.3f * card_h), "hover", B(true), "shadow", B(true), "colour", P(G->C.UI.BACKGROUND_INACTIVE), "one_press", B(true), "button", S("use_card"), "func", S("can_select_card"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_select"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.45), "shadow", B(true)))))
            ))))
        )));
    }
    // sell / use nil: the row's nodes list is empty (ARR drops a trailing NIL)
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("padding", N(0), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.15), "align", S("cl"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"))), "nodes", T(ARR(
                T(sell)
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"))), "nodes", T(ARR(
                T(use)
            ))))
        ))))
    )));
    return t;
}

// UI_definitions.lua:299
UIBox *UIDEF_card_focus_ui(Card *card) {
    Transform *cT = card->moveable.node.T;
    float card_width = cT->w + (tbool(card->ability, "consumeable") ? -0.1f : streq(tstr(card->ability, "set"), "Voucher") ? -0.16f : 0);

    // copy_table(G.C.WHITE): a new colour that belongs to the definition
    float *playing_card_colour = malloc(sizeof(Colour));
    memcpy(playing_card_colour, G->C.WHITE, sizeof(Colour));
    playing_card_colour[3] = 1.5f;
    if (G->hand && card->area == G->hand) ease_value_f(&playing_card_colour[3], -1.5, false, "REAL", -1, 0.2, "quad");

    float tcnx = cT->x + cT->w / 2 - G->ROOM->T->w / 2, tcny = cT->y + cT->h / 2 - G->ROOM->T->h / 2;

    bool not_in_hand = !G->hand || card->area != G->hand;
    UIBox *base_background = UIBox_new(TAB(
        "T", T(ARR(N(card->moveable.VT.x), N(card->moveable.VT.y), N(0), N(0))),
        "definition",
            T(not_in_hand ? TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(card_width + 0.3f), "minh", N(cT->h + 0.3f), "r", N(0.1), "colour", P(adjust_alpha(G->C.BLACK, 0.7f, NULL)), "outline_colour", P(lighten(G->C.JOKER_GREY, 0.5f, NULL)), "outline", N(1.5), "line_emboss", N(0.8))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("id", S("ATTACH_TO_ME"))), "nodes", T(tnew())))
            ))) :
            TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(card_width), "minh", N(cT->h), "r", N(0.1), "colour", P(playing_card_colour))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("id", S("ATTACH_TO_ME"))), "nodes", T(tnew())))
            )))),
        "config", T(TAB(
            "align", S("cm"),
            "offset", T(TAB("x", N(0.007f * tcnx * cT->w), "y", N(0.007f * tcny * cT->h))),
            "parent", O(card),
            "r_bond", S(not_in_hand ? "Weak" : "Strong")))));

    // :325 base_background.set_alignment = function() ... end is UIDEF_card_focus_ui_set_alignment (below),
    // which Card_align calls in place of focused_ui:set_alignment().

    UIElement *base_attach = UIBox_get_UIE_by_ID(base_background, S("ATTACH_TO_ME"), NULL);
    Table *children = base_attach->moveable.node.children;

    //The card UI can have BUY, REDEEM, USE, and SELL buttons depending on the context of the card
    if (card->area == G->shop_jokers && G->shop_jokers) { //Add a buy button
        bool buy_and_use = false;
        if (tbool(card->ability, "consumeable")) {
            tseto(children, "buy_and_use", UIDEF_card_focus_button(TAB(
                "card", O(card), "parent", O(base_attach), "type", S("buy_and_use"),
                "func", S("can_buy_and_use"), "button", S("buy_from_shop"), "card_width", N(card_width))));
            buy_and_use = true;
        }
        tseto(children, "buy", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("buy"),
            "func", S("can_buy"), "button", S("buy_from_shop"), "card_width", N(card_width), "buy_and_use", buy_and_use ? B(true) : NIL)));
    }
    if (card->area == G->shop_vouchers && G->shop_vouchers) { //Add a redeem button
        tseto(children, "redeem", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("buy"),
            "func", S("can_redeem"), "button", S("redeem_from_shop"), "card_width", N(card_width))));
    }
    if (card->area == G->shop_booster && G->shop_booster) { //Add a redeem button
        tseto(children, "redeem", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("buy"),
            "func", S("can_open"), "button", S("open_booster"), "card_width", N(card_width * 0.85f))));
    }
    if (((card->area == G->consumeables && G->consumeables) || (card->area == G->pack_cards && G->pack_cards)) &&
        tbool(card->ability, "consumeable")) { //Add a use button
        tseto(children, "use", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("use"),
            "func", S("can_use_consumeable"), "button", S("use_card"), "card_width", N(card_width))));
    }
    if ((card->area == G->pack_cards && G->pack_cards) && !tbool(card->ability, "consumeable")) { //Add a use button
        tseto(children, "use", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("select"),
            "func", S("can_select_card"), "button", S("use_card"), "card_width", N(card_width))));
    }
    if (((card->area == G->jokers && G->jokers) || (card->area == G->consumeables && G->consumeables)) && G->STATE != STATES_TUTORIAL) { //Add a sell button
        tseto(children, "sell", UIDEF_card_focus_button(TAB(
            "card", O(card), "parent", O(base_attach), "type", S("sell"),
            "func", S("can_sell_card"), "button", S("sell_card"), "card_width", N(card_width))));
    }

    return base_background;
}

// UI_definitions.lua:325 the closure stored as base_background.set_alignment
void UIDEF_card_focus_ui_set_alignment(Card *card) {
    Transform *cT = card->moveable.node.T;
    float cnx = cT->x + cT->w / 2 - G->ROOM->T->w / 2, cny = cT->y + cT->h / 2 - G->ROOM->T->h / 2;
    Moveable_set_alignment((Moveable *)Card_child(card, "focused_ui"), TAB("offset", T(TAB("x", N(0.007f * cnx * cT->w), "y", N(0.007f * cny * cT->h)))));
}

// UI_definitions.lua:382
UIBox *UIDEF_card_focus_button(Table *args) {
    if (!args) return NULL;

    Card *card = tobj(args, "card");
    const char *type = tstr(args, "type");
    bool is_sell = streq(type, "sell");

    Table *button_contents = tnew();
    if (streq(type, "sell")) {
        button_contents =
            TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "maxw", N(1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_sell"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.4), "shadow", B(true)))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("$"), NULL), "colour", P(G->C.WHITE), "scale", N(0.4), "shadow", B(true))))),
                    T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", tget(args, "card"), "ref_value", S("sell_cost_label"), "colour", P(G->C.WHITE), "scale", N(0.55), "shadow", B(true)))))
                ))))
            )));
    } else if (streq(type, "buy")) {
        button_contents = TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_buy"), NULL), "colour", P(G->C.WHITE), "scale", N(0.5))));
    } else if (streq(type, "select")) {
        button_contents = TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_select"), NULL), "colour", P(G->C.WHITE), "scale", N(0.3))));
    } else if (streq(type, "redeem")) {
        button_contents = TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_redeem"), NULL), "colour", P(G->C.WHITE), "scale", N(0.5))));
    } else if (streq(type, "use")) {
        button_contents = TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_use"), NULL), "colour", P(G->C.WHITE), "scale", N(0.5))));
    } else if (streq(type, "buy_and_use")) {
        button_contents =
            TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cr"), "maxw", N(1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_buy"), NULL), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.4), "shadow", B(true)))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cr"), "maxw", N(1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("b_and_use"), NULL), "colour", P(G->C.WHITE), "scale", N(0.3), "shadow", B(true)))))
                ))))
            )));
    }

    Val card_width = tget(args, "card_width");
    // the conditional entries keep their index: ARR holds the leading NIL as a hole and drops the trailing one
    Table *inner_nodes = ARR(
        !is_sell ? T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.2), "minh", N(0.6))), "nodes", T(tnew()))) : NIL,
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "maxw", N(1))), "nodes", T(ARR(
            T(button_contents)
        )))),
        is_sell ? T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.2), "minh", N(0.6))), "nodes", T(tnew()))) : NIL);

    return UIBox_new(TAB(
        "T", T(ARR(N(card->moveable.VT.x), N(card->moveable.VT.y), N(0), N(0))),
        "definition",
            T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("id", streq(type, "buy_and_use") ? S("buy_and_use") : NIL, "ref_table", tget(args, "card"), "ref_parent", tget(args, "parent"), "align", S(is_sell ? "cl" : "cr"), "colour", P(G->C.BLACK), "shadow", B(true), "r", N(0.08), "func", tget(args, "func"), "one_press", B(true), "button", tget(args, "button"), "focus_args", T(TAB("type", S("none"))), "hover", B(true))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S(is_sell ? "cl" : "cr"), "minw", N(1 + (streq(type, "select") ? 0.1 : 0)), "minh", N(is_sell ? 1.5 : 1), "padding", N(0.08),
                        "focus_args", T(TAB("button", S(is_sell ? "leftshoulder" : streq(type, "buy_and_use") ? "leftshoulder" : "rightshoulder"), "scale", N(0.55), "orientation", S(is_sell ? "tli" : "tri"), "offset", T(TAB("x", N(is_sell ? 0.1 : -0.1), "y", N(0))), "type", S("none"))),
                        "func", S("set_button_pip"))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.3))), "nodes", T(tnew()))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(inner_nodes)))
                    ))))
                ))))
            )))),
        "config", T(TAB(
            "align", S(is_sell ? "cl" : "cr"),
            "offset", T(TAB("x", N((is_sell ? -1 : 1) * ((truthy(card_width) ? card_width.n : 0) - 0.17 - card->moveable.node.T->w / 2)), "y", N(streq(type, "buy_and_use") ? 0.6 : tbool(args, "buy_and_use") ? -0.6 : 0))),
            "parent", tget(args, "parent")))));
}

// UI_definitions.lua:444
Table *UIDEF_speech_bubble(Val text_key, Table *loc_vars) {
    Table *text = tnew();
    if (loc_vars && tbool(loc_vars, "quip")) {
        localize(T(TAB("type", S("quips"), "key", truthy(text_key) ? text_key : S("lq_1"), "vars", T(loc_vars ? loc_vars : tnew()), "nodes", T(text))), NULL);
    } else {
        localize(T(TAB("type", S("tutorial"), "key", truthy(text_key) ? text_key : S("sb_1"), "vars", T(loc_vars ? loc_vars : tnew()), "nodes", T(text))), NULL);
    }
    Table *row = tnew();
    for (int k = 1; tgeti(text, k).t != V_NIL; k++) {
        Val v = tgeti(text, k);
        tseti(row, tlen(row) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"))), "nodes", v)));
    }
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minh", N(1), "r", N(0.3), "padding", N(0.07), "minw", N(1), "colour", P(G->C.JOKER_GREY), "shadow", B(true))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(1), "r", N(0.2), "padding", N(0.1), "minw", N(1), "colour", P(G->C.WHITE))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minh", N(1), "r", N(0.2), "padding", N(0.03), "minw", N(1), "colour", P(G->C.WHITE))), "nodes", T(row)))
        ))))
    )));
    return t;
}

// UI_definitions.lua:463
// note: the Lua defines it again, identically, at :6302; weak, so that the build links whether or not the part
// that holds :6302 defines it too.
__attribute__((weak)) Table *create_UIBox_highlight(Node *rect) {
    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minh", N(rect->T->h + 0.1f), "minw", N(rect->T->w + 0.15f), "r", N(0.15), "colour", P(G->C.DARK_EDITION))), "nodes", T(tnew()));
    return t;
}

// UI_definitions.lua:491 the comparator of table.sort(G.playing_cards, ...)
static bool deck_preview_sort(Val a, Val b, void *ctx) {
    return Card_get_nominal(a.obj, "suit") > Card_get_nominal(b.obj, "suit");
}

// UI_definitions.lua:469
Table *UIDEF_deck_preview(void) {
    float _minh = 0.35f, _minw = 0.5f;
    Table *suit_labels = tnew();
    Table *suit_counts = TAB(
        "Spades", N(0),
        "Hearts", N(0),
        "Clubs", N(0),
        "Diamonds", N(0));
    Table *mod_suit_counts = TAB(
        "Spades", N(0),
        "Hearts", N(0),
        "Clubs", N(0),
        "Diamonds", N(0));
    bool mod_suit_diff = false;
    num wheel_flipped = 0;
    Table *wheel_flipped_text = NULL;
    float *flip_col = G->C.WHITE;
    Table *rank_counts = tnew();
    Table *deck_tables = tnew();
    remove_nils(G->playing_cards); // (the Lua discards the result too)
    tsort(G->playing_cards, deck_preview_sort, NULL);
    Table *SUITS = TAB(
        "Spades", T(tnew()),
        "Hearts", T(tnew()),
        "Clubs", T(tnew()),
        "Diamonds", T(tnew()));

    for (TIter it = titer(SUITS); tnext(&it);) {
        for (int i = 1; i <= 14; i++) {
            tseti(it.val.tab, tlen(it.val.tab) + 1, T(tnew()));
        }
    }

    const char *suit_map[4] = {"Spades", "Hearts", "Clubs", "Diamonds"};
    bool has_stones = false; // stones ~= nil
    num stones = 0;
    Table *rank_name_mapping = ARR(S("A"), S("K"), S("Q"), S("J"), S("10"), N(9), N(8), N(7), N(6), N(5), N(4), N(3), N(2));

    for (int k = 1; tgeti(G->playing_cards, k).t != V_NIL; k++) {
        Card *v = tobji(G->playing_cards, k);
        if (streq(tstr(v->ability, "effect"), "Stone Card")) {
            has_stones = true; // stones = stones or 0
        }
        if ((v->area && v->area == G->deck) || tbool(v->ability, "wheel_flipped")) {
            if (tbool(v->ability, "wheel_flipped")) wheel_flipped = wheel_flipped + 1;
            if (streq(tstr(v->ability, "effect"), "Stone Card")) {
                stones = stones + 1;
            } else {
                // note: pairs(suit_counts): the order does not matter here
                for (TIter it = titer(suit_counts); tnext(&it);) {
                    const char *kk = it.key.s;
                    if (streq(tstr(v->base, "suit"), kk)) tsetn(suit_counts, kk, tnum(suit_counts, kk) + 1);
                    if (Card_is_suit(v, kk, false, false)) tsetn(mod_suit_counts, kk, tnum(mod_suit_counts, kk) + 1);
                }
                int id = (int)tnum(v->base, "id");
                Table *slot = ttabi(ttab(SUITS, tstr(v->base, "suit")), id);
                if (slot) {
                    tinsert(slot, O(v));
                }
                Val rc = tgeti(rank_counts, id);
                tseti(rank_counts, id, N((truthy(rc) ? rc.n : 0) + 1));
            }
        }
    }

    wheel_flipped_text = (wheel_flipped > 0) ? TAB("n", N(UIT_T), "config", T(TAB("text", S("?"), "colour", P(G->C.FILTER), "scale", N(0.25), "shadow", B(true)))) : NULL;
    flip_col = wheel_flipped_text ? mix_colours(G->C.FILTER, G->C.WHITE, 0.7f, NULL) : G->C.WHITE;

    tseti(suit_labels, tlen(suit_labels) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.04), "minw", N(_minw), "minh", N(2 * _minh + 0.25f))), "nodes", T(ARR(
        has_stones ? T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s: ", vstr(localize(S("ph_deck_preview_stones"), NULL)))), "colour", P(G->C.WHITE), "scale", N(0.25), "shadow", B(true)))))
        : NIL,
        has_stones ? T(TAB("n", N(UIT_T), "config", T(TAB("text", S(number_tostring(stones)), "colour", P(stones > 0 ? G->C.WHITE : G->C.UI.TRANSPARENT_LIGHT), "scale", N(0.4), "shadow", B(true)))))
        : NIL
    )))));

    Table *_row = tnew();
    float *_bg_col = G->C.JOKER_GREY;
    for (int k = 1; tgeti(rank_name_mapping, k).t != V_NIL; k++) {
        Val v = tgeti(rank_name_mapping, k);
        float _tscale = 0.3f;
        float *_colour = G->C.BLACK;
        float *rank_col = veq(v, S("A")) ? _bg_col : (veq(v, S("K")) || veq(v, S("Q")) || veq(v, S("J"))) ? G->C.WHITE : _bg_col;
        rank_col = mix_colours(rank_col, _bg_col, 0.8f, NULL);

        Val rc = tgeti(rank_counts, 15 - k);
        Table *_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "minw", N(_minw), "minh", N(_minh), "colour", P(rank_col), "emboss", N(0.04), "padding", N(0.03))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(v)), "colour", P(_colour), "scale", N(1.6f * _tscale)))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(_minw + 0.04f), "minh", N(_minh), "colour", P(G->C.L_BLACK), "r", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(number_tostring(truthy(rc) ? rc.n : 0)), "colour", P(flip_col), "scale", N(_tscale), "shadow", B(true)))))
                ))))
            ))))
        )));
        tinsert(_row, T(_col));
    }
    tinsert(deck_tables, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.04))), "nodes", T(_row))));

    for (int j = 1; j <= 4; j++) {
        _row = tnew();
        _bg_col = mix_colours(C_SUITS_get(suit_map[j - 1]), G->C.L_BLACK, 0.7f, NULL);
        for (int i = 14; i >= 2; i--) {
            int n = tlen(ttabi(ttab(SUITS, suit_map[j - 1]), i));
            float _tscale = n > 0 ? 0.3f : 0.25f;
            float *_colour = n > 0 ? flip_col : G->C.UI.TRANSPARENT_LIGHT;

            Table *_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.05), "minw", N(_minw + 0.098f), "minh", N(_minh))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%d", n)), "colour", P(_colour), "scale", N(_tscale), "shadow", B(true), "lang", tget(G->LANGUAGES, "en-us")))))
            )));
            tinsert(_row, T(_col));
        }
        tinsert(deck_tables, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.04), "minh", N(0.4), "colour", P(_bg_col))), "nodes", T(_row))));
    }

    for (int k = 1; k <= 4; k++) {
        const char *v = suit_map[k - 1];
        // (the Lua's and/or chain gives false for another name: there is none)
        int _x = streq(v, "Spades") ? 3 : streq(v, "Hearts") ? 0 : streq(v, "Clubs") ? 2 : 1;
        Sprite *t_s = NEW(Sprite);
        Sprite_init(t_s, 0, 0, 0.3f, 0.3f, ASSET_ATLAS_get(fmt("ui_%d", tbool(G->SETTINGS, "colourblind_option") ? 2 : 1)), TAB("x", N(_x), "y", N(1)));
        t_s->moveable.node.states.drag.can = false;
        t_s->moveable.node.states.hover.can = false;
        t_s->moveable.node.states.collide.can = false;

        bool differs = tnum(mod_suit_counts, v) != tnum(suit_counts, v);
        if (differs) mod_suit_diff = true;

        tseti(suit_labels, tlen(suit_labels) + 1,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.03), "colour", P(G->C.JOKER_GREY))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(_minw), "minh", N(_minh))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("can_collide", B(false), "object", O(t_s)))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(_minw * 2.4f), "minh", N(_minh), "colour", P(G->C.L_BLACK), "r", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(number_tostring(tnum(suit_counts, v))), "colour", P(flip_col), "scale", N(0.3), "shadow", B(true), "lang", tget(G->LANGUAGES, "en-us"))))),
                    differs ? T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" (%s)", number_tostring(tnum(mod_suit_counts, v)))), "colour", P(mix_colours(G->C.BLUE, G->C.WHITE, 0.7f, NULL)), "scale", N(0.28), "shadow", B(true), "lang", tget(G->LANGUAGES, "en-us"))))) : NIL
                ))))
            )))));
    }

    Table *t =
        TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.JOKER_GREY), "r", N(0.1), "emboss", N(0.05), "padding", N(0.07))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "emboss", N(0.05), "colour", P(G->C.BLACK), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.04))), "nodes", T(suit_labels))),
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.02))), "nodes", T(deck_tables)))
                )))),
                mod_suit_diff ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.3), "r", N(0.1), "colour", P(mix_colours(G->C.BLUE, G->C.WHITE, 0.7f, NULL)))), "nodes", T(tnew()))),
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s", vstr(localize(S("ph_deck_preview_effective"), NULL)))), "colour", P(G->C.WHITE), "scale", N(0.3)))))
                )))) : NIL,
                wheel_flipped_text ? T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.3), "r", N(0.1), "colour", P(flip_col))), "nodes", T(tnew()))),
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s", vstr(wheel_flipped > 1 ?
                        localize(T(TAB("type", S("variable"), "key", S("deck_preview_wheel_plural"), "vars", T(ARR(N(wheel_flipped))))), NULL) :
                        localize(T(TAB("type", S("variable"), "key", S("deck_preview_wheel_singular"), "vars", T(ARR(N(wheel_flipped))))), NULL)))), "colour", P(G->C.WHITE), "scale", N(0.3)))))
                )))) : NIL
            ))))
        )));
    return t;
}

// UI_definitions.lua:621
Table *create_UIBox_character_button(Table *args) {
    Val button = truthy(tget(args, "button")) ? tget(args, "button") : S("NONE");
    Val func = truthy(tget(args, "func")) ? tget(args, "func") : NIL;
    Val colour = truthy(tget(args, "colour")) ? tget(args, "colour") : P(G->C.RED);
    Val update_func = truthy(tget(args, "update_func")) ? tget(args, "update_func") : NIL;

    Table *t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tm"), "minw", N(1.9), "padding", N(0.2), "minh", N(1.2), "r", N(0.1), "hover", B(true), "colour", colour, "button", func, "func", update_func, "shadow", B(true), "maxw", tget(args, "maxw"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", button, "scale", N(0.55), "colour", P(G->C.UI.TEXT_LIGHT), "focus_args", T(TAB("button", S("x"), "orientation", S("bm"))), "func", S("set_button_pip")))))
            ))))
        ))))
    )));
    return t;
}
