// game.lua:2018-2448: Game:start_run.
#include "all.h"

// t[key] = t[key] or {}; returns it
static Table *sub_or_new(Table *t, const char *key) {
    if (!truthy(tget(t, key))) tsett(t, key, tnew());
    return ttab(t, key);
}

// (a back object).name, as game.lua:2037 reads it; NULL as nil
static const char *back_name(Val v) {
    if (v.t == V_OBJ && is_a(v.obj, &Back_class)) return ((Back *)v.obj)->name;
    if (v.t == V_TAB) return tstr(v.tab, "name");
    return NULL;
}

// game.lua:2070 closure of the challenge jokers' loop
struct ev_ch_ctx { Table *v; int k; };
static bool ev_challenge_joker(void *ctx) {
    struct ev_ch_ctx *c = ctx;
    Card *_joker = add_joker(tstr(c->v, "id"), tstr(c->v, "edition"), c->k != 1, false);
    if (tbool(c->v, "eternal")) Card_set_eternal(_joker, true);
    if (tbool(c->v, "pinned")) _joker->pinned = true;
    return true;
}

// game.lua:2082
static bool ev_challenge_consumeable(void *ctx) {
    struct ev_ch_ctx *c = ctx;
    add_joker(tstr(c->v, "id"), NULL, c->k != 1, false);
    return true;
}

// game.lua:2093
static bool ev_challenge_voucher(void *ctx) {
    struct ev_ch_ctx *c = ctx;
    Val n = tget(G->GAME, "starting_voucher_count");
    tsetn(G->GAME, "starting_voucher_count", (truthy(n) ? n.n : 0) + 1);
    Card_apply_to_run(NULL, ttab(G->P_CENTERS, tstr(c->v, "id")));
    return true;
}

// game.lua:2193
static bool ev_unlock_load(void *ctx) {
    tset(G->CONTROLLER->locks, "load", NIL);
    return true;
}

// game.lua:2202 (innermost of the three nested events; ctx: saveTable)
static bool ev_action_3(void *ctx) {
    Table *saveTable = ctx;
    // note: G.I.CARD also holds the Blind and Card_Characters, which have no sort_id (nil never equals the number)
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        if (it.val.t != V_OBJ || !is_a(it.val.obj, &Card_class)) continue;
        Card *v = it.val.obj;
        if (veq(N(v->sort_id), tget(ttab(saveTable, "ACTION"), "card"))) {
            // note: the Lua is use_card({config = {ref_table = v}}, nil, true): mute nil, nosave TRUE
            // (button_callbacks.h's comment says "no nosave" for this call; the Lua line is followed)
            FUNCS_use_card(NULL, v, false, true);
        }
    }
    return true;
}
// game.lua:2201
static bool ev_action_2(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_action_3, .ctx = ctx}), NULL, false);
    return true;
}
// game.lua:2200
static bool ev_action_1(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_action_2, .ctx = ctx}), NULL, false);
    return true;
}

// game.lua:2298 never completes (the Lua returns nothing): it runs on every pass until the queue is cleared
static bool ev_spin(void *ctx) {
    Table *spin = ttab(G->ARGS, "spin");
    num _dt = tnum(spin, "amount") > tnum(spin, "eased") ? G->real_dt * 2. : 0.3 * G->real_dt;
    num delta = tnum(spin, "real") - tnum(spin, "eased");
    if (fabs(delta) > _dt) delta = delta * _dt / fabs(delta);
    tsetn(spin, "eased", tnum(spin, "eased") + delta);
    tsetn(spin, "amount", _dt * (tnum(spin, "eased")) + (1 - _dt) * tnum(spin, "amount"));
    G->TIMERS.BACKGROUND = G->TIMERS.BACKGROUND - 60 * (tnum(spin, "eased") - tnum(spin, "amount")) * _dt;
    return false;
}

// game.lua:2326
static bool playing_card_desc(Val a, Val b, void *ctx) {
    return ((Card *)a.obj)->playing_card > ((Card *)b.obj)->playing_card;
}

// game.lua:2368 (x.s or '')..(x.r or '')..(x.e or '')..(x.d or '')..(x.g or '')
static const char *proto_key(Table *x) {
    static const char *const keys[5] = {"s", "r", "e", "d", "g"};
    const char *p[5];
    for (int i = 0; i < 5; i++) {
        Val v = tget(x, keys[i]);
        p[i] = truthy(v) ? vstr(v) : "";
        if (!p[i]) p[i] = "";
    }
    return fmt("%s%s%s%s%s", p[0], p[1], p[2], p[3], p[4]);
}
// game.lua:2367
static bool card_proto_less(Val a, Val b, void *ctx) {
    // note: Lua's string < is strcoll; the keys are ASCII, so strcmp
    return strcmp(proto_key(a.tab), proto_key(b.tab)) < 0;
}

// game.lua:2441
static bool ev_unlock_notify(void *ctx) {
    unlock_notify();
    return true;
}

// note: pairs(G.GAME.hands) picks rules' results by its order (the Orbital tag's hand, To Do List, the most played
// hand's ties), and the oracle's order is Lua 5.1's: the node array of a table of string keys, which depends on how
// the table was filled: by one constructor (init_game_object, the save's "return {...}": the array is made for
// the n keys at once) or key by key from {} (recursive_table_cull's copy, which is what a save writes: the array
// doubles as it fills). This returns the table's entries in the order pairs would walk the Lua's table, given that
// t's own order is the order the keys were written (lstring.c's hash; ltable.c's newkey, rehash and luaH_next).
static unsigned lua51_hash(const char *str) {
    size_t l = strlen(str), step = (l >> 5) + 1;
    unsigned h = (unsigned)l;
    for (size_t l1 = l; l1 >= step; l1 -= step) h = h ^ ((h << 5) + (h >> 2) + (unsigned char)str[l1 - 1]);
    return h;
}
typedef struct { Val key, val; int next; bool used; } Lua51Node;
typedef struct { Lua51Node *node; int size, lastfree; } Lua51Tab;
static void lua51_resize(Lua51Tab *T, int n) {
    T->size = 1;
    while (T->size < n) T->size *= 2;
    T->node = calloc(T->size, sizeof *T->node);
    for (int i = 0; i < T->size; i++) T->node[i].next = -1;
    T->lastfree = T->size;
}
static void lua51_set(Lua51Tab *T, Val key, Val val) {
    Lua51Node *node = T->node;
    int mp = T->size ? lua51_hash(key.s) & (T->size - 1) : 0;
    if (!T->size || node[mp].used) {
        int f = -1;
        while (T->lastfree > 0)
            if (!node[--T->lastfree].used) { f = T->lastfree; break; }
        if (f < 0) { // rehash: the old nodes go into the new array from the last to the first, then the new key
            int old = T->size, n = 1;
            for (int i = 0; i < old; i++) n += node[i].used;
            lua51_resize(T, n);
            for (int i = old - 1; i >= 0; i--)
                if (node[i].used) lua51_set(T, node[i].key, node[i].val);
            free(node);
            lua51_set(T, key, val);
            return;
        }
        int othern = lua51_hash(node[mp].key.s) & (T->size - 1);
        if (othern != mp) { // the colliding node is out of its main position: it moves to the free one
            while (node[othern].next != mp) othern = node[othern].next;
            node[othern].next = f;
            node[f] = node[mp];
            node[mp].next = -1;
        } else { // the new key goes to the free position
            node[f].next = node[mp].next;
            node[mp].next = f;
            mp = f;
        }
    }
    node[mp].key = key, node[mp].val = val, node[mp].used = true;
}
Table *lua51_pairs_order(Table *t, bool constructor) {
    int n = 0;
    for (TIter it = titer(t); tnext(&it);) {
        if (it.key.t != V_STR) return t; // only tables of string keys
        n++;
    }
    Lua51Tab T = {0};
    if (constructor && n) lua51_resize(&T, n);
    for (TIter it = titer(t); tnext(&it);) lua51_set(&T, it.key, it.val);
    Table *out = tnew();
    for (int i = 0; i < T.size; i++)
        if (T.node[i].used) tsetv(out, T.node[i].key, T.node[i].val);
    free(T.node);
    return out;
}

// game.lua:2018
void Game_start_run(Game *self, Table *args) {
    if (!args) args = tnew();

    Table *saveTable = ttab(args, "savetext");
    G->SAVED_GAME = NULL;

    Val _state = tget(saveTable, "STATE");
    Game_prep_stage(self, STAGES_RUN, saveTable && truthy(_state) ? (int)vnum(_state) : STATES_BLIND_SELECT, false);

    G->STAGE = STAGES_RUN;
    if (saveTable) {
        check_for_unlock(TAB("type", S("continue_game")));
    }

    G->STATE_COMPLETE = false;
    G->RESET_BLIND_STATES = true;

    if (!saveTable) ease_background_colour_blind(G->STATE, "Small Blind");
    else {
        // saveTable.BLIND.name:gsub("%s+", "") ~= '': the name has a character that is not whitespace
        const char *name = tstr(ttab(saveTable, "BLIND"), "name");
        if (!name) ERROR("game.lua:2035: saveTable.BLIND.name is nil");
        bool has_text = false;
        for (const char *p = name; *p; p++)
            if (!(*p == ' ' || (*p >= '\t' && *p <= '\r'))) { has_text = true; break; }
        ease_background_colour_blind(G->STATE, has_text ? name : "Small Blind");
    }

    // game.lua:2037 (self.GAME is still the previous one here)
    const char *selected_back_name = saveTable ? tstr(ttab(saveTable, "BACK"), "name") : NULL;
    if (!selected_back_name) selected_back_name = tstr(ttab(ttab(args, "challenge"), "deck"), "type");
    if (!selected_back_name) selected_back_name = back_name(tget(self->GAME, "viewed_back"));
    if (!selected_back_name) selected_back_name = back_name(tget(self->GAME, "selected_back"));
    if (!selected_back_name) selected_back_name = "Red Deck";
    Table *selected_back = get_deck_from_name(selected_back_name);
    self->GAME = saveTable && truthy(tget(saveTable, "GAME")) ? ttab(saveTable, "GAME") : Game_init_game_object(self);
    if (ttab(self->GAME, "hands")) tsett(self->GAME, "hands", lua51_pairs_order(ttab(self->GAME, "hands"), true)); // note: see lua51_pairs_order
    Table *modifiers = sub_or_new(self->GAME, "modifiers");
    {
        Val stake = tget(args, "stake");
        if (!truthy(stake)) stake = tget(self->GAME, "stake");
        if (!truthy(stake)) stake = N(1);
        tset(self->GAME, "stake", stake);
    }
    tsetn(self->GAME, "STOP_USE", 0);
    Back *back = NEW(Back);
    Back_init(back, selected_back);
    tseto(self->GAME, "selected_back", back);
    tsett(self->GAME, "selected_back_key", selected_back);

    for (int i = 0; i < 4; i++) G->C.UI_CHIPS[i] = G->C.BLUE[i];
    for (int i = 0; i < 4; i++) G->C.UI_MULT[i] = G->C.RED[i];

    if (!saveTable) {
        Table *starting_params = ttab(self->GAME, "starting_params");
        num stake = tnum(self->GAME, "stake");
        if (stake >= 2) {
            tsetb(sub_or_new(modifiers, "no_blind_reward"), "Small", true);
        }
        if (stake >= 3) tsetn(modifiers, "scaling", 2);
        if (stake >= 4) tsetb(modifiers, "enable_eternals_in_shop", true);
        if (stake >= 5) tsetn(starting_params, "discards", tnum(starting_params, "discards") - 1);
        if (stake >= 6) tsetn(modifiers, "scaling", 3);
        if (stake >= 7) tsetb(modifiers, "enable_perishables_in_shop", true);
        if (stake >= 8) tsetb(modifiers, "enable_rentals_in_shop", true);

        Back_apply_to_run(back);

        Table *_ch = ttab(args, "challenge");
        if (_ch) {
            tset(self->GAME, "challenge", tget(_ch, "id"));
            tsett(self->GAME, "challenge_tab", _ch);
            Table *list;
            if ((list = ttab(_ch, "jokers"))) {
                for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                    struct ev_ch_ctx *c = NEWCTX(struct ev_ch_ctx);
                    c->v = ttabi(list, k), c->k = k;
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_challenge_joker, .ctx = c}), NULL, false);
                }
            }
            if ((list = ttab(_ch, "consumeables"))) {
                for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                    struct ev_ch_ctx *c = NEWCTX(struct ev_ch_ctx);
                    c->v = ttabi(list, k), c->k = k;
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_challenge_consumeable, .ctx = c}), NULL, false);
                }
            }
            if ((list = ttab(_ch, "vouchers"))) {
                for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                    struct ev_ch_ctx *c = NEWCTX(struct ev_ch_ctx);
                    c->v = ttabi(list, k), c->k = k;
                    tsetv(ttab(G->GAME, "used_vouchers"), tget(c->v, "id"), B(true));
                    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_challenge_voucher, .ctx = c}), NULL, false);
                }
            }
            Table *rules = ttab(_ch, "rules");
            if (rules) {
                if ((list = ttab(rules, "modifiers"))) {
                    for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                        Table *v = ttabi(list, k);
                        tsetv(starting_params, tget(v, "id"), tget(v, "value"));
                    }
                }
                if ((list = ttab(rules, "custom"))) {
                    for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                        Table *v = ttabi(list, k);
                        const char *id = tstr(v, "id");
                        if (streq(id, "no_reward")) {
                            Table *nbr = sub_or_new(modifiers, "no_blind_reward");
                            tsetb(nbr, "Small", true);
                            tsetb(nbr, "Big", true);
                            tsetb(nbr, "Boss", true);
                        } else if (streq(id, "no_reward_specific")) {
                            tsetv(sub_or_new(modifiers, "no_blind_reward"), tget(v, "value"), B(true));
                        } else if (truthy(tget(v, "value"))) {
                            tsetv(modifiers, tget(v, "id"), tget(v, "value"));
                        } else if (streq(id, "no_shop_jokers")) {
                            tsetn(self->GAME, "joker_rate", 0);
                        } else {
                            tsetv(modifiers, tget(v, "id"), B(true));
                        }
                    }
                }
            }
            Table *restrictions = ttab(_ch, "restrictions");
            if (restrictions) {
                Table *banned_keys = ttab(G->GAME, "banned_keys");
                if ((list = ttab(restrictions, "banned_cards"))) {
                    for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                        Table *v = ttabi(list, k);
                        tsetv(banned_keys, tget(v, "id"), B(true));
                        Table *ids = ttab(v, "ids");
                        if (ids) {
                            for (int kk = 1; tgeti(ids, kk).t != V_NIL; kk++) {
                                tsetv(banned_keys, tgeti(ids, kk), B(true));
                            }
                        }
                    }
                }
                if ((list = ttab(restrictions, "banned_tags"))) {
                    for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                        tsetv(banned_keys, tget(ttabi(list, k), "id"), B(true));
                    }
                }
                if ((list = ttab(restrictions, "banned_other"))) {
                    for (int k = 1; tgeti(list, k).t != V_NIL; k++) {
                        tsetv(banned_keys, tget(ttabi(list, k), "id"), B(true));
                    }
                }
            }
        }

        Table *round_resets = ttab(self->GAME, "round_resets");
        tset(round_resets, "hands", tget(starting_params, "hands"));
        tset(round_resets, "discards", tget(starting_params, "discards"));
        tset(round_resets, "reroll_cost", tget(starting_params, "reroll_cost"));
        tset(self->GAME, "dollars", tget(starting_params, "dollars"));
        tset(self->GAME, "base_reroll_cost", tget(starting_params, "reroll_cost"));
        tset(round_resets, "reroll_cost", tget(self->GAME, "base_reroll_cost"));
        tset(ttab(self->GAME, "current_round"), "reroll_cost", tget(self->GAME, "base_reroll_cost"));
    }

    tsets(G->GAME, "chips_text", "");

    Table *pseudorandom_t = ttab(self->GAME, "pseudorandom");
    if (!saveTable) {
        Val seed = tget(args, "seed");
        if (truthy(seed)) tsetb(self->GAME, "seeded", true);
        if (!truthy(seed)) {
            // note: the Lua indexes G.SETTINGS.tutorial_progress unguarded here (an error if it is nil while the
            // tutorial is not complete); a nil one reads as "not completed"
            bool tutorial_done = tbool(G->SETTINGS, "tutorial_complete") ||
                                 truthy(tget(ttab(ttab(G->SETTINGS, "tutorial_progress"), "completed_parts"), "big_blind"));
            seed = !tutorial_done ? S("TUTORIAL") : S(generate_starting_seed());
        }
        tset(pseudorandom_t, "seed", seed);
    }

    {
        const char *seed = vstr(tget(pseudorandom_t, "seed"));
        // note: the keys are collected first so that no table is written while it is iterated
        Table *keys = tnew();
        for (TIter it = titer(pseudorandom_t); tnext(&it);)
            if (veq(it.val, N(0))) tinsert(keys, it.key);
        for (int i = 1; i <= tlen(keys); i++) {
            Val k = tgeti(keys, i);
            tsetv(pseudorandom_t, k, N(pseudohash(fmt("%s%s", vstr(k), seed))));
        }
        tsetn(pseudorandom_t, "hashed_seed", pseudohash(seed));
    }

    Game_save_settings(G);

    Table *round_resets = ttab(self->GAME, "round_resets");
    Table *current_round = ttab(self->GAME, "current_round");
    if (!truthy(tget(round_resets, "blind_tags"))) {
        tsett(round_resets, "blind_tags", tnew());
    }

    if (!saveTable) {
        Table *tutorial_progress = ttab(G->SETTINGS, "tutorial_progress");
        Table *blind_tags = ttab(round_resets, "blind_tags");
        tsets(ttab(round_resets, "blind_choices"), "Boss", get_new_boss());
        Val forced = tget(tutorial_progress, "forced_voucher");
        tset(current_round, "voucher", truthy(forced) ? forced : S(get_next_voucher_key(false)));
        forced = tgeti(ttab(tutorial_progress, "forced_tags"), 1);
        tset(blind_tags, "Small", truthy(forced) ? forced : S(get_next_tag_key(NULL)));
        forced = tgeti(ttab(tutorial_progress, "forced_tags"), 2);
        tset(blind_tags, "Big", truthy(forced) ? forced : S(get_next_tag_key(NULL)));
    } else {
        Table *blind = ttab(round_resets, "blind");
        if (blind && truthy(tget(blind, "key"))) {
            tset(round_resets, "blind", tgetv(G->P_BLINDS, tget(blind, "key")));
        }
    }
    tsetb(G->CONTROLLER->locks, "load", true);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .no_delete = true,
        .trigger = "after",
        .not_blocking = true, .not_blockable = true,
        .delay = 3.5,
        .timer = "TOTAL",
        .func = ev_unlock_load}), NULL, false);

    if (saveTable && truthy(tget(saveTable, "ACTION"))) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.delay = 0.5, .trigger = "after", .not_blocking = true, .not_blockable = true,
            .func = ev_action_1, .ctx = saveTable}), NULL, false);
    }

    struct {
        float discard_W, discard_H, deck_W, deck_H, hand_W, hand_H, play_W, play_H, joker_W, joker_H, consumeable_W, consumeable_H;
    } CAI = {
        .discard_W = G->CARD_W,
        .discard_H = G->CARD_H,
        .deck_W = G->CARD_W * 1.1f,
        .deck_H = 0.95f * G->CARD_H,
        .hand_W = 6 * G->CARD_W,
        .hand_H = 0.95f * G->CARD_H,
        .play_W = 5.3f * G->CARD_W,
        .play_H = 0.95f * G->CARD_H,
        .joker_W = 4.9f * G->CARD_W,
        .joker_H = 0.95f * G->CARD_H,
        .consumeable_W = 2.3f * G->CARD_W,
        .consumeable_H = 0.95f * G->CARD_H,
    };

    Table *starting_params = ttab(self->GAME, "starting_params");

    self->consumeables = NEW(CardArea);
    CardArea_init(self->consumeables, 0, 0, CAI.consumeable_W, CAI.consumeable_H,
                  TAB("card_limit", tget(starting_params, "consumable_slots"), "type", S("joker"), "highlight_limit", N(1)));

    self->jokers = NEW(CardArea);
    CardArea_init(self->jokers, 0, 0, CAI.joker_W, CAI.joker_H,
                  TAB("card_limit", tget(starting_params, "joker_slots"), "type", S("joker"), "highlight_limit", N(1)));

    self->discard = NEW(CardArea);
    CardArea_init(self->discard, 0, 0, CAI.discard_W, CAI.discard_H, TAB("card_limit", N(500), "type", S("discard")));
    self->deck = NEW(CardArea);
    CardArea_init(self->deck, 0, 0, CAI.deck_W, CAI.deck_H, TAB("card_limit", N(52), "type", S("deck")));
    self->hand = NEW(CardArea);
    CardArea_init(self->hand, 0, 0, CAI.hand_W, CAI.hand_H,
                  TAB("card_limit", tget(starting_params, "hand_size"), "type", S("hand")));
    self->play = NEW(CardArea);
    CardArea_init(self->play, 0, 0, CAI.play_W, CAI.play_H, TAB("card_limit", N(5), "type", S("play")));

    G->playing_cards = tnew();

    set_screen_positions();

    G->SPLASH_BACK = NEW(Sprite);
    Sprite_init(G->SPLASH_BACK, -30, -6, G->ROOM->T->w + 60, G->ROOM->T->h + 12, ASSET_ATLAS_get("ui_1"), TAB("x", N(2), "y", N(0)));
    Moveable_set_alignment((Moveable *)G->SPLASH_BACK, TAB(
        "major", O(G->play),
        "type", S("cm"),
        "bond", S("Strong"),
        "offset", T(TAB("x", N(0), "y", N(0)))));

    Table *spin = TAB(
        "amount", N(0),
        "real", N(0),
        "eased", N(0));
    tsett(G->ARGS, "spin", spin);

    // the send entries that point into G's structs use ref_float / ref_colour (game.h)
    Sprite_define_draw_steps(G->SPLASH_BACK, ARR(T(TAB(
        "shader", S("background"),
        "send", T(ARR(
            T(TAB("name", S("time"), "ref_num", P(&G->TIMERS.REAL_SHADER))),
            T(TAB("name", S("spin_time"), "ref_num", P(&G->TIMERS.BACKGROUND))),
            T(TAB("name", S("colour_1"), "ref_colour", P(G->C.BACKGROUND.C))),
            T(TAB("name", S("colour_2"), "ref_colour", P(G->C.BACKGROUND.L))),
            T(TAB("name", S("colour_3"), "ref_colour", P(G->C.BACKGROUND.D))),
            T(TAB("name", S("contrast"), "ref_float", P(&G->C.BACKGROUND.contrast))),
            T(TAB("name", S("spin_amount"), "ref_table", T(spin), "ref_value", S("amount")))))))));

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "immediate",
        .not_blocking = true,
        .not_blockable = true,
        .func = ev_spin}), NULL, false);

    if (saveTable) {
        Table *cardAreas = ttab(saveTable, "cardAreas");
        // note: pairs() order of the save's areas; each area loads independently
        for (TIter it = titer(cardAreas); tnext(&it);) {
            const char *k = vstr(it.key);
            CardArea **area = Game_cardarea(G, k);
            if (area && *area) CardArea_load(*area, it.val.tab);
            else {
                if (streq(k, "shop_jokers")) G->load_shop_jokers = it.val.tab;
                else if (streq(k, "shop_vouchers")) G->load_shop_vouchers = it.val.tab;
                else if (streq(k, "shop_booster")) G->load_shop_booster = it.val.tab;
                else tset(G->object.dyn, fmt("load_%s", k), it.val);
                printf("ERROR LOADING GAME: Card area '%s' not instantiated before load\n", k);
            }
        }

        for (TIter it = titer(G->I.CARD); tnext(&it);) {
            // note: only a Card has playing_card (the Blind in this list has none)
            if (it.val.t == V_OBJ && is_a(it.val.obj, &Card_class) && ((Card *)it.val.obj)->playing_card) {
                tinsert(G->playing_cards, it.val);
            }
        }
        for (TIter it = titer(G->I.CARDAREA); tnext(&it);) {
            CardArea *v = it.val.obj;
            CardArea_align_cards(v);
            CardArea_hard_set_cards(v);
        }
        tsort(G->playing_cards, playing_card_desc, NULL);
    } else {
        Table *card_protos = NULL;
        Table *_de = NULL;
        if (ttab(args, "challenge") && truthy(tget(ttab(args, "challenge"), "deck"))) {
            _de = ttab(ttab(args, "challenge"), "deck");
        }

        if (_de && truthy(tget(_de, "cards"))) {
            card_protos = ttab(_de, "cards");
        }

        if (!card_protos) {
            card_protos = tnew();
            // note: pairs() order of G.P_CARDS; the list is sorted below and equal keys are identical protos, and
            // the 'erratic' draws do not depend on which entry the loop is at
            for (TIter it = titer(self->P_CARDS); tnext(&it);) {
                const char *k = vstr(it.key);
                if (tbool(starting_params, "erratic_suits_and_ranks")) {
                    Val key = NIL;
                    pseudorandom_element(G->P_CARDS, N(pseudoseed("erratic", NULL)), &key);
                    k = vstr(key);
                }
                const char *_r = str_sub(k, 3, 3), *_s = str_sub(k, 1, 1);
                bool keep = true;
                Val _e = NIL, _d = NIL, _g = NIL;
                if (_de) {
                    if (truthy(tget(_de, "yes_ranks")) && !truthy(tget(ttab(_de, "yes_ranks"), _r))) keep = false;
                    if (truthy(tget(_de, "no_ranks")) && truthy(tget(ttab(_de, "no_ranks"), _r))) keep = false;
                    if (truthy(tget(_de, "yes_suits")) && !truthy(tget(ttab(_de, "yes_suits"), _s))) keep = false;
                    if (truthy(tget(_de, "no_suits")) && truthy(tget(ttab(_de, "no_suits"), _s))) keep = false;
                    if (truthy(tget(_de, "enhancement"))) _e = tget(_de, "enhancement");
                    if (truthy(tget(_de, "edition"))) _d = tget(_de, "edition");
                    if (truthy(tget(_de, "gold_seal"))) _g = tget(_de, "gold_seal");
                }

                if (tbool(starting_params, "no_faces") && (streq(_r, "K") || streq(_r, "Q") || streq(_r, "J"))) keep = false;

                if (keep) {
                    Table *proto = tnew();
                    tsets(proto, "s", _s);
                    tsets(proto, "r", _r);
                    tset(proto, "e", _e);
                    tset(proto, "d", _d);
                    tset(proto, "g", _g);
                    tseti(card_protos, tlen(card_protos) + 1, T(proto));
                }
            }
        }

        if (truthy(tget(starting_params, "extra_cards"))) {
            for (TIter it = titer(ttab(starting_params, "extra_cards")); tnext(&it);) {
                tseti(card_protos, tlen(card_protos) + 1, it.val);
            }
        }

        tsort(card_protos, card_proto_less, NULL);

        for (int k = 1; tgeti(card_protos, k).t != V_NIL; k++) {
            card_from_control(ttabi(card_protos, k));
        }

        tsetn(self->GAME, "starting_deck_size", tlen(G->playing_cards));
    }

    delay(0.5, NULL);

    if (!saveTable) {
        tset(ttab(G->GAME, "current_round"), "discards_left", tget(ttab(G->GAME, "round_resets"), "discards"));
        tset(ttab(G->GAME, "current_round"), "hands_left", tget(ttab(G->GAME, "round_resets"), "hands"));
        CardArea_shuffle(self->deck, NULL);
        CardArea_hard_set_T(self->deck, NAN, NAN, NAN, NAN);
        reset_idol_card();
        reset_mail_rank();
        tset(ttab(current_round, "ancient_card"), "suit", NIL);
        reset_ancient_card();
        reset_castle_card();
    }

    Blind *blind = NEW(Blind);
    Blind_init(blind, 0, 0, 2, 1);
    tseto(G->GAME, "blind", blind);
    CardArea_align_cards(self->deck);
    CardArea_hard_set_cards(self->deck);

    // note: the definition is built before the config, as the Lua's constructor evaluates them (C's argument
    // order is unspecified)
    Table *definition = create_UIBox_HUD();
    self->HUD = UIBox_new(TAB(
        "definition", T(definition),
        "config", T(TAB("align", S("cli"), "offset", T(TAB("x", N(-0.7), "y", N(0))), "major", O(G->ROOM_ATTACH)))));
    definition = create_UIBox_HUD_blind();
    UIElement *row_blind = UIBox_get_UIE_by_ID(G->HUD, S("row_blind"), NULL);
    self->HUD_blind = UIBox_new(TAB(
        "definition", T(definition),
        "config", T(TAB("major", O(row_blind), "align", S("cm"), "offset", T(TAB("x", N(0), "y", N(-10))), "bond", S("Weak")))));
    self->HUD_tags = tnew();

    G->hand_text_area = tnew();
    tseto(G->hand_text_area, "chips", UIBox_get_UIE_by_ID(self->HUD, S("hand_chips"), NULL));
    tseto(G->hand_text_area, "mult", UIBox_get_UIE_by_ID(self->HUD, S("hand_mult"), NULL));
    tseto(G->hand_text_area, "ante", UIBox_get_UIE_by_ID(self->HUD, S("ante_UI_count"), NULL));
    tseto(G->hand_text_area, "round", UIBox_get_UIE_by_ID(self->HUD, S("round_UI_count"), NULL));
    tseto(G->hand_text_area, "chip_total", UIBox_get_UIE_by_ID(self->HUD, S("hand_chip_total"), NULL));
    tseto(G->hand_text_area, "handname", UIBox_get_UIE_by_ID(self->HUD, S("hand_name"), NULL));
    tseto(G->hand_text_area, "hand_level", UIBox_get_UIE_by_ID(self->HUD, S("hand_level"), NULL));
    tseto(G->hand_text_area, "game_chips", UIBox_get_UIE_by_ID(self->HUD, S("chip_UI_count"), NULL));
    tseto(G->hand_text_area, "blind_chips", UIBox_get_UIE_by_ID(self->HUD_blind, S("HUD_blind_count"), NULL));
    tseto(G->hand_text_area, "blind_spacer", UIBox_get_UIE_by_ID(self->HUD, S("blind_spacer"), NULL));

    check_and_set_high_score("most_money", tnum(G->GAME, "dollars"));

    if (saveTable) {
        Blind_load(blind, ttab(saveTable, "BLIND"));
        tsett(G->GAME, "tags", tnew());
        Table *tags = ttab(saveTable, "tags");
        for (int k = 1; tgeti(tags, k).t != V_NIL; k++) {
            Tag *_tag = NEW(Tag);
            Tag_init(_tag, "tag_uncommon", false, NULL);
            Tag_load(_tag, ttabi(tags, k));
            add_tag(_tag);
        }
    } else {
        Blind_set_blind(blind, NULL, false, true);
        reset_blinds();
    }

    FUNCS_blind_chip_UI_scale(tobj(G->hand_text_area, "blind_chips"));

    UIBox_recalculate(self->HUD);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "immediate",
        .func = ev_unlock_notify}), NULL, false);
}
