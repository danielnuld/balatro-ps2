// Test hook of the ui comparison (host only): called once per frame when HOOK=ui; the oracle runs the same steps
// from tools/hook_ui.lua (HOOK=<file> there), where the commands are listed. See tools/ui_run.sh.
#include "all.h"

static const char *AREAS[] = {"hand", "jokers", "consumeables", "shop_jokers", "shop_booster", "shop_vouchers", "pack_cards", "play", "deck", "discard"};

static const char *f2(float x) {
    const char *s = fmt("%.2f", x);
    return streq(s, "-0.00") ? "0.00" : s;
}

static const char *card_desc(Card *c) {
    if (streq(c->facing, "back")) return fmt("%s/back", tostring(tget(ttab(c->moveable.node.config, "center"), "key")));
    return fmt("%s/%s/%s%s/%s", tostring(tget(ttab(c->moveable.node.config, "center"), "key")), tostring(tget(c->base, "suit")),
               tostring(tget(c->base, "value")), c->debuff ? "/debuff" : "", c->facing ? c->facing : "nil");
}

// self.string of a DynaText: before the first align_letters the Lua holds the raw config.string[1]
static const char *dyna_string(DynaText *d) {
    if (d->string) return d->string;
    Val v = tgeti(ttab(d->moveable.node.config, "string"), 1);
    return v.t == V_STR ? v.s : "nil";
}

static const char *first_text(UIElement *e) {
    Table *c = e->moveable.node.config;
    if (truthy(tget(c, "text"))) return tostring(tget(c, "text"));
    void *o = tobj(c, "object");
    if (o && is_a(o, &DynaText_class)) return fmt("dyna:%s", dyna_string(o));
    for (int i = 1; truthy(tgeti(e->moveable.node.children, i)); i++) {
        const char *t = first_text(tgeti(e->moveable.node.children, i).p);
        if (t) return t;
    }
    return NULL;
}

static const char *desc(Node *t) {
    if (!t) return "nil";
    if (is_a(t, &Card_class)) {
        Card *fc = (Card *)t;
        const char *name = "?";
        for (int k = 0; k < 10; k++) {
            CardArea *fa = *Game_cardarea(G, AREAS[k]);
            if (fa && fa == fc->area) name = AREAS[k];
        }
        return fmt("card %s %s %s", name, fc->rank ? fmt("%d", fc->rank) : "nil", card_desc(fc));
    }
    if (is_a(t, &UIElement_class)) {
        const char *ft = first_text((UIElement *)t);
        return fmt("ui b=%s id=%s @%s,%s %s", tostring(tget(t->config, "button")), tostring(tget(t->config, "id")), f2(t->T->x), f2(t->T->y), ft ? ft : "nil");
    }
    return fmt("other @%s,%s", f2(t->T->x), f2(t->T->y));
}

static void dump_box(const char *p, const char *path, UIBox *box);

static void dump_el(const char *p, const char *path, UIElement *e) {
    Table *c = e->moveable.node.config;
    Transform *T = e->moveable.node.T;
    printf("%s%s n=%d %s %s %s %s", p, path, (int)e->UIT, f2(T->x), f2(T->y), f2(T->w), f2(T->h));
    if (truthy(tget(c, "text"))) printf(" text=%s", tostring(tget(c, "text")));
    if (truthy(tget(c, "scale"))) printf(" scale=%s", f2(tnum(c, "scale")));
    if (truthy(tget(c, "id"))) printf(" id=%s", tostring(tget(c, "id")));
    if (truthy(tget(c, "button"))) printf(" button=%s", tostring(tget(c, "button")));
    if (truthy(tget(c, "func"))) printf(" func=%s", tostring(tget(c, "func")));
    if (truthy(tget(c, "chosen"))) printf(" chosen=%s", tostring(tget(c, "chosen")));
    float *col = UI_colour(c, "colour");
    if (col && (e->UIT == UIT_T || truthy(tget(c, "button")))) printf(" col=%s,%s,%s,%s", f2(col[0]), f2(col[1]), f2(col[2]), f2(col[3]));
    void *o = tobj(c, "object");
    if (o) {
        if (is_a(o, &UIBox_class)) printf(" obj=UIBox");
        else if (is_a(o, &DynaText_class)) printf(" dyna=%s dscale=%s", dyna_string(o), f2(((DynaText *)o)->scale));
        else if (is_a(o, &Card_class)) printf(" card=%s", card_desc(o));
        else if (is_a(o, &CardArea_class)) {
            printf(" area=%d", CardArea_n(o));
            for (int i = 1; i <= CardArea_n(o); i++) printf(" %s", card_desc(CardArea_card(o, i)));
        } else printf(" obj=other");
    }
    printf("\n");
    if (o && is_a(o, &UIBox_class)) dump_box(p, fmt("%s.o", path), o);
    for (int i = 1; truthy(tgeti(e->moveable.node.children, i)); i++) dump_el(p, fmt("%s.%d", path, i), tgeti(e->moveable.node.children, i).p);
    static const char *named[] = {"button_pip", "alert", "h_popup"};
    for (int k = 0; k < 3; k++)
        if (tobj(e->moveable.node.children, named[k])) dump_box(p, fmt("%s.%s", path, named[k]), tobj(e->moveable.node.children, named[k]));
}

static void dump_box(const char *p, const char *path, UIBox *box) {
    if (!box || box == OVERLAY_MENU_TRUE || !is_a(box, &UIBox_class) || !box->UIRoot) { printf("%s%s none\n", p, path); return; }
    dump_el(p, path, box->UIRoot);
}

static void dump(const char *pre, Val v, int depth) {
    if (v.t == V_NUM) printf("%s=%.10g\n", pre, v.n);
    else if (v.t == V_STR) printf("%s=%s\n", pre, v.s);
    else if (v.t == V_BOOL) printf("%s=%s\n", pre, v.b ? "true" : "false");
    else if (v.t == V_OBJ) printf("%s=<obj>\n", pre);
    else if (v.t == V_TAB) {
        if (depth > 8) return;
        for (TIter it = titer(v.tab); tnext(&it);) {
            if (it.key.t == V_STR && strstr(it.key.s, "colour")) continue;
            dump(fmt("%s.%s", pre, tostring(it.key)), it.val, depth + 1);
        }
    }
}

static bool is_tab(UIElement *e) {
    Val id = tget(e->moveable.node.config, "id");
    return id.t == V_STR && !strncmp(id.s, "tab_but_", 8) && streq(tstr(e->moveable.node.config, "button"), "change_tab");
}
static bool is_cycle_r(UIElement *e) {
    return streq(tstr(e->moveable.node.config, "button"), "option_cycle") && veq(tget(e->moveable.node.config, "ref_value"), S("r"));
}
// the elements of a tree that pass pred, in the order dump_el prints them
static Table *collect(UIElement *e, bool (*pred)(UIElement *), Table *out) {
    if (pred(e)) tinsert(out, O(e));
    void *o = tobj(e->moveable.node.config, "object");
    if (o && is_a(o, &UIBox_class)) collect(((UIBox *)o)->UIRoot, pred, out);
    for (int i = 1; truthy(tgeti(e->moveable.node.children, i)); i++) collect(tgeti(e->moveable.node.children, i).p, pred, out);
    return out;
}

static UIBox *box_named(const char *n) {
    struct { const char *name; UIBox *box; } boxes[] = {
        {"OVERLAY_MENU", G->OVERLAY_MENU}, {"HUD", G->HUD}, {"HUD_blind", G->HUD_blind}, {"buttons", G->buttons},
        {"deck_preview", G->deck_preview}, {"shop", G->shop}, {"blind_select", G->blind_select}, {"round_eval", G->round_eval},
        {"booster_pack", G->booster_pack}, {"MAIN_MENU_UI", G->MAIN_MENU_UI}, {"PROFILE_BUTTON", G->PROFILE_BUTTON},
        {"OVERLAY_TUTORIAL", G->OVERLAY_TUTORIAL}, {"achievement_notification", G->achievement_notification},
        {"blind_prompt_box", G->blind_prompt_box}, {"boss_warning_text", G->boss_warning_text}};
    for (int i = 0; i < (int)(sizeof boxes / sizeof *boxes); i++)
        if (streq(n, boxes[i].name)) return boxes[i].box;
    printf("ERROR hook_ui: unknown box %s\n", n);
    exit(1);
}

static Card *at(CardArea *area, const char *i) { return CardArea_card(area, atoi(i) ? atoi(i) : CardArea_n(area)); }

static UIElement *fake_e(void) {
    UIElement *e = NEW(UIElement);
    e->moveable.node.config = tnew();
    return e;
}

static void step(int frame, char *cmd) {
    char *a[16] = {0};
    int n = 0;
    printf("STEP %d %s state %d\n", frame, cmd, G->STATE);
    for (char *w = strtok(cmd, ":"); w && n < 16; w = strtok(NULL, ":")) a[n++] = w;
    const char *c = a[0];
    CardArea *area = n > 1 && !streq(c, "ui") && !streq(c, "f") && !streq(c, "fa") && !streq(c, "cyc") && !streq(c, "popall") && Game_cardarea(G, a[1]) ? *Game_cardarea(G, a[1]) : NULL;
    Card *card = area && n > 2 ? at(area, a[2]) : NULL;
    if (streq(c, "start")) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL);
        if (a[3]) {
            Back *back = NEW(Back);
            Back_init(back, ttab(G->P_CENTERS, a[3]));
            tseto(G->GAME, "viewed_back", back);
        }
        FUNCS_start_run(NULL, TAB("stake", N(a[2] ? atof(a[2]) : 1), "seed", S(a[1])));
    } else if (streq(c, "blind")) {
        const char *on_deck = tstr(G->GAME, "blind_on_deck");
        const char *key = streq(on_deck, "Small") ? "small" : streq(on_deck, "Big") ? "big" : "boss";
        FUNCS_select_blind(UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, key), S("select_blind_button"), NULL));
    } else if (streq(c, "win")) {
        tsetn(G->GAME, "chips", ((Blind *)tobj(G->GAME, "blind"))->chips);
        G->STATE = STATES_HAND_PLAYED;
        G->STATE_COMPLETE = false;
    } else if (streq(c, "lose")) {
        G->STATE = STATES_GAME_OVER;
        G->STATE_COMPLETE = false;
    } else if (streq(c, "wingame")) {
        win_game();
    } else if (streq(c, "cash")) {
        FUNCS_cash_out(fake_e());
    } else if (streq(c, "money")) {
        ease_dollars(atof(a[1]), true);
    } else if (streq(c, "chips")) {
        tsetn(G->GAME, "chips", atof(a[1]));
    } else if (streq(c, "handlvl")) {
        for (char *q = a[1]; *q; q++) if (*q == '_') *q = ' ';
        level_up_hand(NULL, a[1], true, N(atof(a[2])));
    } else if (streq(c, "give")) {
        Table *center = ttab(G->P_CENTERS, a[1]);
        CardArea *to = streq(tstr(center, "set"), "Joker") ? G->jokers : G->consumeables;
        Card *nc = create_card(tstr(center, "set"), to, false, NIL, false, false, a[1], NULL);
        Card_add_to_deck(nc, false);
        CardArea_emplace(to, nc, NULL, false);
    } else if (streq(c, "ed")) {
        Card_set_edition(card, TAB(a[3], B(true)), true, true);
    } else if (streq(c, "stick")) {
        if (streq(a[3], "eternal")) Card_set_eternal(card, true);
        else if (streq(a[3], "perishable")) Card_set_perishable(card, true);
        else Card_set_rental(card, true);
    } else if (streq(c, "debuff")) {
        Card_set_debuff(card, true);
    } else if (streq(c, "hl")) {
        for (int i = 2; i < n; i++) CardArea_add_to_highlighted(area, at(area, a[i]), true);
    } else if (streq(c, "play")) {
        FUNCS_play_cards_from_highlighted(NULL);
    } else if (streq(c, "discard")) {
        FUNCS_discard_cards_from_highlighted(NULL, false);
    } else if (streq(c, "hover")) {
        Card_hover(card);
    } else if (streq(c, "unhover")) {
        Card_stop_hover(card);
    } else if (streq(c, "ui")) {
        dump_box(fmt("U %d %s ", frame, a[1]), "r", box_named(a[1]));
    } else if (streq(c, "areaui")) { // areaui:<area>:<child>: a box a card area hangs from itself
        dump_box(fmt("U %d areaui %s %s ", frame, a[1], a[2]), "r", area ? tobj(((Node *)area)->children, a[2]) : NULL);
    } else if (streq(c, "roundtext")) { // G.GAME.current_round.round_text (no box shows it)
        printf("U %d roundtext [%s]\n", frame, tostring(tget(ttab(G->GAME, "current_round"), "round_text")));
    } else if (streq(c, "pop")) {
        Node *t = a[1] ? (Node *)card : G->CONTROLLER->focused.target;
        dump_box(fmt("U %d pop ", frame), "r", t ? tobj(t->children, "h_popup") : NULL);
    } else if (streq(c, "popall")) {
        const char *v = a[2];
        Table *pool = ttab(G->P_CENTER_POOLS, a[1]);
        bool playing = streq(a[1], "Enhanced") || streq(a[1], "Default");
        for (int i = 1; i <= tlen(pool); i++) {
            Table *center = tgeti(pool, i).tab;
            Card *nc = NEW(Card);
            Card_init(nc, 5, 5, G->CARD_W, G->CARD_H, playing ? ttab(G->P_CARDS, "H_A") : NULL, center, TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
            if (streq(v, "foil") || streq(v, "holo") || streq(v, "polychrome") || streq(v, "negative")) Card_set_edition(nc, TAB(v, B(true)), true, true);
            else if (streq(v, "eternal")) Card_set_eternal(nc, true);
            else if (streq(v, "perishable")) Card_set_perishable(nc, true);
            else if (streq(v, "rental")) Card_set_rental(nc, true);
            else if (streq(v, "debuff")) Card_set_debuff(nc, true);
            else if (v) Card_set_seal(nc, v, true, true);
            Card_hover(nc);
            dump_box(fmt("U %d pop %s ", frame, tstr(center, "key")), "r", tobj(nc->moveable.node.children, "h_popup"));
            Card_stop_hover(nc);
            Card_remove(nc);
        }
    } else if (streq(c, "unlockall")) {
        Table *ts[] = {G->P_CENTERS, G->P_BLINDS, G->P_TAGS};
        for (int k = 0; k < 3; k++)
            for (TIter it = titer(ts[k]); tnext(&it);) {
                tsetb(it.val.tab, "discovered", true);
                tsetb(it.val.tab, "unlocked", true);
                tsetb(it.val.tab, "alerted", true);
            }
    } else if (streq(c, "notut")) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL);
    } else if (streq(c, "order")) {
        printf("U %d order %s", frame, a[1]);
        for (int i = 1; i <= CardArea_n(area); i++) printf(" %s%s", card_desc(CardArea_card(area, i)), CardArea_card(area, i)->highlighted ? "*" : "");
        printf("\n");
    } else if (streq(c, "tagpop")) {
        Table *pool = ttab(G->P_CENTER_POOLS, "Tag");
        for (int i = 1; i <= tlen(pool); i++) {
            const char *key = tstr(tgeti(pool, i).tab, "key");
            Tag *tag = NEW(Tag);
            Sprite *sprite = NULL;
            Tag_init(tag, key, true, NULL);
            Tag_generate_UI(tag, -1, &sprite);
            CALL(sprite, hover);
            dump_box(fmt("U %d pop %s ", frame, key), "r", tobj(((Node *)sprite)->children, "h_popup"));
            CALL(sprite, stop_hover);
            CALL(sprite, remove);
        }
    } else if (streq(c, "nf")) {
        static const num vs[] = {0, 1.5, 12.34, 123.456, 999, 1000, 12345.6, 1e6, 1234567, 12345678, 1e10, 99999999999, 1e11, 1.2345e11, 9.9995e11, 1e12, 1.5e13, 123456789e9, 1e15, 1e20, 1.797e308, 9007199254740992};
        for (int i = 0; i < (int)(sizeof vs / sizeof *vs); i++) {
            printf("U %d nf %.17g %s %.4f %.4f %.4f", frame, vs[i], number_format(N(vs[i])), score_number_scale(1, N(vs[i])), scale_number(N(vs[i]), 1, -1), scale_number(N(vs[i]), 1, 1000000));
            puts("");
        }
    } else if (streq(c, "discover")) {
        Table *t = ttab(G->P_CENTERS, a[1]);
        discover_card(t ? t : ttab(G->P_BLINDS, a[1]) ? ttab(G->P_BLINDS, a[1]) : ttab(G->P_TAGS, a[1]));
    } else if (streq(c, "unlock")) {
        unlock_card(ttab(G->P_CENTERS, a[1]));
    } else if (streq(c, "chal")) {
        UIElement *e = fake_e();
        tsetn(e->moveable.node.config, "id", atof(a[1]));
        FUNCS_change_challenge_description(e);
    } else if (streq(c, "fa")) {
        Table *back = ttabi(ttab(G->P_CENTER_POOLS, "Back"), atoi(a[2]));
        funcs_lookup_args(a[1])(TAB("to_key", N(atof(a[2])), "to_val", back ? tget(back, "name") : NIL));
    } else if (streq(c, "unseed")) {
        tset(G->GAME, "seeded", NIL);
    } else if (streq(c, "tabs")) {
        Table *list = collect(G->OVERLAY_MENU->UIRoot, is_tab, tnew());
        for (int i = 1; i <= tlen(list); i++) {
            UIElement *e = tgeti(list, i).p;
            FUNCS_change_tab(e);
            dump_box(fmt("U %d tab %s ", frame, tstr(e->moveable.node.config, "id")), "r", G->OVERLAY_MENU);
        }
    } else if (streq(c, "cyc")) {
        for (int j = 1; j <= atoi(a[2]); j++) {
            Table *list = collect(G->OVERLAY_MENU->UIRoot, is_cycle_r, tnew());
            if (!truthy(tgeti(list, atoi(a[1])))) break;
            FUNCS_option_cycle(tgeti(list, atoi(a[1])).p);
            dump_box(fmt("U %d cyc %d ", frame, j), "r", G->OVERLAY_MENU);
        }
    } else if (streq(c, "settings")) {
        dump(fmt("D %d SETTINGS", frame), T(G->SETTINGS), 0);
    } else if (streq(c, "f")) {
        funcs_lookup(a[1])(fake_e());
    } else {
        printf("ERROR hook_ui: unknown command %s\n", c);
        exit(1);
    }
}

void hook_ui(int frame) {
    static const char *last;
    const char *steps = getenv("STEPS");
    if (steps) {
        char *copy = strdup(steps), *save = NULL;
        for (char *w = strtok_r(copy, " ", &save); w; w = strtok_r(NULL, " ", &save)) {
            char *colon = strchr(w, ':');
            if (colon && atoi(w) == frame) step(frame, colon + 1);
        }
    }
    const char *d = desc(G->CONTROLLER->focused.target);
    if (!streq(d, last)) {
        last = d;
        printf("F %d focus=%s\n", frame, d);
    }
}
