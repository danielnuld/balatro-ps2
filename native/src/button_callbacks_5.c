// functions/button_callbacks.lua:2404-3039: the shop / blind select / booster actions and the run transitions.
#include "all.h"

// note: X.alignment.offset.py (the saved y of a box slid off screen) lives in the UIBox's dyn (button_callbacks.h);
// this is the key used here. FUNCS_use_card (another part of the file) must use the same one.
#define PY_KEY "alignment.offset.py"

#define CFG(el) ((el)->moveable.node.config)
// el.children[i]
static UIElement *uie_child(UIElement *el, int i) { return (UIElement *)tobji(el->moveable.node.children, i); }

// button_callbacks.lua:2618, 2776, 2848: for i = 1, #G.GAME.tags do if G.GAME.tags[i]:apply_to_run({type =
// 'new_blind_choice'}) then break end end
static void tags_new_blind_choice(void) {
    Table *tags = ttab(G->GAME, "tags");
    int n = tlen(tags);
    for (int i = 1; i <= n; i++) {
        if (truthy(Tag_apply_to_run((Tag *)tobji(tags, i), TAB("type", S("new_blind_choice"))))) break;
    }
}

// ---------------------------------------------------------------- buy_from_shop
struct buy_ctx { UIElement *e; Card *c1; };

// button_callbacks.lua:2437
static bool ev_buy_calc_joker(void *ctx) {
    Card *c1 = ctx;
    Card_calculate_joker(c1, TAB("buying_card", B(true), "card", O(c1)));
    return true;
}

// button_callbacks.lua:2457
static bool ev_buy_inflation(void *ctx) {
    // note: pairs(G.I.CARD) is hash order in the Lua; each card is independent. v.set_cost: only Cards have it.
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        if (is_a(it.val.obj, &Card_class)) Card_set_cost((Card *)it.val.obj);
    }
    return true;
}

// button_callbacks.lua:2416
static bool ev_buy_from_shop(void *ctx) {
    struct buy_ctx *c = ctx;
    UIElement *e = c->e;
    Card *c1 = c->c1;
    Table *children = c1->moveable.node.children;
    CardArea_remove_card(c1->area, c1, false);
    Card_add_to_deck(c1, false);
    if (Card_child(c1, "price")) CALL(Card_child(c1, "price"), remove);
    tset(children, "price", NIL);
    if (Card_child(c1, "buy_button")) CALL(Card_child(c1, "buy_button"), remove);
    tset(children, "buy_button", NIL);
    remove_nils(children); // result discarded, as the Lua
    const char *set = tstr(c1->ability, "set");
    if (streq(set, "Default") || streq(set, "Enhanced")) {
        inc_career_stat("c_playing_cards_bought", 1);
        G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1: 0 as nil
        CardArea_emplace(G->deck, c1, NULL, false);
        c1->playing_card = G->playing_card;
        playing_card_joker_effects(ARR(O(c1)));
        tinsert(G->playing_cards, O(c1));
    } else if (!veq(tget(CFG(e), "id"), S("buy_and_use"))) {
        if (truthy(tget(c1->ability, "consumeable"))) {
            CardArea_emplace(G->consumeables, c1, NULL, false);
        } else {
            CardArea_emplace(G->jokers, c1, NULL, false);
        }
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_buy_calc_joker, .ctx = c1}), NULL, false);
    }
    // Tallies for unlocks
    Table *cards_purchased = ttab(ttab(G->GAME, "round_scores"), "cards_purchased");
    tsetn(cards_purchased, "amt", tnum(cards_purchased, "amt") + 1);
    if (truthy(tget(c1->ability, "consumeable"))) {
        const char *center_set = tstr(ttab(c1->moveable.node.config, "center"), "set");
        if (streq(center_set, "Planet")) {
            inc_career_stat("c_planets_bought", 1);
        } else if (streq(center_set, "Tarot")) {
            inc_career_stat("c_tarots_bought", 1);
        }
    } else if (streq(tstr(c1->ability, "set"), "Joker")) {
        Table *current_round = ttab(G->GAME, "current_round");
        tsetn(current_round, "jokers_purchased", tnum(current_round, "jokers_purchased") + 1);
    }

    int njokers = CardArea_n(G->jokers);
    for (int i = 1; i <= njokers; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("buying_card", B(true), "card", O(c1)));
    }

    if (truthy(tget(ttab(G->GAME, "modifiers"), "inflation"))) {
        tsetn(G->GAME, "inflation", tnum(G->GAME, "inflation") + 1);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_buy_inflation}), NULL, false);
    }

    play_sound("card1", -1, -1);
    inc_career_stat("c_shop_dollars_spent", c1->cost);
    if (c1->cost != 0) {
        ease_dollars(-c1->cost, false);
    }
    Controller_save_cardarea_focus(G->CONTROLLER, "jokers");
    Controller_recall_cardarea_focus(G->CONTROLLER, S("jokers"));

    if (veq(tget(CFG(e), "id"), S("buy_and_use"))) {
        FUNCS_use_card(e, NULL, true, false);
    }
    return true;
}

// button_callbacks.lua:2404
void FUNCS_buy_from_shop(UIElement *e) {
    Val rt = tget(CFG(e), "ref_table");
    if (rt.t == V_OBJ && is_a(rt.obj, &Card_class)) {
        Card *c1 = rt.obj;
        if (!veq(tget(CFG(e), "id"), S("buy_and_use"))) {
            if (!FUNCS_check_for_buy_space(c1)) {
                e->disable_button = false;
                return; // the Lua returns false: nobody reads it
            }
        }
        struct buy_ctx *c = NEWCTX(struct buy_ctx);
        c->e = e;
        c->c1 = c1;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.1, .func = ev_buy_from_shop, .ctx = c}), NULL, false);
    }
}

// ---------------------------------------------------------------- toggle_shop
// button_callbacks.lua:2490
static bool ev_toggle_shop_slide(void *ctx) {
    G->shop->moveable.alignment.offset.y = G->ROOM->T->y + 29;
    G->SHOP_SIGN->moveable.alignment.offset.y = -15;
    return true;
}

// button_callbacks.lua:2499
static bool ev_toggle_shop_remove(void *ctx) {
    UIBox_remove(G->shop);
    G->shop = NULL;
    UIBox_remove(G->SHOP_SIGN);
    G->SHOP_SIGN = NULL;
    G->STATE_COMPLETE = false;
    G->STATE = STATES_BLIND_SELECT;
    tset(G->CONTROLLER->locks, "toggle_shop", NIL);
    return true;
}

// button_callbacks.lua:2481
void FUNCS_toggle_shop(UIElement *e) {
    stop_use();
    tsetb(G->CONTROLLER->locks, "toggle_shop", true);
    if (G->shop) {
        int njokers = CardArea_n(G->jokers);
        for (int i = 1; i <= njokers; i++) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("ending_shop", B(true)));
        }
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_toggle_shop_slide}), NULL, false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.5, .func = ev_toggle_shop_remove}), NULL, false);
    }
}

// ---------------------------------------------------------------- select_blind
// button_callbacks.lua:2524
static bool ev_select_blind_slide(void *ctx) {
    G->blind_prompt_box->moveable.alignment.offset.y = -10;
    G->blind_select->moveable.alignment.offset.y = 40;
    G->blind_select->moveable.alignment.offset.x = 0;
    return true;
}

// button_callbacks.lua:2532
static bool ev_select_blind_set(void *ctx) {
    UIElement *e = ctx;
    ease_round(1);
    inc_career_stat("c_rounds", 1);
    if (DEMO_MODE) {
        Val rounds = tget(G->SETTINGS, "DEMO_ROUNDS");
        tsetn(G->SETTINGS, "DEMO_ROUNDS", (truthy(rounds) ? rounds.n : 0) + 1);
        inc_steam_stat("demo_rounds");
        Game_save_settings(G);
    }
    Table *round_resets = ttab(G->GAME, "round_resets");
    tset(round_resets, "blind", tget(CFG(e), "ref_table"));
    tsetv(ttab(round_resets, "blind_states"), tget(G->GAME, "blind_on_deck"), S("Current"));
    UIBox_remove(G->blind_select);
    UIBox_remove(G->blind_prompt_box);
    G->blind_select = NULL;
    delay(0.2, NULL);
    return true;
}

// button_callbacks.lua:2550
static bool ev_select_blind_new_round(void *ctx) {
    new_round();
    return true;
}

// button_callbacks.lua:2513
void FUNCS_select_blind(UIElement *e) {
    stop_use();
    if (G->blind_select) {
        tsetb(G->GAME, "facing_blind", true);
        DynaText *d;
        d = tobj(CFG(UIBox_get_UIE_by_ID(G->blind_prompt_box, S("prompt_dynatext1"), NULL)), "object");
        d->pop_delay = 0;
        d = tobj(CFG(UIBox_get_UIE_by_ID(G->blind_prompt_box, S("prompt_dynatext1"), NULL)), "object");
        DynaText_pop_out(d, 5);
        d = tobj(CFG(UIBox_get_UIE_by_ID(G->blind_prompt_box, S("prompt_dynatext2"), NULL)), "object");
        d->pop_delay = 0;
        d = tobj(CFG(UIBox_get_UIE_by_ID(G->blind_prompt_box, S("prompt_dynatext2"), NULL)), "object");
        DynaText_pop_out(d, 5);

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "before", .delay = 0.2, .func = ev_select_blind_slide}), NULL, false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_select_blind_set, .ctx = e}), NULL, false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_select_blind_new_round}), NULL, false);
    }
}

// ---------------------------------------------------------------- skip_booster / end_consumeable
// button_callbacks.lua:2558
void FUNCS_skip_booster(UIElement *e) {
    int njokers = CardArea_n(G->jokers);
    for (int i = 1; i <= njokers; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("skipping_booster", B(true)));
    }
    FUNCS_end_consumeable(e, -1);
}

struct endc_ctx { num delayfac; };

// button_callbacks.lua:2575
static bool ev_endc_remove_pack(void *ctx) {
    UIBox_remove(G->booster_pack);
    G->booster_pack = NULL;
    return true;
}

// button_callbacks.lua:2581
static bool ev_endc_remove_particles(void *ctx) {
    if (G->booster_pack_sparkles) { Particles_remove(G->booster_pack_sparkles); G->booster_pack_sparkles = NULL; }
    if (G->booster_pack_stars) { Particles_remove(G->booster_pack_stars); G->booster_pack_stars = NULL; }
    if (G->booster_pack_meteors) { Particles_remove(G->booster_pack_meteors); G->booster_pack_meteors = NULL; }
    return true;
}

// button_callbacks.lua:2609
static bool ev_endc_snap(void *ctx) {
    if (G->shop) Controller_snap_to(G->CONTROLLER, TAB("node", O(UIBox_get_UIE_by_ID(G->shop, S("next_round_button"), NULL))));
    return true;
}

// button_callbacks.lua:2595-2606: if box and box.alignment.offset.py then offset.y = offset.py; offset.py = nil end
static void restore_py(UIBox *box) {
    if (!box) return;
    Table *dyn = box->moveable.node.object.dyn;
    Val py = tget(dyn, PY_KEY);
    if (truthy(py)) {
        box->moveable.alignment.offset.y = (float)py.n;
        tset(dyn, PY_KEY, NIL);
    }
}

// button_callbacks.lua:2594
static bool ev_endc_restore(void *ctx) {
    restore_py(G->shop);
    restore_py(G->blind_select);
    restore_py(G->round_eval);
    G->CONTROLLER->interrupt.focus = true;

    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_endc_snap}), NULL, false);
    // note: G.STATE is an int; a nil PACK_INTERRUPT (the Lua would leave G.STATE nil) is 0 here
    int pack_interrupt = (int)tnum(G->GAME, "PACK_INTERRUPT");
    G->STATE = pack_interrupt;
    ease_background_colour_blind(pack_interrupt, NULL);
    tset(G->GAME, "PACK_INTERRUPT", NIL);
    return true;
}

// button_callbacks.lua:2622
static bool ev_endc_save(void *ctx) {
    save_run();
    return true;
}

// button_callbacks.lua:2591
static bool ev_endc_main(void *ctx) {
    struct endc_ctx *c = ctx;
    num delayfac = c->delayfac;
    FUNCS_draw_from_hand_to_deck(NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2 * delayfac, .func = ev_endc_restore}), NULL, false);
    tags_new_blind_choice();

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2 * delayfac, .func = ev_endc_save}), NULL, false);

    return true;
}

// button_callbacks.lua:2565
void FUNCS_end_consumeable(UIElement *e, num delayfac_) {
    num delayfac = delayfac_ < 0 ? 1 : (num)delayfac_; // delayfac or 1
    stop_use();
    if (G->booster_pack) {
        if (G->booster_pack_sparkles) Particles_fade(G->booster_pack_sparkles, (1 * delayfac), -1);
        if (G->booster_pack_stars) Particles_fade(G->booster_pack_stars, (1 * delayfac), -1);
        if (G->booster_pack_meteors) Particles_fade(G->booster_pack_meteors, (1 * delayfac), -1);
        G->booster_pack->moveable.alignment.offset.y = G->ROOM->T->y + 9;

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2 * delayfac, .not_blocking = true, .not_blockable = true,
                                                 .func = ev_endc_remove_pack}), NULL, false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 1 * delayfac, .not_blocking = true, .not_blockable = true,
                                                 .func = ev_endc_remove_particles}), NULL, false);
    }

    delay((0.2 * delayfac), NULL);
    struct endc_ctx *c = NEWCTX(struct endc_ctx);
    c->delayfac = delayfac;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2 * delayfac, .func = ev_endc_main, .ctx = c}), NULL, false);
}

// ---------------------------------------------------------------- blind_choice_handler
// button_callbacks.lua:2665
static bool ev_bch_snap(void *ctx) {
    Controller_snap_to(G->CONTROLLER, TAB("node", O(ctx)));
    return true;
}

// button_callbacks.lua:2631
void FUNCS_blind_choice_handler(UIElement *e) {
    Table *config = CFG(e);
    Table *ref_table = ttab(config, "ref_table");
    Val id = tget(config, "id");
    if (!truthy(tget(ref_table, "run_info")) && G->blind_select && G->blind_select->moveable.VT.y < 10 && truthy(id) &&
        truthy(tget(G->blind_select_opts, str_lower(vstr(id))))) {
        if (e->UIBox->moveable.role.xy_bond != BOND_WEAK) Moveable_set_role((Moveable *)e->UIBox, TAB("xy_bond", S("Weak")));
        Val blind_on_deck = tget(G->GAME, "blind_on_deck");
        if ((!streq(tstr(ref_table, "deck"), "on") && veq(id, blind_on_deck)) ||
            (!streq(tstr(ref_table, "deck"), "off") && !veq(id, blind_on_deck))) {

            UIBox *_blind_choice = tobj(G->blind_select_opts, str_lower(vstr(id)));
            UIElement *_top_button = UIBox_get_UIE_by_ID(e->UIBox, S("select_blind_button"), NULL);
            UIElement *_border = uie_child(uie_child(e->UIBox->UIRoot, 1), 1);
            UIElement *_tag = UIBox_get_UIE_by_ID(e->UIBox, S(fmt("tag_%s", vstr(id))), NULL);
            UIElement *_tag_container = UIBox_get_UIE_by_ID(e->UIBox, S("tag_container"), NULL);
            Table *blind_states = ttab(ttab(G->GAME, "round_resets"), "blind_states");
            if (_tag_container && !tbool(G->SETTINGS, "tutorial_complete") &&
                !truthy(tpath(G->SETTINGS, "tutorial_progress.completed_parts.shop_1"))) {
                _tag_container->moveable.node.states.visible = false;
            } else if (_tag_container) {
                _tag_container->moveable.node.states.visible = true;
            }
            if (veq(id, blind_on_deck)) {
                tsets(ref_table, "deck", "on");
                tsetb(config, "draw_after", false);
                tset(config, "colour", P(G->C.CLEAR));
                Table *border_parent_config = _border->moveable.node.parent->config;
                tsetn(border_parent_config, "outline", 2);
                tset(border_parent_config, "outline_colour", P(G->C.UI.TRANSPARENT_DARK));
                Val outline_colour = tget(CFG(_border), "outline_colour");
                tset(CFG(_border), "outline_colour",
                     truthy(tget(CFG(_border), "outline")) && truthy(outline_colour) ? outline_colour : P(get_blind_main_colour(vstr(id))));
                tsetn(CFG(_border), "outline", 1.5);
                _blind_choice->moveable.alignment.offset.y = -0.9f;
                if (_tag && _tag_container) {
                    tsetb(CFG(uie_child(_tag_container, 2)), "draw_after", false);
                    tset(CFG(uie_child(_tag_container, 2)), "colour", P(G->C.BLACK));
                    tsets(CFG(uie_child(_tag, 2)), "button", "skip_blind");
                    tset(CFG(_tag), "outline_colour", P(adjust_alpha(G->C.BLUE, 0.5f, NULL)));
                    tsetb(CFG(uie_child(_tag, 2)), "hover", true);
                    tset(CFG(uie_child(_tag, 2)), "colour", P(G->C.RED));
                    tset(CFG(uie_child(uie_child(_tag, 2), 1)), "colour", P(G->C.UI.TEXT_LIGHT));
                    Node *_sprite = tobj(CFG(_tag), "ref_table"); // the tag's Sprite (UI_definitions.lua:1472)
                    tset(_sprite->config, "force_focus", NIL);
                }
                if (_top_button) {
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_bch_snap, .ctx = _top_button}), NULL, false);
                    tsets(CFG(_top_button), "button", "select_blind");
                    tset(CFG(_top_button), "colour", P(G->C.FILTER));
                    tsetb(CFG(_top_button), "hover", true);
                    tset(CFG(uie_child(_top_button, 1)), "colour", P(G->C.WHITE));
                }
            } else if (!veq(id, blind_on_deck)) {
                tsets(ref_table, "deck", "off");
                tsetb(config, "draw_after", true);
                tset(config, "colour",
                     P(adjust_alpha(streq(vstr(tgetv(blind_states, id)), "Skipped") ? mix_colours(G->C.BLUE, G->C.L_BLACK, 0.1f, NULL) : G->C.L_BLACK,
                                    0.5f, NULL)));
                Table *border_parent_config = _border->moveable.node.parent->config;
                tset(border_parent_config, "outline", NIL);
                tset(border_parent_config, "outline_colour", NIL);
                tset(CFG(_border), "outline_colour", NIL);
                tset(CFG(_border), "outline", NIL);
                _blind_choice->moveable.alignment.offset.y = -0.2f;
                if (_tag && _tag_container) {
                    if (streq(vstr(tgetv(blind_states, id)), "Skipped") || streq(vstr(tgetv(blind_states, id)), "Defeated")) {
                        Moveable_set_role((Moveable *)uie_child(_tag_container, 2), TAB("xy_bond", S("Weak")));
                        UIElement_align(uie_child(_tag_container, 2), 0, 10);
                        Moveable_set_role((Moveable *)uie_child(_tag_container, 1), TAB("xy_bond", S("Weak")));
                        UIElement_align(uie_child(_tag_container, 1), 0, 10);
                    }
                    if (streq(vstr(tgetv(blind_states, id)), "Skipped")) {
                        Table *definition = create_UIBox_card_alert(TAB("text_rot", N(-0.35), "no_bg", B(true), "text", localize(S("k_skipped_cap"), NULL),
                                                                        "bump_amount", N(1), "scale", N(0.9), "maxw", N(3.4)));
                        tseto(_blind_choice->moveable.node.children, "alert",
                              UIBox_new(TAB("definition", T(definition),
                                            "config", T(TAB("align", S("tmi"),
                                                            "offset", T(TAB("x", N(0), "y", N(2.2))),
                                                            "major", O(_blind_choice), "parent", O(_blind_choice))))));
                    }
                    tset(CFG(uie_child(_tag, 2)), "button", NIL);
                    tset(CFG(_tag), "outline_colour", P(G->C.UI.BACKGROUND_INACTIVE));
                    tsetb(CFG(uie_child(_tag, 2)), "hover", false);
                    tset(CFG(uie_child(_tag, 2)), "colour", P(G->C.UI.BACKGROUND_INACTIVE));
                    tset(CFG(uie_child(uie_child(_tag, 2), 1)), "colour", P(G->C.UI.TEXT_INACTIVE));
                    Node *_sprite = tobj(CFG(_tag), "ref_table");
                    tsetb(_sprite->config, "force_focus", true);
                }
                if (_top_button) {
                    tset(CFG(_top_button), "colour", P(G->C.UI.BACKGROUND_INACTIVE));
                    tset(CFG(_top_button), "button", NIL);
                    tsetb(CFG(_top_button), "hover", false);
                    tset(CFG(uie_child(_top_button, 1)), "colour", P(G->C.UI.TEXT_INACTIVE));
                }
            }
        }
    }
}

// ---------------------------------------------------------------- hover_tag_proxy
// button_callbacks.lua:2718
void FUNCS_hover_tag_proxy(UIElement *e) {
    Node *parent = e->moveable.node.parent;
    if (!parent) return; // (e.parent.states is never nil for a Node)
    // note: Node.created_on_pause and G.SETTINGS.paused are compared as booleans (nil as false on both sides); the
    // Lua's nil ~= false case (a node made before the first pause, with paused then set to false) is not kept.
    bool same_pause = e->moveable.node.created_on_pause == tbool(G->SETTINGS, "paused");
    Val alert = tget(parent->children, "alert");
    if ((e->moveable.node.states.hover.is || parent->states.hover.is) && same_pause && !truthy(alert)) {
        Sprite *_sprite = Tag_get_uibox_table((Tag *)tobj(CFG(e), "ref_table"), NULL);
        Table *definition = UIDEF_card_h_popup((Node *)_sprite);
        UIBox *box = UIBox_new(TAB("definition", T(definition),
                                   "config", T(TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.1))),
                                                   "major", O(parent),
                                                   "instance_type", S("POPUP")))));
        tseto(parent->children, "alert", box);
        CALL(_sprite, juice_up, 0.05f, N(0.02));
        play_sound("paper1", (float)(math_random() * 0.1 + 0.55), 0.42f);
        play_sound("tarot2", (float)(math_random() * 0.1 + 0.55), 0.09f);
        ((Node *)tobj(parent->children, "alert"))->states.collide.can = false;
    } else if (truthy(alert) && ((!e->moveable.node.states.collide.is && !parent->states.collide.is) || !same_pause)) {
        CALL(alert.obj, remove);
        tset(parent->children, "alert", NIL);
    }
}

// ---------------------------------------------------------------- skip_blind
// button_callbacks.lua:2749
static bool ev_skip_blind_unlock(void *ctx) {
    tset(G->CONTROLLER->locks, "skip_blind", NIL);
    return true;
}

// button_callbacks.lua:2766
static bool ev_skip_blind_apply(void *ctx) {
    delay(0.3, NULL);
    int njokers = CardArea_n(G->jokers);
    for (int i = 1; i <= njokers; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("skip_blind", B(true)));
    }
    save_run();
    Table *tags = ttab(G->GAME, "tags");
    int ntags = tlen(tags);
    for (int i = 1; i <= ntags; i++) {
        Tag_apply_to_run((Tag *)tobji(tags, i), TAB("type", S("immediate")));
    }
    tags_new_blind_choice();
    return true;
}

// button_callbacks.lua:2740
void FUNCS_skip_blind(UIElement *e) {
    stop_use();
    tsetb(G->CONTROLLER->locks, "skip_blind", true);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .trigger = "after", .not_blocking = true, .not_blockable = true,
                                             .delay = 2.5, .timer = "TOTAL", .func = ev_skip_blind_unlock}), NULL, false);
    UIElement *_tag = UIBox_get_UIE_by_ID(e->UIBox, S("tag_container"), NULL);
    Val skips = tget(G->GAME, "skips");
    tsetn(G->GAME, "skips", (truthy(skips) ? skips.n : 0) + 1);
    if (_tag) {
        add_tag((Tag *)tobj(CFG(_tag), "ref_table"));
        Val blind_on_deck = tget(G->GAME, "blind_on_deck");
        Val skipped = truthy(blind_on_deck) ? blind_on_deck : S("Small");
        const char *skip_to = veq(blind_on_deck, S("Small")) ? "Big" : veq(blind_on_deck, S("Big")) ? "Boss" : "Boss";
        Table *blind_states = ttab(ttab(G->GAME, "round_resets"), "blind_states");
        tsetv(blind_states, skipped, S("Skipped"));
        tsets(blind_states, skip_to, "Select");
        tsets(G->GAME, "blind_on_deck", skip_to);
        play_sound("generic1", -1, -1);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_skip_blind_apply}), NULL, false);
    }
}

// ---------------------------------------------------------------- reroll_boss
// button_callbacks.lua:2784
void FUNCS_reroll_boss_button(UIElement *e) {
    Table *used_vouchers = ttab(G->GAME, "used_vouchers");
    if (((tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at")) - 10 >= 0) &&
        (truthy(tget(used_vouchers, "v_retcon")) ||
         (truthy(tget(used_vouchers, "v_directors_cut")) && !truthy(tget(ttab(G->GAME, "round_resets"), "boss_rerolled"))))) {
        tset(CFG(e), "colour", P(G->C.RED));
        tsets(CFG(e), "button", "reroll_boss");
        tsetb(CFG(uie_child(uie_child(e, 1), 1)), "shadow", true);
        if (uie_child(e, 2)) tsetb(CFG(uie_child(uie_child(e, 2), 1)), "shadow", true);
    } else {
        tset(CFG(e), "colour", P(G->C.UI.BACKGROUND_INACTIVE));
        tset(CFG(e), "button", NIL);
        tsetb(CFG(uie_child(uie_child(e, 1), 1)), "shadow", false);
        if (uie_child(e, 2)) tsetb(CFG(uie_child(uie_child(e, 2), 1)), "shadow", false);
    }
}

// button_callbacks.lua:2808
static bool ev_reroll_boss_slide(void *ctx) {
    play_sound("other1", -1, -1);
    UIBox *boss = tobj(G->blind_select_opts, "boss");
    Moveable_set_role((Moveable *)boss, TAB("xy_bond", S("Weak")));
    boss->moveable.alignment.offset.y = 20;
    return true;
}

// button_callbacks.lua:2840
static bool ev_reroll_boss_unlock(void *ctx) {
    tset(G->CONTROLLER->locks, "boss_reroll", NIL);
    return true;
}

// button_callbacks.lua:2818
static bool ev_reroll_boss_new(void *ctx) {
    UIBox *old = tobj(G->blind_select_opts, "boss");
    Node *par = old->moveable.node.parent;
    tsets(ttab(ttab(G->GAME, "round_resets"), "blind_choices"), "Boss", get_new_boss());

    UIBox_remove(old);
    // the Lua's argument order: the blind choice (it may poll 'orbital'), then the two colours
    Table *T_ = ARR(N(par->T->x), N(0), N(0), N(0));
    Table *blind_choice = create_UIBox_blind_choice("Boss", false);
    float *main_colour = get_blind_main_colour("Boss");
    float *back_colour = mix_colours(G->C.BLACK, get_blind_main_colour("Boss"), 0.8f, NULL);
    Table *definition = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(UIBox_dyn_container(ARR(T(blind_choice)), false, main_colour, back_colour, false, -1))
    )));
    UIBox *boss = UIBox_new(TAB("T", T(T_),
                                "definition", T(definition),
                                "config", T(TAB("align", S("bmi"),
                                                "offset", T(TAB("x", N(0), "y", N(G->ROOM->T->y + 9))),
                                                "major", O(par),
                                                "xy_bond", S("Weak")))));
    tseto(G->blind_select_opts, "boss", boss);
    tseto(par->config, "object", boss);
    UIBox_recalculate(boss);
    boss->moveable.node.parent = par;
    boss->moveable.alignment.offset.y = 0;

    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blocking = true, .trigger = "after", .delay = 0.5, .func = ev_reroll_boss_unlock}), NULL, false);

    save_run();
    tags_new_blind_choice();
    return true;
}

// button_callbacks.lua:2800
void FUNCS_reroll_boss(UIElement *e) {
    stop_use();
    tsetb(ttab(G->GAME, "round_resets"), "boss_rerolled", true);
    if (!G->from_boss_tag) ease_dollars(-10, false);
    G->from_boss_tag = false;
    tsetb(G->CONTROLLER->locks, "boss_reroll", true);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_reroll_boss_slide}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .func = ev_reroll_boss_new}), NULL, false);
}

// ---------------------------------------------------------------- reroll_shop
// button_callbacks.lua:2866
static bool ev_reroll_shop(void *ctx) {
    Table *current_round = ttab(G->GAME, "current_round");
    bool final_free = tnum(current_round, "free_rerolls") > 0;
    tsetn(current_round, "free_rerolls", lmax(tnum(current_round, "free_rerolls") - 1, 0));
    Table *times_rerolled = ttab(ttab(G->GAME, "round_scores"), "times_rerolled");
    tsetn(times_rerolled, "amt", tnum(times_rerolled, "amt") + 1);

    calculate_reroll_cost(final_free);
    for (int i = CardArea_n(G->shop_jokers); i >= 1; i--) {
        Card *c = CardArea_remove_card(G->shop_jokers, CardArea_card(G->shop_jokers, i), false);
        CALL(c, remove);
        c = NULL;
    }

    // save_run()

    play_sound("coin2", -1, -1);
    play_sound("other1", -1, -1);

    num count = tnum(ttab(G->GAME, "shop"), "joker_max") - CardArea_n(G->shop_jokers); // the bound is evaluated once
    for (num i = 1; i <= count; i++) {
        Card *new_shop_card = create_card_for_shop(G->shop_jokers);
        CardArea_emplace(G->shop_jokers, new_shop_card, NULL, false);
        CALL(new_shop_card, juice_up, -1, NIL);
    }
    return true;
}

// button_callbacks.lua:2896
static bool ev_reroll_shop_recall(void *ctx) {
    G->CONTROLLER->interrupt.focus = false;
    tsetb(G->CONTROLLER->locks, "shop_reroll", false);
    Controller_recall_cardarea_focus(G->CONTROLLER, S("shop_jokers"));
    int njokers = CardArea_n(G->jokers);
    for (int i = 1; i <= njokers; i++) {
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("reroll_shop", B(true)));
    }
    return true;
}

// button_callbacks.lua:2894
static bool ev_reroll_shop_after(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_reroll_shop_recall}), NULL, false);
    return true;
}

// button_callbacks.lua:2909
static bool ev_reroll_shop_save(void *ctx) {
    save_run();
    return true;
}

// button_callbacks.lua:2855
void FUNCS_reroll_shop(UIElement *e) {
    stop_use();
    tsetb(G->CONTROLLER->locks, "shop_reroll", true);
    if (Controller_save_cardarea_focus(G->CONTROLLER, "shop_jokers")) G->CONTROLLER->interrupt.focus = true;
    Table *current_round = ttab(G->GAME, "current_round");
    if (tnum(current_round, "reroll_cost") > 0) {
        inc_career_stat("c_shop_dollars_spent", tnum(current_round, "reroll_cost"));
        inc_career_stat("c_shop_rerolls", 1);
        ease_dollars(-tnum(current_round, "reroll_cost"), false);
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_reroll_shop}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .func = ev_reroll_shop_after}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_reroll_shop_save}), NULL, false);
}

// ---------------------------------------------------------------- cash_out
// button_callbacks.lua:2923
static bool ev_cash_out_shop(void *ctx) {
    if (G->round_eval) {
        UIBox_remove(G->round_eval);
        G->round_eval = NULL;
    }
    Table *current_round = ttab(G->GAME, "current_round");
    Table *round_resets = ttab(G->GAME, "round_resets");
    Table *round_bonus = ttab(G->GAME, "round_bonus");
    tsetn(current_round, "jokers_purchased", 0);
    tsetn(current_round, "discards_left", lmax(0, tnum(round_resets, "discards") + tnum(round_bonus, "discards")));
    tsetn(current_round, "hands_left", lmax(1, tnum(round_resets, "hands") + tnum(round_bonus, "next_hands")));
    G->STATE = STATES_SHOP;
    tset(G->GAME, "shop_free", NIL);
    tset(G->GAME, "shop_d6ed", NIL);
    G->STATE_COMPLETE = false;
    return true;
}

// button_callbacks.lua:2940
static bool ev_cash_out_prev_dollars(void *ctx) {
    tset(ttab(G->GAME, "previous_round"), "dollars", tget(G->GAME, "dollars"));
    return true;
}

// button_callbacks.lua:2912
void FUNCS_cash_out(UIElement *e) {
    stop_use();
    Table *round_resets = ttab(G->GAME, "round_resets");
    if (G->round_eval) {
        tset(CFG(e), "button", NIL);
        G->round_eval->moveable.alignment.offset.y = G->ROOM->T->y + 15;
        G->round_eval->moveable.alignment.offset.x = 0;
        CardArea_shuffle(G->deck, fmt("cashout%s", tostring(tget(round_resets, "ante"))));
        CardArea_hard_set_T(G->deck, NAN, NAN, NAN, NAN);
        delay(0.3, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_cash_out_shop}), NULL, false);
        ease_dollars(tnum(ttab(G->GAME, "current_round"), "dollars"), false);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_cash_out_prev_dollars}), NULL, false);
        play_sound("coin7", -1, -1);
        G->VIBRATION = G->VIBRATION + 1;
    }
    ease_chips(0);
    if (streq(tstr(ttab(round_resets, "blind_states"), "Boss"), "Defeated")) {
        tset(round_resets, "blind_ante", tget(round_resets, "ante"));
        tsets(ttab(round_resets, "blind_tags"), "Small", get_next_tag_key(NULL));
        tsets(ttab(round_resets, "blind_tags"), "Big", get_next_tag_key(NULL));
    }
    reset_blinds();
    delay(0.6, NULL);
}

// ---------------------------------------------------------------- run transitions
// button_callbacks.lua:2965, 2987, 3014
static bool ev_delete_run(void *ctx) {
    Game_delete_run(G);
    return true;
}

// button_callbacks.lua:2973
static bool ev_start_run(void *ctx) {
    Game_start_run(G, (Table *)ctx);
    return true;
}

// button_callbacks.lua:2958
void FUNCS_start_run(UIElement *e, Table *args) {
    tsetb(G->SETTINGS, "paused", true);
    if (e && veq(tget(CFG(e), "id"), S("restart_button"))) tset(G->GAME, "viewed_back", NIL);
    EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
    FUNCS_wipe_on(NULL, false, -1, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .func = ev_delete_run}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .no_delete = true, .func = ev_start_run, .ctx = args}), NULL, false);
    FUNCS_wipe_off();
}

// button_callbacks.lua:2996
static bool ev_main_menu(void *ctx) {
    Game_main_menu(G, "game");
    return true;
}

// button_callbacks.lua:2981
void FUNCS_go_to_menu(UIElement *e) {
    tsetb(G->SETTINGS, "paused", true);
    EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
    FUNCS_wipe_on(NULL, false, -1, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .func = ev_delete_run}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .not_blocking = true, .func = ev_main_menu}), NULL, false);
    FUNCS_wipe_off();
}

// button_callbacks.lua:3029
static bool ev_demo_cta_reset(void *ctx) {
    G->video_organ = NIL;
    G->normal_music_speed = false;
    return true;
}

// button_callbacks.lua:3023
static bool ev_demo_cta(void *ctx) {
    Game_demo_cta(G);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .no_delete = true, .not_blockable = true, .not_blocking = true,
                                             .func = ev_demo_cta_reset}), NULL, false);
    return true;
}

// button_callbacks.lua:3004
void FUNCS_go_to_demo_cta(UIElement *e) {
    tsetb(G->SETTINGS, "paused", true);
    EventManager_clear_queue(G->E_MANAGER, NULL, G->exception_queue);
    play_sound("explosion_buildup1", -1, 0.3f);
    play_sound("whoosh1", 0.7f, 0.8f);
    play_sound("introPad1", 0.704f, 0.8f);
    G->video_organ = N(0.6);
    FUNCS_wipe_on(NULL, true, -1, G->C.WHITE);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .func = ev_delete_run}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .not_blocking = true, .func = ev_demo_cta}), NULL, false);
    FUNCS_wipe_off();
}
