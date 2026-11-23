// functions/UI_definitions.lua:637-1273: the shop, attention_text, the play buttons, the tooltip rows, the HUD's
// blind box and add_tag.
#include "all.h"

// {n = uit, config = cfg, nodes = nodes}; nodes NULL: the node has no `nodes` key (T(NULL) is NIL)
#define NODE(uit, cfg, nodes) TAB("n", N(uit), "config", T(cfg), "nodes", T(nodes))
#define LOC(key) localize(S(key), NULL)

// copy_table(colour): a new colour that belongs to whoever keeps it (never freed)
static float *colour_copy(const float *c) {
    float *out = malloc(4 * sizeof(float));
    memcpy(out, c, 4 * sizeof(float));
    return out;
}
static float *colour_new(float r, float g, float b, float a) {
    float c[4] = {r, g, b, a};
    return colour_copy(c);
}

// UI_definitions.lua:684
static bool ev_shop_sign(void *ctx) {
    G->SHOP_SIGN->moveable.alignment.offset.y = 0;
    return true;
}

// UI_definitions.lua:637
Table *UIDEF_shop(void) {
    Transform *hand_T = G->hand->moveable.node.T;
    num joker_max = tnum(ttab(G->GAME, "shop"), "joker_max");

    G->shop_jokers = NEW(CardArea);
    CardArea_init(G->shop_jokers,
                  hand_T->x + 0,
                  hand_T->y + G->ROOM->T->y + 9,
                  joker_max * 1.02 * G->CARD_W,
                  1.05 * G->CARD_H,
                  TAB("card_limit", N(joker_max), "type", S("shop"), "highlight_limit", N(1)));

    G->shop_vouchers = NEW(CardArea);
    CardArea_init(G->shop_vouchers,
                  hand_T->x + 0,
                  hand_T->y + G->ROOM->T->y + 9,
                  2.1 * G->CARD_W,
                  1.05 * G->CARD_H,
                  TAB("card_limit", N(1), "type", S("shop"), "highlight_limit", N(1)));

    G->shop_booster = NEW(CardArea);
    CardArea_init(G->shop_booster,
                  hand_T->x + 0,
                  hand_T->y + G->ROOM->T->y + 9,
                  2.4 * G->CARD_W,
                  1.15 * G->CARD_H,
                  TAB("card_limit", N(2), "type", S("shop"), "highlight_limit", N(1), "card_w", N(1.27 * G->CARD_W)));

    AnimatedSprite *shop_sign = NEW(AnimatedSprite);
    AnimatedSprite_init(shop_sign, 0, 0, 4.4, 2.2, ANIMATION_ATLAS_get("shop_sign"), NULL);
    Sprite_define_draw_steps((Sprite *)shop_sign, ARR(
        T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
        T(TAB("shader", S("dissolve")))));
    G->SHOP_SIGN = UIBox_new(TAB(
        "definition", T(
            NODE(UIT_ROOT, TAB("colour", P(G->C.DYN_UI.MAIN), "emboss", N(0.05), "align", S("cm"), "r", N(0.1), "padding", N(0.1)), ARR(
                T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.1), "minw", N(4.72), "minh", N(3.1), "colour", P(G->C.DYN_UI.DARK), "r", N(0.1)), ARR(
                    T(NODE(UIT_R, TAB("align", S("cm")), ARR(
                        T(NODE(UIT_O, TAB("object", O(shop_sign)), NULL))))),
                    T(NODE(UIT_R, TAB("align", S("cm")), ARR(
                        T(NODE(UIT_O, TAB("object", O(DynaText_new(TAB("string", T(ARR(LOC("ph_improve_run"))), "colours", T(ARR(P(lighten(G->C.GOLD, 0.3, NULL)))), "shadow", B(true), "rotate", B(true), "float", B(true), "bump", B(true), "scale", N(0.5), "spacing", N(1), "pop_in", N(1.5), "maxw", N(4.3))))), NULL))))))))))),
        "config", T(TAB(
            "align", S("cm"),
            "offset", T(TAB("x", N(0), "y", N(-15))),
            "major", O(UIBox_get_UIE_by_ID(G->HUD, S("row_blind"), NULL)),
            "bond", S("Weak")))));
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_shop_sign}), NULL, false);

    Table *t = NODE(UIT_ROOT, TAB("align", S("cl"), "colour", P(G->C.CLEAR)), ARR(
        T(UIBox_dyn_container(ARR(
            T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "emboss", N(0.05), "r", N(0.1), "colour", P(G->C.DYN_UI.BOSS_MAIN)), ARR(
                T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.05)), ARR(
                    T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.1)), ARR(
                        T(NODE(UIT_R, TAB("id", S("next_round_button"), "align", S("cm"), "minw", N(2.8), "minh", N(1.5), "r", N(0.15), "colour", P(G->C.RED), "one_press", B(true), "button", S("toggle_shop"), "hover", B(true), "shadow", B(true)), ARR(
                            T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.07), "focus_args", T(TAB("button", S("y"), "orientation", S("cr"))), "func", S("set_button_pip")), ARR(
                                T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1.3)), ARR(
                                    T(NODE(UIT_T, TAB("text", LOC("b_next_round_1"), "scale", N(0.4), "colour", P(G->C.WHITE), "shadow", B(true)), NULL))))),
                                T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1.3)), ARR(
                                    T(NODE(UIT_T, TAB("text", LOC("b_next_round_2"), "scale", N(0.4), "colour", P(G->C.WHITE), "shadow", B(true)), NULL))))))))))),
                        T(NODE(UIT_R, TAB("align", S("cm"), "minw", N(2.8), "minh", N(1.6), "r", N(0.15), "colour", P(G->C.GREEN), "button", S("reroll_shop"), "func", S("can_reroll"), "hover", B(true), "shadow", B(true)), ARR(
                            T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.07), "focus_args", T(TAB("button", S("x"), "orientation", S("cr"))), "func", S("set_button_pip")), ARR(
                                T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1.3)), ARR(
                                    T(NODE(UIT_T, TAB("text", LOC("k_reroll"), "scale", N(0.4), "colour", P(G->C.WHITE), "shadow", B(true)), NULL))))),
                                T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1.3), "minw", N(1)), ARR(
                                    T(NODE(UIT_T, TAB("text", LOC("$"), "scale", N(0.7), "colour", P(G->C.WHITE), "shadow", B(true)), NULL)),
                                    T(NODE(UIT_T, TAB("ref_table", T(ttab(G->GAME, "current_round")), "ref_value", S("reroll_cost"), "scale", N(0.75), "colour", P(G->C.WHITE), "shadow", B(true)), NULL)))))))))))))),
                    T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.2), "colour", P(G->C.L_BLACK), "emboss", N(0.05), "minw", N(8.2)), ARR(
                        T(NODE(UIT_O, TAB("object", O(G->shop_jokers)), NULL)))))))),
                T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.2)), tnew())),
                T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.1)), ARR(
                    T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.15), "r", N(0.2), "colour", P(G->C.L_BLACK), "emboss", N(0.05)), ARR(
                        T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.2), "colour", P(G->C.BLACK), "maxh", N(G->shop_vouchers->moveable.node.T->h + 0.4)), ARR(
                            T(NODE(UIT_T, TAB("text", localize(T(TAB("type", S("variable"), "key", S("ante_x_voucher"), "vars", T(ARR(tget(ttab(G->GAME, "round_resets"), "ante"))))), NULL), "scale", N(0.45), "colour", P(G->C.L_BLACK), "vert", B(true)), NULL)),
                            T(NODE(UIT_O, TAB("object", O(G->shop_vouchers)), NULL)))))))),
                    T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.15), "r", N(0.2), "colour", P(G->C.L_BLACK), "emboss", N(0.05)), ARR(
                        T(NODE(UIT_O, TAB("object", O(G->shop_booster)), NULL)))))))))))),
            false, NULL, NULL, false, -1))));
    return t;
}

// UI_definitions.lua:779
static bool ev_shop_card_tags(void *ctx) {
    Card *card = ctx;
    Table *tags = ttab(G->GAME, "tags");
    for (int k = 1; truthy(tgeti(tags, k)); k++) {
        if (truthy(Tag_apply_to_run(tobji(tags, k), TAB("type", S("store_joker_modify"), "card", O(card))))) break;
    }
    return true;
}

// UI_definitions.lua:742
Card *create_card_for_shop(CardArea *area) {
    Table *tutorial_progress = ttab(G->SETTINGS, "tutorial_progress");
    Table *forced_shop = ttab(tutorial_progress, "forced_shop");
    if (area == G->shop_jokers && tutorial_progress && forced_shop && truthy(tgeti(forced_shop, tlen(forced_shop)))) {
        Table *t = forced_shop;
        Val c = tgetv(G->P_CENTERS, tgeti(t, tlen(t)));
        Table *_center = truthy(c) ? c.tab : ttab(G->P_CENTERS, "c_empress");
        Transform *area_T = area->moveable.node.T;
        Card *card = NEW(Card);
        Card_init(card, area_T->x + area_T->w / 2, area_T->y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"), _center,
                  TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
        tseti(t, tlen(t), NIL);
        if (!truthy(tgeti(t, 1))) tset(tutorial_progress, "forced_shop", NIL);

        create_shop_card_ui(card, NULL, NULL);
        return card;
    } else {
        Val forced_tag = NIL;
        Table *tags = ttab(G->GAME, "tags");
        for (int k = 1; truthy(tgeti(tags, k)); k++) {
            Tag *v = tobji(tags, k);
            if (!truthy(forced_tag)) {
                forced_tag = Tag_apply_to_run(v, TAB("type", S("store_joker_create"), "area", O(area)));
                if (truthy(forced_tag)) {
                    for (int kk = 1; truthy(tgeti(tags, kk)); kk++) {
                        Tag *vv = tobji(tags, kk);
                        if (truthy(Tag_apply_to_run(vv, TAB("type", S("store_joker_modify"), "card", forced_tag)))) break;
                    }
                    return forced_tag.obj;
                }
            }
        }
        Val spectral_rate = tget(G->GAME, "spectral_rate");
        tset(G->GAME, "spectral_rate", truthy(spectral_rate) ? spectral_rate : N(0));
        num total_rate = tnum(G->GAME, "joker_rate") + tnum(G->GAME, "tarot_rate") + tnum(G->GAME, "planet_rate") + tnum(G->GAME, "playing_card_rate") + tnum(G->GAME, "spectral_rate");
        // 'cdt'..ante: the number as tostring writes it
        num polled_rate = pseudorandom(N(pseudoseed(fmt("cdt%s", vstr(tget(ttab(G->GAME, "round_resets"), "ante"))), NULL)), NIL, NIL) * total_rate;
        num check_rate = 0;
        Table *used_vouchers = ttab(G->GAME, "used_vouchers");
        // the table is built before the loop: the 'illusion' roll happens once per call when the voucher is owned
        Table *rates = ARR(
            T(TAB("type", S("Joker"), "val", tget(G->GAME, "joker_rate"))),
            T(TAB("type", S("Tarot"), "val", tget(G->GAME, "tarot_rate"))),
            T(TAB("type", S("Planet"), "val", tget(G->GAME, "planet_rate"))),
            T(TAB("type", S((tbool(used_vouchers, "v_illusion") && pseudorandom(N(pseudoseed("illusion", NULL)), NIL, NIL) > 0.6) ? "Enhanced" : "Base"), "val", tget(G->GAME, "playing_card_rate"))),
            T(TAB("type", S("Spectral"), "val", tget(G->GAME, "spectral_rate"))));
        for (int i = 1; truthy(tgeti(rates, i)); i++) {
            Table *v = ttabi(rates, i);
            const char *type = tstr(v, "type");
            if (polled_rate > check_rate && polled_rate <= check_rate + tnum(v, "val")) {
                Card *card = create_card(type, area, false, NIL, false, false, NULL, "sho");
                create_shop_card_ui(card, type, area);
                E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_shop_card_tags, .ctx = card}), NULL, false);
                if ((streq(type, "Base") || streq(type, "Enhanced")) && tbool(used_vouchers, "v_illusion") && pseudorandom(N(pseudoseed("illusion", NULL)), NIL, NIL) > 0.8) {
                    num edition_poll = pseudorandom(N(pseudoseed("illusion", NULL)), NIL, NIL);
                    Table *edition = tnew();
                    if (edition_poll > 1 - 0.15) tsetb(edition, "polychrome", true);
                    else if (edition_poll > 0.5) tsetb(edition, "holo", true);
                    else tsetb(edition, "foil", true);
                    Card_set_edition(card, edition, false, false);
                }
                return card;
            }
            check_rate = check_rate + tnum(v, "val");
        }
    }
    return NULL; // the Lua falls off the end
}

// UI_definitions.lua:808
static bool ev_shop_card_ui(void *ctx) {
    Card *card = ctx;
    Table *children = card->moveable.node.children;
    if (tbool(card->moveable.node.object.dyn, "opening")) return true;
    Table *t1 = NODE(UIT_ROOT, TAB("minw", N(0.6), "align", S("tm"), "colour", P(darken(G->C.BLACK, 0.2, NULL)), "shadow", B(true), "r", N(0.05), "padding", N(0.05), "minh", N(1)), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "colour", P(lighten(G->C.BLACK, 0.1, NULL)), "r", N(0.1), "minw", N(1), "minh", N(0.55), "emboss", N(0.05), "padding", N(0.03)), ARR(
            T(NODE(UIT_O, TAB("object", O(DynaText_new(TAB("string", T(ARR(T(TAB("prefix", LOC("$"), "ref_table", O(card), "ref_value", S("cost"))))), "colours", T(ARR(P(G->C.MONEY))), "shadow", B(true), "silent", B(true), "bump", B(true), "pop_in", N(0), "scale", N(0.5))))), NULL)))))));
    const char *set = tstr(card->ability, "set");
    Table *t2;
    if (streq(set, "Voucher")) {
        t2 = NODE(UIT_ROOT, TAB("ref_table", O(card), "minw", N(1.1), "maxw", N(1.3), "padding", N(0.1), "align", S("bm"), "colour", P(G->C.GREEN), "shadow", B(true), "r", N(0.08), "minh", N(0.94), "func", S("can_redeem"), "one_press", B(true), "button", S("redeem_from_shop"), "hover", B(true)), ARR(
            T(NODE(UIT_T, TAB("text", LOC("b_redeem"), "colour", P(G->C.WHITE), "scale", N(0.4)), NULL))));
    } else if (streq(set, "Booster")) {
        t2 = NODE(UIT_ROOT, TAB("ref_table", O(card), "minw", N(1.1), "maxw", N(1.3), "padding", N(0.1), "align", S("bm"), "colour", P(G->C.GREEN), "shadow", B(true), "r", N(0.08), "minh", N(0.94), "func", S("can_open"), "one_press", B(true), "button", S("open_booster"), "hover", B(true)), ARR(
            T(NODE(UIT_T, TAB("text", LOC("b_open"), "colour", P(G->C.WHITE), "scale", N(0.5)), NULL))));
    } else {
        t2 = NODE(UIT_ROOT, TAB("ref_table", O(card), "minw", N(1.1), "maxw", N(1.3), "padding", N(0.1), "align", S("bm"), "colour", P(G->C.GOLD), "shadow", B(true), "r", N(0.08), "minh", N(0.94), "func", S("can_buy"), "one_press", B(true), "button", S("buy_from_shop"), "hover", B(true)), ARR(
            T(NODE(UIT_T, TAB("text", LOC("b_buy"), "colour", P(G->C.WHITE), "scale", N(0.5)), NULL))));
    }
    Table *t3 = NODE(UIT_ROOT, TAB("id", S("buy_and_use"), "ref_table", O(card), "minh", N(1.1), "padding", N(0.1), "align", S("cr"), "colour", P(G->C.RED), "shadow", B(true), "r", N(0.08), "minw", N(1.1), "func", S("can_buy_and_use"), "one_press", B(true), "button", S("buy_from_shop"), "hover", B(true), "focus_args", T(TAB("type", S("none")))), ARR(
        T(NODE(UIT_B, TAB("w", N(0.1), "h", N(0.6)), NULL)),
        T(NODE(UIT_C, TAB("align", S("cm")), ARR(
            T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1)), ARR(
                T(NODE(UIT_T, TAB("text", LOC("b_buy"), "colour", P(G->C.WHITE), "scale", N(0.5)), NULL))))),
            T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(1)), ARR(
                T(NODE(UIT_T, TAB("text", LOC("b_and_use"), "colour", P(G->C.WHITE), "scale", N(0.3)), NULL))))))))));

    UIBox *price = UIBox_new(TAB(
        "definition", T(t1),
        "config", T(TAB(
            "align", S("tm"),
            "offset", T(TAB("x", N(0), "y", N(1.5))),
            "major", O(card),
            "bond", S("Weak"),
            "parent", O(card)))));
    tseto(children, "price", price);

    tseto(children, "buy_button", UIBox_new(TAB(
        "definition", T(t2),
        "config", T(TAB(
            "align", S("bm"),
            "offset", T(TAB("x", N(0), "y", N(-0.3))),
            "major", O(card),
            "bond", S("Weak"),
            "parent", O(card))))));

    if (tbool(card->ability, "consumeable")) {
        tseto(children, "buy_and_use_button", UIBox_new(TAB(
            "definition", T(t3),
            "config", T(TAB(
                "align", S("cr"),
                "offset", T(TAB("x", N(-0.3), "y", N(0))),
                "major", O(card),
                "bond", S("Weak"),
                "parent", O(card))))));
    }

    price->moveable.alignment.offset.y = streq(tstr(card->ability, "set"), "Booster") ? 0.5f : 0.38f;

    return true;
}

// UI_definitions.lua:802 (type and area are never read)
void create_shop_card_ui(Card *card, const char *type, CardArea *area) {
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 0.43,
        .not_blocking = true,
        .not_blockable = true,
        .func = ev_shop_card_ui,
        .ctx = card}), NULL, false);
}

// UI_definitions.lua:914
static bool ev_attention_text_show(void *ctx) {
    Table *args = ctx;
    Table *pos = ttab(args, "pos");
    Val cover = tget(args, "cover");
    Val cover_align = tget(args, "cover_align");
    Val rotate = tget(args, "rotate");
    num cover_padding = tnum(args, "cover_padding"); // (args.cover_padding or 0)
    Table *dynatext_config = TAB("scale", tget(args, "scale"), "string", tget(args, "text"), "maxw", tget(args, "maxw"), "colours", T(ARR(tget(args, "colour"))), "float", B(true), "shadow", B(true), "silent", B(!tbool(args, "noisy")), "pop_in", N(0), "pop_in_rate", N(6), "rotate", truthy(rotate) ? rotate : NIL);
    tseti(dynatext_config, 1, tget(args, "scale")); // the Lua's stray positional args.scale
    UIBox *AT = UIBox_new(TAB(
        "T", T(ARR(tget(pos, "x"), tget(pos, "y"), N(0), N(0))),
        "definition", T(
            NODE(UIT_ROOT, TAB("align", truthy(cover_align) ? cover_align : S("cm"), "minw", N((truthy(cover) ? ((Node *)cover.obj)->T->w : 0.001) + cover_padding), "minh", N((truthy(cover) ? ((Node *)cover.obj)->T->h : 0.001) + cover_padding), "padding", N(0.03), "r", N(0.1), "emboss", tget(args, "emboss"), "colour", tget(args, "cover_colour")), ARR(
                T(NODE(UIT_O, TAB("draw_layer", N(1), "object", O(DynaText_new(dynatext_config))), NULL))))),
        "config", tget(args, "uibox_config")));
    tseto(args, "AT", AT);
    AT->attention_text = true;

    UIElement *child = tobji(AT->UIRoot->moveable.node.children, 1);
    tset(args, "text", tget(child->moveable.node.config, "object"));
    DynaText_pulse(tobj(args, "text"), 0.5);

    if (truthy(cover)) {
        Particles *p = NEW(Particles);
        Particles_init(p, tnum(pos, "x"), tnum(pos, "y"), 0, 0, TAB(
            "timer_type", S("TOTAL"),
            "timer", N(0.01),
            "pulse_max", N(15),
            "max", N(0),
            "scale", N(0.3),
            "vel_variation", N(0.2),
            "padding", N(0.1),
            "fill", B(true),
            "lifespan", N(0.5),
            "speed", N(2.5),
            "attach", O(AT->UIRoot),
            "colours", T(ARR(tget(args, "cover_colour"), tget(args, "cover_colour_l"), tget(args, "cover_colour_d")))));
    }
    if (truthy(tget(args, "backdrop_colour"))) {
        tset(args, "backdrop_colour", P(colour_copy(UI_colour(args, "backdrop_colour"))));
        Val backdrop_scale = tget(args, "backdrop_scale");
        Particles *p = NEW(Particles);
        Particles_init(p, tnum(pos, "x"), tnum(pos, "y"), 0, 0, TAB(
            "timer_type", S("TOTAL"),
            "timer", N(5),
            "scale", N(2.4 * (truthy(backdrop_scale) ? backdrop_scale.n : 1)),
            "lifespan", N(5),
            "speed", N(0),
            "attach", O(AT),
            "colours", T(ARR(tget(args, "backdrop_colour")))));
    }
    return true;
}

// UI_definitions.lua:965 (called every pass until the fade ends)
static bool ev_attention_text_fade(void *ctx) {
    Table *args = ctx;
    if (!truthy(tget(args, "start_time"))) {
        tsetn(args, "start_time", G->TIMERS.TOTAL);
        DynaText_pop_out(tobj(args, "text"), 3);
    } else {
        num fade = lmax(0, 1 - 3 * (G->TIMERS.TOTAL - tnum(args, "start_time")));
        tsetn(args, "fade", fade);
        float *c;
        if ((c = UI_colour(args, "cover_colour"))) c[3] = lmin(c[3], 2 * fade);
        if ((c = UI_colour(args, "cover_colour_l"))) c[3] = lmin(c[3], fade);
        if ((c = UI_colour(args, "cover_colour_d"))) c[3] = lmin(c[3], fade);
        if ((c = UI_colour(args, "backdrop_colour"))) c[3] = lmin(c[3], fade);
        c = UI_colour(args, "colour");
        c[3] = lmin(c[3], fade);
        if (fade <= 0) {
            UIBox_remove(tobj(args, "AT"));
            return true;
        }
    }
    return false;
}

// UI_definitions.lua:883
void attention_text(Table *args) {
    if (!args) args = tnew();
    if (!truthy(tget(args, "text"))) tsets(args, "text", "test");
    if (!truthy(tget(args, "scale"))) tsetn(args, "scale", 1);
    float *colour = UI_colour(args, "colour");
    tset(args, "colour", P(colour_copy(colour ? colour : G->C.WHITE)));
    Val hold = tget(args, "hold");
    tsetn(args, "hold", (truthy(hold) ? hold.n : 0) + 0.1 * G->SPEEDFACTOR);
    if (!truthy(tget(args, "pos"))) tsett(args, "pos", TAB("x", N(0), "y", N(0)));
    if (!truthy(tget(args, "align"))) tsets(args, "align", "cm");
    if (!truthy(tget(args, "emboss"))) tset(args, "emboss", NIL);

    tsetn(args, "fade", 1);

    if (truthy(tget(args, "cover"))) {
        float *cover_colour = UI_colour(args, "cover_colour");
        cover_colour = colour_copy(cover_colour ? cover_colour : G->C.RED);
        tset(args, "cover_colour", P(cover_colour));
        // note: lighten / darken already return a new colour: the Lua's copy_table of it is not repeated
        tset(args, "cover_colour_l", P(lighten(cover_colour, 0.2, NULL)));
        tset(args, "cover_colour_d", P(darken(cover_colour, 0.2, NULL)));
    } else {
        tset(args, "cover_colour", P(colour_copy(G->C.CLEAR)));
    }

    Val offset = tget(args, "offset"), cover = tget(args, "cover"), major = tget(args, "major");
    tsett(args, "uibox_config", TAB(
        "align", tget(args, "align"),
        "offset", truthy(offset) ? offset : T(TAB("x", N(0), "y", N(0))),
        "major", truthy(cover) ? cover : truthy(major) ? major : NIL));

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 0,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_attention_text_show,
        .ctx = args}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = tnum(args, "hold"),
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_attention_text_fade,
        .ctx = args}), NULL, false);
}

// UI_definitions.lua:986
Table *create_UIBox_buttons(void) {
    num text_scale = 0.45;
    num button_height = 1.3;
    Table *play_button = NODE(UIT_C, TAB("id", S("play_button"), "align", S("tm"), "minw", N(2.5), "padding", N(0.3), "r", N(0.1), "hover", B(true), "colour", P(G->C.BLUE), "button", S("play_cards_from_highlighted"), "one_press", B(true), "shadow", B(true), "func", S("can_play")), ARR(
        T(NODE(UIT_R, TAB("align", S("bcm"), "padding", N(0)), ARR(
            T(NODE(UIT_T, TAB("text", LOC("b_play_hand"), "scale", N(text_scale), "colour", P(G->C.UI.TEXT_LIGHT), "focus_args", T(TAB("button", S("x"), "orientation", S("bm"))), "func", S("set_button_pip")), NULL)))))));

    Table *discard_button = NODE(UIT_C, TAB("id", S("discard_button"), "align", S("tm"), "padding", N(0.3), "r", N(0.1), "minw", N(2.5), "minh", N(button_height), "hover", B(true), "colour", P(G->C.RED), "button", S("discard_cards_from_highlighted"), "one_press", B(true), "shadow", B(true), "func", S("can_discard")), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
            T(NODE(UIT_T, TAB("text", LOC("b_discard"), "scale", N(text_scale), "colour", P(G->C.UI.TEXT_LIGHT), "focus_args", T(TAB("button", S("y"), "orientation", S("bm"))), "func", S("set_button_pip")), NULL)))))));

    bool pos1 = veq(tget(G->SETTINGS, "play_button_pos"), N(1));
    Table *t = NODE(UIT_ROOT, TAB("align", S("cm"), "minw", N(1), "minh", N(0.3), "padding", N(0.15), "r", N(0.1), "colour", P(G->C.CLEAR)), ARR(
        T(pos1 ? discard_button : play_button),

        T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.UI.TRANSPARENT_DARK), "outline", N(1.5), "outline_colour", P(mix_colours(G->C.WHITE, G->C.JOKER_GREY, 0.7, NULL)), "line_emboss", N(1)), ARR(
            T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                    T(NODE(UIT_T, TAB("text", LOC("b_sort_hand"), "scale", N(text_scale * 0.8), "colour", P(G->C.UI.TEXT_LIGHT)), NULL))))),
                T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.1)), ARR(
                    T(NODE(UIT_C, TAB("align", S("cm"), "minh", N(0.7), "minw", N(0.9), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.ORANGE), "button", S("sort_hand_value"), "shadow", B(true)), ARR(
                        T(NODE(UIT_T, TAB("text", LOC("k_rank"), "scale", N(text_scale * 0.7), "colour", P(G->C.UI.TEXT_LIGHT)), NULL))))),
                    T(NODE(UIT_C, TAB("align", S("cm"), "minh", N(0.7), "minw", N(0.9), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.ORANGE), "button", S("sort_hand_suit"), "shadow", B(true)), ARR(
                        T(NODE(UIT_T, TAB("text", LOC("k_suit"), "scale", N(text_scale * 0.7), "colour", P(G->C.UI.TEXT_LIGHT)), NULL)))))))))))))),

        T(pos1 ? play_button : discard_button)));
    return t;
}

// UI_definitions.lua:1027
Table *desc_from_rows(Table *desc_nodes, bool empty, Val maxw) {
    Table *t = tnew();
    for (int k = 1; truthy(tgeti(desc_nodes, k)); k++) {
        tinsert(t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", maxw)), "nodes", tgeti(desc_nodes, k))));
    }
    return NODE(UIT_R, TAB("align", S("cm"), "colour", P(empty ? G->C.CLEAR : G->C.UI.BACKGROUND_WHITE), "r", N(0.1), "padding", N(0.04), "minw", N(2), "minh", N(0.8), "emboss", !empty ? N(0.05) : NIL, "filler", B(true)), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), t))));
}

// UI_definitions.lua:1037 (the rows' maxw is the unassigned global `maxw`: nil)
Table *transparent_multiline_text(Table *desc_nodes) {
    Table *t = tnew();
    for (int k = 1; truthy(tgeti(desc_nodes, k)); k++) {
        tinsert(t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", tgeti(desc_nodes, k))));
    }
    return NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), t);
}

// UI_definitions.lua:1045
Table *info_tip_from_rows(Table *desc_nodes, const char *name) {
    Table *t = tnew();
    for (int k = 1; truthy(tgeti(desc_nodes, k)); k++) {
        tinsert(t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", tgeti(desc_nodes, k))));
    }
    return NODE(UIT_R, TAB("align", S("cm"), "colour", P(lighten(G->C.GREY, 0.15, NULL)), "r", N(0.1)), ARR(
        T(NODE(UIT_R, TAB("align", S("tm"), "minh", N(0.36), "padding", N(0.03)), ARR(T(NODE(UIT_T, TAB("text", S(name), "scale", N(0.32), "colour", P(G->C.UI.TEXT_LIGHT)), NULL))))),
        T(NODE(UIT_R, TAB("align", S("cm"), "minw", N(1.5), "minh", N(0.4), "r", N(0.1), "padding", N(0.05), "colour", P(G->C.WHITE)), ARR(T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), t)))))));
}

// UI_definitions.lua:1056
Table *overlay_infotip(Table *text_rows) {
    Table *t = tnew();
    if (!text_rows) text_rows = ARR(S("ERROR"));
    for (int k = 1; truthy(tgeti(text_rows, k)); k++) {
        tinsert(t, T(NODE(UIT_R, TAB("align", S("cm")), ARR(
            T(NODE(UIT_T, TAB("text", tgeti(text_rows, k), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.45), "juice", B(true), "shadow", B(true), "lang", tget(text_rows, "lang")), NULL))))));
    }
    return NODE(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.1)), t);
}

// UI_definitions.lua:1067
Table *name_from_rows(Val name_nodes, const float *background_colour) {
    if (!truthy(name_nodes) || name_nodes.t != V_TAB || tempty(name_nodes.tab)) return NULL;
    return TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", background_colour ? P((void *)background_colour) : NIL, "emboss", background_colour ? N(0.05) : NIL)), "nodes", name_nodes);
}

// UI_definitions.lua:1072
Table *UIDEF_card_h_popup(Node *card) {
    Table *AUT = ttab(card->object.dyn, "ability_UIBox_table");
    if (AUT) {
        // note: card is a Card or a tag's Sprite: the Card's members, else the fields as the Lua would find them
        Card *as_card = is_a(card, &Card_class) ? (Card *)card : NULL;
        bool debuffed = as_card ? as_card->debuff : tbool(card->object.dyn, "debuff");
        Table *ability = as_card ? as_card->ability : ttab(card->object.dyn, "ability");
        Table *center = ttab(card->config, "center");
        const char *AUT_card_type = tstr(AUT, "card_type");
        Table *AUT_badges = ttab(AUT, "badges");
        float *card_type_colour = get_type_colour(center ? center : card->config, as_card);
        float *card_type_background;
        if (streq(AUT_card_type, "Locked")) card_type_background = G->C.BLACK;
        else if (streq(AUT_card_type, "Undiscovered")) card_type_background = darken(G->C.JOKER_GREY, 0.3, NULL);
        else if (streq(AUT_card_type, "Enhanced") || streq(AUT_card_type, "Default")) card_type_background = darken(G->C.BLACK, 0.1, NULL);
        else if (debuffed) card_type_background = darken(G->C.BLACK, 0.1, NULL);
        else if (card_type_colour) card_type_background = darken(G->C.BLACK, 0.1, NULL);
        else if (C_SET_get(AUT_card_type)) card_type_background = C_SET_get(AUT_card_type);
        else card_type_background = colour_new(0, 1, 1, 1);

        num outer_padding = 0.05;
        Val card_type = LOC(fmt("k_%s", str_lower(AUT_card_type)));

        if (streq(AUT_card_type, "Joker") || (AUT_badges && tbool(AUT_badges, "force_rarity"))) card_type = tgetv(ARR(LOC("k_common"), LOC("k_uncommon"), LOC("k_rare"), LOC("k_legendary")), tget(center, "rarity"));
        if (streq(AUT_card_type, "Enhanced")) card_type = localize(T(TAB("type", S("name_text"), "key", tget(center, "key"), "set", S("Enhanced"))), NULL);
        card_type = (debuffed && !streq(AUT_card_type, "Enhanced")) ? LOC("k_debuffed") : card_type;

        // (disp_type is computed by the Lua and never read)
        bool is_playing_card = streq(AUT_card_type, "Enhanced") || streq(AUT_card_type, "Default");

        Table *info_boxes = tnew();
        Table *badges = tnew();

        if (tbool(AUT_badges, "card_type") || tbool(AUT_badges, "force_rarity")) {
            const char *name = tstr(ability, "name");
            Val text = (streq(name, "Pluto") || streq(name, "Ceres") || streq(name, "Eris")) ? LOC("k_dwarf_planet") : (streq(name, "Planet X") ? LOC("k_planet_q") : card_type);
            // note: create_badge takes a string; a name of several lines (a table) would show 'ERROR'
            tinsert(badges, T(create_badge(vstr(text), card_type_colour, NULL, 1.2)));
        }
        if (AUT_badges) {
            for (int k = 1; truthy(tgeti(AUT_badges, k)); k++) {
                const char *v = tstri(AUT_badges, k);
                if (streq(v, "negative_consumable")) v = "negative";
                tinsert(badges, T(create_badge(vstr(localize(S(v), "labels")), get_badge_colour(v), NULL, -1)));
            }
        }

        Table *AUT_info = ttab(AUT, "info");
        if (AUT_info) {
            for (int k = 1; truthy(tgeti(AUT_info, k)); k++) {
                Table *v = ttabi(AUT_info, k);
                tinsert(info_boxes,
                    T(NODE(UIT_R, TAB("align", S("cm")), ARR(
                        T(NODE(UIT_R, TAB("align", S("cm"), "colour", P(lighten(G->C.JOKER_GREY, 0.5, NULL)), "r", N(0.1), "padding", N(0.05), "emboss", N(0.05)), ARR(
                            T(info_tip_from_rows(v, tstr(v, "name"))))))))));
            }
        }

        Moveable *object = NEW(Moveable);
        Moveable_init_args(object, NULL); // Moveable()
        // the name may be nil: a hole at index 1, the others keep their indices
        Table *nodes = tnew();
        tseti(nodes, 1, T(name_from_rows(tget(AUT, "name"), is_playing_card ? G->C.WHITE : NULL)));
        tseti(nodes, 2, T(desc_from_rows(ttab(AUT, "main"), false, NIL)));
        if (truthy(tgeti(badges, 1))) tseti(nodes, 3, T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), badges)));
        return NODE(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(
            T(NODE(UIT_C, TAB("align", S("cm"), "func", S("show_infotip"), "object", O(object), "ref_table", !tempty(info_boxes) ? T(info_boxes) : NIL), ARR(
                T(NODE(UIT_R, TAB("padding", N(outer_padding), "r", N(0.12), "colour", P(lighten(G->C.JOKER_GREY, 0.5, NULL)), "emboss", N(0.07)), ARR(
                    T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.07), "r", N(0.1), "colour", P(adjust_alpha(card_type_background, 0.8, NULL))), nodes))))))))));
    }
    return NULL;
}

// UI_definitions.lua:1135
float *get_badge_colour(const char *key) {
    if (!G->BADGE_COL) G->BADGE_COL = TAB(
        "eternal", P(G->C.ETERNAL),
        "perishable", P(G->C.PERISHABLE),
        "rental", P(G->C.RENTAL),
        "foil", P(G->C.DARK_EDITION),
        "holographic", P(G->C.DARK_EDITION),
        "polychrome", P(G->C.DARK_EDITION),
        "negative", P(G->C.DARK_EDITION),
        "gold_seal", P(G->C.GOLD),
        "red_seal", P(G->C.RED),
        "blue_seal", P(G->C.BLUE),
        "purple_seal", P(G->C.PURPLE),
        "pinned_left", P(G->C.ORANGE));
    float *col = key ? UI_colour(G->BADGE_COL, key) : NULL;
    return col ? col : colour_new(1, 0, 0, 1);
}

// UI_definitions.lua:1153
Table *create_badge(const char *_string, const float *_badge_col, const float *_text_col, float scaling) {
    if (scaling < 0) scaling = 1;
    return NODE(UIT_R, TAB("align", S("cm")), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "colour", P((void *)(_badge_col ? _badge_col : G->C.GREEN)), "r", N(0.1), "minw", N(2), "minh", N(0.4 * scaling), "emboss", N(0.05), "padding", N(0.03 * scaling)), ARR(
            T(NODE(UIT_B, TAB("h", N(0.1), "w", N(0.03)), NULL)),
            T(NODE(UIT_O, TAB("object", O(DynaText_new(TAB("string", S(_string ? _string : "ERROR"), "colours", T(ARR(P((void *)(_text_col ? _text_col : G->C.WHITE)))), "float", B(true), "shadow", B(true), "offset_y", N(-0.05), "silent", B(true), "spacing", N(1), "scale", N(0.33 * scaling))))), NULL)),
            T(NODE(UIT_B, TAB("h", N(0.1), "w", N(0.03)), NULL)))))));
}

// UI_definitions.lua:1164
Table *create_UIBox_detailed_tooltip(Table *_center) {
    Table *full_UI_table = TAB(
        "main", T(tnew()),
        "info", T(tnew()),
        "type", T(tnew()),
        "name", S("done"),
        "badges", T(tnew())); // `badges or {}`: the global is never assigned
    Table *desc = generate_card_ui(_center, full_UI_table, NULL, tstr(_center, "set"), NULL, false, NULL, NULL);
    Table *info_1 = ttabi(ttab(desc, "info"), 1);
    return NODE(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "colour", P(lighten(G->C.JOKER_GREY, 0.5, NULL)), "r", N(0.1), "padding", N(0.05), "emboss", N(0.05)), ARR(
            T(info_tip_from_rows(info_1, tstr(info_1, "name"))))))));
}

// UI_definitions.lua:1180
Table *create_popup_UIBox_tooltip(Table *tooltip) {
    Val title = tget(tooltip, "title");
    Table *text = ttab(tooltip, "text");
    if (!text) text = tnew();
    Table *rows = tnew();
    if (truthy(title)) {
        Table *r = NODE(UIT_R, TAB("align", S("cm")), ARR(
            T(NODE(UIT_C, TAB("align", S("cm")), ARR(
                T(NODE(UIT_T, TAB("text", title, "colour", P(G->C.UI.TEXT_DARK), "scale", N(0.4)), NULL)))))));
        tinsert(rows, T(r));
    }
    for (int i = 1, n = tlen(text); i <= n; i++) {
        Val text_i = tgeti(text, i);
        if (text_i.t == V_TAB) {
            Table *r = NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), ARR(
                T(NODE(UIT_T, TAB("ref_table", tget(text_i.tab, "ref_table"), "ref_value", tget(text_i.tab, "ref_value"), "colour", P(G->C.UI.TEXT_DARK), "scale", N(0.4)), NULL))));
            tinsert(rows, T(r));
        } else {
            Table *r = NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.03)), ARR(
                T(NODE(UIT_T, TAB("text", text_i, "colour", P(G->C.UI.TEXT_DARK), "scale", N(0.4)), NULL))));
            tinsert(rows, T(r));
        }
    }
    Table *filler = ttab(tooltip, "filler");
    if (filler) {
        Val filler_args = tget(filler, "args");
        tinsert(rows, fcall(tget(filler, "func"), 1, &filler_args));
    }
    Table *t = NODE(UIT_ROOT, TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.RED), "emboss", N(0.05)),
        ARR(T(NODE(UIT_C, TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE), "emboss", N(0.05)), rows))));
    return t;
}

// UI_definitions.lua:1210
Table *create_UIBox_HUD_blind(void) {
    num scale = 0.4;
    Val stake = tget(G->GAME, "stake");
    Sprite *stake_sprite = get_stake_sprite(truthy(stake) ? (int)stake.n : 1, 0.5);
    Blind *blind = tobj(G->GAME, "blind");
    Blind_change_dim(blind, 1.5, 1.5);

    // note: made here, in the Lua's order (C does not fix the order in which arguments are evaluated)
    DynaText *blind_name = DynaText_new(TAB("string", T(ARR(T(TAB("ref_table", O(blind), "ref_value", S("loc_name"))))), "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "shadow", B(true), "rotate", B(true), "silent", B(true), "float", B(true), "scale", N(1.6 * scale), "y_offset", N(-4)));
    DynaText *dollars_text = DynaText_new(TAB("string", T(ARR(T(TAB("ref_table", T(ttab(G->GAME, "current_round")), "ref_value", S("dollars_to_be_earned"))))), "colours", T(ARR(P(G->C.MONEY))), "shadow", B(true), "rotate", B(true), "bump", B(true), "silent", B(true), "scale", N(0.45)));

    return NODE(UIT_ROOT, TAB("align", S("cm"), "minw", N(4.5), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05), "padding", N(0.05), "func", S("HUD_blind_visible"), "id", S("HUD_blind")), ARR(
        T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.7), "r", N(0.1), "emboss", N(0.05), "colour", P(G->C.DYN_UI.MAIN)), ARR(
            T(NODE(UIT_C, TAB("align", S("cm"), "minw", N(3)), ARR(
                T(NODE(UIT_O, TAB("object", O(blind_name), "id", S("HUD_blind_name")), NULL)))))))),
        T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(2.74), "r", N(0.1), "colour", P(G->C.DYN_UI.DARK)), ARR(
            T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.05)), ARR(
                T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.3), "maxw", N(4.2)), ARR(
                    T(NODE(UIT_T, TAB("ref_table", T(TAB("val", S(""))), "ref_value", S("val"), "scale", N(scale * 0.9), "colour", P(G->C.UI.TEXT_LIGHT), "func", S("HUD_blind_debuff_prefix")), NULL)),
                    T(NODE(UIT_T, TAB("ref_table", T(blind->loc_debuff_lines), "ref_value", N(1), "scale", N(scale * 0.9), "colour", P(G->C.UI.TEXT_LIGHT), "id", S("HUD_blind_debuff_1"), "func", S("HUD_blind_debuff")), NULL))))),
                T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.3), "maxw", N(4.2)), ARR(
                    T(NODE(UIT_T, TAB("ref_table", T(blind->loc_debuff_lines), "ref_value", N(2), "scale", N(scale * 0.9), "colour", P(G->C.UI.TEXT_LIGHT), "id", S("HUD_blind_debuff_2"), "func", S("HUD_blind_debuff")), NULL)))))))),
            T(NODE(UIT_R, TAB("align", S("cm"), "padding", N(0.15)), ARR(
                T(NODE(UIT_O, TAB("object", O(blind), "draw_layer", N(1)), NULL)),
                T(NODE(UIT_C, TAB("align", S("cm"), "r", N(0.1), "padding", N(0.05), "emboss", N(0.05), "minw", N(2.9), "colour", P(G->C.BLACK)), ARR(
                    T(NODE(UIT_R, TAB("align", S("cm"), "maxw", N(2.8)), ARR(
                        T(NODE(UIT_T, TAB("text", LOC("ph_blind_score_at_least"), "scale", N(0.3), "colour", P(G->C.WHITE), "shadow", B(true)), NULL))))),
                    T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.6)), ARR(
                        T(NODE(UIT_O, TAB("w", N(0.5), "h", N(0.5), "colour", P(G->C.BLUE), "object", O(stake_sprite), "hover", B(true), "can_collide", B(false)), NULL)),
                        T(NODE(UIT_B, TAB("h", N(0.1), "w", N(0.1)), NULL)),
                        T(NODE(UIT_T, TAB("ref_table", O(blind), "ref_value", S("chip_text"), "scale", N(0.001), "colour", P(G->C.RED), "shadow", B(true), "id", S("HUD_blind_count"), "func", S("blind_chip_UI_scale")), NULL))))),
                    T(NODE(UIT_R, TAB("align", S("cm"), "minh", N(0.45), "maxw", N(2.8), "func", S("HUD_blind_reward")), ARR(
                        T(NODE(UIT_T, TAB("text", LOC("ph_blind_reward"), "scale", N(0.3), "colour", P(G->C.WHITE)), NULL)),
                        T(NODE(UIT_O, TAB("object", O(dollars_text), "id", S("dollars_to_be_earned")), NULL))))))))))))))));
}

// UI_definitions.lua:1252
void add_tag(Tag *_tag) {
    if (!G->HUD_tags) G->HUD_tags = tnew();
    Table *tag_sprite_ui = Tag_generate_UI(_tag, -1, NULL);
    // the constructor's arguments are evaluated before the new box is appended
    bool first = !truthy(tgeti(G->HUD_tags, 1));
    UIBox *box = UIBox_new(TAB(
        "definition", T(NODE(UIT_ROOT, TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR)), ARR(
            T(tag_sprite_ui)))),
        "config", T(TAB(
            "align", S(!first ? "tm" : "bri"),
            "offset", T(!first ? TAB("x", N(0), "y", N(0)) : TAB("x", N(0.7), "y", N(0))),
            "major", !first ? tgeti(G->HUD_tags, tlen(G->HUD_tags)) : O(G->ROOM_ATTACH)))));
    tseti(G->HUD_tags, tlen(G->HUD_tags) + 1, O(box));
    discover_card(ttab(G->P_TAGS, _tag->key));

    Table *tags = ttab(G->GAME, "tags");
    for (int i = 1, n = tlen(tags); i <= n; i++) {
        Tag_apply_to_run(tobji(tags, i), TAB("type", S("tag_add"), "tag", O(_tag)));
    }

    tseti(tags, tlen(tags) + 1, O(_tag));
    _tag->HUD_tag = tobji(G->HUD_tags, tlen(G->HUD_tags));
}
