// functions/state_events.lua:1210-1642: the tutorial's driver (tutorial_controller) and its parts (tutorial_part).
#include "all.h"

// G.SETTINGS.tutorial_progress and its two maps, read again at every use as the Lua does
#define TP ttab(G->SETTINGS, "tutorial_progress")
#define CP ttab(TP, "completed_parts")
#define HP ttab(TP, "hold_parts")

// state_events.lua:1210
void FUNCS_tutorial_controller(void) {
    if (G->F_SKIP_TUTORIAL) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL);
        return;
    }
    if (!truthy(tget(G->SETTINGS, "tutorial_progress"))) {
        tsett(G->SETTINGS, "tutorial_progress",
              TAB("forced_shop", T(ARR(S("j_joker"), S("c_empress"))),
                  "forced_voucher", S("v_grabber"),
                  "forced_tags", T(ARR(S("tag_handy"), S("tag_garbage"))),
                  "hold_parts", T(tnew()),
                  "completed_parts", T(tnew())));
    }
    if (!tbool(G->SETTINGS, "paused") && !tbool(G->SETTINGS, "tutorial_complete")) {
        if (G->STATE == STATES_BLIND_SELECT && G->blind_select && !tbool(CP, "small_blind")) {
            tsets(TP, "section", "small_blind");
            FUNCS_tutorial_part("small_blind");
            tsetb(CP, "small_blind", true);
            Game_save_progress(G);
        }
        if (G->STATE == STATES_BLIND_SELECT && G->blind_select && !tbool(CP, "big_blind") && tnum(G->GAME, "round") > 0) {
            tsets(TP, "section", "big_blind");
            FUNCS_tutorial_part("big_blind");
            tsetb(CP, "big_blind", true);
            tset(TP, "forced_tags", NIL);
            Game_save_progress(G);
        }
        if (G->STATE == STATES_SELECTING_HAND && !tbool(CP, "second_hand") && tbool(HP, "big_blind")) {
            tsets(TP, "section", "second_hand");
            FUNCS_tutorial_part("second_hand");
            tsetb(CP, "second_hand", true);
            Game_save_progress(G);
        }
        if (tbool(HP, "second_hand")) {
            tsetb(G->SETTINGS, "tutorial_complete", true);
        }
        if (!tbool(CP, "first_hand_section")) {
            if (G->STATE == STATES_SELECTING_HAND && !tbool(CP, "first_hand")) {
                tsets(TP, "section", "first_hand");
                FUNCS_tutorial_part("first_hand");
                tsetb(CP, "first_hand", true);
                Game_save_progress(G);
            }
            if (G->STATE == STATES_SELECTING_HAND && !tbool(CP, "first_hand_2") && tbool(HP, "first_hand")) {
                FUNCS_tutorial_part("first_hand_2");
                tsetb(CP, "first_hand_2", true);
                Game_save_progress(G);
            }
            if (G->STATE == STATES_SELECTING_HAND && !tbool(CP, "first_hand_3") && tbool(HP, "first_hand_2")) {
                FUNCS_tutorial_part("first_hand_3");
                tsetb(CP, "first_hand_3", true);
                Game_save_progress(G);
            }
            if (G->STATE == STATES_SELECTING_HAND && !tbool(CP, "first_hand_4") && tbool(HP, "first_hand_3")) {
                FUNCS_tutorial_part("first_hand_4");
                tsetb(CP, "first_hand_4", true);
                tsetb(CP, "first_hand_section", true);
                Game_save_progress(G);
            }
        }
        if (G->STATE == STATES_SHOP && G->shop && G->shop->moveable.VT.y < 5 && !tbool(CP, "shop_1")) {
            tsets(TP, "section", "shop");
            FUNCS_tutorial_part("shop_1");
            tsetb(CP, "shop_1", true);
            tset(TP, "forced_voucher", NIL);
            Game_save_progress(G);
        }
    }
}

// ---------------------------------------------------------------- lookups the rows of tutorial_part repeat
// note: where the Lua indexes a nil (a missing box or element) it raises an error; here that is a NULL dereference.

// node.children[i]
static Node *child(Node *node, int i) { return (Node *)tobji(node->children, i); }
// G.blind_select.UIRoot.children[1].children[n]
static Node *BS(int n) { return child(child((Node *)G->blind_select->UIRoot, 1), n); }
// G.blind_select.UIRoot.children[1].children[n].config.object:get_UIE_by_ID(id)
static Val BS_id(int n, const char *id) {
    return O(UIBox_get_UIE_by_ID((UIBox *)tobj(BS(n)->config, "object"), S(id), NULL));
}
// G.HUD:get_UIE_by_ID(id)
static Node *HUD_node(const char *id) { return (Node *)UIBox_get_UIE_by_ID(G->HUD, S(id), NULL); }
static Val HUD_id(const char *id) { return O(HUD_node(id)); }
// G.HUD:get_UIE_by_ID('dollar_text_UI').parent.parent.parent
static Val dollar_UI(void) { return O(HUD_node("dollar_text_UI")->parent->parent->parent); }
// {major = major, type = type, offset = {x = x, y = y}}
static Table *attach(void *major, const char *type, num x, num y) {
    return TAB("major", O(major), "type", S(type), "offset", T(TAB("x", N(x), "y", N(y))));
}

// ---------------------------------------------------------------- snap_to closures (called with no arguments)
// state_events.lua:1309 (n = 1) and :1368 (n = 2); ctx: n
static Val snap_blind_select(void *ctx, int argc, Val *argv) {
    int n = (int)(intptr_t)ctx;
    if (G->blind_select && G->blind_select->UIRoot && child((Node *)G->blind_select->UIRoot, 1) && BS(n) &&
        truthy(tget(BS(n)->config, "object"))) {
        return BS_id(n, "select_blind_button");
    }
    return NIL;
}
// state_events.lua:1397
static Val snap_run_info(void *ctx, int argc, Val *argv) { return HUD_id("run_info_button"); }
// state_events.lua:1409, :1487
static Val snap_hand_card1(void *ctx, int argc, Val *argv) { return O(CardArea_card(G->hand, 1)); }
// state_events.lua:1509 (cards[2]) and :1553 (cards[1]); ctx: the index
static Val snap_shop_jokers_card(void *ctx, int argc, Val *argv) {
    return O(CardArea_card(G->shop_jokers, (int)(intptr_t)ctx));
}
// state_events.lua:1586
static Val snap_shop_vouchers_card1(void *ctx, int argc, Val *argv) { return O(CardArea_card(G->shop_vouchers, 1)); }
// state_events.lua:1607
static Val snap_shop_booster_card1(void *ctx, int argc, Val *argv) { return O(CardArea_card(G->shop_booster, 1)); }
// state_events.lua:1616
static Val snap_next_round(void *ctx, int argc, Val *argv) {
    if (G->shop) return O(UIBox_get_UIE_by_ID(G->shop, S("next_round_button"), NULL));
    return NIL;
}

// ---------------------------------------------------------------- highlight closures (evaluated when the step shows)
// note: identical closures of the Lua share one function. A nil entry (a card that is not there) is a hole or a
// shorter array, as in the Lua's constructor.
// state_events.lua:1518, :1528
static Val hl_jokers_card1(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(CardArea_card(G->jokers, 1))));
}
// state_events.lua:1538
static Val hl_jokers(void *ctx, int argc, Val *argv) { return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(G->jokers))); }
// state_events.lua:1548
static Val hl_shop_jokers_card1(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(CardArea_card(G->shop_jokers, 1))));
}
// state_events.lua:1561
static Val hl_consumeables_last(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(CardArea_card(G->consumeables, CardArea_n(G->consumeables)))));
}
// state_events.lua:1571
static Val hl_consumeables(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(G->consumeables)));
}
// state_events.lua:1581, :1592
static Val hl_shop_vouchers(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(G->shop_vouchers)));
}
// state_events.lua:1602
static Val hl_shop_booster(void *ctx, int argc, Val *argv) {
    return T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(G->shop_booster)));
}
// state_events.lua:1613
static Val hl_next_round(void *ctx, int argc, Val *argv) {
    return T(ARR(O(UIBox_get_UIE_by_ID(G->shop, S("next_round_button"), NULL))));
}

// state_events.lua:1625 the poller that removes the overlay once every step of the part is done
struct ev_tutorial_end_ctx { num step; const char *_part; };
static bool ev_tutorial_end(void *ctx) {
    struct ev_tutorial_end_ctx *c = ctx;
    Table *OT = G->OVERLAY_TUTORIAL->moveable.node.object.dyn; // step, step_complete, skip_steps, Jimbo, content
    if ((veq(tget(OT, "step"), N(c->step)) && !tbool(OT, "step_complete")) || tbool(OT, "skip_steps")) {
        if (tbool(OT, "Jimbo")) Card_Character_remove((Card_Character *)tobj(OT, "Jimbo"));
        if (tbool(OT, "content")) UIBox_remove((UIBox *)tobj(OT, "content"));
        UIBox_remove(G->OVERLAY_TUTORIAL);
        G->OVERLAY_TUTORIAL = NULL;
        tsetb(HP, c->_part, true);
        return true;
    }
    return tnum(OT, "step") > c->step || tbool(OT, "skip_steps");
}

// state_events.lua:1281
void FUNCS_tutorial_part(const char *_part) {
    num step = 1;
    tsetb(G->SETTINGS, "paused", true);
    if (streq(_part, "small_blind")) {
        step = tutorial_info(TAB("text_key", S("sb_1"),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("sb_2"),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("sb_3"),
                                 "highlight", T(ARR(BS_id(1, "blind_name"), BS_id(1, "blind_desc"))),
                                 "attach", T(attach(BS(1), "tr", 2, 4)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("sb_4"),
                                 "highlight", T(ARR(O(BS(1)))),
                                 "snap_to", FN(snap_blind_select, (void *)(intptr_t)1),
                                 "attach", T(attach(BS(1), "tr", 2, 4)),
                                 "step", N(step),
                                 "no_button", B(true),
                                 "button_listen", S("select_blind")));
    } else if (streq(_part, "big_blind")) {
        step = tutorial_info(TAB("text_key", S("bb_1"),
                                 "highlight", T(ARR(BS_id(2, "blind_name"), BS_id(2, "blind_desc"))),
                                 "hard_set", B(true),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("bb_2"),
                                 "highlight", T(ARR(BS_id(2, "blind_name"), BS_id(2, "tag_desc"))),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("bb_3"),
                                 "highlight", T(ARR(BS_id(3, "blind_name"), BS_id(3, "blind_desc"))),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("bb_4"),
                                 "highlight", T(ARR(BS_id(3, "blind_name"), BS_id(3, "blind_desc"), BS_id(3, "blind_extras"),
                                                    HUD_id("hud_ante"))),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("bb_5"),
                                 "loc_vars", T(ARR(tget(G->GAME, "win_ante"))),
                                 "highlight", T(ARR(O(G->blind_select), HUD_id("hud_ante"))),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step),
                                 "no_button", B(true),
                                 "snap_to", FN(snap_blind_select, (void *)(intptr_t)2),
                                 "button_listen", S("select_blind")));
    } else if (streq(_part, "first_hand")) {
        step = tutorial_info(TAB("text_key", S("fh_1"),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("fh_2"),
                                 "highlight", T(ARR(HUD_id("hand_text_area"))),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("fh_3"),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "highlight", T(ARR(HUD_id("run_info_button"))),
                                 "no_button", B(true),
                                 "button_listen", S("run_info"),
                                 "snap_to", FN(snap_run_info, NULL),
                                 "step", N(step)));
    } else if (streq(_part, "first_hand_2")) {
        step = tutorial_info(TAB("hard_set", B(true),
                                 "text_key", S("fh_4"),
                                 "highlight", T(ARR(O(G->hand), HUD_id("run_info_button"))),
                                 "attach", T(attach(G->hand, "cl", -1.5, 0)),
                                 "snap_to", FN(snap_hand_card1, NULL),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("fh_5"),
                                 "highlight", T(ARR(O(G->hand), O(UIBox_get_UIE_by_ID(G->buttons, S("play_button"), NULL)),
                                                    HUD_id("run_info_button"))),
                                 "attach", T(attach(G->hand, "cl", -1.5, 0)),
                                 "no_button", B(true),
                                 "button_listen", S("play_cards_from_highlighted"),
                                 "step", N(step)));
    } else if (streq(_part, "first_hand_3")) {
        step = tutorial_info(TAB("hard_set", B(true),
                                 "text_key", S("fh_6"),
                                 "highlight", T(ARR(O(G->hand), O(UIBox_get_UIE_by_ID(G->buttons, S("discard_button"), NULL)),
                                                    HUD_id("run_info_button"))),
                                 "attach", T(attach(G->hand, "cl", -1.5, 0)),
                                 "no_button", B(true),
                                 "button_listen", S("discard_cards_from_highlighted"),
                                 "step", N(step)));
    } else if (streq(_part, "first_hand_4")) {
        step = tutorial_info(TAB("hard_set", B(true),
                                 "text_key", S("fh_7"),
                                 "highlight", T(ARR(O(HUD_node("hud_hands")->parent))),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("fh_8"),
                                 "highlight", T(ARR(O(HUD_node("hud_hands")->parent), HUD_id("row_dollars_chips"),
                                                    O(G->HUD_blind))),
                                 "attach", T(attach(G->ROOM_ATTACH, "cm", 0, 0)),
                                 "step", N(step)));
    } else if (streq(_part, "second_hand")) {
        step = tutorial_info(TAB("text_key", S("sh_1"),
                                 "hard_set", B(true),
                                 "highlight", T(ARR(O(G->jokers))),
                                 "attach", T(attach(G->HUD, "cm", 0, -2)),
                                 "step", N(step)));
        Card *empress = (Card *)tobji(find_joker("The Empress", false), 1);
        if (empress) {
            step = tutorial_info(TAB("text_key", S("sh_2"),
                                     "highlight", T(ARR(O(empress))),
                                     "attach", T(attach(G->HUD, "cm", 0, -2)),
                                     "step", N(step)));
            step = tutorial_info(TAB("text_key", S("sh_3"),
                                     "attach", T(attach(G->HUD, "cm", 0, -2)),
                                     "highlight", T(ARR(O(empress), O(G->hand))),
                                     "no_button", B(true),
                                     "button_listen", S("use_card"),
                                     "snap_to", FN(snap_hand_card1, NULL),
                                     "step", N(step)));
        }
    } else if (streq(_part, "shop_1")) {
        step = tutorial_info(TAB("hard_set", B(true),
                                 "text_key", S("s_1"),
                                 "highlight", T(ARR(O(G->SHOP_SIGN), dollar_UI())),
                                 "attach", T(attach(G->shop, "tm", 0, 4)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_2"),
                                 "highlight", T(ARR(O(G->SHOP_SIGN), dollar_UI(), O(CardArea_card(G->shop_jokers, 2)))),
                                 "snap_to", FN(snap_shop_jokers_card, (void *)(intptr_t)2),
                                 "attach", T(attach(G->shop, "tr", -0.5, 6)),
                                 "no_button", B(true),
                                 "button_listen", S("buy_from_shop"),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_3"),
                                 "loc_vars", T(ARR(N(tlen(ttab(G->P_CENTER_POOLS, "Joker"))))),
                                 "highlight", FN(hl_jokers_card1, NULL),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_4"),
                                 "highlight", FN(hl_jokers_card1, NULL),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_5"),
                                 "highlight", FN(hl_jokers, NULL),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_6"),
                                 "highlight", FN(hl_shop_jokers_card1, NULL),
                                 "snap_to", FN(snap_shop_jokers_card, (void *)(intptr_t)1),
                                 "no_button", B(true),
                                 "button_listen", S("buy_from_shop"),
                                 "attach", T(attach(G->shop, "tr", -0.5, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_7"),
                                 "highlight", FN(hl_consumeables_last, NULL),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_8"),
                                 "highlight", FN(hl_consumeables, NULL),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_9"),
                                 "highlight", FN(hl_shop_vouchers, NULL),
                                 "snap_to", FN(snap_shop_vouchers_card1, NULL),
                                 "attach", T(attach(G->shop, "tr", -4, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_10"),
                                 "highlight", FN(hl_shop_vouchers, NULL),
                                 "attach", T(attach(G->shop, "tr", -4, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_11"),
                                 "highlight", FN(hl_shop_booster, NULL),
                                 "snap_to", FN(snap_shop_booster_card1, NULL),
                                 "attach", T(attach(G->shop, "tl", 3, 6)),
                                 "step", N(step)));
        step = tutorial_info(TAB("text_key", S("s_12"),
                                 "highlight", FN(hl_next_round, NULL),
                                 "snap_to", FN(snap_next_round, NULL),
                                 "no_button", B(true),
                                 "button_listen", S("toggle_shop"),
                                 "attach", T(attach(G->shop, "tm", 0, 6)),
                                 "step", N(step)));
    }

    struct ev_tutorial_end_ctx *c = NEWCTX(struct ev_tutorial_end_ctx);
    c->step = step; // the final value: the closure is made after the last assignment
    c->_part = _part;
    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .timer = "REAL", .func = ev_tutorial_end, .ctx = c}),
          "tutorial", false);
    tsetb(G->SETTINGS, "paused", false);
}
