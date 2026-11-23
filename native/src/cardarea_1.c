// cardarea.lua:1-631: the CardArea class and every method but load and remove (cardarea_2.c).
#include "all.h"

#define CONFIG(a) ((a)->moveable.node.config)
#define CHILDREN(a) ((a)->moveable.node.children)
#define STATES(a) ((a)->moveable.node.states)
#define TR(a) ((a)->moveable.node.T)

// self.config.type == name
static bool type_is(CardArea *self, const char *name) { return streq(CardArea_type(self), name); }
// cardarea.h CardArea_type: reads config.type again
void CardArea_type_fill(CardArea *self) {
    self->type_snap.type = tstr(CONFIG(self), "type");
    self->type_snap.config = CONFIG(self), self->type_snap.stamp = tstamp(CONFIG(self));
}

// cardarea.lua:2 CardArea = Moveable:extend()
static void cls_update(void *self, float dt) { CardArea_update(self, dt); }
static void cls_move(void *self, float dt) { CardArea_move(self, dt); }
static void cls_draw(void *self) { CardArea_draw(self); }
static void cls_remove(void *self) { CardArea_remove(self); }
static void cls_click(void *self) { CardArea_click(self); }
static void cls_hard_set_T(void *self, float X, float Y, float W, float H) { CardArea_hard_set_T(self, X, Y, W, H); }
// note: the class object is defined here (the Lua's line 2); CardArea_remove is in part 2.
Class CardArea_class = {
    .name = "CardArea",
    .super = &Moveable_class,
    .update = cls_update,
    .move = cls_move,
    .draw = cls_draw,
    .remove = cls_remove,
    .click = cls_click,
    .hard_set_T = cls_hard_set_T,
};

// cardarea.lua:5
void CardArea_init(CardArea *self, float X, float Y, float W, float H, Table *config) {
    Moveable_init(&self->moveable, X, Y, W, H);

    STATES(self).drag.can = false;
    STATES(self).hover.can = false;
    STATES(self).click.can = false;

    // :13 note: `config or {}` cannot help: the next line indexes config itself (cardarea.h: never NULL)
    CONFIG(self) = config;
    Val v = tget(config, "card_w");
    self->card_w = truthy(v) ? (float)vnum(v) : G->CARD_W;
    self->cards = tnew();
    CHILDREN(self) = tnew();
    self->highlighted = tnew();
    v = tget(config, "highlight_limit");
    tset(config, "highlighted_limit", truthy(v) ? v : N(5));
    v = tget(config, "card_limit");
    tset(config, "card_limit", truthy(v) ? v : N(52));
    tset(config, "temp_limit", tget(config, "card_limit"));
    tsetn(config, "card_count", 0);
    v = tget(config, "type");
    tset(config, "type", truthy(v) ? v : S("deck"));
    v = tget(config, "sort");
    tset(config, "sort", truthy(v) ? v : S("desc"));
    v = tget(config, "lr_padding");
    tset(config, "lr_padding", truthy(v) ? v : N(0.1));
    self->shuffle_amt = 0;

    if (self->moveable.node.object.cls == &CardArea_class) { // :27
        tinsert(G->I.CARDAREA, O(self));
    }
}

// cardarea.lua:32
void CardArea_emplace(CardArea *self, Card *card, const char *location, bool stay_flipped) {
    if (streq(location, "front") || type_is(self, "deck")) {
        tinsert_at(self->cards, 1, O(card));
    } else {
        tinsert(self->cards, O(card)); // :36
    }
    if (streq(card->facing, "back") && !type_is(self, "discard") && !type_is(self, "deck") && !stay_flipped) {
        Card_flip(card);
    }
    if (self == G->hand && stay_flipped) {
        tsetb(card->ability, "wheel_flipped", true);
    }

    if (tlen(self->cards) > tnum(CONFIG(self), "card_limit")) { // :45
        if (self == G->deck) {
            tsetn(CONFIG(self), "card_limit", tlen(self->cards));
        }
    }

    Card_set_card_area(card, self);
    CardArea_set_ranks(self);
    CardArea_align_cards(self);

    if (self == G->jokers) { // :55
        num joker_tally = 0;
        for (int i = 1; i <= tlen(G->jokers->cards); i++) {
            if (streq(tstr(CardArea_card(G->jokers, i)->ability, "set"), "Joker")) joker_tally = joker_tally + 1;
        }
        if (joker_tally > tnum(G->GAME, "max_jokers")) tsetn(G->GAME, "max_jokers", joker_tally);
        check_for_unlock(TAB("type", S("modify_jokers")));
    }
    if (self == G->deck) check_for_unlock(TAB("type", S("modify_deck"), "deck", O(self))); // :63
}

// cardarea.lua:66
Card *CardArea_remove_card(CardArea *self, Card *card, bool discarded_only) {
    if (!self->cards) return NULL;
    Table *_cards = discarded_only ? tnew() : self->cards;
    if (discarded_only) {
        for (int k = 1;; k++) { // ipairs
            Card *v = CardArea_card(self, k);
            if (!v) break;
            if (v->ability && tbool(v->ability, "discarded")) {
                tinsert(_cards, O(v));
            }
        }
    }
    if (type_is(self, "discard") || type_is(self, "deck")) { // :76
        if (!card) card = tobji(_cards, tlen(_cards));
    } else {
        if (!card) card = tobji(_cards, 1);
    }
    for (int i = tlen(self->cards); i >= 1; i--) { // :81
        if (CardArea_card(self, i) == card) {
            Card_remove_from_area(card);
            tremove(self->cards, i);
            CardArea_remove_from_highlighted(self, card, true);
            break;
        }
    }
    CardArea_set_ranks(self);
    if (self == G->deck) check_for_unlock(TAB("type", S("modify_deck"), "deck", O(self)));
    return card;
}

struct change_size_ctx { CardArea *self; num delta; };
// cardarea.lua:104
static bool ev_change_size_sort(void *ctx) {
    CardArea *self = ctx;
    CardArea_sort(self, NULL);
    return true;
}
// cardarea.lua:97
static bool ev_change_size(void *ctx) {
    struct change_size_ctx *c = ctx;
    CardArea *self = c->self;
    num delta = c->delta;
    Val rcl = tget(CONFIG(self), "real_card_limit");
    tsetn(CONFIG(self), "real_card_limit", (truthy(rcl) ? vnum(rcl) : tnum(CONFIG(self), "card_limit")) + delta);
    tsetn(CONFIG(self), "card_limit", lmax(0, tnum(CONFIG(self), "real_card_limit")));
    if (delta > 0 && tnum(CONFIG(self), "real_card_limit") > 1 && self == G->hand && CardArea_card(self, 1) &&
        (G->STATE == STATES_DRAW_TO_HAND || G->STATE == STATES_SELECTING_HAND)) { // :100
        num card_count = fabs(delta);
        for (num i = 1; i <= card_count; i++) {
            draw_card(G->deck, G->hand, (float)(i * 100 / card_count), NULL, false, NULL, 0.07, false, false, -1, false);
            E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_change_size_sort, .ctx = self}), NULL, false);
        }
    }
    if (self == G->hand) check_for_unlock(TAB("type", S("min_hand_size"))); // :107
    return true;
}

// cardarea.lua:94
void CardArea_change_size(CardArea *self, num delta) {
    if (delta != 0) {
        struct change_size_ctx *c = NEWCTX(struct change_size_ctx);
        c->self = self, c->delta = delta;
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_change_size, .ctx = c}), NULL, false);
    }
}

// cardarea.lua:113
bool CardArea_can_highlight(CardArea *self, Card *card) {
    if (G->CONTROLLER->HID.controller) {
        if (type_is(self, "hand")) {
            return true;
        }
    } else {
        if (type_is(self, "hand") || type_is(self, "joker") || type_is(self, "consumeable") ||
            (type_is(self, "shop") && tnum(CONFIG(self), "highlighted_limit") > 0)) {
            return true;
        }
    }
    return false;
}

// cardarea.lua:131
void CardArea_add_to_highlighted(CardArea *self, Card *card, bool silent) {
    if (type_is(self, "shop")) {
        if (CardArea_highlighted(self, 1)) {
            CardArea_remove_from_highlighted(self, CardArea_highlighted(self, 1), false);
        }
        tinsert(self->highlighted, O(card)); // :138
        Card_highlight(card, true);
        if (!silent) play_sound("cardSlide1", -1, -1);
    } else if (type_is(self, "joker") || type_is(self, "consumeable")) {
        if (tlen(self->highlighted) >= tnum(CONFIG(self), "highlighted_limit")) {
            CardArea_remove_from_highlighted(self, CardArea_highlighted(self, 1), false);
        }
        tinsert(self->highlighted, O(card)); // :145
        Card_highlight(card, true);
        if (!silent) play_sound("cardSlide1", -1, -1);
    } else {
        if (tlen(self->highlighted) >= tnum(CONFIG(self), "highlighted_limit")) { // :149
            Card_highlight(card, false);
        } else {
            tinsert(self->highlighted, O(card));
            Card_highlight(card, true);
            if (!silent) play_sound("cardSlide1", -1, -1);
        }
        if (self == G->hand && G->STATE == STATES_SELECTING_HAND) {
            CardArea_parse_highlighted(self);
        }
    }
}

// cardarea.lua:162
void CardArea_parse_highlighted(CardArea *self) {
    G->boss_throw_hand = false;
    // :164 the Lua's second return value is the localized name
    const char *disp_text = NULL;
    Table *poker_hands = NULL;
    const char *text = FUNCS_get_poker_hand_info(self->highlighted, &disp_text, &poker_hands, NULL, NULL);
    if (streq(text, "NULL")) {
        update_hand_text(TAB("immediate", B(true), "nopulse", B(true), "delay", N(0)),
                         TAB("mult", N(0), "chips", N(0), "level", S(""), "handname", S("")));
    } else {
        Blind *blind = tobj(G->GAME, "blind");
        if (blind && Blind_debuff_hand(blind, self->highlighted, poker_hands, text, true)) { // :168
            G->boss_throw_hand = true;
        } else {
        }
        bool backwards = false;
        for (TIter it = titer(self->highlighted); tnext(&it);) { // :174 pairs
            Card *v = it.val.obj;
            if (streq(v->facing, "back")) {
                backwards = true;
            }
        }
        if (backwards) {
            update_hand_text(TAB("immediate", B(true), "delay", N(0)),
                             TAB("handname", S("????"), "level", S("?"), "mult", S("?"), "chips", S("?")));
        } else {
            Table *hand = ttab(ttab(G->GAME, "hands"), text);
            update_hand_text(TAB("immediate", B(true), "delay", N(0)),
                             TAB("handname", S(disp_text), "level", tget(hand, "level"), "mult", tget(hand, "mult"), "chips",
                                 tget(hand, "chips")));
        }
    }
}

// cardarea.lua:187
void CardArea_remove_from_highlighted(CardArea *self, Card *card, bool force) {
    if ((!force) && card && tbool(card->ability, "forced_selection") && self == G->hand) return;
    for (int i = tlen(self->highlighted); i >= 1; i--) {
        if (CardArea_highlighted(self, i) == card) {
            tremove(self->highlighted, i);
            break;
        }
    }
    if (!card) ERROR("cardarea.lua:195: attempt to index local 'card' (a nil value)");
    Card_highlight(card, false);
    if (self == G->hand && G->STATE == STATES_SELECTING_HAND) {
        CardArea_parse_highlighted(self);
    }
}

// cardarea.lua:201
void CardArea_unhighlight_all(CardArea *self) {
    for (int i = tlen(self->highlighted); i >= 1; i--) {
        if (tbool(CardArea_highlighted(self, i)->ability, "forced_selection") && self == G->hand) {
        } else {
            Card_highlight(CardArea_highlighted(self, i), false);
            tremove(self->highlighted, i);
        }
    }
    if (self == G->hand && G->STATE == STATES_SELECTING_HAND) {
        CardArea_parse_highlighted(self);
    }
}

// cardarea.lua:214
void CardArea_set_ranks(CardArea *self) {
    for (int k = 1;; k++) { // ipairs
        Card *card = CardArea_card(self, k);
        if (!card) break;
        card->rank = k;
        STATES(card).collide.can = true;
        if (k > 1 && type_is(self, "deck")) {
            STATES(card).drag.can = false;
            STATES(card).collide.can = false;
        } else if (type_is(self, "play") || type_is(self, "shop") || type_is(self, "consumeable")) {
            STATES(card).drag.can = false;
        } else {
            STATES(card).drag.can = true;
        }
    }
}

// cardarea.lua:229
void CardArea_move(CardArea *self, float dt) {
    // Set sliding up/down for the hand area
    if (self == G->hand) {
        float desired_y = G->TILE_H - TR(G->hand)->h -
                          1.9f * ((!G->deck_preview && (G->STATE == STATES_SELECTING_HAND || G->STATE == STATES_DRAW_TO_HAND)) ? 1 : 0);
        float real_dt = (float)G->real_dt;
        TR(G->hand)->y = 15 * real_dt * desired_y + (1 - 15 * real_dt) * TR(G->hand)->y;
        if (fabsf(desired_y - TR(G->hand)->y) < 0.01f) TR(G->hand)->y = desired_y;
        if (G->STATE == STATES_TUTORIAL) {
            TR(G->play)->y = TR(G->hand)->y - (3 + 0.6f);
        }
    }
    Moveable_move(&self->moveable, dt);
    CardArea_align_cards(self);
}

// cardarea.lua:243
void CardArea_update(CardArea *self, float dt) {
    if (self == G->hand) {
        Val X = tget(ttab(G->GAME, "modifiers"), "minus_hand_size_per_X_dollar");
        if (truthy(X)) {
            if (!truthy(tget(CONFIG(self), "last_poll_size"))) tsetn(CONFIG(self), "last_poll_size", 0); // :246
            if (floor(tnum(G->GAME, "dollars") / vnum(X)) != tnum(CONFIG(self), "last_poll_size")) {
                CardArea_change_size(self, tnum(CONFIG(self), "last_poll_size") - floor(tnum(G->GAME, "dollars") / vnum(X)));
                tsetn(CONFIG(self), "last_poll_size", floor(tnum(G->GAME, "dollars") / vnum(X)));
            }
        }
        for (TIter it = titer(self->cards); tnext(&it);) { // :252 pairs
            Card *v = it.val.obj;
            if (tbool(v->ability, "forced_selection") && !CardArea_highlighted(self, 1)) {
                CardArea_add_to_highlighted(self, v, false);
            }
        }
    }
    if (self == G->deck) { // :258
        STATES(self).collide.can = true;
        STATES(self).hover.can = true;
        STATES(self).click.can = true;
    }
    // Check and see if controller is being used
    if (G->CONTROLLER->HID.controller && self != G->hand) CardArea_unhighlight_all(self);
    if (self == G->deck && tnum(CONFIG(self), "card_limit") > tlen(G->playing_cards)) tsetn(CONFIG(self), "card_limit", tlen(G->playing_cards));
    tsetn(CONFIG(self), "temp_limit", lmax(tlen(self->cards), tnum(CONFIG(self), "card_limit")));
    tsetn(CONFIG(self), "card_count", tlen(self->cards));
}

// a {n=G.UIT.R, config={align = "cm" (+ maxw)}, nodes={{n=G.UIT.T, config={text, scale, colour = G.C.WHITE (+ shadow)}}}} row
static Val text_row(Val text, num scale, bool shadow, bool maxw) {
    Table *tc = TAB("text", text, "scale", N(scale), "colour", P(G->C.WHITE));
    if (shadow) tsetb(tc, "shadow", true);
    Table *rc = TAB("align", S("cm"));
    if (maxw) tsetn(rc, "maxw", 2);
    return T(TAB("n", N(UIT_R), "config", T(rc), "nodes", T(ARR(T(TAB("n", N(UIT_T), "config", T(tc)))))));
}

// cardarea.lua:270
void CardArea_draw(CardArea *self) {
    if (!STATES(self).visible) return;
    if (G->VIEWING_DECK && (self == G->deck || self == G->hand || self == G->play)) return;

    int state = G->TAROT_INTERRUPT ? G->TAROT_INTERRUPT : G->STATE;
    const char *ctype = CardArea_type(self); // read once: only CardArea_init writes config.type (cardarea.lua:22)

    // :276 self.ARGS.invisible_area_types: a constant test (cardarea.h)
    bool invisible = streq(ctype, "discard") || streq(ctype, "voucher") || streq(ctype, "play") || streq(ctype, "consumeable") ||
                     streq(ctype, "title") || streq(ctype, "title_2");
    bool hand_state = state == STATES_SHOP || state == STATES_TAROT_PACK || state == STATES_SPECTRAL_PACK || state == STATES_STANDARD_PACK ||
                      state == STATES_BUFFOON_PACK || state == STATES_PLANET_PACK || state == STATES_ROUND_EVAL || state == STATES_BLIND_SELECT;
    if (invisible || (streq(ctype, "hand") && hand_state) || (streq(ctype, "deck") && self != G->deck) ||
        (streq(ctype, "shop") && self != G->shop_vouchers)) {
    } else {
        if (!CardArea_child(self, "area_uibox")) { // :282
            Table *card_count = NULL;
            if (self != G->shop_vouchers) {
                card_count = TAB(
                    "n", N(UIT_R), "config",
                    T(TAB("align", S(self == G->jokers ? "cl" : self == G->hand ? "cm" : "cr"), "padding", N(0.03), "no_fill", B(true))),
                    "nodes",
                    T(ARR(T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.1))))),
                          T(TAB("n", N(UIT_T), "config",
                                T(TAB("ref_table", T(CONFIG(self)), "ref_value", S("card_count"), "scale", N(0.3), "colour", P(G->C.WHITE))))),
                          T(TAB("n", N(UIT_T), "config", T(TAB("text", S("/"), "scale", N(0.3), "colour", P(G->C.WHITE))))),
                          T(TAB("n", N(UIT_T), "config",
                                T(TAB("ref_table", T(CONFIG(self)), "ref_value", S("card_limit"), "scale", N(0.3), "colour", P(G->C.WHITE))))),
                          T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.1))))))));
            }

            // :294
            Table *row_config = TAB("minw", N(TR(self)->w), "minh", N(TR(self)->h), "align", S("cm"), "padding", N(0.1), "mid", B(true), "r",
                                    N(0.1), "ref_table", O(self));
            if (self != G->shop_vouchers) {
                // note: the Lua's {0,0,0,0.1} literal, a new colour per box (never freed, as the port's other colours)
                float *col = calloc(4, sizeof(float));
                col[3] = 0.1f;
                tset(row_config, "colour", P(col));
            }
            Table *row_nodes = tnew();
            if (self == G->shop_vouchers) { // :295
                tinsert(row_nodes,
                        T(TAB("n", N(UIT_C), "config",
                              T(TAB("align", S("cm"), "paddin", N(0.1), "func", S("shop_voucher_empty"), "visible", B(false))), "nodes",
                              // note: the three texts are literals of the Lua: LITERALS.cardarea of data/game.dat (data.h), whose 1 is
                              // the "/" of the count above, 2-4 these and 5-6 the two of the deck's button below
                              T(ARR(text_row(S(lit("cardarea", 2)), 0.6, false, false), text_row(S(lit("cardarea", 3)), 0.4, false, false),
                                    text_row(S(lit("cardarea", 4)), 0.4, false, false))))));
            }
            Table *root_nodes = ARR(T(TAB("n", N(UIT_R), "config", T(row_config), "nodes", T(row_nodes))));
            if (card_count) tinsert(root_nodes, T(card_count)); // :308

            tseto(CHILDREN(self), "area_uibox",
                  UIBox_new(TAB("definition",
                                T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(root_nodes))),
                                "config",
                                T(TAB("align", S("cm"), "offset", T(TAB("x", N(0), "y", N(0))), "major", O(self), "parent", O(self))))));
        }
        UIBox_draw(CardArea_child(self, "area_uibox")); // :313
    }

    Node_draw_boundingrect((Node *)self);
    add_to_drawhash(self);

    // :319
    if (!self->draw_layers) {
        self->draw_layers = ttab(CONFIG(self), "draw_layers");
        if (!self->draw_layers) self->draw_layers = ARR(S("shadow"), S("card"));
    }
    Node *focused = G->CONTROLLER->focused.target;
    int n = tlen(self->cards);
    for (int k = 1;; k++) { // ipairs
        const char *v = tstri(self->draw_layers, k);
        if (!v) break;
        if (streq(ctype, "deck")) { // :321
            Val td = tget(CONFIG(self), "thin_draw");
            num thin_draw = truthy(td) ? vnum(td) : 9;
            for (int i = n; i >= 1; i--) {
                Card *c = CardArea_card(self, i);
                if ((Node *)c != G->CONTROLLER->focused.target) {
                    if (i == 1 || lmod(i, thin_draw) == 0 || i == n || fabsf(c->moveable.VT.x - TR(self)->x) > 1 ||
                        fabsf(c->moveable.VT.y - TR(self)->y) > 1) {
                        if (G->CONTROLLER->dragging.target != (Node *)c) Card_draw(c, v);
                    }
                }
            }
        }

        if (streq(ctype, "joker") || streq(ctype, "consumeable") || streq(ctype, "shop") || streq(ctype, "title_2")) { // :331
            for (int i = 1; i <= n; i++) {
                Card *c = CardArea_card(self, i);
                if ((Node *)c != G->CONTROLLER->focused.target) {
                    if (!c->highlighted) {
                        if (G->CONTROLLER->dragging.target != (Node *)c) Card_draw(c, v);
                    }
                }
            }
            for (int i = 1; i <= n; i++) {
                Card *c = CardArea_card(self, i);
                if ((Node *)c != G->CONTROLLER->focused.target) {
                    if (c->highlighted) {
                        if (G->CONTROLLER->dragging.target != (Node *)c) Card_draw(c, v);
                    }
                }
            }
        }

        if (streq(ctype, "discard")) { // :348
            for (int i = 1; i <= n; i++) {
                Card *c = CardArea_card(self, i);
                if ((Node *)c != G->CONTROLLER->focused.target) {
                    if (fabsf(c->moveable.VT.x - TR(self)->x) > 1) {
                        if (G->CONTROLLER->dragging.target != (Node *)c) Card_draw(c, v);
                    }
                }
            }
        }

        if (streq(ctype, "hand") || streq(ctype, "play") || streq(ctype, "title") || streq(ctype, "voucher")) { // :358
            for (int i = 1; i <= n; i++) {
                Card *c = CardArea_card(self, i);
                if ((Node *)c != G->CONTROLLER->focused.target || self == G->hand) {
                    if (G->CONTROLLER->dragging.target != (Node *)c) Card_draw(c, v);
                }
            }
        }
    }

    if (self == G->deck) { // :367
        if (G->CONTROLLER->HID.controller && G->STATE == STATES_SELECTING_HAND && !CardArea_child(self, "peek_deck")) {
            UIBox *peek_deck = UIBox_new(TAB(
                "definition",
                T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes",
                      T(ARR(T(TAB("n", N(UIT_R), "config",
                                  T(TAB("align", S("cm"), "r", N(0.1), "colour", P(adjust_alpha(G->C.L_BLACK, 0.5f, NULL)), "func",
                                        S("set_button_pip"), "focus_args",
                                        T(TAB("button", S("triggerleft"), "orientation", S("bm"), "scale", N(0.6), "type", S("none"))))),
                                  "nodes", T(ARR(text_row(S(lit("cardarea", 5)), 0.48, true, false), text_row(S(lit("cardarea", 6)), 0.38, true, false))))))))),
                "config", T(TAB("align", S("cl"), "offset", T(TAB("x", N(-0.5), "y", N(0.1))), "major", O(self), "parent", O(self)))));
            tseto(CHILDREN(self), "peek_deck", peek_deck);
            STATES(peek_deck).collide.can = false; // :383
        } else if ((!G->CONTROLLER->HID.controller || G->STATE != STATES_SELECTING_HAND) && CardArea_child(self, "peek_deck")) {
            UIBox_remove(CardArea_child(self, "peek_deck"));
            tset(CHILDREN(self), "peek_deck", NIL);
        }
        if (!CardArea_child(self, "view_deck")) { // :388
            void *major = CardArea_card(self, 1) ? (void *)CardArea_card(self, 1) : (void *)self;
            UIBox *view_deck = UIBox_new(TAB(
                "definition",
                T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes",
                      T(ARR(T(TAB("n", N(UIT_R), "config",
                                  T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", P(adjust_alpha(G->C.BLACK, 0.5f, NULL)),
                                        "func", S("set_button_pip"), "focus_args",
                                        T(TAB("button", S("triggerright"), "orientation", S("bm"), "scale", N(0.6))), "button",
                                        S("deck_info"))),
                                  "nodes",
                                  T(ARR(text_row(localize(S("k_view"), NULL), 0.48, true, true),
                                        text_row(localize(S("k_deck"), NULL), 0.38, true, true))))))))),
                "config", T(TAB("align", S("cm"), "offset", T(TAB("x", N(0), "y", N(0))), "major", O(major), "parent", O(self)))));
            tseto(CHILDREN(self), "view_deck", view_deck);
            STATES(view_deck).collide.can = false; // :403
        }
        if (G->deck_preview || STATES(self).collide.is || (G->buttons && STATES(G->buttons).collide.is && G->CONTROLLER->HID.controller))
            UIBox_draw(CardArea_child(self, "view_deck"));
        if (CardArea_child(self, "peek_deck")) UIBox_draw(CardArea_child(self, "peek_deck"));
    }
}

// table.sort(self.cards, function (a, b) return a.T.x + a.T.w/2 < b.T.x + b.T.w/2 end)
static bool less_center_x(Val a, Val b, void *ctx) {
    Card *ca = a.obj, *cb = b.obj;
    return TR(ca)->x + TR(ca)->w / 2 < TR(cb)->x + TR(cb)->w / 2;
}
// cardarea.lua:493
static bool less_ability_order(Val a, Val b, void *ctx) {
    return tnum(((Card *)a.obj)->ability, "order") < tnum(((Card *)b.obj)->ability, "order");
}
// cardarea.lua:528 note: in double, so that 100*sort_id does not eat the position's precision
static bool less_center_x_pinned(Val a, Val b, void *ctx) {
    Card *ca = a.obj, *cb = b.obj;
    return (num)TR(ca)->x + (num)TR(ca)->w / 2 - 100 * (ca->pinned ? ca->sort_id : 0) <
           (num)TR(cb)->x + (num)TR(cb)->w / 2 - 100 * (cb->pinned ? cb->sort_id : 0);
}

// The x every row layout shares (:441, :456, :471, :486, :500, :514), with the area's width W:
// self.T.x + (W-self.card_w)*((k-1)/math.max(max_cards-1, 1) - 0.5*(#self.cards-max_cards)/math.max(max_cards-1, 1)) + 0.5*(self.card_w - card.T.w)
static float row_x(CardArea *self, Card *card, int k, float W) {
    float n = tlen(self->cards);
    float max_cards = lmax(n, tnum(CONFIG(self), "temp_limit"));
    float d = lmax(max_cards - 1, 1);
    return TR(self)->x + (W - self->card_w) * ((k - 1) / d - 0.5f * (n - max_cards) / d) + 0.5f * (self->card_w - TR(card)->w);
}

// cardarea.lua:410
void CardArea_align_cards(CardArea *self) {
    if (self == G->hand || self == G->deck || self == G->discard || self == G->play) {
        // :411 G.view_deck and G.view_deck[1] and G.view_deck[1].cards (G.view_deck is never assigned by the game)
        Val v1 = G->view_deck ? tgeti(G->view_deck, 1) : NIL;
        if (v1.t == V_OBJ && is_a(v1.obj, &CardArea_class) && ((CardArea *)v1.obj)->cards) return;
        if (v1.t == V_TAB && truthy(tget(v1.tab, "cards"))) return;
    }
    float n = tlen(self->cards); // #self.cards: float, the layout divides by it
    float rm = SETTINGS_snap()->reduced_motion ? 0 : 1;
    float REAL = (float)G->TIMERS.REAL;
    Transform *sT = TR(self);
    const char *ctype = CardArea_type(self); // read once: only CardArea_init writes config.type (cardarea.lua:22)
    if (streq(ctype, "deck")) { // :412
        Val dh = tget(CONFIG(self), "deck_height");
        float deck_height = (float)((truthy(dh) ? vnum(dh) : 0.15) / 52);
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (streq(card->facing, "front")) Card_flip(card);

            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->x = sT->x + 0.5f * (sT->w - cT->w) + self->moveable.shadow_parrallax.x * deck_height * (n / (self == G->deck ? 1 : 2) - k) +
                        0.9f * self->shuffle_amt * (1 - k * 0.01f) * (k % 2 == 1 ? 1 : -0);
                cT->y = sT->y + 0.5f * (sT->h - cT->h) + self->moveable.shadow_parrallax.y * deck_height * (n / (self == G->deck ? 1 : 2) - k);
                cT->r = 0 + 0.3f * self->shuffle_amt * (1 + k * 0.05f) * (k % 2 == 1 ? 1 : -0);
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
    }
    if (streq(ctype, "discard")) { // :425
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (streq(card->facing, "front")) Card_flip(card);

            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->x = sT->x + (sT->w - cT->w) * card->discard_pos.x;
                cT->y = sT->y + (sT->h - cT->h) * card->discard_pos.y;
                cT->r = card->discard_pos.r;
            }
        }
    }
    bool pack_state = G->STATE == STATES_TAROT_PACK || G->STATE == STATES_SPECTRAL_PACK || G->STATE == STATES_PLANET_PACK;
    if (streq(ctype, "hand") && pack_state) { // :436
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0.4f * (-n / 2 - 0.5f + k) / n + rm * 0.02f * sinf(wrap_angle(2 * REAL + cT->x));
                cT->x = row_x(self, card, k, sT->w);
                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                float a = fabsf(1.3f * (-n / 2 + k - 0.5f) / n);
                cT->y = TR(G->hand)->y - 1.8f * cT->h - highlight_height + rm * 0.1f * sinf(wrap_angle(0.666f * REAL + cT->x)) + a * a - 0.3f;
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x, NULL);
    }
    if (streq(ctype, "hand") && !pack_state) { // :450
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0.2f * (-n / 2 - 0.5f + k) / n + rm * 0.02f * sinf(wrap_angle(2 * REAL + cT->x));
                cT->x = row_x(self, card, k, sT->w);

                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height + rm * 0.03f * sinf(wrap_angle(0.666f * REAL + cT->x)) +
                        fabsf(0.5f * (-n / 2 + k - 0.5f) / n) - 0.2f;
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x, NULL);
    }
    if (streq(ctype, "title") || (streq(ctype, "voucher") && tlen(self->cards) == 1)) { // :466
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0.2f * (-n / 2 - 0.5f + k) / n + rm * 0.02f * sinf(wrap_angle(2 * REAL + cT->x));
                cT->x = row_x(self, card, k, sT->w);
                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height + rm * 0.03f * sinf(wrap_angle(0.666f * REAL + cT->x)) +
                        fabsf(0.5f * (-n / 2 + k - 0.5f) / n) - (n > 1 ? 0.2f : 0);
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x, NULL);
    }
    if (streq(ctype, "voucher") && tlen(self->cards) > 1) { // :480
        float self_w = lmax(sT->w, 3.2);
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0.2f * (-n / 2 - 0.5f + k) / n + rm * 0.02f * sinf(wrap_angle(2 * REAL + cT->x + cT->y)) + (k % 2 == 0 ? 1 : -1) * 0.08f;
                cT->x = row_x(self, card, k, self_w) + (k % 2 == 1 ? 1 : -1) * 0.27f + (sT->w - self_w) / 2;
                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height + rm * 0.03f * sinf(wrap_angle(0.666f * REAL + cT->x)) +
                        fabsf(0.5f * (-n / 2 + k - 0.5f) / n) - (n > 1 ? 0.2f : 0);
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_ability_order, NULL);
    }
    if (streq(ctype, "play") || streq(ctype, "shop")) { // :495
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0;
                cT->x = row_x(self, card, k, sT->w) + (tnum(CONFIG(self), "card_limit") == 1 ? 0.5f * (sT->w - cT->w) : 0);
                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height;
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x, NULL);
    }
    if (streq(ctype, "joker") || streq(ctype, "title_2")) { // :509
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                cT->r = 0.1f * (-n / 2 - 0.5f + k) / n + rm * 0.02f * sinf(wrap_angle(2 * REAL + cT->x));
                cT->x = row_x(self, card, k, sT->w);
                if (n > 2 || (n > 1 && self == G->consumeables) || (n > 1 && tbool(CONFIG(self), "spread"))) {
                    cT->x = sT->x + (sT->w - self->card_w) * ((k - 1) / (n - 1)) + 0.5f * (self->card_w - cT->w);
                } else if (n > 1 && self != G->consumeables) {
                    cT->x = sT->x + (sT->w - self->card_w) * ((k - 0.5f) / n) + 0.5f * (self->card_w - cT->w);
                } else {
                    cT->x = sT->x + sT->w / 2 - self->card_w / 2 + 0.5f * (self->card_w - cT->w);
                }
                float highlight_height = G->HIGHLIGHT_H / 2;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height + rm * 0.03f * sinf(wrap_angle(0.666f * REAL + cT->x));
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x_pinned, NULL);
    }
    if (streq(ctype, "consumeable")) { // :530
        for (int k = 1;; k++) {
            Card *card = CardArea_card(self, k);
            if (!card) break;
            if (!STATES(card).drag.is) {
                Transform *cT = TR(card);
                if (n > 1) {
                    cT->x = sT->x + (sT->w - self->card_w) * ((k - 1) / (n - 1)) + 0.5f * (self->card_w - cT->w);
                } else {
                    cT->x = sT->x + sT->w / 2 - self->card_w / 2 + 0.5f * (self->card_w - cT->w);
                }
                float highlight_height = G->HIGHLIGHT_H;
                if (!card->highlighted) highlight_height = 0;
                cT->y = sT->y + sT->h / 2 - cT->h / 2 - highlight_height +
                        (!card->highlighted ? rm * 0.05f * sinf(wrap_angle(2 * 1.666f * REAL + cT->x)) : 0);
                cT->x = cT->x + card->moveable.shadow_parrallax.x / 30;
            }
        }
        tsort(self->cards, less_center_x, NULL);
    }
    for (int k = 1;; k++) { // :546
        Card *card = CardArea_card(self, k);
        if (!card) break;
        card->rank = k;
    }
    if (CardArea_child(self, "view_deck")) { // :549
        void *major = CardArea_card(self, 1) ? (void *)CardArea_card(self, 1) : (void *)self;
        Moveable_set_role(CardArea_child(self, "view_deck"), TAB("major", O(major)));
    }
}

// cardarea.lua:554
void CardArea_hard_set_T(CardArea *self, float X, float Y, float W, float H) {
    float x = isnan(X) ? TR(self)->x : X;
    float y = isnan(Y) ? TR(self)->y : Y;
    float w = isnan(W) ? TR(self)->w : W;
    float h = isnan(H) ? TR(self)->h : H;
    Moveable_hard_set_T(&self->moveable, x, y, w, h);
    Moveable_calculate_parrallax(&self->moveable);
    CardArea_align_cards(self);
    CardArea_hard_set_cards(self);
}

// cardarea.lua:565
void CardArea_hard_set_cards(CardArea *self) {
    for (TIter it = titer(self->cards); tnext(&it);) { // pairs
        Card *card = it.val.obj;
        Card_hard_set_T(card, NAN, NAN, NAN, NAN);
        Moveable_calculate_parrallax(&card->moveable);
    }
}

// cardarea.lua:572
void CardArea_shuffle(CardArea *self, const char *_seed) {
    pseudoshuffle(self->cards, N(pseudoseed(_seed ? _seed : "shuffle", NULL)));
    CardArea_set_ranks(self);
}

// cardarea.lua:580-588, the comparators of CardArea:sort
static bool less_desc(Val a, Val b, void *ctx) { return Card_get_nominal(a.obj, NULL) > Card_get_nominal(b.obj, NULL); }
static bool less_asc(Val a, Val b, void *ctx) { return Card_get_nominal(a.obj, NULL) < Card_get_nominal(b.obj, NULL); }
static bool less_suit_desc(Val a, Val b, void *ctx) { return Card_get_nominal(a.obj, "suit") > Card_get_nominal(b.obj, "suit"); }
static bool less_suit_asc(Val a, Val b, void *ctx) { return Card_get_nominal(a.obj, "suit") < Card_get_nominal(b.obj, "suit"); }
// a.config.card.order or a.config.center.order
static num card_order(Card *c) {
    Val v = tget(ttab(c->moveable.node.config, "card"), "order");
    return truthy(v) ? vnum(v) : tnum(ttab(c->moveable.node.config, "center"), "order");
}
static bool less_order(Val a, Val b, void *ctx) { return card_order(a.obj) < card_order(b.obj); }

// cardarea.lua:577
void CardArea_sort(CardArea *self, const char *method) {
    if (method) tsets(CONFIG(self), "sort", method);
    const char *sort = tstr(CONFIG(self), "sort");
    if (streq(sort, "desc")) {
        tsort(self->cards, less_desc, NULL);
    } else if (streq(sort, "asc")) {
        tsort(self->cards, less_asc, NULL);
    } else if (streq(sort, "suit desc")) {
        tsort(self->cards, less_suit_desc, NULL);
    } else if (streq(sort, "suit asc")) {
        tsort(self->cards, less_suit_asc, NULL);
    } else if (streq(sort, "order")) {
        tsort(self->cards, less_order, NULL);
    }
}

// cardarea.lua:592
bool CardArea_draw_card_from(CardArea *self, CardArea *area, bool stay_flipped_arg, bool discarded_only) {
    if (is_a(area, &CardArea_class)) {
        if (tlen(self->cards) < tnum(CONFIG(self), "card_limit") || self == G->deck || self == G->hand) {
            Card *card = CardArea_remove_card(area, NULL, discarded_only);
            if (card) {
                if (area == G->discard) {
                    TR(card)->r = 0;
                }
                // :600 shadows the parameter
                Blind *blind = G->GAME ? tobj(G->GAME, "blind") : NULL;
                bool stay_flipped = blind && Blind_stay_flipped(blind, self, card);
                Val flipped_cards = tget(ttab(G->GAME, "modifiers"), "flipped_cards");
                if ((self == G->hand) && truthy(flipped_cards)) {
                    if (pseudorandom(N(pseudoseed("flipped_card", NULL)), NIL, NIL) < 1 / vnum(flipped_cards)) {
                        stay_flipped = true;
                    }
                }
                CardArea_emplace(self, card, NULL, stay_flipped);
                return true;
            }
        }
    }
    return false;
}

// cardarea.lua:613
void CardArea_click(CardArea *self) {
    if (self == G->deck) {
        FUNCS_deck_info(NULL);
    }
}

// cardarea.lua:619
Table *CardArea_save(CardArea *self) {
    if (!self->cards) return NULL;
    Table *cards = tnew();
    Table *cardAreaTable = TAB("cards", T(cards), "config", T(CONFIG(self)));
    for (int i = 1; i <= tlen(self->cards); i++) {
        tinsert(cards, T(Card_save(CardArea_card(self, i))));
    }

    return cardAreaTable;
}
