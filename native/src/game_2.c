// game.lua:216-844: Game:init_item_prototypes()
#include "all.h"

// The table of data/game.dat (data.h), loaded on first use and kept.
static Table *game_2_data(void) { return game_dat(); }

// string.find(k, '^xx')
static bool starts(const char *k, const char *prefix) { return k && strncmp(k, prefix, strlen(prefix)) == 0; }

// game.lua:763 function (a, b) return not a.order or not b.order or a.order < b.order end
static bool less_locked(Val a, Val b, void *ctx) {
    Val ao = tget(a.tab, "order"), bo = tget(b.tab, "order");
    return !truthy(ao) || !truthy(bo) || ao.n < bo.n;
}
// game.lua:826 function (a, b) return a.order < b.order end
static bool less_order(Val a, Val b, void *ctx) { return tnum(a.tab, "order") < tnum(b.tab, "order"); }
// game.lua:834 the Back pool: unlocked decks first
static bool less_back(Val a, Val b, void *ctx) {
    return (tnum(a.tab, "order") - (tbool(a.tab, "unlocked") ? 100 : 0)) < (tnum(b.tab, "order") - (tbool(b.tab, "unlocked") ? 100 : 0));
}
// game.lua:839 the Demo pool
static bool less_demo(Val a, Val b, void *ctx) {
    return tnum(a.tab, "order") + (streq(tstr(a.tab, "set"), "Joker") ? 1000 : 0) <
           tnum(b.tab, "order") + (streq(tstr(b.tab, "set"), "Joker") ? 1000 : 0);
}

// game.lua:769-776 (and :783-790, :798-805): discovered / alerted of a blind, tag or seal from the profile's meta
static void meta_discovered_alerted(Table *v, const char *k, Table *meta) {
    // (TESTHELPER_unlocks is constant false: game.lua:768)
    if (!tbool(v, "discovered") && truthy(tget(ttab(meta, "discovered"), k))) {
        tsetb(v, "discovered", true);
    }
    if (tbool(v, "discovered") && truthy(tget(ttab(meta, "alerted"), k))) {
        tsetb(v, "alerted", true);
    } else if (tbool(v, "discovered")) {
        tsetb(v, "alerted", false);
    }
}

// game.lua:216
void Game_init_item_prototypes(Game *self) {
    // game.lua:218-727: the literal tables, from the data file (section "prototypes", captured at game.lua:729).
    // A fresh deep copy every call, as the Lua builds new tables every call.
    Table *p = tcopy(ttab(game_2_data(), "prototypes"));
    self->P_SEALS = ttab(p, "P_SEALS");
    self->P_TAGS = ttab(p, "P_TAGS");
    self->tag_undiscovered = ttab(p, "tag_undiscovered");
    self->P_STAKES = ttab(p, "P_STAKES");
    self->P_BLINDS = ttab(p, "P_BLINDS");
    // game.lua:266 vars = {localize('ph_most_played')}: the data file holds a placeholder text there
    tseti(ttab(ttab(self->P_BLINDS, "bl_ox"), "vars"), 1, localize(S("ph_most_played"), NULL));
    // game.lua:266-294 boss_colour = HEX(...): {r, g, b, a} in the data file, P(float *) in a table (misc_functions.h)
    for (TIter it = titer(self->P_BLINDS); tnext(&it);) {
        Table *col = ttab(it.val.tab, "boss_colour");
        if (col) {
            float *c = malloc(4 * sizeof(float));
            for (int i = 0; i < 4; i++) c[i] = (float)tnumi(col, i + 1);
            tset(it.val.tab, "boss_colour", P(c));
        }
    }
    self->b_undiscovered = ttab(p, "b_undiscovered");
    self->P_CARDS = ttab(p, "P_CARDS");
    self->j_locked = ttab(p, "j_locked");
    self->v_locked = ttab(p, "v_locked");
    self->c_locked = ttab(p, "c_locked");
    self->j_undiscovered = ttab(p, "j_undiscovered");
    self->t_undiscovered = ttab(p, "t_undiscovered");
    self->p_undiscovered = ttab(p, "p_undiscovered");
    self->s_undiscovered = ttab(p, "s_undiscovered");
    self->v_undiscovered = ttab(p, "v_undiscovered");
    self->booster_undiscovered = ttab(p, "booster_undiscovered");
    self->P_CENTERS = ttab(p, "P_CENTERS");
    self->P_CENTER_POOLS = ttab(p, "P_CENTER_POOLS");             // game.lua:704, every pool empty
    self->P_JOKER_RARITY_POOLS = ttab(p, "P_JOKER_RARITY_POOLS"); // game.lua:723 {{},{},{},{}}
    self->P_LOCKED = ttab(p, "P_LOCKED");                         // game.lua:727 {}

    Game_save_progress(self); // game.lua:729

    // game.lua:733 local TESTHELPER_unlocks = false and not _RELEASE_MODE: constant false
    const bool TESTHELPER_unlocks = false;
    const char *profile = vstr(tget(G->SETTINGS, "profile")); // G.SETTINGS.profile..''
    const char *meta_file = fmt("%s/meta.jkr", profile);
    if (!love_filesystem_getInfo(profile, NULL)) love_filesystem_createDirectory(profile);     // game.lua:735
    if (!love_filesystem_getInfo(meta_file, NULL)) love_filesystem_append(meta_file, "return {}", 9); // game.lua:736

    convert_save_to_meta(); // game.lua:738

    const char *packed = get_compressed(meta_file); // game.lua:740
    Table *meta = STR_UNPACK(packed ? packed : "return {}");
    if (!truthy(tget(meta, "unlocked"))) tsett(meta, "unlocked", tnew());
    if (!truthy(tget(meta, "discovered"))) tsett(meta, "discovered", tnew());
    if (!truthy(tget(meta, "alerted"))) tsett(meta, "alerted", tnew());

    // game.lua:745
    // note: pairs() order is the Lua's hash order; here it is the order of the source (titer). It decides the
    // order of P_LOCKED before its sort (ties of `order` between jokers, vouchers and backs) and of the pools
    // that are never sorted (Edition, Default).
    for (TIter it = titer(self->P_CENTERS); tnext(&it);) {
        const char *k = it.key.s;
        Table *v = it.val.tab;
        if (!tbool(v, "wip") && !tbool(v, "demo")) {
            if (TESTHELPER_unlocks) { tsetb(v, "unlocked", true); tsetb(v, "discovered", true); tsetb(v, "alerted", true); }
            if (!tbool(v, "unlocked") && (starts(k, "j_") || starts(k, "b_") || starts(k, "v_")) && truthy(tget(ttab(meta, "unlocked"), k))) {
                tsetb(v, "unlocked", true);
            }
            if (!tbool(v, "unlocked") && (starts(k, "j_") || starts(k, "b_") || starts(k, "v_"))) tseti(self->P_LOCKED, tlen(self->P_LOCKED) + 1, T(v));
            if (!tbool(v, "discovered") &&
                (starts(k, "j_") || starts(k, "b_") || starts(k, "e_") || starts(k, "c_") || starts(k, "p_") || starts(k, "v_")) &&
                truthy(tget(ttab(meta, "discovered"), k))) {
                tsetb(v, "discovered", true);
            }
            if ((tbool(v, "discovered") && truthy(tget(ttab(meta, "alerted"), k))) || streq(tstr(v, "set"), "Back") || tbool(v, "start_alerted")) {
                tsetb(v, "alerted", true);
            } else if (tbool(v, "discovered")) {
                tsetb(v, "alerted", false);
            }
        }
    }

    tsort(self->P_LOCKED, less_locked, NULL); // game.lua:763

    // game.lua:765
    for (TIter it = titer(self->P_BLINDS); tnext(&it);) {
        const char *k = it.key.s;
        Table *v = it.val.tab;
        tsets(v, "key", k);
        if (!tbool(v, "wip") && !tbool(v, "demo")) {
            meta_discovered_alerted(v, k, meta);
        }
    }
    // game.lua:779
    for (TIter it = titer(self->P_TAGS); tnext(&it);) {
        const char *k = it.key.s;
        Table *v = it.val.tab;
        tsets(v, "key", k);
        if (!tbool(v, "wip") && !tbool(v, "demo")) {
            meta_discovered_alerted(v, k, meta);
            tinsert(ttab(self->P_CENTER_POOLS, "Tag"), T(v));
        }
    }
    // game.lua:794
    for (TIter it = titer(self->P_SEALS); tnext(&it);) {
        const char *k = it.key.s;
        Table *v = it.val.tab;
        tsets(v, "key", k);
        if (!tbool(v, "wip") && !tbool(v, "demo")) {
            meta_discovered_alerted(v, k, meta);
            tinsert(ttab(self->P_CENTER_POOLS, "Seal"), T(v));
        }
    }
    // game.lua:809
    for (TIter it = titer(self->P_STAKES); tnext(&it);) {
        Table *v = it.val.tab;
        tsets(v, "key", it.key.s);
        tinsert(ttab(self->P_CENTER_POOLS, "Stake"), T(v));
    }

    // game.lua:814
    for (TIter it = titer(self->P_CENTERS); tnext(&it);) {
        Table *v = it.val.tab;
        const char *set = tstr(v, "set");
        tsets(v, "key", it.key.s);
        if (streq(set, "Joker")) tinsert(ttab(self->P_CENTER_POOLS, "Joker"), T(v));
        if (set && tbool(v, "demo") && tbool(v, "pos")) tinsert(ttab(self->P_CENTER_POOLS, "Demo"), T(v));
        if (!tbool(v, "wip")) {
            if (set && !streq(set, "Joker") && !tbool(v, "skip_pool") && !tbool(v, "omit")) tinsert(ttab(self->P_CENTER_POOLS, set), T(v));
            if (streq(set, "Tarot") || streq(set, "Planet")) tinsert(ttab(self->P_CENTER_POOLS, "Tarot_Planet"), T(v));
            if (tbool(v, "consumeable")) tinsert(ttab(self->P_CENTER_POOLS, "Consumeables"), T(v));
            if (tbool(v, "rarity") && streq(set, "Joker") && !tbool(v, "demo")) tinsert(ttabi(self->P_JOKER_RARITY_POOLS, (int)tnum(v, "rarity")), T(v));
        }
    }

    // game.lua:826-839
    tsort(ttab(self->P_CENTER_POOLS, "Joker"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Tarot"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Planet"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Tarot_Planet"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Spectral"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Voucher"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Booster"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Consumeables"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Back"), less_back, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Enhanced"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Stake"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Tag"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Seal"), less_order, NULL);
    tsort(ttab(self->P_CENTER_POOLS, "Demo"), less_demo, NULL);
    for (int i = 1; i <= 4; i++) { // game.lua:840
        tsort(ttabi(self->P_JOKER_RARITY_POOLS, i), less_order, NULL);
    }
}
