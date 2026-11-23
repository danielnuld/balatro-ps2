// functions/common_events.lua:580-1162: eval_card, set_alerts, set_main_menu_UI, card_eval_status_text,
// add_round_eval_row, change_shop_size, juice_card, update_canvas_juice, juice_card_until.
#include "all.h"

// common_events.lua:580
Table *eval_card(Card *card, Table *context) {
    if (!context) context = tnew();
    Table *ret = tnew();

    if (tbool(context, "repetition_only")) {
        Table *seals = Card_calculate_seal(card, context);
        if (seals) tsett(ret, "seals", seals);
        return ret;
    }

    // note: veq keeps the Lua's nil == nil (no cardarea in the context while the area does not exist)
    if (veq(tget(context, "cardarea"), O(G->play))) {
        num chips = Card_get_chip_bonus(card);
        if (chips > 0) tsetn(ret, "chips", chips);

        num mult = Card_get_chip_mult(card);
        if (mult > 0) tsetn(ret, "mult", mult);

        num x_mult = Card_get_chip_x_mult(card, context);
        if (x_mult > 0) tsetn(ret, "x_mult", x_mult);

        num p_dollars = Card_get_p_dollars(card);
        if (p_dollars > 0) tsetn(ret, "p_dollars", p_dollars);

        Val jokers = Card_calculate_joker(card, context);
        if (truthy(jokers)) tset(ret, "jokers", jokers);

        Table *edition = Card_get_edition(card);
        if (edition) tsett(ret, "edition", edition);
    }

    if (veq(tget(context, "cardarea"), O(G->hand))) {
        num h_mult = Card_get_chip_h_mult(card);
        if (h_mult > 0) tsetn(ret, "h_mult", h_mult);

        num h_x_mult = Card_get_chip_h_x_mult(card);
        if (h_x_mult > 0) tsetn(ret, "x_mult", h_x_mult);

        Val jokers = Card_calculate_joker(card, context);
        if (truthy(jokers)) tset(ret, "jokers", jokers);
    }

    // :641 context.card (not cardarea) is compared with G.consumeables, as the Lua writes it
    if (veq(tget(context, "cardarea"), O(G->jokers)) || veq(tget(context, "card"), O(G->consumeables))) {
        Val jokers = NIL;
        if (tbool(context, "edition")) {
            jokers = T(Card_get_edition(card));
        } else if (tbool(context, "other_joker")) {
            jokers = Card_calculate_joker((Card *)tobj(context, "other_joker"), context);
        } else {
            jokers = Card_calculate_joker(card, context);
        }
        if (truthy(jokers)) tset(ret, "jokers", jokers);
    }

    return ret;
}

static bool discovered_not_alerted(Table *v) { return tbool(v, "discovered") && !tbool(v, "alerted"); }

// common_events.lua:658
void set_alerts(void) {
    if (!G->REFRESH_ALERTS) return;
    G->REFRESH_ALERTS = false;
    bool alert_joker = false, alert_voucher = false, alert_tarot = false, alert_planet = false, alert_spectral = false,
         alert_blind = false, alert_edition = false, alert_tag = false, alert_seal = false, alert_booster = false;
    for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
        Table *v = it.val.tab;
        if (discovered_not_alerted(v)) {
            const char *set = tstr(v, "set");
            if (streq(set, "Voucher")) alert_voucher = true;
            if (streq(set, "Tarot")) alert_tarot = true;
            if (streq(set, "Planet")) alert_planet = true;
            if (streq(set, "Spectral")) alert_spectral = true;
            if (streq(set, "Joker")) alert_joker = true;
            if (streq(set, "Edition")) alert_edition = true;
            if (streq(set, "Booster")) alert_booster = true;
        }
    }
    for (TIter it = titer(G->P_BLINDS); tnext(&it);)
        if (discovered_not_alerted(it.val.tab)) alert_blind = true;
    for (TIter it = titer(G->P_TAGS); tnext(&it);)
        if (discovered_not_alerted(it.val.tab)) alert_tag = true;
    for (TIter it = titer(G->P_SEALS); tnext(&it);)
        if (discovered_not_alerted(it.val.tab)) alert_seal = true;

    bool alert_any = alert_voucher || alert_joker || alert_tarot || alert_planet || alert_spectral || alert_blind ||
                     alert_edition || alert_seal || alert_tag;

    Table *alertables = ttab(G->ARGS, "set_alerts_alertables");
    if (!alertables) {
        static const char *const ids[11] = {"your_collection",          "your_collection_jokers",   "your_collection_tarots",
                                            "your_collection_planets",  "your_collection_spectrals", "your_collection_vouchers",
                                            "your_collection_editions", "your_collection_blinds",   "your_collection_tags",
                                            "your_collection_seals",    "your_collection_boosters"};
        alertables = tnew();
        for (int i = 0; i < 11; i++)
            tinsert(alertables, T(TAB("id", S(ids[i]), "alert_uibox_name", S(fmt("%s_alert", ids[i])))));
        tsett(G->ARGS, "set_alerts_alertables", alertables);
    }
    bool should[11] = {alert_any,     alert_joker, alert_tarot, alert_planet, alert_spectral, alert_voucher,
                       alert_edition, alert_blind, alert_tag,   alert_seal,   alert_booster};
    for (int i = 0; i < 11; i++) tsetb(ttabi(alertables, i + 1), "should_alert", should[i]);

    Table *Gdyn = G->object.dyn; // G[v.alert_uibox_name] (game.h, "in dyn:")
    for (int k = 1; truthy(tgeti(alertables, k)); k++) {
        Table *v = ttabi(alertables, k);
        const char *alert_uibox_name = tstr(v, "alert_uibox_name");
        UIBox *box = tobj(Gdyn, alert_uibox_name);
        UIElement *uie = G->OVERLAY_MENU ? UIBox_get_UIE_by_ID(G->OVERLAY_MENU, tget(v, "id"), NULL) : NULL;
        if (uie) {
            if (tbool(v, "should_alert")) {
                if (!box) {
                    box = UIBox_new(TAB("definition", T(create_UIBox_card_alert(TAB("red_bad", B(true)))),
                                        "config", T(TAB("align", S("tri"), "offset", T(TAB("x", N(0.05), "y", N(-0.05))),
                                                        "major", O(uie), "instance_type", S("ALERT")))));
                    tseto(Gdyn, alert_uibox_name, box);
                    box->moveable.node.states.collide.can = false;
                }
            } else if (box) {
                CALL(box, remove);
                tset(Gdyn, alert_uibox_name, NIL);
            }
        } else if (box) {
            CALL(box, remove);
            tset(Gdyn, alert_uibox_name, NIL);
        }
    }

    if (G->MAIN_MENU_UI) {
        if (alert_any) {
            if (!G->collection_alert) {
                G->collection_alert = UIBox_new(
                    TAB("definition", T(create_UIBox_card_alert(NULL)),
                        "config", T(TAB("align", S("tri"), "offset", T(TAB("x", N(0.05), "y", N(-0.05))),
                                        "major", O(UIBox_get_UIE_by_ID(G->MAIN_MENU_UI, S("collection_button"), NULL))))));
                G->collection_alert->moveable.node.states.collide.can = false;
            }
        } else if (G->collection_alert) {
            CALL(G->collection_alert, remove);
            G->collection_alert = NULL;
        }
    } else if (G->collection_alert) {
        CALL(G->collection_alert, remove);
        G->collection_alert = NULL;
    }
}

// common_events.lua:763
static bool ev_main_menu_profile_button(void *ctx) {
    if (!truthy(G->F_DISP_USERNAME) || G->F_DISP_USERNAME.t == V_STR) {
        G->PROFILE_BUTTON = UIBox_new(TAB("definition", T(create_UIBox_profile_button()),
                                          "config", T(TAB("align", S("bli"), "offset", T(TAB("x", N(-10), "y", N(0))),
                                                          "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
        G->PROFILE_BUTTON->moveable.alignment.offset.x = 0;
        Moveable_align_to_major((Moveable *)G->PROFILE_BUTTON);
        return true;
    }
    return false; // the Lua returns nothing: the event never completes
}

// common_events.lua:753
void set_main_menu_UI(void) {
    G->MAIN_MENU_UI = UIBox_new(TAB("definition", T(create_UIBox_main_menu_buttons()),
                                    "config", T(TAB("align", S("bmi"), "offset", T(TAB("x", N(0), "y", N(10))),
                                                    "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
    G->MAIN_MENU_UI->moveable.alignment.offset.y = 0;
    Moveable_align_to_major((Moveable *)G->MAIN_MENU_UI);
    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .not_blocking = true, .func = ev_main_menu_profile_button}),
          NULL, false);

    Controller_snap_to(G->CONTROLLER, TAB("node", O(UIBox_get_UIE_by_ID(G->MAIN_MENU_UI, S("main_menu_play"), NULL))));
}

struct status_text_ctx {
    Card *card;
    Val text;
    Val config_scale; // config.scale, NIL as nil
    num delay;
    float *colour;
    const char *card_aligned;
    float y_off;
    const char *sound;
    float percent, volume;
    Table *extra;
};

// common_events.lua:882-896 and :902-916 (the same body, now or in the event)
static void status_text_show(struct status_text_ctx *c) {
    // (extrafunc is always nil)
    attention_text(TAB("text", c->text,
                       "scale", truthy(c->config_scale) ? c->config_scale : N(1),
                       "hold", N(c->delay - 0.2),
                       "backdrop_colour", P(c->colour),
                       "align", S(c->card_aligned),
                       "major", O(c->card),
                       "offset", T(TAB("x", N(0), "y", N(c->y_off)))));
    play_sound(c->sound, 0.8 + c->percent * 0.2, c->volume);
    if (!c->extra || !tbool(c->extra, "no_juice")) {
        CALL(c->card, juice_up, 0.6f, N(0.1));
        Table *room = G->ROOM->object.dyn;
        tsetn(room, "jiggle", tnum(room, "jiggle") + 0.7);
    }
}

static bool ev_status_text(void *ctx) {
    status_text_show(ctx);
    return true;
}

// common_events.lua:779
void card_eval_status_text(Card *card, const char *eval_type, num amt, float percent, const char *dir, Table *extra) {
    if (percent < 0) percent = 0.9 + 0.2 * math_random();
    if (streq(dir, "down")) percent = 1 - percent;

    if (extra && tbool(extra, "focus")) card = tobj(extra, "focus");

    Val text = S("");
    const char *sound = NULL;
    float volume = 1;
    const char *card_aligned = "bm";
    float y_off = 0.15 * G->CARD_H;
    if (card->area == G->jokers || card->area == G->consumeables) {
        y_off = 0.05 * card->moveable.node.T->h;
    } else if (card->area == G->hand) {
        y_off = -0.05 * G->CARD_H;
        card_aligned = "tm";
    } else if (card->area == G->play) {
        y_off = -0.05 * G->CARD_H;
        card_aligned = "tm";
    } else if (tbool(card->moveable.node.object.dyn, "jimbo")) {
        y_off = -0.05 * G->CARD_H;
        card_aligned = "tm";
    }
    Val config_scale = NIL; // config.scale (config.type is never read; config.colour is always nil)
    num delay = 0.65;
    float *colour = (extra && tbool(extra, "colour")) ? UI_colour(extra, "colour") : G->C.FILTER;

    if (streq(eval_type, "debuff")) {
        sound = "cancel";
        amt = 1;
        colour = G->C.RED;
        config_scale = N(0.6);
        text = localize(S("k_debuffed"), NULL);
    } else if (streq(eval_type, "chips")) {
        sound = "chips1";
        colour = G->C.CHIPS;
        text = localize(T(TAB("type", S("variable"), "key", S("a_chips"), "vars", T(ARR(N(amt))))), NULL);
        delay = 0.6;
    } else if (streq(eval_type, "mult")) {
        sound = "multhit1";
        text = localize(T(TAB("type", S("variable"), "key", S("a_mult"), "vars", T(ARR(N(amt))))), NULL);
        colour = G->C.MULT;
        config_scale = N(0.7);
    } else if (streq(eval_type, "x_mult") || streq(eval_type, "h_x_mult")) {
        sound = "multhit2";
        volume = 0.7;
        text = localize(T(TAB("type", S("variable"), "key", S("a_xmult"), "vars", T(ARR(N(amt))))), NULL);
        colour = G->C.XMULT;
        config_scale = N(0.7);
    } else if (streq(eval_type, "h_mult")) {
        sound = "multhit1";
        text = localize(T(TAB("type", S("variable"), "key", S("a_mult"), "vars", T(ARR(N(amt))))), NULL);
        colour = G->C.MULT;
        config_scale = N(0.7);
    } else if (streq(eval_type, "dollars")) {
        sound = "coin3";
        text = S(fmt("%s%s%s", amt < -0.01 ? "-" : "", vstr(localize(S("$"), NULL)), number_tostring(fabs(amt))));
        colour = amt < -0.01 ? G->C.RED : G->C.MONEY;
    } else if (streq(eval_type, "swap")) {
        sound = "generic1";
        text = localize(S("k_swapped_ex"), NULL);
        colour = G->C.PURPLE;
    } else if (streq(eval_type, "extra") || streq(eval_type, "jokers")) {
        bool edition = tbool(extra, "edition");
        sound = edition ? "foil2" : tbool(extra, "mult_mod") ? "multhit1" : tbool(extra, "Xmult_mod") ? "multhit2" : "generic1";
        if (edition) colour = G->C.DARK_EDITION;
        volume = edition ? 0.3 : streq(sound, "multhit2") ? 0.7 : 1;
        delay = tbool(extra, "delay") ? vnum(tget(extra, "delay")) : 0.75;
        amt = 1;
        if (tbool(extra, "message")) text = tget(extra, "message");
        if (!edition && (tbool(extra, "mult_mod") || tbool(extra, "Xmult_mod"))) colour = G->C.MULT;
        if (tbool(extra, "chip_mod")) {
            colour = G->C.CHIPS;
            config_scale = N(0.7);
        } else if (tbool(extra, "swap")) {
            colour = G->C.PURPLE;
            config_scale = N(0.7);
        } else {
            config_scale = N(0.7);
        }
    }
    delay = delay * 1.25;

    if (amt > 0 || amt < 0) {
        struct status_text_ctx *c = NEWCTX(struct status_text_ctx);
        *c = (struct status_text_ctx){card, text, config_scale, delay, colour, card_aligned, y_off, sound, percent, volume, extra};
        if (extra && tbool(extra, "instant")) {
            status_text_show(c);
        } else {
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = delay, .func = ev_status_text, .ctx = c}), NULL,
                  false);
        }
    }
    if (extra && tbool(extra, "playing_cards_created")) playing_card_joker_effects(ttab(extra, "playing_cards_created"));
}

// what the closures of add_round_eval_row capture; dollar_row is the shared upvalue the dollar events increment
struct round_eval_ctx {
    Table *config;
    float width;
    num num_dollars;
    int dollar_row;
};
struct round_eval_dollar_ctx {
    struct round_eval_ctx *c;
    num i;
};
static const num round_eval_scale = 0.9; // local scale (:931)

static UIElement *round_eval_target(Table *config) {
    return UIBox_get_UIE_by_ID(G->round_eval, S(tbool(config, "bonus") ? "bonus_round_eval" : "base_round_eval"), NULL);
}

// {shader = 'dissolve', shadow_height = 0.05}, {shader = 'dissolve'} (:963, :987)
static Table *round_eval_draw_steps(void) {
    return ARR(T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))), T(TAB("shader", S("dissolve"))));
}

// the small light DynaText of a row's left text
static Val round_eval_dyna(Val string, float *colour, num scale) {
    return O(DynaText_new(TAB("string", string, "colours", T(ARR(P(colour))), "shadow", B(true), "pop_in", N(0), "scale",
                              N(scale), "silent", B(true))));
}

// common_events.lua:938
static bool ev_round_eval_divider(void *ctx) {
    struct round_eval_ctx *c = ctx;
    Table *spacer = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(c->width))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB(
            "string", T(ARR(S("......................................"))), "colours", T(ARR(P(G->C.WHITE))),
            "shadow", B(true), "float", B(true), "y_offset", N(-30), "scale", N(0.45), "spacing", N(13.5),
            "font", T(ttab(ttab(G->LANGUAGES, "en-us"), "font")), "pop_in", N(0)))))))))));
    UIBox_add_child(G->round_eval, spacer, round_eval_target(c->config));
    return true;
}

// common_events.lua:957
static bool ev_round_eval_row(void *ctx) {
    struct round_eval_ctx *c = ctx;
    Table *config = c->config;
    const char *name = tstr(config, "name");
    const num scale = round_eval_scale;
    Blind *blind = tobj(G->GAME, "blind");
    // Add the far left text and context first:
    Table *left_text = tnew();
    if (streq(name, "blind1")) {
        Val stake = tget(G->GAME, "stake");
        Sprite *stake_sprite = get_stake_sprite(truthy(stake) ? (int)vnum(stake) : 1, 0.5f);
        AnimatedSprite *blind_sprite = NEW(AnimatedSprite);
        AnimatedSprite_init(blind_sprite, 0, 0, 1.2f, 1.2f, ANIMATION_ATLAS_get("blind_chips"), copy_table(blind->pos));
        Sprite_define_draw_steps((Sprite *)blind_sprite, round_eval_draw_steps());
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("w", N(1.2), "h", N(1.2), "object", O(blind_sprite),
                                                                   "hover", B(true), "can_collide", B(false))))));

        Table *col_config = TAB("padding", N(0.05), "align", S("cm"));
        if (tbool(config, "saved")) {
            Table *o = TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
                T(ARR(S(fmt(" %s ", vstr(localize(S("ph_mr_bones"), NULL)))))), G->C.FILTER, 0.5 * scale))));
            Table *row = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(T(o))));
            tinsert(left_text, T(TAB("n", N(UIT_C), "config", T(col_config), "nodes", T(ARR(T(row))))));
        } else {
            Table *o = TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
                T(ARR(S(fmt(" %s ", vstr(localize(S("ph_score_at_least"), NULL)))))), G->C.UI.TEXT_LIGHT, 0.4 * scale))));
            Table *row1 = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(T(o))));
            Table *stake_o = TAB("n", N(UIT_O), "config", T(TAB("w", N(0.5), "h", N(0.5), "object", O(stake_sprite),
                                                                 "hover", B(true), "can_collide", B(false))));
            Table *chips_t = TAB("n", N(UIT_T), "config", T(TAB("text", S(blind->chip_text),
                                                                 "scale", N(scale_number(N(blind->chips), scale, 100000)),
                                                                 "colour", P(G->C.RED), "shadow", B(true))));
            Table *row2 = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.8))), "nodes",
                              T(ARR(T(stake_o), T(chips_t))));
            tinsert(left_text, T(TAB("n", N(UIT_C), "config", T(col_config), "nodes", T(ARR(T(row1), T(row2))))));
        }
    } else if (str_find(name, "tag", 1)) {
        Sprite *blind_sprite = NEW(Sprite);
        Sprite_init(blind_sprite, 0, 0, 0.7f, 0.7f, ASSET_ATLAS_get("tags"), copy_table(ttab(config, "pos")));
        Sprite_define_draw_steps(blind_sprite, round_eval_draw_steps());
        CALL(blind_sprite, juice_up, -1, NIL);
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("w", N(0.7), "h", N(0.7), "object", O(blind_sprite),
                                                                   "hover", B(true), "can_collide", B(false))))));
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
            T(ARR(tget(config, "condition"))), G->C.UI.TEXT_LIGHT, 0.4 * scale))))));
    } else if (streq(name, "hands")) {
        Val disp = tget(config, "disp");
        Val per = tget(ttab(G->GAME, "modifiers"), "money_per_hand");
        tinsert(left_text, T(TAB("n", N(UIT_T), "config", T(TAB("text", truthy(disp) ? disp : tget(config, "dollars"),
            "scale", N(0.8 * scale), "colour", P(G->C.BLUE), "shadow", B(true), "juice", B(true))))));
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
            T(ARR(S(fmt(" %s", vstr(localize(T(TAB("type", S("variable"), "key", S("remaining_hand_money"),
                                                     "vars", T(ARR(truthy(per) ? per : N(1))))), NULL)))))),
            G->C.UI.TEXT_LIGHT, 0.4 * scale))))));
    } else if (streq(name, "discards")) {
        Val disp = tget(config, "disp");
        Val per = tget(ttab(G->GAME, "modifiers"), "money_per_discard");
        tinsert(left_text, T(TAB("n", N(UIT_T), "config", T(TAB("text", truthy(disp) ? disp : tget(config, "dollars"),
            "scale", N(0.8 * scale), "colour", P(G->C.RED), "shadow", B(true), "juice", B(true))))));
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
            T(ARR(S(fmt(" %s", vstr(localize(T(TAB("type", S("variable"), "key", S("remaining_discard_money"),
                                                     "vars", T(ARR(truthy(per) ? per : N(0))))), NULL)))))),
            G->C.UI.TEXT_LIGHT, 0.4 * scale))))));
    } else if (str_find(name, "joker", 1)) {
        Table *center = ttab(((Card *)tobj(config, "card"))->moveable.node.config, "center");
        // string is the localized name itself, not wrapped in a table (:1001)
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
            localize(T(TAB("type", S("name_text"), "set", tget(center, "set"), "key", tget(center, "key"))), NULL),
            G->C.FILTER, 0.6 * scale))))));
    } else if (streq(name, "interest")) {
        num interest_amount = tnum(G->GAME, "interest_amount");
        tinsert(left_text, T(TAB("n", N(UIT_T), "config", T(TAB("text", N(c->num_dollars), "scale", N(0.8 * scale),
            "colour", P(G->C.MONEY), "shadow", B(true), "juice", B(true))))));
        tinsert(left_text, T(TAB("n", N(UIT_O), "config", T(TAB("object", round_eval_dyna(
            T(ARR(S(fmt(" %s", vstr(localize(T(TAB("type", S("variable"), "key", S("interest"), "vars", T(ARR(
                N(interest_amount), N(5), N(interest_amount * tnum(G->GAME, "interest_cap") / 5))))), NULL)))))),
            G->C.UI.TEXT_LIGHT, 0.4 * scale))))));
    }
    Table *dollar_col = TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "id", S(fmt("dollar_%s", name)))), "nodes", T(tnew()));
    Table *left_col = TAB("n", N(UIT_C), "config",
                          T(TAB("padding", N(0.05), "minw", N(c->width * 0.55), "minh", N(0.61), "align", S("cl"))),
                          "nodes", T(left_text));
    Table *right_col = TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.05), "minw", N(c->width * 0.45), "align", S("cr"))),
                           "nodes", T(ARR(T(dollar_col))));
    Table *full_row = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(5))), "nodes",
                          T(ARR(T(left_col), T(right_col))));

    if (streq(name, "blind1")) Blind_juice_up(blind, -1, NIL);
    UIBox_add_child(G->round_eval, full_row, round_eval_target(config));
    Val pitch = tget(config, "pitch");
    play_sound("cancel", truthy(pitch) ? vnum(pitch) : 1, -1);
    // :1016 (1.5*config.pitch) or 1: the `or` never applies, a nil pitch is the Lua's error
    if (pitch.t != V_NUM) ERROR("attempt to perform arithmetic on field 'pitch' (a %s value)", lua_typename_of(pitch));
    play_sound("highlight1", 1.5 * pitch.n, 0.2f);
    if (tbool(config, "card")) CALL(tobj(config, "card"), juice_up, 0.7f, N(0.46));
    return true;
}

// common_events.lua:1026
static bool ev_round_eval_dollars_many(void *ctx) {
    struct round_eval_ctx *c = ctx;
    const char *name = tstr(c->config, "name");
    DynaText *dt = DynaText_new(TAB(
        "string", T(ARR(S(fmt("%s%s", vstr(localize(S("$"), NULL)), number_tostring(c->num_dollars))))),
        "colours", T(ARR(P(G->C.MONEY))), "shadow", B(true), "pop_in", N(0), "scale", N(0.65), "float", B(true)));
    Table *o = TAB("n", N(UIT_O), "config", T(TAB("object", O(dt))));
    UIBox_add_child(G->round_eval,
                    TAB("n", N(UIT_R), "config",
                        T(TAB("align", S("cm"), "id", S(fmt("dollar_row_%d_%s", c->dollar_row + 1, name)))), "nodes", T(ARR(T(o)))),
                    UIBox_get_UIE_by_ID(G->round_eval, S(fmt("dollar_%s", name)), NULL));

    play_sound("coin3", 0.9 + 0.2 * math_random(), 0.7f);
    play_sound("coin6", 1.3f, 0.8f);
    return true;
}

// common_events.lua:1042
static bool ev_round_eval_dollar(void *ctx) {
    struct round_eval_dollar_ctx *d = ctx;
    struct round_eval_ctx *c = d->c;
    const char *name = tstr(c->config, "name");
    num num_dollars = c->num_dollars;
    if (lmod(d->i, 30) == 1) {
        UIBox_add_child(G->round_eval,
            TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "id", S(fmt("dollar_row_%d_%s", c->dollar_row + 1, name)))),
                "nodes", T(tnew())),
            UIBox_get_UIE_by_ID(G->round_eval, S(fmt("dollar_%s", name)), NULL));
        c->dollar_row = c->dollar_row + 1;
    }

    Table *r = TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("$"), NULL), "colour", P(G->C.MONEY),
        "scale", N(num_dollars > 20 ? 0.28 : num_dollars > 9 ? 0.43 : 0.58),
        "shadow", B(true), "hover", B(true), "can_collide", B(false), "juice", B(true))));
    play_sound("coin3", 0.9 + 0.2 * math_random(), 0.7 - (num_dollars > 20 ? 0.2 : 0));

    if (streq(name, "blind1")) {
        Table *current_round = ttab(G->GAME, "current_round");
        tsets(current_round, "dollars_to_be_earned", str_sub(tstr(current_round, "dollars_to_be_earned"), 2, -1));
    }

    UIBox_add_child(G->round_eval, r, UIBox_get_UIE_by_ID(G->round_eval, S(fmt("dollar_row_%d_%s", c->dollar_row, name)), NULL));
    G->VIBRATION = G->VIBRATION + 0.4;
    return true;
}

// common_events.lua:1068
static bool ev_round_eval_bottom(void *ctx) {
    struct round_eval_ctx *c = ctx;
    Table *config = c->config;
    Table *t1 = TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s: ", vstr(localize(S("b_cash_out"), NULL)))),
                                                    "scale", N(1), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))));
    Table *t2 = TAB("n", N(UIT_T), "config",
                    T(TAB("text", S(fmt("%s%s", vstr(localize(S("$"), NULL)), tostring(tget(config, "dollars")))),
                          "scale", N(1.2 * round_eval_scale), "colour", P(G->C.WHITE), "shadow", B(true), "juice", B(true))));
    Table *button = TAB("n", N(UIT_R), "config",
                        T(TAB("id", S("cash_out_button"), "align", S("cm"), "padding", N(0.1), "minw", N(7), "r", N(0.15),
                              "colour", P(G->C.ORANGE), "shadow", B(true), "hover", B(true), "one_press", B(true),
                              "button", S("cash_out"), "focus_args", T(TAB("snap_to", B(true))))),
                        "nodes", T(ARR(T(t1), T(t2))));
    UIBox_new(TAB("definition", T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))),
                                      "nodes", T(ARR(T(button))))),
                  "config", T(TAB("align", S("tmi"), "offset", T(TAB("x", N(0), "y", N(0.4))), "major", O(G->round_eval)))));

    tset(ttab(G->GAME, "current_round"), "dollars", tget(config, "dollars"));

    Val pitch = tget(config, "pitch");
    play_sound("coin6", truthy(pitch) ? vnum(pitch) : 1, -1);
    G->VIBRATION = G->VIBRATION + 1;
    return true;
}

// common_events.lua:927
void add_round_eval_row(Table *config) {
    if (!config) config = tnew();
    struct round_eval_ctx *c = NEWCTX(struct round_eval_ctx);
    c->config = config;
    c->width = G->round_eval->moveable.node.T->w - 0.51;
    Val dollars = tget(config, "dollars");
    num num_dollars = truthy(dollars) ? vnum(dollars) : 1;
    c->num_dollars = num_dollars;
    const char *name = tstr(config, "name");

    if (!streq(name, "bottom")) {
        if (!streq(name, "blind1")) {
            Table *re_dyn = G->round_eval->moveable.node.object.dyn;
            if (!tbool(re_dyn, "divider_added")) {
                E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.25, .func = ev_round_eval_divider, .ctx = c}),
                      NULL, false);
                delay(0.6, NULL);
                tsetb(re_dyn, "divider_added", true);
            }
        } else {
            delay(0.2, NULL);
        }

        delay(0.2, NULL);

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.5, .func = ev_round_eval_row, .ctx = c}), NULL,
              false);
        c->dollar_row = 0;
        if (num_dollars > 60) {
            // (local dollar_string, :1023, is never used)
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.trigger = "before", .delay = 0.38, .func = ev_round_eval_dollars_many, .ctx = c}), NULL, false);
        } else {
            for (num i = 1; i <= num_dollars; i++) {
                struct round_eval_dollar_ctx *d = NEWCTX(struct round_eval_dollar_ctx);
                d->c = c;
                d->i = i;
                E_add(G->E_MANAGER,
                      Event_new((EventCfg){.trigger = "before",
                                           .delay = 0.18 - (num_dollars > 20 ? 0.13 : num_dollars > 9 ? 0.1 : 0),
                                           .func = ev_round_eval_dollar,
                                           .ctx = d}),
                      NULL, false);
            }
        }
    } else {
        delay(0.4, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.5, .func = ev_round_eval_bottom, .ctx = c}), NULL,
              false);
    }
}

// common_events.lua:1097
void change_shop_size(num mod) {
    Table *shop = ttab(G->GAME, "shop");
    if (!shop) return;
    tsetn(shop, "joker_max", tnum(shop, "joker_max") + mod);
    if (G->shop_jokers && G->shop_jokers->cards) {
        if (mod < 0) {
            // Remove jokers in shop
            for (int i = CardArea_n(G->shop_jokers); i >= tnum(shop, "joker_max") + 1; i--) {
                if (CardArea_card(G->shop_jokers, i)) CALL(CardArea_card(G->shop_jokers, i), remove);
            }
        }
        tsetn(G->shop_jokers->moveable.node.config, "card_limit", tnum(shop, "joker_max"));
        G->shop_jokers->moveable.node.T->w = tnum(shop, "joker_max") * 1.01 * G->CARD_W;
        UIBox_recalculate(G->shop);
        if (mod > 0) {
            num n = tnum(shop, "joker_max") - CardArea_n(G->shop_jokers);
            for (num i = 1; i <= n; i++) CardArea_emplace(G->shop_jokers, create_card_for_shop(G->shop_jokers), NULL, false);
        }
    }
}

static bool ev_juice_card(void *ctx) {
    CALL(ctx, juice_up, 0.7f, NIL);
    return true;
}

// common_events.lua:1120
void juice_card(Card *card) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_juice_card, .ctx = card}), NULL, false);
}

// common_events.lua:1127
void update_canvas_juice(float dt) {
    Table *room = G->ROOM->object.dyn; // G.ROOM.jiggle
    Val jiggle = tget(room, "jiggle");
    G->JIGGLE_VIBRATION = truthy(jiggle) ? vnum(jiggle) : 0;
    Val ss = tget(G->SETTINGS, "screenshake");
    bool reduced_motion = tbool(G->SETTINGS, "reduced_motion");
    if (!truthy(ss) || ss.t != V_NUM) {
        ss = N(reduced_motion ? 0 : 50);
        tset(G->SETTINGS, "screenshake", ss);
    }
    num screenshake = ss.n;
    num shake_amt = (reduced_motion ? 0 : 1) * lmax(0, screenshake - 30) / 100;
    Transform *cur = G->CURSOR->moveable.node.T, *RT = G->ROOM->T;
    Vec2 cp = G->CONTROLLER->cursor_position;
    Table *e = ttab(G->ARGS, "eased_cursor_pos");
    if (!e) {
        e = TAB("x", N(cur->x), "y", N(cur->y), "sx", N(cp.x), "sy", N(cp.y));
        tsett(G->ARGS, "eased_cursor_pos", e);
    }
    tsetn(e, "x", tnum(e, "x") * (1 - 3 * dt) + 3 * dt * (shake_amt * cur->x + (1 - shake_amt) * RT->w / 2));
    tsetn(e, "y", tnum(e, "y") * (1 - 3 * dt) + 3 * dt * (shake_amt * cur->y + (1 - shake_amt) * RT->h / 2));
    tsetn(e, "sx", tnum(e, "sx") * (1 - 3 * dt) + 3 * dt * (shake_amt * cp.x + (1 - shake_amt) * G->WINDOWTRANS.real_window_w / 2));
    tsetn(e, "sy", tnum(e, "sy") * (1 - 3 * dt) + 3 * dt * (shake_amt * cp.y + (1 - shake_amt) * G->WINDOWTRANS.real_window_h / 2));

    shake_amt = (reduced_motion ? 0 : 1) * screenshake / 100 * 3;
    if (shake_amt < 0.05) shake_amt = 0;

    num jig = (truthy(jiggle) ? vnum(jiggle) : 0) * (1 - 5 * dt) * (shake_amt > 0.05 ? 1 : 0);
    tsetn(room, "jiggle", jig);
    float REAL = (float)G->TIMERS.REAL;
    // (the room's sway: the sines in float, wrap_angle of core.h; a double sin() is all library calls on the PS2)
    RT->r = (0.001 * sinf(wrap_angle(0.3f * REAL)) + 0.002 * jig * sinf(wrap_angle(39.913f * REAL))) * shake_amt;
    RT->x = G->ROOM_ORIG.x + shake_amt * (0.015 * sinf(wrap_angle(0.913f * REAL)) + 0.01 * (jig * shake_amt) * sinf(wrap_angle(19.913f * REAL)) +
                                          (tnum(e, "x") - 0.5 * (RT->w + G->ROOM_ORIG.x)) * 0.01);
    RT->y = G->ROOM_ORIG.y + shake_amt * (0.015 * sinf(wrap_angle(0.952f * REAL)) + 0.01 * (jig * shake_amt) * sinf(wrap_angle(21.913f * REAL)) +
                                          (tnum(e, "y") - 0.5 * (RT->h + G->ROOM_ORIG.y)) * 0.01);

    G->JIGGLE_VIBRATION = G->JIGGLE_VIBRATION * (1 - 5 * dt);
    // (G.CURR_VIBRATION or 0: the member is 0 as nil)
    G->CURR_VIBRATION = lmin(1, G->CURR_VIBRATION + G->VIBRATION + G->JIGGLE_VIBRATION * 0.2);
    G->VIBRATION = 0;
    G->CURR_VIBRATION = (1 - 15 * dt) * G->CURR_VIBRATION;
    if (!tbool(G->SETTINGS, "rumble")) G->CURR_VIBRATION = 0;
    if (G->CONTROLLER->GAMEPAD.object && G->F_RUMBLE)
        Joystick_setVibration(G->CONTROLLER->GAMEPAD.object, G->CURR_VIBRATION * 0.4 * G->F_RUMBLE,
                              G->CURR_VIBRATION * 0.4 * G->F_RUMBLE);
}

struct juice_until_ctx {
    Card *card;
    Val eval_func;
};

static bool ev_juice_card_until(void *ctx) {
    struct juice_until_ctx *c = ctx;
    Val arg = O(c->card);
    if (truthy(fcall(c->eval_func, 1, &arg))) {
        CALL(c->card, juice_up, 0.1f, N(0.1)); // `not first or first` is always true
        juice_card_until(c->card, c->eval_func, false, 0.8);
    }
    return true;
}

// common_events.lua:1156
void juice_card_until(Card *card, Val eval_func, bool first, num delay) {
    struct juice_until_ctx *c = NEWCTX(struct juice_until_ctx);
    c->card = card;
    c->eval_func = eval_func;
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = delay < 0 ? 0.1 : delay, .not_blocking = true, .not_blockable = true,
                               .timer = "REAL", .func = ev_juice_card_until, .ctx = c}),
          NULL, false);
}
