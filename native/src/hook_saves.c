// Test hook of the saves comparison (host only): called once per frame when HOOK=saves; the oracle runs the same steps
// from tools/hook_saves.lua (HOOK=<file> there), where the environment variables are described. See tools/saves.sh.
#include "all.h"

static bool at(const char *k, int frame) { return getenv(k) && atoi(getenv(k)) == frame; }

typedef struct { char *s; size_t n, cap; } Buf;
static void put(Buf *b, const char *s) {
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) b->s = realloc(b->s, b->cap = (b->n + l + 1) * 2);
    memcpy(b->s + b->n, s, l + 1);
    b->n += l;
}

typedef struct { const char *k; Val v; } KV;
static int kv_cmp(const void *a, const void *b) { return strcmp(((const KV *)a)->k, ((const KV *)b)->k); }

static void dump(Buf *b, Val v) {
    if (v.t == V_NUM || v.t == V_STR) { put(b, vstr(v)); return; }
    if (v.t == V_BOOL) { put(b, v.b ? "true" : "false"); return; }
    if (v.t != V_TAB) { put(b, "obj"); return; }
    int n = 0, cap = 16;
    KV *kv = malloc(cap * sizeof *kv);
    for (TIter it = titer(v.tab); tnext(&it);) {
        const char *s = it.key.t == V_BOOL ? (it.key.b ? "true" : "false") : vstr(it.key);
        if (!s) s = "obj";
        if (strstr(s, "colour") || streq(s, "blind")) continue;
        if (n == cap) kv = realloc(kv, (cap *= 2) * sizeof *kv);
        kv[n].k = s, kv[n++].v = it.val;
    }
    qsort(kv, n, sizeof *kv, kv_cmp);
    put(b, "{");
    for (int i = 0; i < n; i++) {
        if (i) put(b, ",");
        put(b, kv[i].k), put(b, "="), dump(b, kv[i].v);
    }
    put(b, "}");
    free(kv);
}

static void cards(Buf *b, CardArea *area) {
    if (!area || !area->cards) { put(b, "nil"); return; }
    int n = CardArea_n(area);
    put(b, fmt("%d: ", n));
    for (int i = 1; i <= n; i++) {
        Card *c = CardArea_card(area, i);
        if (i > 1) put(b, " ");
        if (tstr(c->base, "value")) put(b, fmt("%s%s/", tstr(c->base, "value"), str_sub(tstr(c->base, "suit"), 1, 1)));
        put(b, tstr(c->ability, "name"));
        if (c->edition) put(b, fmt("+%s", tstr(c->edition, "type")));
        if (c->seal) put(b, fmt("+%s", c->seal));
        if (tbool(c->ability, "eternal")) put(b, "+E");
        if (tbool(c->ability, "perishable")) put(b, "+P");
        if (tbool(c->ability, "rental")) put(b, "+R");
        if (c->pinned) put(b, "+pin");
        if (c->debuff) put(b, "+D");
        if (streq(c->facing, "back")) put(b, "+B");
        put(b, fmt("$%s", number_tostring(c->sell_cost)));
    }
}

// one line per key of t, sorted; the keys GAME and cardAreas one line per key of theirs
static void dump_keys(const char *prefix, Table *t, bool deep) {
    int n = 0, cap = 64;
    KV *kv = malloc(cap * sizeof *kv);
    for (TIter it = titer(t); tnext(&it);) {
        if (n == cap) kv = realloc(kv, (cap *= 2) * sizeof *kv);
        kv[n].k = vstr(it.key), kv[n++].v = it.val;
    }
    qsort(kv, n, sizeof *kv, kv_cmp);
    for (int i = 0; i < n; i++) {
        if (deep && (streq(kv[i].k, "GAME") || streq(kv[i].k, "cardAreas"))) {
            dump_keys(fmt("%s%s.", prefix, kv[i].k), kv[i].v.tab, false);
            continue;
        }
        Buf b = {0};
        dump(&b, kv[i].v);
        printf("%s%s %s\n", prefix, kv[i].k, b.s);
        free(b.s);
    }
    free(kv);
}

static const char *vs(Val v) { // tostring(v) of a number, a string or nil
    return v.t == V_NIL ? "nil" : vstr(v);
}

static void sum(int frame) {
    if (G->STAGE != STAGES_RUN || !G->deck || !G->GAME) return;
    Blind *blind = tobj(G->GAME, "blind");
    Back *back = tobj(G->GAME, "selected_back");
    printf("SUM %d state %d back %s blind %s blindchips %s handlimit %s jokerlimit %s conslimit %s tags %d overlay %s paused %s\n", frame, G->STATE,
           back->name, blind && blind->name ? blind->name : "nil", blind ? number_tostring(blind->chips) : "nil",
           vs(tget(G->hand->moveable.node.config, "card_limit")), vs(tget(G->jokers->moveable.node.config, "card_limit")),
           vs(tget(G->consumeables->moveable.node.config, "card_limit")), tlen(ttab(G->GAME, "tags")), G->OVERLAY_MENU ? "true" : "false", tbool(G->SETTINGS, "paused") ? "true" : "false");
    static const char *areas[] = {"deck", "hand", "play", "discard", "jokers", "consumeables", "shop_jokers", "shop_vouchers", "shop_booster"};
    for (int i = 0; i < 9; i++) {
        Buf b = {0};
        cards(&b, *Game_cardarea(G, areas[i]));
        printf("SUM %d %s %s\n", frame, areas[i], b.s);
        free(b.s);
    }
    Buf tags = {0};
    put(&tags, "");
    Table *game_tags = ttab(G->GAME, "tags");
    for (int i = 1; i <= tlen(game_tags); i++) {
        Tag *t = tobji(game_tags, i);
        put(&tags, fmt("%s%s/%s/%s/%s/%s", i > 1 ? " " : "", t->key, vs(tget(t->ability, "orbital_hand")), vs(tget(t->ability, "blind_type")),
                       number_tostring(t->tally), t->HUD_tag ? "true" : "false"));
    }
    printf("SUM %d tags %s\n", frame, tags.s);
    free(tags.s);
    Buf order = {0};
    for (TIter it = titer(ttab(G->GAME, "hands")); tnext(&it);) put(&order, order.n ? "," : ""), put(&order, vstr(it.key));
    printf("SUM %d handorder %s\n", frame, order.s);
    free(order.s);
    int n = 0, cap = 64;
    KV *kv = malloc(cap * sizeof *kv);
    for (TIter it = titer(G->GAME); tnext(&it);) {
        if (n == cap) kv = realloc(kv, (cap *= 2) * sizeof *kv);
        kv[n].k = vstr(it.key), kv[n++].v = it.val;
    }
    qsort(kv, n, sizeof *kv, kv_cmp);
    for (int i = 0; i < n; i++) {
        if (streq(kv[i].k, "challenge_tab")) continue;
        Buf b = {0};
        dump(&b, kv[i].v);
        printf("SUM %d G.%s %s\n", frame, kv[i].k, b.s);
        free(b.s);
    }
    free(kv);
}

// button_callbacks.lua:1805
static bool ev_change_lang(void *ctx) {
    Game_delete_run(G);
    Game_init_item_prototypes(G);
    Game_main_menu(G, NULL);
    return true;
}

void hook_saves(int frame) {
    if (frame == 2 && getenv("NOTUT")) tsetb(G->SETTINGS, "tutorial_complete", true);
    if (at("LANGAT", frame) && getenv("GLANG")) {
        tsets(G->SETTINGS, "language", getenv("GLANG"));
        Game_set_language(G);
        EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
        FUNCS_wipe_on(NULL, false, -1, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .not_blocking = true, .func = ev_change_lang}), NULL, false);
        FUNCS_wipe_off();
    }
    if (at("START", frame)) {
        if (getenv("DECK")) {
            Back *viewed_back = NEW(Back);
            Back_init(viewed_back, ttab(G->P_CENTERS, getenv("DECK")));
            tseto(G->GAME, "viewed_back", viewed_back);
        }
        Table *args = tnew();
        if (getenv("STAKE")) tsetn(args, "stake", atoi(getenv("STAKE")));
        if (getenv("SEED")) tsets(args, "seed", getenv("SEED"));
        if (getenv("CHALLENGE")) tset(args, "challenge", tgeti(G->CHALLENGES, atoi(getenv("CHALLENGE"))));
        FUNCS_start_run(NULL, args);
    }
    if (at("CONT", frame)) {
        const char *packed = get_compressed(fmt("%s/save.jkr", vstr(tget(G->SETTINGS, "profile"))));
        G->SAVED_GAME = packed ? STR_UNPACK(packed) : NULL;
        printf("SUM %d saved %s\n", frame, G->SAVED_GAME ? "true" : "false");
        if (G->SAVED_GAME) FUNCS_start_run(NULL, TAB("savetext", T(G->SAVED_GAME)));
    }
    if (at("SELECT", frame) && G->blind_select_opts) {
        if (getenv("BOSS")) {
            tset(ttab(G->GAME, "round_resets"), "ante", tget(G->GAME, "win_ante"));
            tsets(G->GAME, "blind_on_deck", "Boss");
            FUNCS_select_blind(UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, "boss"), S("select_blind_button"), NULL));
        } else
            FUNCS_select_blind(UIBox_get_UIE_by_ID(tobj(G->blind_select_opts, "small"), S("select_blind_button"), NULL));
    }
    if ((at("PLAY", frame) || at("PLAY2", frame) || at("PLAY3", frame) || at("PLAY4", frame)) && G->hand && G->STATE == STATES_SELECTING_HAND) {
        for (int i = 1; i <= 5; i++)
            if (CardArea_card(G->hand, i)) CardArea_add_to_highlighted(G->hand, CardArea_card(G->hand, i), true);
        FUNCS_play_cards_from_highlighted(NULL);
    }
    if (at("EASY", frame) && tobj(G->GAME, "blind")) {
        Blind *blind = tobj(G->GAME, "blind");
        blind->chips = 1, blind->chip_text = "1";
    }
    if (at("NEXT", frame) && G->shop) FUNCS_toggle_shop(NULL);
    if (at("UNSEED", frame) && G->GAME) tset(G->GAME, "seeded", NIL);
    int exitov = getenv("EXITOV") ? atoi(getenv("EXITOV")) : 0;
    if (exitov && frame >= exitov && frame < exitov + 600 && (frame - exitov) % 60 == 0 && G->OVERLAY_MENU) printf("SUM %d overlay closed\n", frame), FUNCS_exit_overlay_menu();
    if (at("TAGS", frame) && G->GAME) {
        static const char *keys[] = {"tag_orbital", "tag_juggle", "tag_investment"};
        for (int i = 0; i < 3; i++) {
            Tag *t = NEW(Tag);
            Tag_init(t, keys[i], false, i == 0 ? "Small" : NULL);
            add_tag(t);
        }
    }
    if (at("FORCESAVE", frame) && G->FILE_HANDLER) tsetb(G->FILE_HANDLER, "force", true);
    if (at("DUMPSAVE", frame)) {
        const char *packed = get_compressed(fmt("%s/save.jkr", vstr(tget(G->SETTINGS, "profile"))));
        Table *t = packed ? STR_UNPACK(packed) : tnew();
        dump_keys(fmt("SUM %d save.", frame), t, true);
    }
    if (at("PROFILE", frame)) {
        tsetn(G->SETTINGS, "profile", atoi(getenv("PROFILE_N")));
        G->focused_profile = atoi(getenv("PROFILE_N"));
        FUNCS_load_profile(false);
    }
    if (at("DELPROFILE", frame)) {
        G->focused_profile = (int)tnum(G->SETTINGS, "profile");
        FUNCS_load_profile(true);
    }
    if (at("UNLOCKS", frame) || at("UNLOCKS2", frame)) {
        int u = 0, d = 0, a = 0;
        for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
            u += tbool(it.val.tab, "unlocked"), d += tbool(it.val.tab, "discovered"), a += tbool(it.val.tab, "alerted");
        }
        printf("SUM %d centers unlocked %d discovered %d alerted %d profile %s\n", frame, u, d, a, vs(tget(G->SETTINGS, "profile")));
        Table *P = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
        static const char *keys[] = {"career_stats", "progress", "high_scores", "joker_usage", "deck_usage", "hand_usage", "consumeable_usage",
                                     "voucher_usage", "challenge_progress", "MEMORY", "stake", "all_unlocked", "name"};
        for (int i = 0; i < 13; i++) {
            Buf b = {0};
            Val v = tget(P, keys[i]);
            if (v.t == V_NIL) put(&b, "nil"); else dump(&b, v);
            printf("SUM %d P.%s %s\n", frame, keys[i], b.s);
            free(b.s);
        }
        int n = 0, cap = 64;
        KV *kv = malloc(cap * sizeof *kv);
        for (TIter it = titer(G->SETTINGS); tnext(&it);) {
            if (n == cap) kv = realloc(kv, (cap *= 2) * sizeof *kv);
            kv[n].k = vstr(it.key), kv[n++].v = it.val;
        }
        qsort(kv, n, sizeof *kv, kv_cmp);
        for (int i = 0; i < n; i++) {
            if (streq(kv[i].k, "WINDOW")) continue;
            Buf b = {0};
            dump(&b, kv[i].v);
            printf("SUM %d S.%s %s\n", frame, kv[i].k, b.s);
            free(b.s);
        }
        free(kv);
    }
    if (getenv("FLAMES") && frame % atoi(getenv("FLAMES")) == 0) {
        Table *F = ttab(G->ARGS, "chip_flames"), *AC = ttab(G->SETTINGS, "ambient_control");
        printf("FLM %d intensity %s change %s fire2 %s organ %s\n", frame, vs(tget(F, "real_intensity")), vs(tget(F, "change")),
               vs(tget(ttab(AC, "ambientFire2"), "vol")), vs(tget(ttab(AC, "ambientOrgan1"), "vol")));
    }
    if (getenv("SUM") && atoi(getenv("SUM")) > 0 && frame % atoi(getenv("SUM")) == 0) sum(frame);
}
