// Test hook of the jokers comparison (host only): called once per frame when HOOK=jokers; the oracle runs the same steps
// from tools/hook_jokers.lua (HOOK=<file> there), which documents the environment (CASE, CONS, SEED, ACT<i>, STAKE).
// Every step prints "JK <step> ..." lines; tools/jokers.sh diffs them against the oracle's.
#include "all.h"

static struct { int step, act, idle, last, r2; const char *phase; num hc, hm; } H = {.phase = "boot"};

// split(s, sep): the non-empty pieces; returns the count (at most max)
static int split(const char *s, char sep, const char **out, int max) {
    int n = 0;
    while (s && *s && n < max) {
        const char *e = strchr(s, sep);
        int len = e ? (int)(e - s) : (int)strlen(s);
        if (len > 0) out[n++] = fmt("%.*s", len, s);
        if (!e) break;
        s = e + 1;
    }
    return n;
}
static const char *fv(Val v) {
    if (v.t == V_NUM) return fmt("%.10g", v.n);
    if (v.t == V_BOOL) return v.b ? "true" : "false";
    if (v.t == V_STR) return v.s;
    return NULL;
}
static const char *fn(num n) { return fmt("%.10g", n); }
static const char *ts(Val v) { return v.t == V_NUM ? fmt("%.14g", v.n) : tostring(v); }
static int cmpstr(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
// the string keys of t, sorted
static int sorted_keys(Table *t, const char **keys, int max) {
    int n = 0;
    for (TIter it = titer(t); tnext(&it);)
        if (it.key.t == V_STR && n < max) keys[n++] = it.key.s;
    qsort(keys, n, sizeof *keys, cmpstr);
    return n;
}
static const char *dump(Table *t, const char *prefix, const char *out) {
    const char *keys[256];
    int n = sorted_keys(t, keys, 256);
    for (int i = 0; i < n; i++) {
        Val v = tget(t, keys[i]);
        if (v.t == V_TAB) {
            if (streq(keys[i], "extra")) out = dump(v.tab, fmt("%s%s.", prefix, keys[i]), out);
        } else if (fv(v)) out = fmt("%s%s%s%s=%s", out, *out ? "," : "", prefix, keys[i], fv(v));
    }
    return out;
}
static bool is_edition(const char *p) { return streq(p, "foil") || streq(p, "holo") || streq(p, "polychrome") || streq(p, "negative"); }
static void apply(Card *card, const char *spec) {
    const char *parts[16];
    int n = split(spec, '+', parts, 16);
    if (n && ttab(G->P_CARDS, parts[0])) Card_set_base(card, ttab(G->P_CARDS, parts[0]), false);
    for (int i = 1; i < n; i++) {
        const char *p = parts[i];
        if (ttab(G->P_CENTERS, p)) Card_set_ability(card, ttab(G->P_CENTERS, p), false, true);
        else if (is_edition(p)) Card_set_edition(card, TAB(p, B(true)), true, true);
        else if (streq(p, "eternal")) Card_set_eternal(card, true);
        else if (streq(p, "perishable")) Card_set_perishable(card, true);
        else if (streq(p, "rental")) Card_set_rental(card, true);
        else Card_set_seal(card, p, true, true);
    }
}
static Table *cfg(Card *c) { return c->moveable.node.config; }
static const char *edsuffix(Card *c) { return c->edition ? fmt(":%s", ts(tget(c->edition, "type"))) : ""; }
static const char *cardstr(Card *c) {
    const char *suit = tstr(c->base, "suit");
    const char *s = fmt("%c%s", suit ? suit[0] : '?', ts(N(tnum(c->base, "id"))));
    if (!streq(tstr(cfg(c), "center_key"), "c_base")) s = fmt("%s:%s", s, ts(tget(cfg(c), "center_key")));
    s = fmt("%s%s", s, edsuffix(c));
    if (c->seal) s = fmt("%s:%s", s, c->seal);
    if (tnum(c->ability, "perma_bonus") != 0) s = fmt("%s:pb%s", s, fn(tnum(c->ability, "perma_bonus")));
    if (c->debuff) s = fmt("%s:D", s);
    return s;
}
static const char *areasum(CardArea *a) {
    num s = 0;
    for (int i = 1; i <= CardArea_n(a); i++) {
        Card *c = CardArea_card(a, i);
        const char *suit = tstr(c->base, "suit");
        s += i * (tnum(c->base, "id") * 100 + (unsigned char)(suit ? suit[0] : '?'));
    }
    return fmt("%d/%s", CardArea_n(a), ts(N(s)));
}
static void summary(const char *label, int frame) {
    Table *g = G->GAME, *cr = ttab(g, "current_round"), *rr = ttab(g, "round_resets");
    num pr = 0;
    const char *keys[512];
    Table *prt = ttab(g, "pseudorandom");
    int nk = sorted_keys(prt, keys, 512);
    for (int i = 0; i < nk; i++)
        if (tget(prt, keys[i]).t == V_NUM) pr += tnum(prt, keys[i]);
    H.step++;
    const char *p = fmt("JK %d %s ", H.step, label);
    printf("%sA st=%d chips=%s hc=%s hm=%s $=%s round=%s ante=%s hands=%s/%s disc=%s/%s hsize=%s jslots=%s cslots=%s deck=%s discard=%s all=%d"
           " prob=%s icap=%s iamt=%s disc%%=%s bankrupt=%s reroll=%s free=%s played=%s dused=%s skips=%s last=%s idol=%s/%s mail=%s"
           " ancient=%s castle=%s pr=%s f=%d\n",
           p, (int)G->STATE, fn(tnum(g, "chips")), fn(H.hc), fn(H.hm), fn(tnum(g, "dollars")), ts(tget(g, "round")), ts(tget(rr, "ante")),
           ts(tget(cr, "hands_left")), ts(tget(rr, "hands")), ts(tget(cr, "discards_left")), ts(tget(rr, "discards")),
           ts(tget(G->hand->moveable.node.config, "card_limit")), ts(tget(G->jokers->moveable.node.config, "card_limit")),
           ts(tget(G->consumeables->moveable.node.config, "card_limit")), areasum(G->deck), areasum(G->discard), tlen(G->playing_cards),
           fn(tnum(ttab(g, "probabilities"), "normal")), fn(tnum(g, "interest_cap")), fn(tnum(g, "interest_amount")), fn(tnum(g, "discount_percent")),
           fn(tnum(g, "bankrupt_at")), fn(tnum(cr, "reroll_cost")), fn(tnum(cr, "free_rerolls")), fn(tnum(cr, "hands_played")),
           fn(tnum(cr, "discards_used")), fn(tnum(g, "skips")), tostring(tget(g, "last_hand_played")), tostring(tget(ttab(cr, "idol_card"), "rank")),
           tostring(tget(ttab(cr, "idol_card"), "suit")), tostring(tget(ttab(cr, "mail_card"), "rank")), tostring(tget(ttab(cr, "ancient_card"), "suit")),
           tostring(tget(ttab(cr, "castle_card"), "suit")), fn(pr), frame);
    for (int i = 1; i <= CardArea_n(G->jokers); i++) {
        Card *c = CardArea_card(G->jokers, i);
        printf("%sJ%d %s ed=%s sell=%s cost=%s debuff=%s %s\n", p, i, tostring(tget(cfg(c), "center_key")),
               c->edition ? tostring(tget(c->edition, "type")) : "nil", fn(c->sell_cost), fn(c->cost), c->debuff ? "true" : "false", dump(c->ability, "", ""));
    }
    const char *t = "";
    for (int i = 1; i <= CardArea_n(G->consumeables); i++) {
        Card *c = CardArea_card(G->consumeables, i);
        t = fmt("%s%s%s%s", t, *t ? "," : "", tostring(tget(cfg(c), "center_key")), edsuffix(c));
    }
    printf("%sC %s\n", p, t);
    t = "";
    for (int i = 1; i <= CardArea_n(G->hand); i++) t = fmt("%s%s%s", t, *t ? " " : "", cardstr(CardArea_card(G->hand, i)));
    printf("%sH %s\n", p, t);
    t = "";
    for (int i = 1; i <= tlen(G->handlist); i++) {
        Table *v = ttab(ttab(g, "hands"), tgeti(G->handlist, i).s);
        t = fmt("%s%s%s:%sx%s:%s", t, *t ? " " : "", fn(tnum(v, "level")), fn(tnum(v, "chips")), fn(tnum(v, "mult")), fn(tnum(v, "played")));
    }
    printf("%sL %s\n", p, t);
    if (G->shop_jokers && G->shop_jokers->cards) {
        t = "";
        CardArea *areas[3] = {G->shop_jokers, G->shop_vouchers, G->shop_booster};
        for (int a = 0; a < 3; a++)
            for (int i = 1; i <= CardArea_n(areas[a]); i++) {
                Card *c = CardArea_card(areas[a], i);
                t = fmt("%s%s%s%s$%s", t, *t ? " " : "", tostring(tget(cfg(c), "center_key")), edsuffix(c), fn(c->cost));
            }
        printf("%sS %s\n", p, t);
    }
    fflush(stdout);
    H.hc = H.hm = 0;
}
static void finish(const char *msg) {
    printf("JK END %s st=%d\n", msg, (int)G->STATE);
    fflush(stdout);
    exit(0);
}

// the actions of round 1, in order; a discard with none left is skipped, the last hand is forced to win
static const char *ACTS[] = {
    "P:H_A,H_2,H_3,H_4,H_5", "D:H_J,C_J,D_K", "U", "P:S_K,C_K,D_K,H_Q,C_Q|S_K,S_Q", "D:#1", "U", "P:S_T,H_T,D_T,C_T", "D:#5", "P:S_6,D_6,C_9",
    "P:#5", "P:#5", "P:#5", "P:#5", "P:#5", "P:#5",
};
static const char *act(int i) {
    const char *e = getenv(fmt("ACT%d", i));
    return e ? e : i < (int)(sizeof ACTS / sizeof *ACTS) ? ACTS[i] : "P:#5";
}

// rewrites and highlights the cards of an action; returns false if there are no cards
static bool pick(const char *spec) {
    const char *parts[2], *cs[16];
    int np = split(spec, '|', parts, 2);
    int n = CardArea_n(G->hand);
    if (np > 1) {
        int nh = split(parts[1], ',', cs, 16);
        for (int i = 1; i <= nh; i++)
            if (CardArea_card(G->hand, n - nh + i)) apply(CardArea_card(G->hand, n - nh + i), cs[i - 1]);
    }
    const char *list = np > 0 ? parts[0] : "#5";
    int count;
    if (list[0] == '#') count = atoi(list + 1);
    else {
        count = split(list, ',', cs, 16);
        for (int i = 1; i <= count; i++)
            if (CardArea_card(G->hand, i)) apply(CardArea_card(G->hand, i), cs[i - 1]);
    }
    Card *picked[16];
    int npicked = 0;
    for (int i = 1; i <= (count < n ? count : n); i++) picked[npicked++] = CardArea_card(G->hand, i);
    for (int i = 0; i < npicked; i++) CardArea_add_to_highlighted(G->hand, picked[i], false);
    return npicked > 0;
}
static const char *env(const char *name, const char *def) { return getenv(name) ? getenv(name) : def; }

void hook_jokers(int frame) {
    if (frame == 60) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL);
        tsetn(G->SETTINGS, "GAMESPEED", 4);
        FUNCS_start_run(NULL, TAB("stake", N(atoi(env("STAKE", "1"))), "seed", S(env("SEED", "JOKERS"))));
        H.phase = "select1";
        H.last = frame;
        return;
    }
    if (frame < 60 || !G->GAME || !ttab(G->GAME, "current_round") || !G->hand) return;
    Table *g = G->GAME, *cr = ttab(g, "current_round");

    Table *ch = ttab(cr, "current_hand");
    if (ch && tget(ch, "chips").t == V_NUM && tnum(ch, "chips") != 0) H.hc = tnum(ch, "chips");
    if (ch && tget(ch, "mult").t == V_NUM && tnum(ch, "mult") != 0) H.hm = tnum(ch, "mult");

    if (G->STATE == STATES_GAME_OVER) summary("gameover", frame), finish("gameover");
    int busy = 0;
    Table *base = ttab(G->E_MANAGER->queues, "base");
    for (int i = 1; i <= tlen(base); i++)
        if (((Event *)tgeti(base, i).obj)->blocking) busy++;
    if (frame - H.last > 8000)
        summary("timeout", frame), finish(fmt("timeout %s complete=%s locked=%s", H.phase, G->STATE_COMPLETE ? "true" : "false", G->CONTROLLER->locked ? "true" : "false"));

    // ready: the state has settled and nothing is queued, for 10 frames
    if (busy == 0 && G->STATE_COMPLETE && !G->CONTROLLER->locked) H.idle++; else H.idle = 0;
    int st = G->STATE;
    bool did = true;
    // the round evaluation keeps an event queued: its cash out button, for 30 frames, is the signal there
    static int evalf;
    UIElement *btn = NULL;
    if (st == STATES_ROUND_EVAL && G->round_eval) // the button is in a UIBox of its own (common_events.lua:1069)
        for (int i = 1; i <= tlen(G->I.UIBOX) && !btn; i++) btn = UIBox_get_UIE_by_ID(tgeti(G->I.UIBOX, i).obj, S("cash_out_button"), NULL);
    if (btn) evalf++; else evalf = 0;
    if (streq(H.phase, "round1") && btn && evalf >= 30) {
        summary("eval", frame);
        FUNCS_cash_out(btn);
        H.phase = "shop";
        H.idle = 0, H.last = frame;
        return;
    }
    if (streq(H.phase, "round2") && btn && evalf >= 30) summary("eval2", frame), finish("done");
    if (H.idle < 10) return;

    if (streq(H.phase, "select1") && st == STATES_BLIND_SELECT && G->blind_select) {
        tsetn(g, "dollars", atof(env("DOLLARS", "20")));
        tsetn(G->consumeables->moveable.node.config, "card_limit", 3);
        const char *specs[32], *parts[16];
        int n = split(env("CASE", ""), ',', specs, 32);
        for (int i = 0; i < n; i++) {
            split(specs[i], '+', parts, 16);
            Card *card = create_card("Joker", G->jokers, false, NIL, false, false, parts[0], NULL);
            apply(card, specs[i]);
            Card_add_to_deck(card, false);
            CardArea_emplace(G->jokers, card, NULL, false);
        }
        n = split(env("CONS", "c_pluto,c_hermit,c_strength"), ',', specs, 32);
        for (int i = 0; i < n; i++) {
            Card *card = create_card("Tarot_Planet", G->consumeables, false, NIL, false, false, specs[i], NULL);
            Card_add_to_deck(card, false);
            CardArea_emplace(G->consumeables, card, NULL, false);
        }
        summary("start", frame);
        FUNCS_select_blind(UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, "small"), S("select_blind_button"), NULL));
        H.phase = "round1";
    } else if (streq(H.phase, "round1") && st == STATES_SELECTING_HAND && CardArea_n(G->hand) > 0) {
        Blind *blind = tobj(g, "blind");
        if (H.act == 0) blind->chips = atof(env("BLIND", "1e9"));
        summary(fmt("r1a%d", H.act), frame);
        for (;;) {
            const char *a = act(H.act);
            H.act++;
            char kind = a[0];
            if (tnum(cr, "hands_left") <= 1 && kind != 'P') {
                // the last hand is played at once
            } else if (kind == 'U') {
                Card *c = CardArea_card(G->consumeables, 1);
                if (c && Card_can_use_consumeable(c, false, false)) { FUNCS_use_card(NULL, c, false, false); break; }
            } else if (kind == 'D') {
                if (tnum(cr, "discards_left") > 0 && pick(a + 2)) { FUNCS_discard_cards_from_highlighted(NULL, false); break; }
            } else {
                if (tnum(cr, "hands_left") <= 1 && !getenv("NOWIN")) tsetn(g, "chips", blind->chips);
                pick(a + 2);
                FUNCS_play_cards_from_highlighted(NULL);
                break;
            }
        }
    } else if (streq(H.phase, "shop") && st == STATES_SHOP && G->shop) {
        summary("shop", frame);
        Card *c = CardArea_card(G->consumeables, CardArea_n(G->consumeables));
        if (getenv("SELLJ")) c = CardArea_card(G->jokers, atoi(getenv("SELLJ")));
        if (c) {
            UIElement *e = calloc(1, sizeof *e); // the Lua hook's {config = {ref_table = c}}
            e->moveable.node.config = TAB("ref_table", O(c));
            FUNCS_sell_card(e);
        }
        H.phase = "shop2";
    } else if (streq(H.phase, "shop2") && st == STATES_SHOP && G->shop) {
        summary("sold", frame);
        if (tnum(g, "dollars") - tnum(g, "bankrupt_at") >= tnum(cr, "reroll_cost")) FUNCS_reroll_shop(NULL);
        H.phase = "shop3";
    } else if (streq(H.phase, "shop3") && st == STATES_SHOP && G->shop) {
        summary("rerolled", frame);
        FUNCS_toggle_shop(NULL);
        H.phase = "select2";
    } else if (streq(H.phase, "select2") && st == STATES_BLIND_SELECT && G->blind_select) {
        summary("select2", frame);
        UIElement *e = UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, "big"), S("select_blind_button"), NULL);
        if (getenv("SKIP")) {
            FUNCS_skip_blind(e);
            H.phase = "select3";
        } else {
            FUNCS_select_blind(e);
            H.phase = "round2";
        }
    } else if (streq(H.phase, "select3") && st == STATES_BLIND_SELECT && G->blind_select) {
        summary("select3", frame);
        FUNCS_select_blind(UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, "boss"), S("select_blind_button"), NULL));
        H.phase = "round2";
    } else if (streq(H.phase, "round2") && st == STATES_SELECTING_HAND && CardArea_n(G->hand) > 0) {
        summary(fmt("r2a%d", H.r2), frame);
        H.r2++;
        if (H.r2 == 1) {
            ((Blind *)tobj(g, "blind"))->chips = 1e9;
            pick("#5");
            FUNCS_play_cards_from_highlighted(NULL);
        } else if (H.r2 == 2 && tnum(cr, "discards_left") > 0) {
            pick("#2");
            FUNCS_discard_cards_from_highlighted(NULL, false);
        } else finish("done");
    } else did = false;
    if (did) H.idle = 0, H.last = frame;
}
