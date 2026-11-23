// functions/button_callbacks.lua:1937-2403: the HUD's total and flames, the run's button predicates, use_card,
// sell_card, the contest name, the tutorial skip and the shop helpers.
#include "all.h"

#define CFG(e) ((e)->moveable.node.config)

// e.config.colour = colour; e.config.button = name (NULL as nil)
static void set_button(UIElement *e, float *colour, const char *button) {
    tset(CFG(e), "colour", P(colour));
    tset(CFG(e), "button", S(button));
}

// X.alignment.offset.py of a UIBox: in its dyn (button_callbacks.h), NIL when not saved.
// note: Moveable.alignment.offset is a Vec2 with no py; the dyn key is "alignment.offset.py", the name the header
// gives the field. end_consumeable (another part of this file) must use the same key.
#define PY_KEY "alignment.offset.py"
static Val py_get(UIBox *box) { return tget(box->moveable.node.object.dyn, PY_KEY); }
// box.alignment.offset.py = box.alignment.offset.y; box.alignment.offset.y = y
static void py_save(UIBox *box, float y) {
    tsetn(box->moveable.node.object.dyn, PY_KEY, box->moveable.alignment.offset.y);
    box->moveable.alignment.offset.y = y;
}
// box.alignment.offset.y = box.alignment.offset.py; box.alignment.offset.py = nil
static void py_restore(UIBox *box) {
    Val py = py_get(box);
    // note: with py nil the Lua writes nil into offset.y (and fails at the next alignment); a float cannot hold
    // it, so y is left as it is.
    if (truthy(py)) box->moveable.alignment.offset.y = (float)py.n;
    tset(box->moveable.node.object.dyn, PY_KEY, NIL);
}

// button_callbacks.lua:1937
void FUNCS_hand_chip_total_UI_set(UIElement *e) {
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    if (tnum(current_hand, "chip_total") < 1) {
        tsets(current_hand, "chip_total_text", "");
    } else {
        const char *new_chip_total_text = number_format(tget(current_hand, "chip_total"));
        if (!veq(S(new_chip_total_text), tget(current_hand, "chip_total_text"))) {
            DynaText *object = tobj(CFG(e), "object");
            object->scale = scale_number(tget(current_hand, "chip_total"), 0.95f, 100000000);

            tsets(current_hand, "chip_total_text", new_chip_total_text);
            Val last = tget(G->ARGS, "hand_chip_total_UI_set");
            if (!truthy(last) || last.n < tnum(current_hand, "chip_total")) {
                FUNCS_text_super_juice(e, (float)floor(log10(tnum(current_hand, "chip_total"))));
            }
            tsetn(G->ARGS, "hand_chip_total_UI_set", tnum(current_hand, "chip_total"));
        }
    }
}

// button_callbacks.lua:1955
void FUNCS_text_super_juice(UIElement *e, float _amount) {
    DynaText *object = tobj(CFG(e), "object");
    DynaText_set_quiver(object, 0.03f * _amount);
    DynaText_pulse(object, 0.3f + 0.08f * _amount);
    DynaText_update_text(object, false);
    DynaText_align_letters(object);
    UIElement_update_object(e);
}

// button_callbacks.lua:1963
void FUNCS_flame_handler(UIElement *e) {
    // G.C.UI_CHIPLICK = G.C.UI_CHIPLICK or {1, 1, 1, 1} (game.h: all zero is the Lua's nil)
    float *chiplick = G->C.UI_CHIPLICK, *multlick = G->C.UI_MULTLICK;
    if (chiplick[0] == 0 && chiplick[1] == 0 && chiplick[2] == 0 && chiplick[3] == 0)
        chiplick[0] = chiplick[1] = chiplick[2] = chiplick[3] = 1;
    if (multlick[0] == 0 && multlick[1] == 0 && multlick[2] == 0 && multlick[3] == 0)
        multlick[0] = multlick[1] = multlick[2] = multlick[3] = 1;
    for (int i = 0; i < 3; i++) {
        chiplick[i] = (float)lmin(lmax(pow((G->C.UI_CHIPS[i] * 0.5 + G->C.YELLOW[i] * 0.5) + 0.1, 2), 0.1), 1);
        multlick[i] = (float)lmin(lmax(pow((G->C.UI_MULT[i] * 0.5 + G->C.YELLOW[i] * 0.5) + 0.1, 2), 0.1), 1);
    }

    if (!truthy(tget(G->ARGS, "flame_handler"))) {
        tsett(G->ARGS, "flame_handler", TAB(
            "chips", T(TAB(
                "id", S("flame_chips"),
                "arg_tab", S("chip_flames"),
                "colour", P(G->C.UI_CHIPS),
                "accent", P(G->C.UI_CHIPLICK))),
            "mult", T(TAB(
                "id", S("flame_mult"),
                "arg_tab", S("mult_flames"),
                "colour", P(G->C.UI_MULT),
                "accent", P(G->C.UI_MULTLICK)))));
    }
    for (TIter it = titer(ttab(G->ARGS, "flame_handler")); tnext(&it);) {
        Table *v = it.val.tab;
        if (veq(tget(CFG(e), "id"), tget(v, "id"))) {
            const char *arg_tab = tstr(v, "arg_tab");
            void *object = tobj(CFG(e), "object");
            if (!is_a(object, &Sprite_class) || !veq(N(((Node *)object)->ID), tget(v, "ID"))) {
                CALL(object, remove);
                Sprite *sprite = NEW(Sprite);
                Sprite_init(sprite, 0, 0, 2.5f, 2.5f, ASSET_ATLAS_get("ui_1"), TAB("x", N(2), "y", N(0)));
                tseto(CFG(e), "object", sprite);
                tsetn(v, "ID", sprite->moveable.node.ID);
                Table *F = TAB(
                    "intensity", N(0),
                    "real_intensity", N(0),
                    "intensity_vel", N(0),
                    "colour_1", tget(v, "colour"),
                    "colour_2", tget(v, "accent"),
                    "timer", N(G->TIMERS.REAL));
                tsett(G->ARGS, arg_tab, F);
                Moveable_set_alignment((Moveable *)sprite, TAB(
                    "major", O(e->moveable.node.parent),
                    "type", S("bmi"),
                    "offset", T(TAB("x", N(0), "y", N(0))),
                    "xy_bond", S("Weak")));
                // note: image_dims and RETS.get_pos_pixel are not tables in C: the entry refers to the sprite itself
                // and sprite.c's custom send reads them from it by these ref_value names.
                Sprite_define_draw_steps(sprite, ARR(T(TAB(
                    "shader", S("flame"),
                    "send", T(ARR(
                        T(TAB("name", S("time"), "ref_table", T(F), "ref_value", S("timer"))),
                        T(TAB("name", S("amount"), "ref_table", T(F), "ref_value", S("real_intensity"))),
                        T(TAB("name", S("image_details"), "ref_table", O(sprite), "ref_value", S("image_dims"))),
                        T(TAB("name", S("texture_details"), "ref_table", O(sprite), "ref_value", S("get_pos_pixel"))),
                        T(TAB("name", S("colour_1"), "ref_table", T(F), "ref_value", S("colour_1"))),
                        T(TAB("name", S("colour_2"), "ref_table", T(F), "ref_value", S("colour_2"))),
                        T(TAB("name", S("id"), "val", N(sprite->moveable.node.ID)))))))));
                float pos_pixel[4]; // note: the Lua fills RETS.get_pos_pixel here; the send computes it each time
                CALL(sprite, get_pos_pixel, pos_pixel);
            }
            Table *_F = ttab(G->ARGS, arg_tab);
            num real_dt = G->real_dt;
            num exptime = exp(-0.4 * real_dt);

            Table *score_intensity = ttab(G->ARGS, "score_intensity");
            if (tnum(score_intensity, "earned_score") >= tnum(score_intensity, "required_score") &&
                tnum(score_intensity, "required_score") > 0) {
                // math.log(x, 5) is log(x)/log(5)
                tsetn(_F, "intensity", ((G->pack_cards && !G->pack_cards->moveable.node.REMOVED) || G->TAROT_INTERRUPT)
                                           ? 0
                                           : lmax(0., log(tnum(score_intensity, "earned_score")) / log(5) - 2));
            } else {
                tsetn(_F, "intensity", 0);
            }

            tsetn(_F, "timer", tnum(_F, "timer") + real_dt * (1 + tnum(_F, "intensity") * 0.2));
            if (tnum(_F, "intensity_vel") < 0) tsetn(_F, "intensity_vel", tnum(_F, "intensity_vel") * (1 - 10 * real_dt));
            tsetn(_F, "intensity_vel", (1 - exptime) * (tnum(_F, "intensity") - tnum(_F, "real_intensity")) * real_dt * 25 +
                                           exptime * tnum(_F, "intensity_vel"));
            tsetn(_F, "real_intensity", lmax(0, tnum(_F, "real_intensity") + tnum(_F, "intensity_vel")));
            Val change = tget(_F, "change");
            tsetn(_F, "change", (truthy(change) ? change.n : 0) * (1 - 4. * real_dt) +
                                    (4. * real_dt) * (tnum(_F, "real_intensity") < tnum(_F, "intensity") - 0.0 ? 1 : 0) *
                                        tnum(_F, "real_intensity"));
        }
    }
}

// button_callbacks.lua:2036
void FUNCS_hand_text_UI_set(UIElement *e) {
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    if (!veq(tget(current_hand, "handname"), tget(current_hand, "handname_text"))) {
        tset(current_hand, "handname_text", tget(current_hand, "handname"));
        DynaText *object = tobj(CFG(e), "object");
        int len = str_len(tstr(current_hand, "handname"));
        if (len >= 13) {
            object->scale = (float)(12 * 0.56 / len);
        } else {
            object->scale = (float)(2.4 / sqrt(len + 5));
        }
        DynaText_update_text(object, false);
    }
}

// button_callbacks.lua:2048
void FUNCS_can_play(UIElement *e) {
    Blind *blind = tobj(G->GAME, "blind");
    if (CardArea_n_highlighted(G->hand) <= 0 || blind->block_play || CardArea_n_highlighted(G->hand) > 5) {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    } else {
        set_button(e, G->C.BLUE, "play_cards_from_highlighted");
    }
}

// button_callbacks.lua:2058
void FUNCS_can_start_run(UIElement *e) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    if (!tbool(ttab(viewed_back->effect, "center"), "unlocked")) {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    } else {
        set_button(e, G->C.BLUE, "start_setup_run");
    }
}

// button_callbacks.lua:2068
void FUNCS_visible_blind(UIElement *e) {
    Blind *blind = tobj(G->GAME, "blind");
    if (!tempty(ttab(blind->moveable.node.config, "blind"))) {
        e->moveable.node.states.visible = true;
    } else {
        e->moveable.node.states.visible = false;
    }
}

// button_callbacks.lua:2076
void FUNCS_can_reroll(UIElement *e) {
    num reroll_cost = tnum(ttab(G->GAME, "current_round"), "reroll_cost");
    if (((tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at")) - reroll_cost < 0) && reroll_cost != 0) {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    } else {
        set_button(e, G->C.GREEN, "reroll_shop");
    }
}

// button_callbacks.lua:2092
void FUNCS_can_discard(UIElement *e) {
    if (tnum(ttab(G->GAME, "current_round"), "discards_left") <= 0 || CardArea_n_highlighted(G->hand) <= 0) {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    } else {
        set_button(e, G->C.RED, "discard_cards_from_highlighted");
    }
}

// button_callbacks.lua:2102
void FUNCS_can_use_consumeable(UIElement *e) {
    if (Card_can_use_consumeable(tobj(CFG(e), "ref_table"), false, false)) {
        set_button(e, G->C.RED, "use_card");
    } else {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    }
}

// button_callbacks.lua:2112
void FUNCS_can_select_card(UIElement *e) {
    Card *card = tobj(CFG(e), "ref_table");
    if (!streq(tstr(card->ability, "set"), "Joker") || (card->edition && tbool(card->edition, "negative")) ||
        CardArea_n(G->jokers) < tnum(G->jokers->moveable.node.config, "card_limit")) {
        set_button(e, G->C.GREEN, "use_card");
    } else {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    }
}

// button_callbacks.lua:2122
void FUNCS_can_sell_card(UIElement *e) {
    if (Card_can_sell_card(tobj(CFG(e), "ref_table"), NULL)) {
        set_button(e, G->C.GREEN, "sell_card");
    } else {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    }
}

// button_callbacks.lua:2132
void FUNCS_can_skip_booster(UIElement *e) {
    if (G->pack_cards && CardArea_card(G->pack_cards, 1) &&
        (G->STATE == STATES_PLANET_PACK || G->STATE == STATES_STANDARD_PACK || G->STATE == STATES_BUFFOON_PACK ||
         (G->hand && (CardArea_card(G->hand, 1) || tnum(G->hand->moveable.node.config, "card_limit") <= 0)))) {
        set_button(e, G->C.GREY, "skip_booster");
    } else {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    }
}

// button_callbacks.lua:2143
void FUNCS_show_infotip(UIElement *e) {
    Val ref_table = tget(CFG(e), "ref_table");
    if (truthy(ref_table)) {
        UIBox *info = UIBox_new(TAB(
            "definition", T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.02))), "nodes", ref_table)),
            "config", T(TAB("offset", T(TAB("x", N(-0.03), "y", N(0))), "align", S("cl"), "parent", O(e)))));
        tseto(e->moveable.node.children, "info", info);
        Moveable_align_to_major((Moveable *)info);
        tset(CFG(e), "ref_table", NIL);
    }
}

// The upvalues of use_card's closures. dont_dissolve and delay_fac are assigned before the events that read them
// are queued, so their final values are copied.
struct use_card_ctx {
    UIElement *e;
    Card *card;
    CardArea *area;
    int prev_state;
    bool dont_dissolve;
    num delay_fac;
};

// button_callbacks.lua:2164
static bool ev_use_card_rearm(void *ctx) {
    struct use_card_ctx *c = ctx;
    if (c->e) { // e NULL: the made-up element of the direct callers (button_callbacks.h)
        c->e->disable_button = false;
        tsets(CFG(c->e), "button", "use_card");
    }
    return true;
}

// button_callbacks.lua:2300
static bool ev_use_card_refocus(void *ctx) {
    struct use_card_ctx *c = ctx;
    G->CONTROLLER->interrupt.focus = false;
    if (streq(tstr(c->card->ability, "set"), "Voucher")) {
        Controller_snap_to(G->CONTROLLER, TAB("node", O(UIBox_get_UIE_by_ID(G->shop, S("next_round_button"), NULL))));
    } else if (c->area) {
        Controller_recall_cardarea_focus(G->CONTROLLER, O(c->area));
    }
    return true;
}

// button_callbacks.lua:2299
static bool ev_use_card_refocus_outer(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_use_card_refocus, .ctx = ctx}), NULL, false);
    return true;
}

// button_callbacks.lua:2262
static bool ev_use_card_restore(void *ctx) {
    struct use_card_ctx *c = ctx;
    int prev_state = c->prev_state;
    CardArea *area = c->area;
    G->STATE = prev_state;
    G->TAROT_INTERRUPT = 0;
    tsetb(G->CONTROLLER->locks, "use", false);

    if ((prev_state == STATES_TAROT_PACK || prev_state == STATES_PLANET_PACK ||
         prev_state == STATES_SPECTRAL_PACK || prev_state == STATES_STANDARD_PACK ||
         prev_state == STATES_BUFFOON_PACK) && G->booster_pack) {
        Val pack_choices = tget(G->GAME, "pack_choices");
        if (area == G->consumeables) {
            py_restore(G->booster_pack);
        } else if (truthy(pack_choices) && pack_choices.n > 1) {
            if (truthy(py_get(G->booster_pack))) {
                py_restore(G->booster_pack);
            }
            tsetn(G->GAME, "pack_choices", pack_choices.n - 1);
        } else {
            G->CONTROLLER->interrupt.focus = true;
            if (prev_state == STATES_TAROT_PACK) inc_career_stat("c_tarot_reading_used", 1);
            if (prev_state == STATES_PLANET_PACK) inc_career_stat("c_planetarium_used", 1);
            FUNCS_end_consumeable(NULL, c->delay_fac);
        }
    } else {
        if (G->shop) py_restore(G->shop);
        if (G->blind_select) py_restore(G->blind_select);
        if (G->round_eval) py_restore(G->round_eval);
        if (area && CardArea_card(area, 1)) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_use_card_refocus_outer, .ctx = c}), NULL, false);
        }
    }
    return true;
}

// button_callbacks.lua:2259
static bool ev_use_card_dissolve(void *ctx) {
    struct use_card_ctx *c = ctx;
    if (!c->dont_dissolve) Card_start_dissolve(c->card, NULL, false, -1, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_use_card_restore, .ctx = c}), NULL, false);
    return true;
}

// card.children[name]: remove it and set it to nil, if there
static void remove_card_child(Card *card, const char *name) {
    void *child = Card_child(card, name);
    if (child) {
        CALL(child, remove);
        tset(card->moveable.node.children, name, NIL);
    }
}

// button_callbacks.lua:2155
void FUNCS_use_card(UIElement *e, Card *card, bool mute, bool nosave) {
    if (e) tset(CFG(e), "button", NIL);
    if (!card) card = tobj(CFG(e), "ref_table"); // e.config.ref_table is this card everywhere below
    CardArea *area = card->area;
    int prev_state = G->STATE;
    bool dont_dissolve = false;
    num delay_fac = 1;

    struct use_card_ctx *c = NEWCTX(struct use_card_ctx);
    c->e = e, c->card = card, c->area = area, c->prev_state = prev_state;

    if (Card_check_use(card)) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_use_card_rearm, .ctx = c}), NULL, false);
        return;
    }

    const char *set = tstr(card->ability, "set");
    if (streq(set, "Booster") && !nosave && G->STATE == STATES_SHOP) {
        save_with_action(TAB(
            "type", S("use_card"),
            "card", N(card->sort_id)));
    }

    G->TAROT_INTERRUPT = G->STATE;
    if (streq(set, "Booster")) tsetn(G->GAME, "PACK_INTERRUPT", G->STATE);
    G->STATE = (G->STATE == STATES_TAROT_PACK) ? STATES_TAROT_PACK :
               (G->STATE == STATES_PLANET_PACK) ? STATES_PLANET_PACK :
               (G->STATE == STATES_SPECTRAL_PACK) ? STATES_SPECTRAL_PACK :
               (G->STATE == STATES_STANDARD_PACK) ? STATES_STANDARD_PACK :
               (G->STATE == STATES_BUFFOON_PACK) ? STATES_BUFFOON_PACK :
               STATES_PLAY_TAROT;

    tsetb(G->CONTROLLER->locks, "use", true);
    Val pack_choices = tget(G->GAME, "pack_choices");
    if (G->booster_pack && !truthy(py_get(G->booster_pack)) &&
        (truthy(tget(card->ability, "consumeable")) || !(truthy(pack_choices) && pack_choices.n > 1))) {
        py_save(G->booster_pack, G->ROOM->T->y + 29);
    }
    if (G->shop && !truthy(py_get(G->shop))) {
        py_save(G->shop, G->ROOM->T->y + 29);
    }
    if (G->blind_select && !truthy(py_get(G->blind_select))) {
        py_save(G->blind_select, G->ROOM->T->y + 39);
    }
    if (G->round_eval && !truthy(py_get(G->round_eval))) {
        py_save(G->round_eval, G->ROOM->T->y + 29);
    }

    remove_card_child(card, "use_button");
    remove_card_child(card, "sell_button");
    remove_card_child(card, "price");

    if (card->area) CardArea_remove_card(card->area, card, false);

    if (truthy(tget(card->ability, "consumeable"))) {
        if (G->STATE == STATES_TAROT_PACK || G->STATE == STATES_PLANET_PACK || G->STATE == STATES_SPECTRAL_PACK) {
            Transform *T = card->moveable.node.T, *hand_T = G->hand->moveable.node.T;
            T->x = hand_T->x + hand_T->w / 2 - T->w / 2;
            T->y = hand_T->y + hand_T->h / 2 - T->h / 2 - 0.5f;
            discover_card(ttab(card->moveable.node.config, "center"));
        } else {
            draw_card(G->hand, G->play, 1, "up", true, card, -1, mute, false, -1, false);
        }
        delay(0.2, NULL);
        Card_use_consumeable(card, area, NULL);
        for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("using_consumeable", B(true), "consumeable", O(card)));
        }
    } else if (streq(set, "Enhanced") || streq(set, "Default")) {
        G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1: 0 as nil
        CardArea_emplace(G->deck, card, NULL, false);
        play_sound("card1", 0.8f, 0.6f);
        play_sound("generic1", -1, -1);
        card->playing_card = G->playing_card;
        playing_card_joker_effects(ARR(O(card)));
        Card_add_to_deck(card, false);
        tinsert(G->playing_cards, O(card));
        dont_dissolve = true;
        delay_fac = 0.2;
    } else if (streq(set, "Joker")) {
        Card_add_to_deck(card, false);
        CardArea_emplace(G->jokers, card, NULL, false);
        play_sound("card1", 0.8f, 0.6f);
        play_sound("generic1", -1, -1);
        dont_dissolve = true;
        delay_fac = 0.2;
    } else if (streq(set, "Booster")) {
        delay(0.1, NULL);
        Val booster_pos = tget(card->ability, "booster_pos");
        if (truthy(booster_pos)) tsetv(ttab(ttab(G->GAME, "current_round"), "used_packs"), booster_pos, S("USED"));
        draw_card(G->hand, G->play, 1, "up", true, card, -1, true, false, -1, false);
        if (!tbool(card->moveable.node.object.dyn, "from_tag")) { // card.from_tag: not a member of Card
            Table *cards_purchased = ttab(ttab(G->GAME, "round_scores"), "cards_purchased");
            tsetn(cards_purchased, "amt", tnum(cards_purchased, "amt") + 1);
        }
        Card_open(card);
    } else if (streq(set, "Voucher")) {
        delay(0.1, NULL);
        draw_card(G->hand, G->play, 1, "up", true, card, -1, true, false, -1, false);
        Table *cards_purchased = ttab(ttab(G->GAME, "round_scores"), "cards_purchased");
        tsetn(cards_purchased, "amt", tnum(cards_purchased, "amt") + 1);
        Card_redeem(card);
    }
    // note: card.ability is re-read as the Lua does (no call above replaces a Booster's ability)
    if (streq(tstr(card->ability, "set"), "Booster")) {
        tsetb(G->CONTROLLER->locks, "use", false);
        G->TAROT_INTERRUPT = 0;
    } else {
        c->dont_dissolve = dont_dissolve, c->delay_fac = delay_fac;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_use_card_dissolve, .ctx = c}), NULL, false);
    }
}

// button_callbacks.lua:2318
void FUNCS_sell_card(UIElement *e) {
    Card *card = tobj(CFG(e), "ref_table");
    Card_sell_card(card);
    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++) {
        if (CardArea_card(G->jokers, i) != card) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("selling_card", B(true), "card", O(card)));
        }
    }
}

// button_callbacks.lua:2328
void FUNCS_can_confirm_contest_name(UIElement *e) {
    Table *COMP = ttab(G->SETTINGS, "COMP");
    if (COMP && !veq(tget(COMP, "name"), S(""))) {
        set_button(e, G->C.UI.BACKGROUND_INACTIVE, NULL);
    } else {
        set_button(e, G->C.PALE_GREEN, "confirm_contest_name");
    }
}

// button_callbacks.lua:2338
void FUNCS_confirm_contest_name(UIElement *e) {
    tsetb(ttab(G->SETTINGS, "COMP"), "submission_name", true);
    if (G->MAIN_MENU_UI) UIBox_remove(G->MAIN_MENU_UI);
    if (G->PROFILE_BUTTON) UIBox_remove(G->PROFILE_BUTTON);
    set_main_menu_UI();
    Game_save_progress(G);
    tsetb(G->FILE_HANDLER, "force", true);
}

// button_callbacks.lua:2347
void FUNCS_change_contest_name(UIElement *e) {
    tsets(ttab(G->SETTINGS, "COMP"), "name", "");
    if (G->MAIN_MENU_UI) UIBox_remove(G->MAIN_MENU_UI);
    if (G->PROFILE_BUTTON) UIBox_remove(G->PROFILE_BUTTON);
    set_main_menu_UI();
}

// button_callbacks.lua:2354
void FUNCS_skip_tutorial_section(UIElement *e) {
    Table *tutorial = G->OVERLAY_TUTORIAL->moveable.node.object.dyn; // skip_steps, Jimbo, content: in dyn (game.h)
    tsetb(tutorial, "skip_steps", true);

    if (tobj(tutorial, "Jimbo")) CALL(tobj(tutorial, "Jimbo"), remove);
    if (tobj(tutorial, "content")) CALL(tobj(tutorial, "content"), remove);
    UIBox_remove(G->OVERLAY_TUTORIAL);
    G->OVERLAY_TUTORIAL = NULL;
    EventManager_clear_queue(G->E_MANAGER, "tutorial", NULL);
    Table *tutorial_progress = ttab(G->SETTINGS, "tutorial_progress");
    Table *completed_parts = ttab(tutorial_progress, "completed_parts");
    const char *section = tstr(tutorial_progress, "section");
    if (streq(section, "small_blind")) {
        tsetb(completed_parts, "small_blind", true);
    } else if (streq(section, "big_blind")) {
        tsetb(completed_parts, "big_blind", true);
        tset(tutorial_progress, "forced_tags", NIL);
    } else if (streq(section, "second_hand")) {
        tsetb(completed_parts, "second_hand", true);
        tsetb(ttab(tutorial_progress, "hold_parts"), "second_hand", true);
    } else if (streq(section, "first_hand")) {
        tsetb(completed_parts, "first_hand", true);
        tsetb(completed_parts, "first_hand_2", true);
        tsetb(completed_parts, "first_hand_3", true);
        tsetb(completed_parts, "first_hand_4", true);
        tsetb(completed_parts, "first_hand_section", true);
    } else if (streq(section, "shop")) {
        tsetb(completed_parts, "shop_1", true);
        tset(tutorial_progress, "forced_voucher", NIL);
    }
}

// button_callbacks.lua:2382
void FUNCS_shop_voucher_empty(UIElement *e) {
    if (G->shop_vouchers && G->shop_vouchers->cards &&
        (CardArea_card(G->shop_vouchers, 1) || truthy(tget(ttab(G->GAME, "current_round"), "voucher")))) {
        e->moveable.node.states.visible = false;
    } else if (!streq(tstr(G->SETTINGS, "language"), "en-us")) {
        e->moveable.node.states.visible = false;
    } else {
        e->moveable.node.states.visible = true;
    }
}

// button_callbacks.lua:2392
bool FUNCS_check_for_buy_space(Card *card) {
    const char *set = tstr(card->ability, "set");
    bool consumeable = truthy(tget(card->ability, "consumeable"));
    int negative = (card->edition && tbool(card->edition, "negative")) ? 1 : 0;
    if (!streq(set, "Voucher") &&
        !streq(set, "Enhanced") &&
        !streq(set, "Default") &&
        !(streq(set, "Joker") && CardArea_n(G->jokers) < tnum(G->jokers->moveable.node.config, "card_limit") + negative) &&
        !(consumeable && CardArea_n(G->consumeables) < tnum(G->consumeables->moveable.node.config, "card_limit") + negative)) {
        alert_no_space(card, consumeable ? G->consumeables : G->jokers);
        return false;
    }
    return true;
}
