// functions/button_callbacks.lua:1-648: the HUD / shop predicates, the profile and unlock-all buttons, the alerts
// and pips, the generic widgets (toggle, slider, option cycle) and the first cycle callbacks.
#include "all.h"

// e.config, e.children
static inline Table *cfg_of(void *node) { return ((Node *)node)->config; }
static inline Table *children_of(void *node) { return ((Node *)node)->children; }

// "disable(e)": e.config.colour = G.C.UI.BACKGROUND_INACTIVE; e.config.button = nil
static void disable_button(UIElement *e) {
    tset(cfg_of(e), "colour", P(G->C.UI.BACKGROUND_INACTIVE));
    tset(cfg_of(e), "button", NIL);
}
// e.config.colour = colour; e.config.button = button
static void enable_button(UIElement *e, float *colour, const char *button) {
    tset(cfg_of(e), "colour", P(colour));
    tsets(cfg_of(e), "button", button);
}

// ref_table[ref_value] = v for a widget's binding. note: ref_table is a table, or an object whose bound fields live
// in its dyn table (ref_table = G for 'run_setup_seed', game.h "in dyn:"); ui_ref_get is the matching read.
static void ref_set(Val ref_table, Val ref_value, Val v) {
    if (ref_table.t == V_TAB) tsetv(ref_table.tab, ref_value, v);
    else if (ref_table.t == V_OBJ) tsetv(((Object *)ref_table.obj)->dyn, ref_value, v);
    else ERROR("attempt to index a nil ref_table");
}

// G.FILE_HANDLER.force = true
static void file_handler_force(void) {
    if (!G->FILE_HANDLER) ERROR("attempt to index field 'FILE_HANDLER' (a nil value)");
    tsetb(G->FILE_HANDLER, "force", true);
}

// button_callbacks.lua:5
void FUNCS_tut_next(UIElement *e) {
    if (G->OVERLAY_TUTORIAL) {
        Table *dyn = G->OVERLAY_TUTORIAL->moveable.node.object.dyn;
        Card_Character_remove_button((Card_Character *)tobj(dyn, "Jimbo"));
        Card_Character_remove_speech_bubble((Card_Character *)tobj(dyn, "Jimbo"));
        tsetb(dyn, "step_complete", false);
        tsetn(dyn, "step", tnum(dyn, "step") + 1);
    }
}

// button_callbacks.lua:20
void FUNCS_blueprint_compat(UIElement *e) {
    Table *ability = ((Card *)tobj(cfg_of(e), "ref_table"))->ability;
    if (!veq(tget(ability, "blueprint_compat"), tget(ability, "blueprint_compat_check"))) {
        if (streq(tstr(ability, "blueprint_compat"), "compatible")) {
            tset(cfg_of(e), "colour", P(mix_colours(G->C.GREEN, G->C.JOKER_GREY, 0.8f, NULL)));
        } else if (streq(tstr(ability, "blueprint_compat"), "incompatible")) {
            tset(cfg_of(e), "colour", P(mix_colours(G->C.RED, G->C.JOKER_GREY, 0.8f, NULL)));
        }
        tsets(ability, "blueprint_compat_ui",
              fmt(" %s ", vstr(localize(S(fmt("k_%s", vstr(tget(ability, "blueprint_compat")))), NULL))));
        tset(ability, "blueprint_compat_check", tget(ability, "blueprint_compat"));
    }
}

// button_callbacks.lua:36
void FUNCS_sort_hand_suit(UIElement *e) {
    CardArea_sort(G->hand, "suit desc");
    play_sound("paper1", -1, -1);
}

// button_callbacks.lua:45
void FUNCS_sort_hand_value(UIElement *e) {
    CardArea_sort(G->hand, "desc");
    play_sound("paper1", -1, -1);
}

// button_callbacks.lua:55
void FUNCS_can_buy(UIElement *e) {
    Card *card = (Card *)tobj(cfg_of(e), "ref_table");
    if ((card->cost > tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at")) && (card->cost > 0)) {
        disable_button(e);
    } else {
        enable_button(e, G->C.ORANGE, "buy_from_shop");
    }
    Node *ref_parent = (Node *)tobj(cfg_of(e), "ref_parent");
    if (ref_parent && tobj(ref_parent->children, "buy_and_use")) {
        if (((Node *)tobj(ref_parent->children, "buy_and_use"))->states.visible) {
            e->UIBox->moveable.alignment.offset.y = -0.6f;
        } else {
            e->UIBox->moveable.alignment.offset.y = 0;
        }
    }
}

// button_callbacks.lua:77
void FUNCS_can_buy_and_use(UIElement *e) {
    Card *card = (Card *)tobj(cfg_of(e), "ref_table");
    if (((card->cost > tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at")) && (card->cost > 0)) ||
        (!Card_can_use_consumeable(card, false, false))) {
        e->UIBox->moveable.node.states.visible = false;
        disable_button(e);
    } else {
        if (card->highlighted) {
            e->UIBox->moveable.node.states.visible = true;
        }
        enable_button(e, G->C.SECONDARY_SET.Voucher, "buy_from_shop");
    }
}

// button_callbacks.lua:96
void FUNCS_can_redeem(UIElement *e) {
    Card *card = (Card *)tobj(cfg_of(e), "ref_table");
    if (card->cost > tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at")) {
        disable_button(e);
    } else {
        enable_button(e, G->C.GREEN, "use_card");
    }
}

// button_callbacks.lua:111
void FUNCS_can_open(UIElement *e) {
    Card *card = (Card *)tobj(cfg_of(e), "ref_table");
    if (card->cost > 0 && (card->cost > tnum(G->GAME, "dollars") - tnum(G->GAME, "bankrupt_at"))) {
        disable_button(e);
    } else {
        enable_button(e, G->C.GREEN, "use_card");
    }
}

// button_callbacks.lua:125
void FUNCS_HUD_blind_visible(UIElement *e) {
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    if (blind && (!streq(blind->name, "") && blind->blind_set)) {
        blind->moveable.node.states.visible = true;
        Blind_sync_states(blind);
    } else if (blind) {
        blind->moveable.node.states.visible = false;
        Blind_sync_states(blind);
    }
}

// button_callbacks.lua:138
void FUNCS_HUD_blind_debuff(UIElement *e) {
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    Table *config = cfg_of(e), *pconfig = cfg_of(e->moveable.node.parent);
    if (blind && blind->loc_debuff_text && !streq(blind->loc_debuff_text, "")) {
        if (veq(tget(pconfig, "minh"), N(0)) || !veq(tget(config, "prev_loc"), S(blind->loc_debuff_text))) {
            tsetn(pconfig, "minh", 0.35);
            tsetn(config, "scale", 0.36);
            if (veq(tgetv(blind->loc_debuff_lines, tget(config, "ref_value")), S(""))) {
                tsetn(config, "scale", 0.0);
                tsetn(pconfig, "minh", 0.001);
            }
            tsets(config, "prev_loc", blind->loc_debuff_text);
            UIBox_recalculate(e->UIBox);
        }
    } else {
        if (tnum(pconfig, "minh") > 0) {
            tsetn(pconfig, "minh", 0);
            tsetn(config, "scale", 0);
            UIBox_recalculate(e->UIBox);
        }
    }
}

// button_callbacks.lua:160
void FUNCS_HUD_blind_debuff_prefix(UIElement *e) {
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    Table *config = cfg_of(e);
    if ((blind && streq(blind->name, "The Wheel") && !blind->disabled) || veq(tget(config, "id"), S("bl_wheel"))) {
        tsets(ttab(config, "ref_table"), "val", tostring(tpath(G->GAME, "probabilities.normal")));
        tsetn(config, "scale", 0.32);
    } else {
        tsets(ttab(config, "ref_table"), "val", "");
        tsetn(config, "scale", 0);
    }
}

// button_callbacks.lua:171
void FUNCS_HUD_blind_reward(UIElement *e) {
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    Table *config = cfg_of(e);
    Val no_blind_reward = tpath(G->GAME, "modifiers.no_blind_reward");
    const char *type = truthy(no_blind_reward) && blind ? Blind_get_type(blind) : NULL;
    if (truthy(no_blind_reward) && (blind && type && truthy(tget(no_blind_reward.tab, type)))) {
        if (tnum(config, "minh") > 0.44) {
            tsetn(config, "minh", 0.4);
            tset(cfg_of(tobji(children_of(e), 1)), "text", localize(S("k_no_reward"), NULL));
            // e.children[2].states.visible = false (commented out in the Lua)
            UIBox_recalculate(e->UIBox);
        }
    } else {
        if (tnum(config, "minh") < 0.45) {
            tsetn(config, "minh", 0.45);
            tsets(cfg_of(tobji(children_of(e), 1)), "text", fmt("%s: ", vstr(localize(S("k_reward"), NULL))));
            ((Node *)tobji(children_of(e), 2))->states.visible = true;
            UIBox_recalculate(e->UIBox);
        }
    }
}

// button_callbacks.lua:193
bool FUNCS_can_continue(UIElement *e) {
    if (!e || truthy(tget(cfg_of(e), "func"))) { // refers to this function, so this doesn't run repeatedly
        bool _can_continue = false;
        const char *path = fmt("%s/save.jkr", tostring(tget(G->SETTINGS, "profile")));
        bool savefile = love_filesystem_getInfo(path, NULL);
        if (!savefile) {
            if (e) disable_button(e);
        } else {
            if (!G->SAVED_GAME) {
                const char *packed = get_compressed(path);
                if (packed != NULL) G->SAVED_GAME = STR_UNPACK(packed);
            }
            // note: a save that exists but does not read leaves G.SAVED_GAME nil and the Lua fails indexing it; here
            // its VERSION reads as nil: cannot continue (spec 2.2).
            Val version = tget(G->SAVED_GAME, "VERSION");
            // a string comparison (parity trap 2)
            if (!truthy(version) || strcmp(vstr(version), "0.9.2") < 0) {
                if (e) disable_button(e);
            } else {
                _can_continue = true;
            }
        }
        if (e) tset(cfg_of(e), "func", NIL);
        return _can_continue;
    }
    return false;
}

// button_callbacks.lua:217
void FUNCS_can_load_profile(UIElement *e) {
    if (veq(tget(G->SETTINGS, "profile"), N(G->focused_profile))) {
        disable_button(e);
    } else {
        enable_button(e, G->C.BLUE, "load_profile");
    }
}

struct load_profile_ctx { bool delete_prof_data; };
// button_callbacks.lua:233
static bool ev_load_profile_reload(void *ctx) {
    struct load_profile_ctx *c = ctx;
    Game_delete_run(G);
    Val _name = NIL;
    Val name = tget(ttabi(G->PROFILES, G->focused_profile), "name");
    if (truthy(name) && !veq(name, S(""))) {
        _name = name;
    }
    if (c->delete_prof_data) tseti(G->PROFILES, G->focused_profile, T(tnew()));
    G->DISCOVER_TALLIES = NULL;
    G->PROGRESS = NULL;
    Game_load_profile(G, G->focused_profile);
    tset(ttabi(G->PROFILES, G->focused_profile), "name", _name);
    Game_init_item_prototypes(G);
    return true;
}
// button_callbacks.lua:252
static bool ev_load_profile_menu(void *ctx) {
    Game_main_menu(G, NULL);
    file_handler_force();
    return true;
}

// button_callbacks.lua:227
void FUNCS_load_profile(bool delete_prof_data) {
    G->SAVED_GAME = NULL;
    EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
    FUNCS_wipe_on(NULL, false, -1, NULL);
    struct load_profile_ctx *c = NEWCTX(struct load_profile_ctx);
    c->delete_prof_data = delete_prof_data;
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .func = ev_load_profile_reload, .ctx = c}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .not_blocking = true, .func = ev_load_profile_menu}), NULL, false);
    FUNCS_wipe_off();
}

// button_callbacks.lua:261
void FUNCS_can_delete_profile(UIElement *e) {
    G->CHECK_PROFILE_DATA = G->CHECK_PROFILE_DATA || love_filesystem_getInfo(fmt("%d/profile.jkr", G->focused_profile), NULL);
    if ((!G->CHECK_PROFILE_DATA) || tbool(cfg_of(e), "disable_button")) {
        G->CHECK_PROFILE_DATA = false;
        disable_button(e);
    } else {
        enable_button(e, G->C.RED, "delete_profile");
    }
}

// button_callbacks.lua:280, :328
static bool ev_confirm_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}
// button_callbacks.lua:283, :331 ctx: the button e
static bool ev_confirm_rearm(void *ctx) {
    tset(cfg_of(ctx), "disable_button", NIL);
    return true;
}
// The tail both two-press confirms share (button_callbacks.lua:279-286, :327-334)
static void confirm_first_press(UIElement *e) {
    tsetb(cfg_of(e), "disable_button", true);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.06, .not_blockable = true, .not_blocking = true, .func = ev_confirm_sound}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.35, .not_blockable = true, .not_blocking = true, .func = ev_confirm_rearm, .ctx = e}), NULL, false);

    play_sound("tarot2", 1, 0.4f);
}

// button_callbacks.lua:273
void FUNCS_delete_profile(UIElement *e) {
    UIElement *warning_text = UIBox_get_UIE_by_ID(e->UIBox, S("warning_text"), NULL);
    if (UI_colour(cfg_of(warning_text), "colour") != G->C.WHITE) { // by reference, as the Lua
        UIElement_juice_up(warning_text, -1, NIL);
        tset(cfg_of(warning_text), "colour", P(G->C.WHITE));
        tsetb(cfg_of(warning_text), "shadow", true);
        confirm_first_press(e);
    } else {
        love_filesystem_remove(fmt("%d/profile.jkr", G->focused_profile));
        love_filesystem_remove(fmt("%d/save.jkr", G->focused_profile));
        love_filesystem_remove(fmt("%d/meta.jkr", G->focused_profile));
        love_filesystem_remove(fmt("%d/unlock_notify.jkr", G->focused_profile));
        love_filesystem_remove(fmt("%d", G->focused_profile));
        G->SAVED_GAME = NULL;
        G->DISCOVER_TALLIES = NULL;
        G->PROGRESS = NULL;
        tseti(G->PROFILES, G->focused_profile, T(tnew()));
        if (veq(N(G->focused_profile), tget(G->SETTINGS, "profile"))) {
            FUNCS_load_profile(true);
        } else {
            UIElement *tab_but = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S(fmt("tab_but_%d", G->focused_profile)), NULL);
            FUNCS_change_tab(tab_but);
        }
    }
}

// button_callbacks.lua:306
void FUNCS_can_unlock_all(UIElement *e) {
    if (tbool(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "all_unlocked") || tbool(cfg_of(e), "disable_button")) {
        disable_button(e);
    } else {
        enable_button(e, G->C.GREY, "unlock_all");
    }
}

// button_callbacks.lua:337-357: one of the three loops
static void unlock_all_of(Table *prototypes) {
    for (TIter it = titer(prototypes); tnext(&it);) {
        Table *v = it.val.tab;
        if (!tbool(v, "demo") && !tbool(v, "wip")) {
            tsetb(v, "alerted", true);
            tsetb(v, "discovered", true);
            tsetb(v, "unlocked", true);
        }
    }
}

// button_callbacks.lua:316
void FUNCS_unlock_all(UIElement *e) {
    UIElement *_infotip_object = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("overlay_menu_infotip"), NULL);

    if ((!tbool(cfg_of(_infotip_object), "set")) && (!G->F_NO_ACHIEVEMENTS)) {
        CALL(tobj(cfg_of(_infotip_object), "object"), remove);
        Val text_rows = localize(S(G->F_TROPHIES ? "ml_unlock_all_trophies" : "ml_unlock_all_explanation"), NULL);
        UIBox *object = UIBox_new(TAB(
            "definition", T(overlay_infotip(text_rows.t == V_TAB ? text_rows.tab : NULL)),
            "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("bm"), "parent", O(_infotip_object)))));
        tseto(cfg_of(_infotip_object), "object", object);
        UIElement_juice_up(object->UIRoot, -1, NIL);
        tsetb(cfg_of(_infotip_object), "set", true);
        confirm_first_press(e);
    } else {
        tsetb(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "all_unlocked", true);
        unlock_all_of(G->P_CENTERS);
        unlock_all_of(G->P_BLINDS);
        unlock_all_of(G->P_TAGS);
        set_profile_progress();
        set_discover_tallies();
        Game_save_progress(G);
        file_handler_force();

        UIElement *tab_but = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S(fmt("tab_but_%d", G->focused_profile)), NULL);
        FUNCS_change_tab(tab_but);
    }
}

// button_callbacks.lua:372
void FUNCS_high_score_alert(UIElement *e) {
    Val id = tget(cfg_of(e), "id");
    if (truthy(id) && !tobj(children_of(e), "alert")) {
        Val score = tgetv(ttab(G->GAME, "round_scores"), id);
        if (truthy(score) && tbool(score.tab, "high_score")) {
            UIBox *alert = UIBox_new(TAB(
                "definition", T(create_UIBox_card_alert(TAB("no_bg", B(true), "text", localize(S("k_high_score_ex"), NULL), "scale", N(0.3)))),
                "config", T(TAB(
                    "instance_type", S("ALERT"),
                    "align", S("tri"),
                    "offset", T(TAB("x", N(0.3), "y", N(-0.18))),
                    "major", O(e), "parent", O(e)))));
            tseto(children_of(e), "alert", alert);
            alert->moveable.node.states.collide.can = false;
        }
    }
}

// button_callbacks.lua:388
void FUNCS_beta_lang_alert(UIElement *e) {
    if (!tobj(children_of(e), "alert")) {
        Val ref_table = tget(cfg_of(e), "ref_table");
        if (truthy(ref_table) && tbool(ref_table.tab, "beta")) {
            UIBox *alert = UIBox_new(TAB(
                "definition", T(create_UIBox_card_alert(TAB("no_bg", B(true), "text", S(lit("beta_lang_alert", 1)), "scale", N(0.35)))),
                "config", T(TAB(
                    "instance_type", S("ALERT"),
                    "align", S("tri"),
                    "offset", T(TAB("x", N(0.07), "y", N(-0.07))),
                    "major", O(e), "parent", O(e)))));
            tseto(children_of(e), "alert", alert);
            alert->moveable.node.states.collide.can = false;
        }
    }
}

// button_callbacks.lua:408
void FUNCS_set_button_pip(UIElement *e) {
    Table *focus_args = ttab(cfg_of(e), "focus_args");
    if (G->CONTROLLER->HID.controller && focus_args && !tobj(children_of(e), "button_pip")) {
        Val align = tget(focus_args, "orientation");
        if (!truthy(align)) align = S("cr");
        // focus_args.offset or (focus_args.orientation == 'bm' and {x = 0, y = 0.02} or {x = 0.1, y = 0.02})
        Val offset = tget(focus_args, "offset");
        if (!truthy(offset)) {
            offset = veq(tget(focus_args, "orientation"), S("bm")) ? T(TAB("x", N(0), "y", N(0.02))) : T(TAB("x", N(0.1), "y", N(0.02)));
        }
        UIBox *button_pip = UIBox_new(TAB(
            "definition", T(create_button_binding_pip(TAB("button", tget(focus_args, "button"), "scale", tget(focus_args, "scale")))),
            "config", T(TAB(
                "align", align,
                "offset", offset,
                "major", O(e), "parent", O(e)))));
        tseto(children_of(e), "button_pip", button_pip);
        button_pip->moveable.node.states.collide.can = false;
    }
    if (!G->CONTROLLER->HID.controller && tobj(children_of(e), "button_pip")) {
        CALL(tobj(children_of(e), "button_pip"), remove);
        tset(children_of(e), "button_pip", NIL);
    }
}

// button_callbacks.lua:429
void FUNCS_flash(UIElement *e) {
    Table *config = cfg_of(e);
    if (G->CONTROLLER->text_input_hook) {
        if (lmod(floor(G->TIMERS.REAL * 2), 2) == 1) {
            UI_colour(config, "colour")[3] = 0;
        } else {
            UI_colour(config, "colour")[3] = 1;
        }
        if (!veq(tget(config, "w"), N(0.1))) {
            tsetn(config, "w", 0.1);
            UIBox_recalculate(e->UIBox);
        }
    } else {
        UI_colour(config, "colour")[3] = 0;
        if (!veq(tget(config, "w"), N(0))) {
            tsetn(config, "w", 0);
            UIBox_recalculate(e->UIBox);
        }
    }
}

// button_callbacks.lua:447
void FUNCS_pip_dynatext(UIElement *e) {
    Table *config = cfg_of(e);
    DynaText *ref_table = (DynaText *)tobj(config, "ref_table");
    if (veq(S(fmt("pip_%d", ref_table->focused_string)), tget(config, "id"))) {
        if (!veq(tget(config, "pip_state"), N(1))) {
            tset(config, "colour", tget(config, "pipcol1"));
            tsetn(config, "pip_state", 1);
        }
    } else if (!veq(tget(config, "pip_state"), N(2))) {
        tset(config, "colour", tget(config, "pipcol2"));
        tsetn(config, "pip_state", 2);
    }
}

// button_callbacks.lua:463
void FUNCS_toggle_button(UIElement *e) {
    Table *rt = ttab(cfg_of(e), "ref_table");
    ref_set(tget(rt, "ref_table"), tget(rt, "ref_value"), B(!truthy(ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value")))));
    if (truthy(tget(cfg_of(e), "toggle_callback"))) {
        Val arg = ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value"));
        fcall(tget(cfg_of(e), "toggle_callback"), 1, &arg);
    }
}

// button_callbacks.lua:474
void FUNCS_toggle(UIElement *e) {
    Table *config = cfg_of(e);
    Table *rt = ttab(config, "ref_table");
    Val value = ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value"));
    if (!truthy(value) && tbool(config, "toggle_active")) {
        tset(config, "toggle_active", NIL);
        tset(config, "colour", tget(rt, "inactive_colour"));
        Node *child = (Node *)tobji(children_of(e), 1);
        child->states.visible = false;
        ((Node *)tobj(child->config, "object"))->states.visible = false;
    } else if (truthy(value) && !tbool(config, "toggle_active")) {
        tsetb(config, "toggle_active", true);
        tset(config, "colour", tget(rt, "active_colour"));
        Node *child = (Node *)tobji(children_of(e), 1);
        child->states.visible = true;
        ((Node *)tobj(child->config, "object"))->states.visible = true;
    }
}

// button_callbacks.lua:505-507, :525-527: the slider's text and bar follow its new value
static void slider_show(Node *c, Table *rt, num value) {
    tsets(rt, "text", fmt("%.*f", (int)tnum(rt, "decimal_places"), value));
    c->T->w = (value - tnum(rt, "min")) / (tnum(rt, "max") - tnum(rt, "min")) * tnum(rt, "w");
    tsetn(c->config, "w", c->T->w);
}

// button_callbacks.lua:496
void FUNCS_slider(UIElement *e) {
    Node *c = (Node *)tobji(children_of(e), 1);
    e->moveable.node.states.drag.can = true;
    c->states.drag.can = true;
    if (G->CONTROLLER && G->CONTROLLER->dragging.target &&
        (G->CONTROLLER->dragging.target == (Node *)e ||
         G->CONTROLLER->dragging.target == c)) {
        Table *rt = ttab(c->config, "ref_table");
        num value = lmin(tnum(rt, "max"), lmax(tnum(rt, "min"), tnum(rt, "min") + (tnum(rt, "max") - tnum(rt, "min")) *
            ((num)G->CURSOR->moveable.node.T->x - (num)e->moveable.node.parent->T->x - (num)G->ROOM->T->x) / (num)e->moveable.node.T->w));
        ref_set(tget(rt, "ref_table"), tget(rt, "ref_value"), N(value));
        slider_show(c, rt, value);
        if (truthy(tget(rt, "callback"))) {
            FuncsArgsFn callback = funcs_lookup_args(tstr(rt, "callback"));
            if (!callback) ERROR("attempt to call G.FUNCS.%s (a nil value)", tstr(rt, "callback"));
            callback(rt);
        }
    }
}

// button_callbacks.lua:518
void FUNCS_slider_descreet(UIElement *e, num per) {
    Node *c = (Node *)tobji(children_of(e), 1);
    e->moveable.node.states.drag.can = true;
    c->states.drag.can = true;
    // (if per: every caller passes a number)
    Table *rt = ttab(c->config, "ref_table");
    num value = lmin(tnum(rt, "max"), lmax(tnum(rt, "min"),
        vnum(ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value"))) + per * (tnum(rt, "max") - tnum(rt, "min"))));
    ref_set(tget(rt, "ref_table"), tget(rt, "ref_value"), N(value));
    slider_show(c, rt, value);
}

// button_callbacks.lua:546 ctx: cycle_main
static bool ev_option_cycle_hover(void *ctx) {
    CALL(ctx, hover);
    return true;
}

// button_callbacks.lua:537
void FUNCS_option_cycle(UIElement *e) {
    Table *config = cfg_of(e);
    Table *ref_table = ttab(config, "ref_table");
    Table *options = ttab(ref_table, "options");
    Node *start = e->moveable.node.parent->parent;
    Val from_val = tgeti(options, (int)tnum(ref_table, "current_option"));
    Val from_key = tget(ref_table, "current_option");
    UIElement *old_pip = UIBox_get_UIE_by_ID(e->UIBox, S(fmt("pip_%s", tostring(tget(ref_table, "current_option")))), start);
    UIElement *cycle_main = UIBox_get_UIE_by_ID(e->UIBox, S("cycle_main"), start);

    if (cycle_main && truthy(tget(cfg_of(cycle_main), "h_popup"))) {
        CALL(cycle_main, stop_hover);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_option_cycle_hover, .ctx = cycle_main}), NULL, false);
    }

    if (veq(tget(config, "ref_value"), S("l"))) {
        // cycle left
        tsetn(ref_table, "current_option", tnum(ref_table, "current_option") - 1);
        if (tnum(ref_table, "current_option") <= 0) tsetn(ref_table, "current_option", tlen(options));
    } else {
        // cycle right
        tsetn(ref_table, "current_option", tnum(ref_table, "current_option") + 1);
        if (tnum(ref_table, "current_option") > tlen(options)) tsetn(ref_table, "current_option", 1);
    }
    Val to_val = tgeti(options, (int)tnum(ref_table, "current_option"));
    Val to_key = tget(ref_table, "current_option");
    tset(ref_table, "current_option_val", tgeti(options, (int)tnum(ref_table, "current_option")));

    UIElement *new_pip = UIBox_get_UIE_by_ID(e->UIBox, S(fmt("pip_%s", tostring(tget(ref_table, "current_option")))), start);

    if (old_pip) tset(cfg_of(old_pip), "colour", P(G->C.BLACK));
    if (new_pip) tset(cfg_of(new_pip), "colour", P(G->C.WHITE));

    if (truthy(tget(ref_table, "opt_callback"))) {
        FuncsArgsFn opt_callback = funcs_lookup_args(tstr(ref_table, "opt_callback"));
        if (!opt_callback) ERROR("attempt to call G.FUNCS.%s (a nil value)", tstr(ref_table, "opt_callback"));
        opt_callback(TAB(
            "from_val", from_val,
            "to_val", to_val,
            "from_key", from_key,
            "to_key", to_key,
            "cycle_config", T(ref_table)));
    }
}

// ---------------------------------------------------------------- cycle callbacks

// button_callbacks.lua:591
void FUNCS_test_framework_cycle_callback(Table *args) {
    if (!args) args = tnew();
    Table *cycle_config = ttab(args, "cycle_config");
    if (cycle_config && truthy(tget(cycle_config, "ref_table")) && truthy(tget(cycle_config, "ref_value"))) {
        ref_set(tget(cycle_config, "ref_table"), tget(cycle_config, "ref_value"), tget(args, "to_val"));
    }
}

// button_callbacks.lua:604-610, :629-635: every card of G.your_collection is removed
static void your_collection_clear(Table *your_collection) {
    for (int j = 1; j <= tlen(your_collection); j++) {
        CardArea *area = (CardArea *)tobji(your_collection, j);
        for (int i = CardArea_n(area); i >= 1; i--) {
            Card *c = CardArea_remove_card(area, CardArea_card(area, i), false);
            CALL(c, remove);
            c = NULL;
        }
    }
}

// button_callbacks.lua:602
void FUNCS_your_collection_joker_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    Table *your_collection = G->your_collection.tab;
    your_collection_clear(your_collection);
    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= tlen(your_collection); j++) {
            CardArea *area = (CardArea *)tobji(your_collection, j);
            Table *center = ttabi(ttab(G->P_CENTER_POOLS, "Joker"),
                (int)(i + (j - 1) * 5 + (5 * tlen(your_collection) * (tnum(ttab(args, "cycle_config"), "current_option") - 1))));
            if (!center) break;
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H,
                      ttab(G->P_CARDS, "empty"), center, NULL);
            card->sticker = vstr(get_joker_win_sticker(center, false));
            CardArea_emplace(area, card, NULL, false);
        }
    }
    INIT_COLLECTION_CARD_ALERTS();
}

// button_callbacks.lua:627
void FUNCS_your_collection_tarot_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    Table *your_collection = G->your_collection.tab;
    your_collection_clear(your_collection);

    for (int j = 1; j <= tlen(your_collection); j++) {
        CardArea *area = (CardArea *)tobji(your_collection, j);
        for (int i = 1; i <= 4 + j; i++) {
            Table *center = ttabi(ttab(G->P_CENTER_POOLS, "Tarot"),
                (int)(i + (j - 1) * (5) + (11 * (tnum(ttab(args, "cycle_config"), "current_option") - 1))));
            if (!center) break;
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H,
                      ttab(G->P_CARDS, "empty"), center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }
    INIT_COLLECTION_CARD_ALERTS();
}
