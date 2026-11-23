// Test hook of the blinds comparison (host only): called once per frame when HOOK=blinds; the oracle runs the same steps
// from tools/hook_blinds.lua (HOOK=<file> there). See tools/oracle_hook.sh. Keep the two files line for line alike.
#include "all.h"

static const char *env(const char *k) {
    const char *v = getenv(k);
    return v && *v ? v : NULL;
}
static struct { int st, since, blinds, step, shop, dec; bool setup, started, won, over, info; } H = {.st = -1, .shop = -1};

static const char *n(Val x) { return x.t == V_NIL ? "nil" : fmt("%.10g", x.n); }
static const char *s_or_nil(const char *s) { return s ? s : "nil"; }
static const char *cards(CardArea *area) {
    const char *s = "";
    if (area && area->cards)
        for (int i = 1; i <= CardArea_n(area); i++) {
            Card *c = CardArea_card(area, i);
            Table *cfg = c->moveable.node.config;
            const char *k = tstr(cfg, "card_key") ? tstr(cfg, "card_key") : tstr(cfg, "center_key") ? tstr(cfg, "center_key") : "?";
            s = fmt("%s%s%s%s%s%s", s, i > 1 ? "," : "", k, c->debuff ? "d" : "", streq(c->facing, "back") ? "b" : "", c->highlighted ? "h" : "");
        }
    return s;
}
static const char *shopc(CardArea *area) {
    const char *s = "";
    if (area && area->cards)
        for (int i = 1; i <= CardArea_n(area); i++) {
            Card *c = CardArea_card(area, i);
            const char *k = tstr(c->moveable.node.config, "center_key");
            s = fmt("%s%s%s:%s%s%s", s, i > 1 ? "," : "", k ? k : "?", n(N(c->cost)), c->edition ? ":" : "", c->edition ? s_or_nil(tstr(c->edition, "type")) : "");
        }
    return s;
}
static void cmp(int frame, const char *label) {
    Table *g = G->GAME;
    const char *tags = "";
    num lv = 0;
    Table *gt = ttab(g, "tags");
    for (int i = 1; i <= tlen(gt); i++) tags = fmt("%s%s%s", tags, i > 1 ? "," : "", ((Tag *)tobji(gt, i))->key);
    for (TIter it = titer(ttab(g, "hands")); tnext(&it);) lv += tnum(it.val.tab, "level");
    Table *rr = ttab(g, "round_resets"), *cr = ttab(g, "current_round");
    Blind *b = tobj(g, "blind");
    Table *bs = ttab(rr, "blind_states"), *bt = ttab(rr, "blind_tags");
    printf("CMP %d %s st=%d ante=%s round=%s chips=%s bchips=%s $=%s hs=%s hl=%s dl=%s deck=%d pc=%d lv=%s blind=%s dis=%s boss=%s bs=%s/%s/%s bt=%s/%s won=%s tags=[%s] jk=[%s] cons=[%s] hand=[%s] sj=[%s] sv=[%s] sb=[%s] rr=%s\n",
           frame, label, G->STATE, n(tget(rr, "ante")), n(tget(g, "round")), n(tget(g, "chips")), b ? n(N(b->chips)) : "nil", n(tget(g, "dollars")),
           G->hand ? n(tget(G->hand->moveable.node.config, "card_limit")) : "nil", n(tget(cr, "hands_left")), n(tget(cr, "discards_left")),
           G->deck ? CardArea_n(G->deck) : -1, G->playing_cards ? tlen(G->playing_cards) : -1, n(N(lv)), b ? s_or_nil(b->name) : "nil",
           b && b->disabled ? "true" : "false", s_or_nil(tstr(ttab(rr, "blind_choices"), "Boss")), s_or_nil(tstr(bs, "Small")), s_or_nil(tstr(bs, "Big")),
           s_or_nil(tstr(bs, "Boss")), s_or_nil(tstr(bt, "Small")), s_or_nil(tstr(bt, "Big")), tbool(g, "won") ? "true" : "false", tags, cards(G->jokers),
           cards(G->consumeables), cards(G->hand), shopc(G->shop_jokers), shopc(G->shop_vouchers), shopc(G->shop_booster), n(tget(cr, "reroll_cost")));
}
static void pick(int k) {
    for (int i = 1; i <= CardArea_n(G->hand); i++) {
        if (CardArea_n_highlighted(G->hand) >= k) break;
        if (!CardArea_card(G->hand, i)->highlighted) CardArea_add_to_highlighted(G->hand, CardArea_card(G->hand, i), true);
    }
}

void hook_blinds(int frame) {
    if (frame == 1300) {
        tsetb(G->SETTINGS, "tutorial_complete", true);
        tset(G->SETTINGS, "tutorial_progress", NIL); // as state_events.lua:1213; else it forces the first two tags
        if (env("SPEED")) tsetn(G->SETTINGS, "GAMESPEED", atof(env("SPEED")));
        G->FORCE_BOSS = env("BOSS");
        G->FORCE_TAG = env("FTAG");
        FUNCS_start_run(NULL, TAB("stake", N(atof(env("STAKE") ? env("STAKE") : "1")), "seed", S(env("SEED") ? env("SEED") : "BLINDS")));
        H.started = true;
        return;
    }
    if (!H.started || !G->GAME || !G->hand || !ttab(G->GAME, "round_resets")) return;
    if (G->STATE != H.st) { H.st = G->STATE; H.since = frame; }
    if (frame % 300 == 0) cmp(frame, "tick");
    if (env("DBG") && frame >= atoi(env("DBG")) && frame < atoi(env("DBG")) + 30) { // the clocks and the queue, to tell a tie from a bug
        Table *base = ttab(G->E_MANAGER->queues, "base");
        if (env("DBGQ")) {
            const char *q = "";
            for (int i = 1; i <= tlen(base) && i <= 6; i++) {
                Event *v = tobji(base, i);
                q = fmt("%s%s%s:%s:%.5f:%.3f:%s%s", q, i > 1 ? " " : "", v->trigger, v->timer, v->time, v->delay, v->blocking ? "B" : "-", v->start_timer ? "S" : "-");
            }
            printf("DBGQ %d %s\n", frame, q);
        }
        Event *e = NULL; // the first timed event of the queue
        for (int i = 1; i <= tlen(base); i++)
            if (!e && ((Event *)tobji(base, i))->delay > 0) e = tobji(base, i);
        printf("DBG %d total=%.6f real=%.6f base=%d head=%s/%s time=%.9f delay=%.17g\n", frame, (double)G->TIMERS.TOTAL, (double)G->TIMERS.REAL, tlen(base),
               e ? e->trigger : "-", e ? e->timer : "-", e ? e->time : 0, e ? e->delay : 0);
    }
    int wait = frame - H.since;
    if (env("NOAUTO")) return;
    Table *g = G->GAME;
    Table *rr = ttab(g, "round_resets"), *cr = ttab(g, "current_round");
    Blind *blind = tobj(g, "blind");

    if (G->STATE == STATES_BLIND_SELECT && wait >= 300 && G->blind_select && G->blind_select_opts && !G->OVERLAY_MENU) {
        if (!H.setup) {
            H.setup = true;
            if (env("UNSEED")) tsetb(g, "seeded", false);
            if (env("ANTE")) { tsetn(rr, "ante", atof(env("ANTE"))); tsetn(rr, "blind_ante", atof(env("ANTE"))); }
            if (env("MONEY")) tsetn(g, "dollars", atof(env("MONEY")));
            if (env("JOKERS")) {
                char buf[512];
                snprintf(buf, sizeof buf, "%s", env("JOKERS"));
                for (char *k = strtok(buf, ","); k; k = strtok(NULL, ",")) add_joker(fmt("%s", k), NULL, false, false);
            }
        }
        const char *plan = env("PLAN") ? env("PLAN") : "p";
        H.dec = H.dec + 1;
        int i = H.dec < (int)strlen(plan) ? H.dec : (int)strlen(plan);
        char c = plan[i - 1];
        const char *on_deck = tstr(g, "blind_on_deck");
        if (streq(on_deck, "Boss") && c == 's') c = 'p';
        UIBox *opt = tobj(G->blind_select_opts, str_lower(on_deck));
        cmp(frame, fmt("select_%c_%s", c, on_deck));
        if (c == 'r') { // the boss reroll of the Retcon voucher
            tsetb(ttab(g, "used_vouchers"), "v_retcon", true);
            FUNCS_reroll_boss(NULL);
        } else if (c == 's') {
            FUNCS_skip_blind(UIBox_get_UIE_by_ID(opt, S("tag_container"), NULL));
        } else {
            FUNCS_select_blind(UIBox_get_UIE_by_ID(opt, S("select_blind_button"), NULL));
            H.blinds = H.blinds + 1;
            H.step = 0;
        }
        H.since = frame;
    } else if (G->STATE == STATES_SELECTING_HAND && wait >= 180 && CardArea_n(G->hand) > 0 && (!G->OVERLAY_MENU || H.info)) {
        const char *seq = env("HAND") ? env("HAND") : "pdw";
        H.step = H.step + 1;
        char c = H.step <= (int)strlen(seq) ? seq[H.step - 1] : 'w';
        if (H.info) c = 'o';
        cmp(frame, fmt("hand_%c", c));
        if (c == 'p') {
            pick(5); FUNCS_play_cards_from_highlighted(NULL);
        } else if (c == 'd') {
            if (tnum(cr, "discards_left") > 0) { pick(3); FUNCS_discard_cards_from_highlighted(NULL, false); }
        } else if (c == 'w') {
            tsetn(g, "chips", blind->chips);
            pick(5); FUNCS_play_cards_from_highlighted(NULL);
        } else if (c == 'l') {
            tsetn(g, "chips", 0);
            tsetn(cr, "hands_left", 1);
            pick(1); FUNCS_play_cards_from_highlighted(NULL);
        } else if (c == 'x') {
            Blind_disable(blind);
        } else if (c == 'S') {
            if (CardArea_card(G->jokers, 1)) Card_sell_card(CardArea_card(G->jokers, 1));
        } else if (c == 'i') {
            H.info = true; FUNCS_run_info(NULL);
        } else if (c == 'v') {
            H.info = true; FUNCS_deck_info(NULL);
        } else if (c == 'o') {
            H.info = false; H.step = H.step - 1; FUNCS_exit_overlay_menu();
        }
        H.since = frame;
    } else if (G->STATE == STATES_ROUND_EVAL && wait >= 120) {
        if (G->OVERLAY_MENU) {
            if (wait >= 400 && !H.won) {
                H.won = true;
                cmp(frame, "win");
                FUNCS_exit_overlay_menu();
                H.since = frame;
            }
        } else if (G->round_eval) {
            UIElement *e = NULL; // the button is a box of its own (common_events.lua:1069)
            for (int i = 1; i <= tlen(G->I.UIBOX); i++) {
                UIElement *b = UIBox_get_UIE_by_ID(tobji(G->I.UIBOX, i), S("cash_out_button"), NULL);
                if (b && truthy(tget(b->moveable.node.config, "button"))) e = b;
            }
            if (e && truthy(tget(e->moveable.node.config, "button"))) {
                cmp(frame, "cashout");
                FUNCS_cash_out(e);
                H.since = frame;
            }
        }
    } else if (G->STATE == STATES_SHOP && wait >= 300 && G->shop && H.shop != H.blinds && !G->OVERLAY_MENU) {
        H.shop = H.blinds;
        cmp(frame, "shop");
        FUNCS_toggle_shop(NULL);
        H.since = frame;
    } else if ((G->STATE == STATES_TAROT_PACK || G->STATE == STATES_PLANET_PACK || G->STATE == STATES_SPECTRAL_PACK || G->STATE == STATES_STANDARD_PACK || G->STATE == STATES_BUFFOON_PACK) &&
               wait >= 300 && G->pack_cards && !G->OVERLAY_MENU) {
        cmp(frame, "pack");
        FUNCS_skip_booster(NULL);
        H.since = frame;
    } else if (G->STATE == STATES_GAME_OVER && wait >= 300 && !H.over) {
        H.over = true;
        cmp(frame, "gameover");
    }
}
