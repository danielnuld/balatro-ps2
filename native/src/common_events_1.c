// functions/common_events.lua:1-579: screen positions, the eased HUD counters, ease_value and the background
// colours, delay, add_joker, draw_card, highlight_card, play_area_status_text, level_up_hand, update_hand_text.
#include "all.h"

// A colour kept in a table. note: the headers say P(float *); a prototype loaded by data.h may still hold the
// Lua's {r, g, b, a} table (boss_colour), so that shape is read too. NULL as nil.
static float *colour_val(Val v) {
    if (v.t == V_PTR) return (float *)v.p;
    if (v.t == V_TAB) {
        float *c = calloc(4, sizeof(float));
        for (int i = 0; i < 4; i++) c[i] = (float)tnumi(v.tab, i + 1);
        return c;
    }
    return NULL;
}

// <UIElement>.config.object:update() (the Lua passes no dt)
static void object_update(UIElement *e) {
    CALL(tobj(e->moveable.node.config, "object"), update, 0.0f);
}

// common_events.lua:1
void set_screen_positions(void) {
    if (G->STAGE == STAGES_RUN) {
        Transform *hand = G->hand->moveable.node.T, *play = G->play->moveable.node.T, *jokers = G->jokers->moveable.node.T,
                  *consumeables = G->consumeables->moveable.node.T, *deck = G->deck->moveable.node.T,
                  *discard = G->discard->moveable.node.T;
        hand->x = G->TILE_W - hand->w - 2.85f;
        hand->y = G->TILE_H - hand->h;

        play->x = hand->x + (hand->w - play->w) / 2;
        play->y = hand->y - 3.6f;

        jokers->x = hand->x - 0.1f;
        jokers->y = 0;

        consumeables->x = jokers->x + jokers->w + 0.2f;
        consumeables->y = 0;

        deck->x = G->TILE_W - deck->w - 0.5f;
        deck->y = G->TILE_H - deck->h;

        discard->x = jokers->x + jokers->w / 2 + 0.3f + 15;
        discard->y = 4.2f;

        CALL(G->hand, hard_set_VT);
        CALL(G->play, hard_set_VT);
        CALL(G->jokers, hard_set_VT);
        CALL(G->consumeables, hard_set_VT);
        CALL(G->deck, hard_set_VT);
        CALL(G->discard, hard_set_VT);
    }
    if (G->STAGE == STAGES_MAIN_MENU) {
        Transform *title_top = G->title_top->moveable.node.T;
        if (G->STATE == STATES_DEMO_CTA) {
            title_top->x = G->TILE_W / 2 - title_top->w / 2;
            title_top->y = G->TILE_H / 2 - title_top->h / 2 - 2;
        } else {
            title_top->x = G->TILE_W / 2 - title_top->w / 2;
            title_top->y = G->TILE_H / 2 - title_top->h / 2 - (G->debug_splash_size_toggle ? 2 : 1.2f);
        }

        CALL(G->title_top, hard_set_VT);
    }
}

// common_events.lua:57, :272 function(t) return math.floor(t) end
static num ease_floor(num t, void *ctx) { return floor(t); }

struct mod_ctx { num mod; bool silent; };

// common_events.lua:44
static bool ev_ease_chips(void *ctx) {
    struct mod_ctx *c = ctx;
    UIElement *chip_UI = UIBox_get_UIE_by_ID(G->HUD, S("chip_UI_count"), NULL);

    // (mod = mod or 0: mod is never nil here)
    // Ease from current chips to the new number of chips
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "ease", .not_blockable = true, .ref_table = G->GAME, .ref_value = "chips",
                               .ease_to = c->mod, .delay = 0.3, .ease_func = ease_floor}),
          NULL, false);
    // Popup text next to the chips in UI showing number of chips gained/lost
    UIElement_juice_up(chip_UI, -1, NIL);
    // Play a chip sound
    play_sound("chips2", -1, -1);
    return true;
}

// common_events.lua:41
void ease_chips(num mod) {
    struct mod_ctx *c = NEWCTX(struct mod_ctx);
    c->mod = mod;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_chips, .ctx = c}), NULL, false);
}

// common_events.lua:69 local function _mod(mod)
static void ease_dollars_mod(num mod) {
    UIElement *dollar_UI = UIBox_get_UIE_by_ID(G->HUD, S("dollar_text_UI"), NULL);
    const char *text = fmt("+%s", vstr(localize(S("$"), NULL)));
    float *col = G->C.MONEY;
    if (mod < 0) {
        text = fmt("-%s", vstr(localize(S("$"), NULL)));
        col = G->C.RED;
    } else {
        inc_career_stat("c_dollars_earned", mod);
    }
    // Ease from current chips to the new number of chips
    tsetn(G->GAME, "dollars", tnum(G->GAME, "dollars") + mod);
    check_and_set_high_score("most_money", tnum(G->GAME, "dollars"));
    check_for_unlock(TAB("type", S("money")));
    object_update(dollar_UI);
    UIBox_recalculate(G->HUD);
    // Popup text next to the chips in UI showing number of chips gained/lost
    attention_text(TAB("text", S(fmt("%s%s", text, number_tostring(fabs(mod)))), "scale", N(0.8), "hold", N(0.7), "cover",
                       O(dollar_UI->moveable.node.parent), "cover_colour", P(col), "align", S("cm")));
    // Play a chip sound
    play_sound("coin1", -1, -1);
}

// common_events.lua:103
static bool ev_ease_dollars(void *ctx) {
    ease_dollars_mod(((struct mod_ctx *)ctx)->mod);
    return true;
}

// common_events.lua:68
void ease_dollars(num mod, bool instant) {
    if (instant) {
        ease_dollars_mod(mod);
    } else {
        struct mod_ctx *c = NEWCTX(struct mod_ctx);
        c->mod = mod;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_dollars, .ctx = c}), NULL, false);
    }
}

// common_events.lua:112 local _mod = function(mod) (silent is its upvalue)
static void ease_discard_mod(num mod, bool silent) {
    Table *current_round = ttab(G->GAME, "current_round");
    if (fabs(lmax(tnum(current_round, "discards_left"), mod)) == 0) return;
    UIElement *discard_UI = UIBox_get_UIE_by_ID(G->HUD, S("discard_UI_count"), NULL);
    mod = lmax(-tnum(current_round, "discards_left"), mod);
    const char *text = "+";
    float *col = G->C.GREEN;
    if (mod < 0) {
        text = "";
        col = G->C.RED;
    }
    // Ease from current chips to the new number of chips
    tsetn(current_round, "discards_left", tnum(current_round, "discards_left") + mod);
    // Popup text next to the chips in UI showing number of chips gained/lost
    object_update(discard_UI);
    UIBox_recalculate(G->HUD);
    attention_text(TAB("text", S(fmt("%s%s", text, number_tostring(mod))), "scale", N(0.8), "hold", N(0.7), "cover",
                       O(discard_UI->moveable.node.parent), "cover_colour", P(col), "align", S("cm")));
    // Play a chip sound
    if (!silent) play_sound("chips2", -1, -1);
}

// common_events.lua:144
static bool ev_ease_discard(void *ctx) {
    struct mod_ctx *c = ctx;
    ease_discard_mod(c->mod, c->silent);
    return true;
}

// common_events.lua:111
void ease_discard(num mod, bool instant, bool silent) {
    if (instant) {
        ease_discard_mod(mod, silent);
    } else {
        struct mod_ctx *c = NEWCTX(struct mod_ctx);
        c->mod = mod;
        c->silent = silent;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_discard, .ctx = c}), NULL, false);
    }
}

// common_events.lua:153 local _mod = function(mod)
static void ease_hands_played_mod(num mod) {
    Table *current_round = ttab(G->GAME, "current_round");
    UIElement *hand_UI = UIBox_get_UIE_by_ID(G->HUD, S("hand_UI_count"), NULL);
    const char *text = "+";
    float *col = G->C.GREEN;
    if (mod < 0) {
        text = "";
        col = G->C.RED;
    }
    // Ease from current chips to the new number of chips
    tsetn(current_round, "hands_left", tnum(current_round, "hands_left") + mod);
    object_update(hand_UI);
    UIBox_recalculate(G->HUD);
    // Popup text next to the chips in UI showing number of chips gained/lost
    attention_text(TAB("text", S(fmt("%s%s", text, number_tostring(mod))), "scale", N(0.8), "hold", N(0.7), "cover",
                       O(hand_UI->moveable.node.parent), "cover_colour", P(col), "align", S("cm")));
    // Play a chip sound
    play_sound("chips2", -1, -1);
}

// common_events.lua:183
static bool ev_ease_hands_played(void *ctx) {
    ease_hands_played_mod(((struct mod_ctx *)ctx)->mod);
    return true;
}

// common_events.lua:152
void ease_hands_played(num mod, bool instant) {
    if (instant) {
        ease_hands_played_mod(mod);
    } else {
        struct mod_ctx *c = NEWCTX(struct mod_ctx);
        c->mod = mod;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_hands_played, .ctx = c}), NULL, false);
    }
}

// common_events.lua:194
static bool ev_ease_ante(void *ctx) {
    num mod = ((struct mod_ctx *)ctx)->mod;
    Table *round_resets = ttab(G->GAME, "round_resets");
    UIElement *ante_UI = tobj(G->hand_text_area, "ante");
    const char *text = "+";
    float *col = G->C.IMPORTANT;
    if (mod < 0) {
        text = "-";
        col = G->C.RED;
    }
    tsetn(round_resets, "ante", tnum(round_resets, "ante") + mod);
    check_and_set_high_score("furthest_ante", tnum(round_resets, "ante"));
    object_update(ante_UI);
    UIBox_recalculate(G->HUD);
    // Popup text next to the chips in UI showing number of chips gained/lost
    attention_text(TAB("text", S(fmt("%s%s", text, number_tostring(fabs(mod)))), "scale", N(1), "hold", N(0.7), "cover",
                       O(ante_UI->moveable.node.parent), "cover_colour", P(col), "align", S("cm")));
    // Play a chip sound
    play_sound("highlight2", 0.685f, 0.2f);
    play_sound("generic1", -1, -1);
    return true;
}

// common_events.lua:191
void ease_ante(num mod) {
    struct mod_ctx *c = NEWCTX(struct mod_ctx);
    c->mod = mod;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_ante, .ctx = c}), NULL, false);
}

// common_events.lua:227
static bool ev_ease_round(void *ctx) {
    num mod = ((struct mod_ctx *)ctx)->mod;
    UIElement *round_UI = tobj(G->hand_text_area, "round");
    const char *text = "+";
    float *col = G->C.IMPORTANT;
    if (mod < 0) {
        text = "";
        col = G->C.RED;
    }
    tsetn(G->GAME, "round", tnum(G->GAME, "round") + mod);
    check_and_set_high_score("furthest_round", tnum(G->GAME, "round"));
    check_and_set_high_score("furthest_ante", tnum(ttab(G->GAME, "round_resets"), "ante"));
    object_update(round_UI);
    UIBox_recalculate(G->HUD);
    // Popup text next to the chips in UI showing number of chips gained/lost
    attention_text(TAB("text", S(fmt("%s%s", text, number_tostring(fabs(mod)))), "scale", N(1), "hold", N(0.7), "cover",
                       O(round_UI->moveable.node.parent), "cover_colour", P(col), "align", S("cm")));
    // Play a chip sound
    play_sound("timpani", 0.8f, -1);
    play_sound("generic1", -1, -1);
    return true;
}

// common_events.lua:224
void ease_round(num mod) {
    struct mod_ctx *c = NEWCTX(struct mod_ctx);
    c->mod = mod;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_ease_round, .ctx = c}), NULL, false);
}

// common_events.lua:262 the Event of ease_value; cfg holds the ref form, current is ref_table[ref_value] now.
// (type = ease_type is a key Event does not read: not set.)
static void ease_value_add(EventCfg cfg, num current, num mod, bool floored, const char *timer_type, int not_blockable,
                           num delay) {
    cfg.trigger = "ease";
    cfg.not_blockable = !(not_blockable == 0); // blockable = (not_blockable == false)
    cfg.not_blocking = true;
    cfg.ease_to = current + mod;
    cfg.timer = timer_type;
    cfg.delay = delay < 0 ? 0.3 : delay;
    cfg.ease_func = floored ? ease_floor : NULL; // NULL: function(t) return t end
    E_add(G->E_MANAGER, Event_new(cfg), NULL, false);
}

// common_events.lua:258
void ease_value(Table *ref_table, const char *ref_value, num mod, bool floored, const char *timer_type, int not_blockable,
                num delay, const char *ease_type) {
    ease_value_add((EventCfg){.ref_table = ref_table, .ref_value = ref_value}, tnum(ref_table, ref_value), mod, floored,
                   timer_type, not_blockable, delay);
}

// common_events.lua:258 (ref_table[ref_value] is a float)
void ease_value_f(float *ref, num mod, bool floored, const char *timer_type, int not_blockable, num delay,
                  const char *ease_type) {
    ease_value_add((EventCfg){.ref_float = ref}, *ref, mod, floored, timer_type, not_blockable, delay);
}

// common_events.lua:258 (ref_table[ref_value] is a num)
void ease_value_n(num *ref, num mod, bool floored, const char *timer_type, int not_blockable, num delay,
                  const char *ease_type) {
    ease_value_add((EventCfg){.ref_num = ref}, *ref, mod, floored, timer_type, not_blockable, delay);
}

// common_events.lua:276
void ease_background_colour(Table *args) {
    float *new_colour = colour_val(tget(args, "new_colour"));
    float *special_colour = colour_val(tget(args, "special_colour"));
    float *tertiary_colour = colour_val(tget(args, "tertiary_colour"));
    // pairs(G.C.BACKGROUND): L, D, C, contrast (game.h); contrast is not 'C', 'L' or 'D'
    const char *keys[3] = {"L", "D", "C"};
    float *vs[3] = {G->C.BACKGROUND.L, G->C.BACKGROUND.D, G->C.BACKGROUND.C};
    for (int i = 0; i < 3; i++) {
        const char *k = keys[i];
        float *v = vs[i];
        if (new_colour) {
            if (special_colour && tertiary_colour) {
                float *col = streq(k, "L") ? new_colour : streq(k, "C") ? special_colour : tertiary_colour;
                ease_value_f(&v[0], (num)col[0] - v[0], false, NULL, 1, 0.6, NULL);
                ease_value_f(&v[1], (num)col[1] - v[1], false, NULL, 1, 0.6, NULL);
                ease_value_f(&v[2], (num)col[2] - v[2], false, NULL, 1, 0.6, NULL);
            } else {
                num brightness = streq(k, "L") ? 1.3 : streq(k, "D") ? (special_colour ? 0.4 : 0.7) : 0.9;
                if (streq(k, "C") && special_colour) {
                    ease_value_f(&v[0], (num)special_colour[0] - v[0], false, NULL, 1, 0.6, NULL);
                    ease_value_f(&v[1], (num)special_colour[1] - v[1], false, NULL, 1, 0.6, NULL);
                    ease_value_f(&v[2], (num)special_colour[2] - v[2], false, NULL, 1, 0.6, NULL);
                } else {
                    ease_value_f(&v[0], new_colour[0] * brightness - v[0], false, NULL, 1, 0.6, NULL);
                    ease_value_f(&v[1], new_colour[1] * brightness - v[1], false, NULL, 1, 0.6, NULL);
                    ease_value_f(&v[2], new_colour[2] * brightness - v[2], false, NULL, 1, 0.6, NULL);
                }
            }
        }
    }
    if (truthy(tget(args, "contrast"))) {
        ease_value_f(&G->C.BACKGROUND.contrast, tnum(args, "contrast") - G->C.BACKGROUND.contrast, false, NULL, 1, 0.6, NULL);
    }
}

// common_events.lua:303
void ease_colour(float *old_colour, const float *new_colour, num delay) {
    ease_value_f(&old_colour[0], (num)new_colour[0] - old_colour[0], false, "REAL", -1, delay, NULL);
    ease_value_f(&old_colour[1], (num)new_colour[1] - old_colour[1], false, "REAL", -1, delay, NULL);
    ease_value_f(&old_colour[2], (num)new_colour[2] - old_colour[2], false, "REAL", -1, delay, NULL);
    ease_value_f(&old_colour[3], (num)new_colour[3] - old_colour[3], false, "REAL", -1, delay, NULL);
}

// common_events.lua:311
// note: the colours made here (mix_colours, darken, lighten) are read before the callee returns, so they are locals.
void ease_background_colour_blind(int state, const char *blind_override) {
    Blind *blind = tobj(G->GAME, "blind");
    const char *blindname = blind_override ? blind_override
                            : (blind && blind->name && !streq(blind->name, "")) ? blind->name
                            : "Small Blind";
    blindname = streq(blindname, "") ? "Small Blind" : blindname;
    Colour tmp, tmp2;

    // For the blind related colours
    if (state == STATES_SHOP) {
        ease_colour(G->C.DYN_UI.MAIN, mix_colours(G->C.RED, G->C.BLACK, 0.9f, tmp), -1);
    } else if (state == STATES_TAROT_PACK) {
        ease_colour(G->C.DYN_UI.MAIN, mix_colours(G->C.WHITE, G->C.BLACK, 0.9f, tmp), -1);
    } else if (state == STATES_SPECTRAL_PACK) {
        ease_colour(G->C.DYN_UI.MAIN, mix_colours(G->C.SECONDARY_SET.Spectral, G->C.BLACK, 0.9f, tmp), -1);
    } else if (state == STATES_STANDARD_PACK) {
        ease_colour(G->C.DYN_UI.MAIN, G->C.RED, -1);
    } else if (state == STATES_BUFFOON_PACK) {
        ease_colour(G->C.DYN_UI.MAIN, G->C.FILTER, -1);
    } else if (state == STATES_PLANET_PACK) {
        ease_colour(G->C.DYN_UI.MAIN, mix_colours(G->C.SECONDARY_SET.Planet, G->C.BLACK, 0.9f, tmp), -1);
    } else if (blind) {
        Blind_change_colour(blind, NULL);
    }
    // For the actual background colour
    if (state == STATES_TAROT_PACK) {
        ease_background_colour(TAB("new_colour", P(G->C.PURPLE), "special_colour", P(darken(G->C.BLACK, 0.2f, tmp)), "contrast", N(1.5)));
    } else if (state == STATES_SPECTRAL_PACK) {
        ease_background_colour(TAB("new_colour", P(G->C.SECONDARY_SET.Spectral), "special_colour", P(darken(G->C.BLACK, 0.2f, tmp)),
                                   "contrast", N(2)));
    } else if (state == STATES_STANDARD_PACK) {
        ease_background_colour(TAB("new_colour", P(darken(G->C.BLACK, 0.2f, tmp)), "special_colour", P(G->C.RED), "contrast", N(3)));
    } else if (state == STATES_BUFFOON_PACK) {
        ease_background_colour(TAB("new_colour", P(G->C.FILTER), "special_colour", P(G->C.BLACK), "contrast", N(2)));
    } else if (state == STATES_PLANET_PACK) {
        ease_background_colour(TAB("new_colour", P(G->C.BLACK), "contrast", N(3)));
    } else if (tbool(G->GAME, "won")) {
        ease_background_colour(TAB("new_colour", P(G->C.BLIND.won), "contrast", N(1)));
    } else if (streq(blindname, "Small Blind") || streq(blindname, "Big Blind") || streq(blindname, "")) {
        ease_background_colour(TAB("new_colour", P(G->C.BLIND.Small), "contrast", N(1)));
    } else {
        float *boss_col = G->C.BLACK;
        for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
            Table *v = it.val.tab;
            if (streq(tstr(v, "name"), blindname)) {
                if (tbool(ttab(v, "boss"), "showdown")) {
                    ease_background_colour(TAB("new_colour", P(G->C.BLUE), "special_colour", P(G->C.RED), "tertiary_colour",
                                               P(darken(G->C.BLACK, 0.4f, tmp)), "contrast", N(3)));
                    return;
                }
                float *bc = colour_val(tget(v, "boss_colour"));
                boss_col = bc ? bc : G->C.BLACK;
            }
        }
        ease_background_colour(TAB("new_colour", P(lighten(mix_colours(boss_col, G->C.BLACK, 0.3f, tmp), 0.1f, tmp2)),
                                   "special_colour", P(boss_col), "contrast", N(2)));
    }
}

// common_events.lua:366
static bool ev_true(void *ctx) { return true; }

// common_events.lua:362
void delay(num time, const char *queue) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = time < 0 ? 1 : time, .func = ev_true}), queue, false);
}

// common_events.lua:372
Card *add_joker(const char *joker, const char *edition, bool silent, bool eternal) {
    Table *center = ttab(G->P_CENTERS, joker);
    CardArea *_area = (tbool(center, "consumeable") && G->consumeables) ? G->consumeables : G->jokers;
    float _T_x, _T_y;
    if (_area) {
        _T_x = _area->moveable.node.T->x;
        _T_y = _area->moveable.node.T->y;
    } else {
        _T_x = G->ROOM->T->w / 2 - G->CARD_W / 2;
        _T_y = G->ROOM->T->h / 2 - G->CARD_H / 2;
    }
    Card *card = NEW(Card);
    Card_init(card, _T_x, _T_y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"), center,
              TAB("discover", B(true), "bypass_discovery_center", B(true), "bypass_discovery_ui", B(true), "bypass_back",
                  T(((Back *)tobj(G->GAME, "selected_back"))->pos)));
    Card_start_materialize(card, NULL, silent, -1);
    if (_area) Card_add_to_deck(card, false);
    if (edition) Card_set_edition(card, TAB(edition, B(true)), false, false);
    if (eternal) Card_set_eternal(card, true);
    if (_area && streq(tstr(card->ability, "set"), "Joker")) CardArea_emplace(_area, card, NULL, false);
    else if (G->consumeables) CardArea_emplace(G->consumeables, card, NULL, false);
    card->moveable.node.created_on_pause = false; // = nil (the member is a bool)
    return card;
}

struct draw_card_ctx {
    CardArea *from, *to;
    float percent, vol;
    bool sort, mute, stay_flipped, discarded_only;
    Card *card;
};

// common_events.lua:398
static bool ev_draw_card(void *ctx) {
    struct draw_card_ctx *c = ctx;
    bool drawn = false;
    if (c->card) {
        if (c->from) c->card = CardArea_remove_card(c->from, c->card, false);
        if (c->card) drawn = true;
        Blind *blind = tobj(G->GAME, "blind");
        bool stay_flipped = G->GAME && blind && Blind_stay_flipped(blind, c->to, c->card);
        Val flipped_cards = tget(ttab(G->GAME, "modifiers"), "flipped_cards");
        if (truthy(flipped_cards) && c->to == G->hand) {
            if (pseudorandom(N(pseudoseed("flipped_card", NULL)), NIL, NIL) < 1 / vnum(flipped_cards)) {
                stay_flipped = true;
            }
        }
        CardArea_emplace(c->to, c->card, NULL, stay_flipped);
    } else {
        if (CardArea_draw_card_from(c->to, c->from, c->stay_flipped, c->discarded_only)) drawn = true;
    }
    if (!c->mute && drawn) {
        if (c->from == G->deck || c->from == G->hand || c->from == G->play || c->from == G->jokers ||
            c->from == G->consumeables || c->from == G->discard) {
            G->VIBRATION = G->VIBRATION + 0.6f;
        }
        play_sound("card1", 0.85 + c->percent * 0.2 / 100, 0.6 * (c->vol < 0 ? 1 : c->vol));
    }
    if (c->sort) {
        CardArea_sort(c->to, NULL);
    }
    return true;
}

// common_events.lua:386
void draw_card(CardArea *from, CardArea *to, float percent, const char *dir, bool sort, Card *card, num delay, bool mute,
               bool stay_flipped, float vol, bool discarded_only) {
    percent = percent < 0 ? 50 : percent;
    delay = delay < 0 ? 0.1 : delay;
    if (streq(dir, "down")) {
        percent = 1 - percent;
    }
    struct draw_card_ctx *c = NEWCTX(struct draw_card_ctx);
    c->from = from, c->to = to, c->percent = percent, c->vol = vol;
    c->sort = sort, c->mute = mute, c->stay_flipped = stay_flipped, c->discarded_only = discarded_only;
    c->card = card;

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = delay, .func = ev_draw_card, .ctx = c}), NULL, false);
}

struct highlight_card_ctx { Card *card; float percent; bool highlight; };

// common_events.lua:437
static bool ev_highlight_card(void *ctx) {
    struct highlight_card_ctx *c = ctx;
    Card_highlight(c->card, c->highlight);
    play_sound("cardSlide1", 0.85 + c->percent * 0.2, -1);
    return true;
}

// common_events.lua:426
void highlight_card(Card *card, float percent, const char *dir) {
    percent = percent < 0 ? 0.5f : percent;
    bool highlight = true;
    if (streq(dir, "down")) {
        percent = 1 - percent;
        highlight = false;
    }

    struct highlight_card_ctx *c = NEWCTX(struct highlight_card_ctx);
    c->card = card, c->percent = percent, c->highlight = highlight;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.1, .func = ev_highlight_card, .ctx = c}), NULL, false);
}

struct status_text_ctx { const char *text; bool silent; };

// common_events.lua:450
static bool ev_play_area_status_text(void *ctx) {
    struct status_text_ctx *c = ctx;
    attention_text(TAB("scale", N(0.9), "text", S(c->text), "hold", N(0.9), "align", S("tm"), "major", O(G->play), "offset",
                       T(TAB("x", N(0), "y", N(-1)))));
    if (!c->silent) {
        tsetn(G->ROOM->object.dyn, "jiggle", tnum(G->ROOM->object.dyn, "jiggle") + 2);
        play_sound("cardFan2", -1, -1);
    }
    return true;
}

// common_events.lua:445
void play_area_status_text(const char *text, bool silent, num delay) {
    delay = delay < 0 ? 0.6 : delay;
    struct status_text_ctx *c = NEWCTX(struct status_text_ctx);
    c->text = text, c->silent = silent;
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = (delay == 0 ? "immediate" : "before"), .delay = delay, .func = ev_play_area_status_text, .ctx = c}),
          NULL, false);
}

struct level_up_ctx { void *card; const char *hand; };

// card:juice_up(0.8, 0.5): a Tag is not a Moveable (common_events.h)
static void level_up_juice(void *card) {
    if (!card) return;
    if (is_a(card, &Tag_class)) Tag_juice_up((Tag *)card, 0.8f, N(0.5));
    else CALL(card, juice_up, 0.8f, N(0.5));
}

// common_events.lua:470
static bool ev_level_up_1(void *ctx) {
    struct level_up_ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    level_up_juice(c->card);
    G->TAROT_INTERRUPT_PULSE = true;
    return true;
}

// common_events.lua:476
static bool ev_level_up_2(void *ctx) {
    struct level_up_ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    level_up_juice(c->card);
    return true;
}

// common_events.lua:481
static bool ev_level_up_3(void *ctx) {
    struct level_up_ctx *c = ctx;
    play_sound("tarot1", -1, -1);
    level_up_juice(c->card);
    G->TAROT_INTERRUPT_PULSE = false; // = nil
    return true;
}

// common_events.lua:491
static bool ev_level_up_unlock(void *ctx) {
    struct level_up_ctx *c = ctx;
    check_for_unlock(TAB("type", S("upgrade_hand"), "hand", S(c->hand), "level", tget(ttab(ttab(G->GAME, "hands"), c->hand), "level")));
    return true;
}

// common_events.lua:464
void level_up_hand(void *card, const char *hand, bool instant, Val amount) {
    num amt = truthy(amount) ? vnum(amount) : 1;
    Table *h = ttab(ttab(G->GAME, "hands"), hand);
    tsetn(h, "level", lmax(0, tnum(h, "level") + amt));
    tsetn(h, "mult", lmax(tnum(h, "s_mult") + tnum(h, "l_mult") * (tnum(h, "level") - 1), 1));
    tsetn(h, "chips", lmax(tnum(h, "s_chips") + tnum(h, "l_chips") * (tnum(h, "level") - 1), 0));
    struct level_up_ctx *c = NEWCTX(struct level_up_ctx);
    c->card = card, c->hand = str(hand);
    if (!instant) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_level_up_1, .ctx = c}), NULL, false);
        update_hand_text(TAB("delay", N(0)), TAB("mult", tget(h, "mult"), "StatusText", B(true)));
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.9, .func = ev_level_up_2, .ctx = c}), NULL, false);
        update_hand_text(TAB("delay", N(0)), TAB("chips", tget(h, "chips"), "StatusText", B(true)));
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.9, .func = ev_level_up_3, .ctx = c}), NULL, false);
        update_hand_text(TAB("sound", S("button"), "volume", N(0.7), "pitch", N(0.9), "delay", N(0)), TAB("level", tget(h, "level")));
        delay(1.3, NULL);
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_level_up_unlock, .ctx = c}), NULL, false);
}

struct hand_text_ctx { Table *config, *vals; };

// common_events.lua:572
static bool ev_hand_text_modded_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}

// common_events.lua:503-508, :525-530: the text of the change of a hand value; *col becomes RED for a loss
static const char *hand_text_delta(Val val, Val current, float **col) {
    num delta = (val.t == V_NUM && current.t == V_NUM) ? (val.n - current.n) : 0;
    const char *text;
    if (delta < 0) {
        text = number_tostring(delta);
        *col = G->C.RED;
    } else if (delta > 0) {
        text = fmt("+%s", number_tostring(delta));
    } else {
        text = number_tostring(delta);
    }
    if (val.t == V_STR) text = val.s;
    return text;
}

// common_events.lua:500
static bool ev_update_hand_text(void *ctx) {
    struct hand_text_ctx *c = ctx;
    Table *config = c->config, *vals = c->vals;
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    float *col = G->C.GREEN;
    Val chips = tget(vals, "chips");
    if (truthy(chips) && !veq(tget(current_hand, "chips"), chips)) {
        const char *delta = hand_text_delta(chips, tget(current_hand, "chips"), &col);
        tset(current_hand, "chips", chips);
        UIElement *chips_UI = tobj(G->hand_text_area, "chips");
        UIElement_update(chips_UI, 0);
        if (tbool(vals, "StatusText")) {
            attention_text(TAB("text", S(delta), "scale", N(0.8), "hold", N(1), "cover", O(chips_UI->moveable.node.parent),
                               "cover_colour", P(mix_colours(G->C.CHIPS, col, 0.1f, NULL)), "emboss", N(0.05), "align", S("cm"),
                               "cover_align", S("cr")));
        }
    }
    Val mult = tget(vals, "mult");
    if (truthy(mult) && !veq(tget(current_hand, "mult"), mult)) {
        const char *delta = hand_text_delta(mult, tget(current_hand, "mult"), &col);
        tset(current_hand, "mult", mult);
        UIElement *mult_UI = tobj(G->hand_text_area, "mult");
        UIElement_update(mult_UI, 0);
        if (tbool(vals, "StatusText")) {
            attention_text(TAB("text", S(delta), "scale", N(0.8), "hold", N(1), "cover", O(mult_UI->moveable.node.parent),
                               "cover_colour", P(mix_colours(G->C.MULT, col, 0.1f, NULL)), "emboss", N(0.05), "align", S("cm"),
                               "cover_align", S("cl")));
        }
        if (!G->TAROT_INTERRUPT) UIElement_juice_up(mult_UI, -1, NIL);
    }
    Val handname = tget(vals, "handname");
    if (truthy(handname) && !veq(tget(current_hand, "handname"), handname)) {
        tset(current_hand, "handname", handname);
        if (!tbool(config, "nopulse")) {
            UIElement *handname_UI = tobj(G->hand_text_area, "handname");
            DynaText_pulse(tobj(handname_UI->moveable.node.config, "object"), 0.2f);
        }
    }
    Val chip_total = tget(vals, "chip_total");
    if (truthy(chip_total)) {
        tset(current_hand, "chip_total", chip_total);
        UIElement *chip_total_UI = tobj(G->hand_text_area, "chip_total");
        DynaText_pulse(tobj(chip_total_UI->moveable.node.config, "object"), 0.5f);
    }
    Val level = tget(vals, "level");
    if (truthy(level)) {
        const char *level_text = fmt(" %s%s", vstr(localize(S("k_lvl"), NULL)), tostring(level));
        if (!veq(tget(current_hand, "hand_level"), S(level_text))) {
            if (level.t == V_STR && streq(level.s, "")) {
                tset(current_hand, "hand_level", level);
            } else {
                tsets(current_hand, "hand_level", level_text);
                UIElement *hand_level_UI = tobj(G->hand_text_area, "hand_level");
                if (level.t == V_NUM) {
                    tset(hand_level_UI->moveable.node.config, "colour", P(G->C.HAND_LEVELS[(int)lmin(level.n, 7)]));
                } else {
                    tset(hand_level_UI->moveable.node.config, "colour", P(G->C.HAND_LEVELS[1]));
                }
                UIElement_juice_up(hand_level_UI, -1, NIL);
            }
        }
    }
    if (truthy(tget(config, "sound")) && !tbool(config, "modded")) {
        Val pitch = tget(config, "pitch"), volume = tget(config, "volume");
        play_sound(tstr(config, "sound"), truthy(pitch) ? (float)vnum(pitch) : 1, truthy(volume) ? (float)vnum(volume) : 1);
    }
    if (tbool(config, "modded")) {
        UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_1"), NULL), 0.3f, N(0));
        UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_debuff_2"), NULL), 0.3f, N(0));
        Blind_juice_up(tobj(G->GAME, "blind"), -1, NIL);
        E_add(G->E_MANAGER,
              Event_new((EventCfg){.trigger = "after", .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"), .not_blockable = true,
                                   .not_blocking = true, .func = ev_hand_text_modded_sound}),
              NULL, false);
        play_sound("tarot2", 1, 0.4f);
    }
    return true;
}

// common_events.lua:495
void update_hand_text(Table *config, Table *vals) {
    struct hand_text_ctx *c = NEWCTX(struct hand_text_ctx);
    c->config = config, c->vals = vals;
    Val delay = tget(config, "delay");
    // This is the Hand name text for the poker hand
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "before", .not_blockable = tbool(config, "immediate"),
                               .delay = truthy(delay) ? vnum(delay) : 0.8, .func = ev_update_hand_text, .ctx = c}),
          NULL, false);
}
