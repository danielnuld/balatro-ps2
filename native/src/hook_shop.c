// Test hook of the shop comparison (host only): called once per frame when HOOK=shop; the oracle runs the same steps
// from tools/hook_shop.lua (HOOK=<file> there). See tools/oracle_hook.sh and tools/shop_run.sh.
// STEPS="1250:start:SEED 1500:blind ..." in the environment: the commands are listed in tools/hook_shop.lua.
#include "all.h"

static const char *AREAS[] = {"hand", "jokers", "consumeables", "shop_jokers", "shop_booster", "shop_vouchers", "pack_cards", "play", "deck", "discard"};

static void dump(const char *pre, Val v, int depth) {
    if (v.t == V_NUM) printf("%s=%.10g\n", pre, v.n);
    else if (v.t == V_STR) printf("%s=%s\n", pre, v.s);
    else if (v.t == V_BOOL) printf("%s=%s\n", pre, v.b ? "true" : "false");
    else if (v.t == V_OBJ) printf("%s=<obj>\n", pre);
    else if (v.t == V_TAB) {
        if (depth > 8) return;
        for (TIter it = titer(v.tab); tnext(&it);) {
            if (it.key.t == V_STR && (strstr(it.key.s, "colour") || streq(it.key.s, "example"))) continue;
            dump(fmt("%s.%s", pre, tostring(it.key)), it.val, depth + 1);
        }
    }
}

static void dump_all(int frame) {
    const char *p = fmt("D %d ", frame);
    printf("%sSTATE=%d\n", p, G->STATE);
    dump(fmt("%sGAME", p), T(G->GAME), 0);
    for (int k = 0; k < 10; k++) {
        const char *name = AREAS[k];
        CardArea *a = *Game_cardarea(G, name);
        if (!a || !a->cards) continue;
        printf("%s%s.n=%d\n", p, name, CardArea_n(a));
        printf("%s%s.card_limit=%s\n", p, name, tostring(tget(a->moveable.node.config, "card_limit")));
        printf("%s%s.highlighted=%d\n", p, name, CardArea_n_highlighted(a));
        if (k < 8) {
            for (int i = 1; i <= CardArea_n(a); i++) {
                Card *c = CardArea_card(a, i);
                const char *q = fmt("%s%s.%d", p, name, i);
                printf("%s.center=%s\n", q, tostring(tget(ttab(c->moveable.node.config, "center"), "key")));
                dump(fmt("%s.ability", q), T(c->ability), 0);
                dump(fmt("%s.base", q), T(c->base), 0);
                dump(fmt("%s.edition", q), T(c->edition), 0);
                printf("%s.seal=%s\n", q, c->seal ? c->seal : "nil");
                printf("%s.cost=%s\n", q, number_tostring(c->cost));
                printf("%s.sell_cost=%s\n", q, number_tostring(c->sell_cost));
                printf("%s.debuff=%s\n", q, c->debuff ? "true" : "false");
                printf("%s.facing=%s\n", q, c->facing ? c->facing : "nil");
            }
        } else {
            printf("%s%s.cards=", p, name);
            for (int i = 1; i <= CardArea_n(a); i++) {
                Card *c = CardArea_card(a, i);
                const char *key = tstr(ttab(c->moveable.node.config, "center"), "key");
                printf(" %c%s%s", tstr(c->base, "suit") ? tstr(c->base, "suit")[0] : 'n', tostring(tget(c->base, "id")), streq(key, "c_base") ? "" : key);
            }
            printf("\n");
        }
    }
}

// area.cards[i], i 0: the last card
static Card *at(CardArea *area, const char *i) { return CardArea_card(area, atoi(i) ? atoi(i) : CardArea_n(area)); }

// the Lua's made-up {config = {ref_table = card}}
static UIElement *fake_e(Card *card) {
    UIElement *e = NEW(UIElement);
    e->moveable.node.config = card ? TAB("ref_table", O(card)) : tnew();
    return e;
}

static void step(int frame, char *cmd) {
    char *a[16] = {0};
    int n = 0;
    printf("STEP %d %s state %d\n", frame, cmd, G->STATE);
    for (char *w = strtok(cmd, ":"); w && n < 16; w = strtok(NULL, ":")) a[n++] = w;
    const char *c = a[0];
    CardArea *area = n > 1 && Game_cardarea(G, a[1]) ? *Game_cardarea(G, a[1]) : NULL;
    Card *card = area && n > 2 ? at(area, a[2]) : NULL;
    Table *params = TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true));
    if (streq(c, "start")) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL);
        if (a[3]) { // the deck, as the run setup screen sets it
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
    } else if (streq(c, "cash")) {
        FUNCS_cash_out(fake_e(NULL));
    } else if (streq(c, "money")) {
        ease_dollars(atof(a[1]), true);
    } else if (streq(c, "set")) {
        tsetn(G->GAME, a[1], atof(a[2]));
    } else if (streq(c, "shop")) {
        Table *center = ttab(G->P_CENTERS, a[1]);
        const char *set = tstr(center, "set");
        if (streq(set, "Booster")) {
            Transform *t = G->shop_booster->moveable.node.T;
            Card *nc = NEW(Card);
            Card_init(nc, t->x + t->w / 2, t->y, G->CARD_W * 1.27, G->CARD_H * 1.27, NULL, center, params);
            create_shop_card_ui(nc, "Booster", G->shop_booster);
            Card_start_materialize(nc, NULL, false, -1);
            CardArea_emplace(G->shop_booster, nc, NULL, false);
        } else if (streq(set, "Voucher")) {
            Table *cfg = G->shop_vouchers->moveable.node.config;
            tsetn(cfg, "card_limit", tnum(cfg, "card_limit") + 1);
            Transform *t = G->shop_vouchers->moveable.node.T;
            Card *nc = NEW(Card);
            Card_init(nc, t->x + t->w / 2, t->y, G->CARD_W, G->CARD_H, NULL, center, params);
            create_shop_card_ui(nc, "Voucher", G->shop_vouchers);
            Card_start_materialize(nc, NULL, false, -1);
            CardArea_emplace(G->shop_vouchers, nc, NULL, false);
        } else {
            Transform *t = G->shop_jokers->moveable.node.T;
            Card *nc = NEW(Card);
            Card_init(nc, t->x + t->w / 2, t->y, G->CARD_W, G->CARD_H, a[2] ? ttab(G->P_CARDS, a[2]) : NULL, center, params);
            create_shop_card_ui(nc, set, G->shop_jokers);
            Card_start_materialize(nc, NULL, false, -1);
            CardArea_emplace(G->shop_jokers, nc, NULL, false);
        }
    } else if (streq(c, "give")) {
        Table *center = ttab(G->P_CENTERS, a[1]);
        CardArea *to = streq(tstr(center, "set"), "Joker") ? G->jokers : G->consumeables;
        Card *nc = create_card(tstr(center, "set"), to, false, NIL, false, false, a[1], NULL);
        Card_add_to_deck(nc, false);
        CardArea_emplace(to, nc, NULL, false);
    } else if (streq(c, "buy")) {
        FUNCS_buy_from_shop(fake_e(card));
    } else if (streq(c, "use")) {
        if (truthy(tget(card->ability, "consumeable")) && !Card_can_use_consumeable(card, false, false)) printf("D %d use=no\n", frame);
        else FUNCS_use_card(NULL, card, false, false);
    } else if (streq(c, "sell")) {
        FUNCS_sell_card(fake_e(card));
    } else if (streq(c, "canuse")) {
        printf("D %d canuse=%s\n", frame, truthy(tget(card->ability, "consumeable")) && Card_can_use_consumeable(card, false, false) ? "true" : "false");
    } else if (streq(c, "hl")) {
        for (int i = 2; i < n; i++) CardArea_add_to_highlighted(area, at(area, a[i]), true);
    } else if (streq(c, "unhl")) {
        CardArea_unhighlight_all(area);
    } else if (streq(c, "reroll")) {
        FUNCS_reroll_shop(NULL);
    } else if (streq(c, "skip")) {
        if (G->booster_pack) FUNCS_skip_booster(NULL);
    } else if (streq(c, "leave")) {
        FUNCS_toggle_shop(NULL);
    } else if (streq(c, "chips")) { // the score the blind asks for
        ((Blind *)tobj(G->GAME, "blind"))->chips = atof(a[1]);
    } else if (streq(c, "play")) {
        FUNCS_play_cards_from_highlighted(NULL);
    } else if (streq(c, "discard")) {
        FUNCS_discard_cards_from_highlighted(NULL, false);
    } else if (streq(c, "hover")) { // the card's popup (its description), as the cursor makes it
        Card_hover(card);
    } else if (streq(c, "unhover")) {
        Card_stop_hover(card);
    } else if (streq(c, "focus")) { // what the pad's cursor is on
        Node *t = G->CONTROLLER->focused.target;
        const char *s = "nil";
        if (t && is_a(t, &Card_class)) {
            Card *fc = (Card *)t;
            const char *name = "?";
            for (int k = 0; k < 10; k++) {
                CardArea *fa = *Game_cardarea(G, AREAS[k]);
                if (fa && fa == fc->area) name = AREAS[k];
            }
            s = fmt("card %s %d %s", name, fc->rank, tostring(tget(ttab(t->config, "center"), "key")));
        } else if (t && is_a(t, &UIElement_class)) {
            s = fmt("ui %s %s", tostring(tget(t->config, "button")), tostring(tget(t->config, "id")));
        } else if (t) s = "other";
        printf("D %d focus=%s\n", frame, s);
    } else if (streq(c, "dump")) {
        dump_all(frame);
    } else {
        printf("ERROR hook_shop: unknown command %s\n", c);
        exit(1);
    }
}

void hook_shop(int frame) {
    const char *steps = getenv("STEPS");
    if (!steps) return;
    char *copy = strdup(steps), *save = NULL;
    for (char *w = strtok_r(copy, " ", &save); w; w = strtok_r(NULL, " ", &save)) {
        char *colon = strchr(w, ':');
        if (colon && atoi(w) == frame) step(frame, colon + 1);
    }
    free(copy);
}
