// functions/misc_functions.lua:1-626: display info, boot bar, small table helpers, the seeded random functions,
// HEX, blind colours, poker hand evaluation, reset_drawhash.
#include "all.h"

// misc_functions.lua:5
int GET_DISPLAYINFO(const char *screenmode, int display) {
    Table *WINDOW = ttab(G->SETTINGS, "WINDOW");
    // :7 display = display or G.SETTINGS.WINDOW.selcted_display [sic] or 1
    if (!display) {
        Val sd = tget(WINDOW, "selcted_display");
        display = truthy(sd) ? (int)sd.n : 1;
    }
    // :10
    if (!screenmode) {
        Val sm = tget(WINDOW, "screenmode");
        screenmode = truthy(sm) ? sm.s : "Windowed";
    }

    int display_count = love_window_getDisplayCount();
    int res_option = 1;
    int realw, realh;
    love_window_getMode(&realw, &realh);
    Table *current_res_values = TAB("w", N(realw), "h", N(realh));

    // :19
    tsett(WINDOW, "display_names", tnew());

    for (int i = 1; i <= display_count; i++) {
        // :23-28
        Table *D = tnew();
        tseti(ttab(WINDOW, "DISPLAYS"), i, T(D));
        Table *strings = tnew(), *values = tnew();
        tsett(D, "screen_resolutions", TAB("strings", T(strings), "values", T(values)));
        tseti(ttab(WINDOW, "display_names"), i, S(fmt("%d", i)));

        // :31-32
        int render_w, render_h;
        love_window_getDesktopDimensions(&render_w, &render_h);
        // note: love.h gives the one fullscreen mode through out-pointers and returns the count of modes
        int mode_w, mode_h;
        int nmodes = love_window_getFullscreenModes(&mode_w, &mode_h);
        Table *unscaled_dims = TAB("width", N(mode_w), "height", N(mode_h));

        // :35-36
        tsetn(D, "DPI_scale", 1);
        tsett(D, "MONITOR_DIMS", unscaled_dims);

        if (streq(screenmode, "Fullscreen")) {
            // :40
            for (int m = 1; m <= nmodes; m++) {
                num v_width = mode_w, v_height = mode_h;
                num _w = v_width * tnum(D, "DPI_scale"), _h = v_height * tnum(D, "DPI_scale");
                if (_w <= tnum(ttab(D, "MONITOR_DIMS"), "width") && _h <= tnum(ttab(D, "MONITOR_DIMS"), "height")) {
                    tseti(strings, tlen(strings) + 1, S(fmt("%s X %s", number_tostring(v_width), number_tostring(v_height))));
                    tseti(values, tlen(values) + 1, T(TAB("w", N(v_width), "h", N(v_height))));
                    if (veq(N(i), tget(WINDOW, "selected_display")) && i == display && tnum(current_res_values, "w") == v_width &&
                        tnum(current_res_values, "h") == v_height) {
                        res_option = tlen(values);
                    }
                }
            }
        } else if (streq(screenmode, "Windowed")) {
            // :51-52
            tseti(strings, 1, S("-"));
            tseti(values, 1, T(TAB("w", N(1280), "h", N(720))));
        } else if (streq(screenmode, "Borderless")) {
            // :54-57
            tseti(strings, 1,
                  S(fmt("%s X %s", number_tostring(tnum(ttab(D, "MONITOR_DIMS"), "width") / tnum(D, "DPI_scale")),
                        number_tostring(tnum(ttab(D, "MONITOR_DIMS"), "height") / tnum(D, "DPI_scale")))));
            tseti(values, 1, T(current_res_values));
        }
    }
    return res_option;
}

// misc_functions.lua:63
void timer_checkpoint(const char *label, const char *type, bool reset) {
    // :64 G.PREV_GARB = G.PREV_GARB or 0: PREV_GARB is a num member, 0 from the start
    if (!G->F_ENABLE_PERF_OVERLAY) return;
    // :66-102 debug-only (the performance overlay's checkpoints, collectgarbage("count")): not translated
}

// misc_functions.lua:105
void boot_timer(const char *_label, const char *_next, float progress) {
    // :106 progress = progress or 0 (0 as nil)
    // :107 note: the stray `love.graphics.dis` of the constructor is a nil array item: nothing
    if (!G->LOADING) G->LOADING = TAB("font", P(love_graphics_setNewFont("resources/fonts/m6x11plus.ttf", 20)));
    int w, h;
    love_window_getMode(&w, &h);
    float realw = w, realh = h;
    love_graphics_setCanvas(NULL);
    love_graphics_push();
    love_graphics_setShader(NULL);
    love_graphics_clear(0, 0, 0, 1);
    love_graphics_setColor(0.6f, 0.8f, 0.9f, 1);
    // note: love.h's rectangle has no corner radius (the Lua passes 5)
    if (progress > 0) love_graphics_rectangle("fill", realw / 2 - 150, realh / 2 - 15, progress * 300, 30);
    love_graphics_setColor(1, 1, 1, 1);
    love_graphics_setLineWidth(3);
    love_graphics_rectangle("line", realw / 2 - 150, realh / 2 - 15, 300, 30);
    if (G->F_VERBOSE && !RELEASE_MODE) love_graphics_print(fmt("LOADING: %s", _next), realw / 2 - 150, realh / 2 + 40, 0, 1, 1);
    love_graphics_pop();
    love_graphics_present();

    // :125-126
    tsetn(G->ARGS, "bt", love_timer_getTime());
}

// misc_functions.lua:129
Table *EMPTY(Table *t) {
    if (!t) return tnew();
    tclear(t);
    return t;
}

// misc_functions.lua:137
Val interp(Val per, Val max, Val min) {
    num min_ = truthy(min) ? vnum(min) : 0;
    if (truthy(per) && truthy(max)) return N(vnum(per) * (vnum(max) - min_) + min_);
    return NIL;
}

// misc_functions.lua:144
void remove_all(Table *t) {
    for (int i = tlen(t); i >= 1; i--) {
        Val v = tgeti(t, i);
        tremove(t, i);
        if (truthy(v) && ((Node *)v.obj)->children) remove_all(((Node *)v.obj)->children);
        if (truthy(v)) CALL(v.obj, remove);
    }
    // note: pairs(t) while the nodes' remove may clear their own entry of t: titer walks the live table as the Lua's next
    for (TIter it = titer(t); tnext(&it);) {
        Val v = it.val;
        if (v.t != V_OBJ) ERROR("remove_all: a value that is not a node"); // the Lua's v:remove() fails the same way
        if (((Node *)v.obj)->children) remove_all(((Node *)v.obj)->children);
        CALL(v.obj, remove);
    }
}

// misc_functions.lua:161 (mid does not exist: see the header)
float Vector_Dist(Vec2 trans1, Vec2 trans2) {
    float x = trans1.x - trans2.x + 0;
    float y = trans1.y - trans2.y + 0;
    return sqrtf((x) * (x) + (y) * (y));
}

// misc_functions.lua:167
float Vector_Len(Vec2 trans1) {
    return sqrtf((trans1.x) * (trans1.x) + (trans1.y) * (trans1.y));
}

// misc_functions.lua:171
Vec2 Vector_Sub(Vec2 trans1, Vec2 trans2) {
    return (Vec2){trans1.x - trans2.x, trans1.y - trans2.y};
}

// misc_functions.lua:175
Val get_index(Table *t, Val val) {
    Val index = NIL;
    for (TIter it = titer(t); tnext(&it);) {
        if (veq(it.val, val)) index = it.key;
    }
    return index;
}

// misc_functions.lua:185
int table_length(Table *t) {
    int count = 0;
    for (TIter it = titer(t); tnext(&it);) count = count + 1;
    return count;
}

// misc_functions.lua:191
Table *remove_nils(Table *t) {
    Table *ans = tnew();
    for (TIter it = titer(t); tnext(&it);) tseti(ans, tlen(ans) + 1, it.val);
    return ans;
}

// misc_functions.lua:199
void SWAP(Table *t, Val i, Val j) {
    if (!t || !truthy(i) || !truthy(j)) return;
    Val temp = tgetv(t, i);
    tsetv(t, i, tgetv(t, j));
    tsetv(t, j, temp);
}

// v.sort_id of a value: a Card's member, another object's dyn field, a table's key; NIL when it has none
static Val sort_id_of(Val v) {
    if (v.t == V_OBJ) {
        if (is_a(v.obj, &Card_class)) return N(((Card *)v.obj)->sort_id);
        return tget(((Object *)v.obj)->dyn, "sort_id");
    }
    if (v.t == V_TAB) return tget(v.tab, "sort_id");
    return NIL;
}

// misc_functions.lua:210 function (a, b) return (a.sort_id or 1) < (b.sort_id or 2) end
static bool pseudoshuffle_less(Val a, Val b, void *ctx) {
    Val ia = sort_id_of(a), ib = sort_id_of(b);
    return (truthy(ia) ? vnum(ia) : 1) < (truthy(ib) ? vnum(ib) : 2);
}

// misc_functions.lua:206
void pseudoshuffle(Table *list, Val seed) {
    if (truthy(seed)) math_randomseed(seed.n);

    if (truthy(tgeti(list, 1)) && truthy(sort_id_of(tgeti(list, 1)))) {
        tsort(list, pseudoshuffle_less, NULL);
    }

    for (int i = tlen(list); i >= 2; i--) {
        int j = (int)math_random_to(i);
        Val li = tgeti(list, i), lj = tgeti(list, j);
        tseti(list, i, lj);
        tseti(list, j, li);
    }
}

// misc_functions.lua:219
const char *generate_starting_seed(void) {
#ifdef TEST_SEED // -DTEST_SEED='"ABC"' (EE): the host's FIXSEED, for a scripted run (the PS2 build has no environment)
    return str(TEST_SEED);
#endif
    if (getenv("FIXSEED")) return str(getenv("FIXSEED")); // host test hook only (tools/hook_ui.lua replaces the function the same way)
    ControllerCursor *ch = &G->CONTROLLER->cursor_hover;
    if (tnum(G->GAME, "stake") >= 8) {
        Table *r_leg = tnew(), *g_leg = tnew();
        num r_tally = 0, g_tally = 0;
        for (TIter it = titer(ttabi(G->P_JOKER_RARITY_POOLS, 4)); tnext(&it);) {
            Table *v = it.val.tab;
            Val win_ante = get_joker_win_sticker(v, true);
            if (truthy(win_ante) && (vnum(win_ante) >= 8)) {
                tset(g_leg, tstr(v, "key"), B(true));
                g_tally = g_tally + 1;
            } else {
                tset(r_leg, tstr(v, "key"), B(true));
                r_tally = r_tally + 1;
            }
        }
        if (r_tally > 0 && g_tally > 0) {
            const char *seed_found = NULL;
            num extra_num = 0;
            while (!seed_found) {
                extra_num = extra_num + 0.561892350821;
                seed_found = random_string(8, N(extra_num + ch->T.x * 0.33411983 + ch->T.y * 0.874146 + 0.412311010 * ch->time));
                if (!tbool(r_leg, get_first_legendary(seed_found))) seed_found = NULL;
            }
            return seed_found;
        }
    }

    return random_string(8, N(ch->T.x * 0.33411983 + ch->T.y * 0.874146 + 0.412311010 * ch->time));
}

// misc_functions.lua:248
const char *get_first_legendary(const char *_key) {
    Val _t = pseudorandom_element(ttabi(G->P_JOKER_RARITY_POOLS, 4), N(pseudoseed("Joker4", _key)), NULL);
    return tstr(_t.tab, "key");
}

// misc_functions.lua:261 function (a, b) return a.v.sort_id < b.v.sort_id end
static bool pre_less_sort_id(Val a, Val b, void *ctx) {
    return vnum(sort_id_of(tget(a.tab, "v"))) < vnum(sort_id_of(tget(b.tab, "v")));
}

// misc_functions.lua:263 function (a, b) return a.k < b.k end
static bool pre_less_k(Val a, Val b, void *ctx) {
    Val ka = tget(a.tab, "k"), kb = tget(b.tab, "k");
    if (ka.t == V_NUM && kb.t == V_NUM) return ka.n < kb.n;
    if (ka.t == V_STR && kb.t == V_STR) return strcmp(ka.s, kb.s) < 0; // bytewise, as the Lua under the C locale
    ERROR("attempt to compare %s with %s", lua_typename_of(ka), lua_typename_of(kb));
}

// misc_functions.lua:253
Val pseudorandom_element(Table *_t, Val seed, Val *key) {
    if (truthy(seed)) math_randomseed(seed.n);
    Table *keys = tnew();
    for (TIter it = titer(_t); tnext(&it);) {
        tseti(keys, tlen(keys) + 1, T(TAB("k", it.key, "v", it.val)));
    }

    Val k1 = tgeti(keys, 1);
    Val v1 = truthy(k1) ? tget(k1.tab, "v") : NIL;
    // note: type(v) == 'table' is a table or an object here
    if (truthy(k1) && truthy(v1) && (v1.t == V_TAB || v1.t == V_OBJ) && truthy(sort_id_of(v1))) {
        tsort(keys, pre_less_sort_id, NULL);
    } else {
        tsort(keys, pre_less_k, NULL);
    }

    // :266 an empty _t: math.random(0) / indexing nil
    if (tlen(keys) == 0) ERROR("pseudorandom_element: empty table");
    Val key_ = tget(ttabi(keys, (int)math_random_to(tlen(keys))), "k");
    if (key) *key = key_;
    return tgetv(_t, key_);
}

// misc_functions.lua:270
const char *random_string(int length, Val seed) {
    if (truthy(seed)) math_randomseed(seed.n);
    char *ret = calloc((size_t)(length > 0 ? length : 0) + 1, 1);
    for (int i = 1; i <= length; i++) {
        ret[i - 1] = (char)(math_random() > 0.7 ? math_random_range('1', '9')
                                                : (math_random() > 0.45 ? math_random_range('A', 'N') : math_random_range('P', 'Z')));
    }
    const char *out = str_upper(ret);
    free(ret);
    return out;
}

// misc_functions.lua:279 (the `else` branch after `if true` is dead code)
num pseudohash(const char *str) {
    num num_ = 1;
    for (int i = str_len(str); i >= 1; i--) {
        num_ = lmod((1.1239285023 / num_) * (unsigned char)str[i - 1] * M_PI + M_PI * i, 1);
    }
    return num_;
}

// math.abs(tonumber(string.format("%.13f", x))) (:303, :311)
// ponytail: relies on libc's snprintf/strtod rounding doubles correctly; an integer version (spec trap 1) if the PS2's do not
static num pseudoseed_round13(num x) {
    char buf[400];
    snprintf(buf, sizeof buf, "%.13f", x);
    return fabs(strtod(buf, NULL));
}

// pseudohash(a .. b) without keeping the joined string forever
static num pseudohash2(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *s = malloc(la + lb + 1);
    memcpy(s, a, la);
    memcpy(s + la, b, lb + 1);
    num h = pseudohash(s);
    free(s);
    return h;
}

// misc_functions.lua:298
num pseudoseed(const char *key, const char *predict_seed) {
    if (streq(key, "seed")) return math_random();

    if (predict_seed) {
        num _pseed = pseudohash2(key, predict_seed);
        _pseed = pseudoseed_round13(lmod(2.134453429141 + _pseed * 1.72431234, 1));
        return (_pseed + pseudohash(predict_seed)) / 2;
    }

    Table *PR = ttab(G->GAME, "pseudorandom");
    if (!truthy(tget(PR, key))) {
        Val seed = tget(PR, "seed");
        tsetn(PR, key, pseudohash2(key, truthy(seed) ? vstr(seed) : ""));
    }

    tsetn(PR, key, pseudoseed_round13(lmod(2.134453429141 + tnum(PR, key) * 1.72431234, 1)));
    Val hashed_seed = tget(PR, "hashed_seed");
    return (tnum(PR, key) + (truthy(hashed_seed) ? vnum(hashed_seed) : 0)) / 2;
}

// misc_functions.lua:315
num pseudorandom(Val seed, Val min, Val max) {
    if (seed.t == V_STR) seed = N(pseudoseed(seed.s, NULL));
    math_randomseed(seed.n);
    if (truthy(min) && truthy(max)) return math_random_range(vnum(min), vnum(max));
    else return math_random();
}

// misc_functions.lua:322
const char *tprint(Table *tbl, int indent) {
    const char *toprint = fmt("%s{\r\n", str_rep(" ", indent));
    indent = indent + 2;
    for (TIter it = titer(tbl); tnext(&it);) {
        Val k = it.key, v = it.val;
        toprint = fmt("%s%s", toprint, str_rep(" ", indent));
        if (k.t == V_NUM) {
            toprint = fmt("%s[%s] = ", toprint, number_tostring(k.n));
        } else if (k.t == V_STR) {
            toprint = fmt("%s%s= ", toprint, k.s);
        }
        if (v.t == V_NUM) {
            toprint = fmt("%s%s,\r\n", toprint, number_tostring(v.n));
        } else if (v.t == V_STR) {
            toprint = fmt("%s\"%s\",\r\n", toprint, v.s);
        } else if (v.t == V_TAB) {
            if (indent >= 10) {
                toprint = fmt("%s%s,\r\n", toprint, tostring(v));
            } else {
                toprint = fmt("%s%s%s,\r\n", toprint, tostring(v), tprint(v.tab, indent + 1));
            }
        } else {
            toprint = fmt("%s\"%s\",\r\n", toprint, tostring(v));
        }
    }
    toprint = fmt("%s%s}", toprint, str_rep(" ", indent - 2));
    return toprint;
}

// misc_functions.lua:351
bool sortingFunction(Val e1, Val e2, void *ctx) {
    return vnum(tget(e1.tab, "order")) < vnum(tget(e2.tab, "order"));
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// misc_functions.lua:355
float *HEX(const char *hex) {
    if (str_len(hex) <= 6) hex = fmt("%sFF", hex);
    // :357 hex:find('(%x%x)(%x%x)(%x%x)(%x%x)'): the first run of 8 hex digits, anywhere in the string
    int len = str_len(hex), at = -1;
    for (int s = 0; s + 8 <= len && at < 0; s++) {
        int n = 0;
        while (n < 8 && hex_digit(hex[s + n]) >= 0) n++;
        if (n == 8) at = s;
    }
    if (at < 0) ERROR("HEX: bad colour '%s'", hex); // the Lua's tonumber(nil, 16)
    float *color = malloc(sizeof(Colour));
    // :358 (the `or 255` is dead)
    for (int i = 0; i < 4; i++) color[i] = (hex_digit(hex[at + 2 * i]) * 16 + hex_digit(hex[at + 2 * i + 1])) / 255.0f;
    return color;
}

// misc_functions.lua:362
float *get_blind_main_colour(const char *blind) {
    bool disabled = false;
    if (!blind) blind = "";
    if (streq(blind, "Boss") || streq(blind, "Small") || streq(blind, "Big")) {
        Table *round_resets = ttab(G->GAME, "round_resets");
        if (!truthy(tget(round_resets, "blind_states"))) tsett(round_resets, "blind_states", tnew());
        const char *state = tstr(ttab(round_resets, "blind_states"), blind);
        if (streq(state, "Defeated") || streq(state, "Skipped")) disabled = true;
        blind = tstr(ttab(round_resets, "blind_choices"), blind); // NULL as nil: G.P_BLINDS[nil] is nil below
    }
    Table *P_BLIND = blind ? ttab(G->P_BLINDS, blind) : NULL;
    if (disabled || !P_BLIND) return G->C.BLACK;
    Val boss_colour = tget(P_BLIND, "boss_colour");
    if (truthy(boss_colour)) return boss_colour.p;
    if (streq(blind, "bl_small")) return mix_colours(G->C.BLUE, G->C.BLACK, 0.6f, NULL);
    if (streq(blind, "bl_big")) return mix_colours(G->C.ORANGE, G->C.BLACK, 0.6f, NULL);
    return G->C.BLACK;
}

// for i=1, #src do dst[#dst+1] = src[i] end
static void append_all(Table *dst, Table *src) {
    for (int i = 1; i <= tlen(src); i++) tseti(dst, tlen(dst) + 1, tgeti(src, i));
}

// {src[1], ..., src[n]} (nils leave holes, as the Lua's constructor)
static Table *first_n(Table *src, int n) {
    Table *t = tnew();
    for (int i = 1; i <= n; i++) {
        Val v = tgeti(src, i);
        if (v.t != V_NIL) tseti(t, i, v);
    }
    return t;
}

// results[name] = value; if not results.top then results.top = results[name] end
static void set_result(Table *results, const char *name, Table *value) {
    tsett(results, name, value);
    if (!truthy(tget(results, "top"))) tset(results, "top", tget(results, name));
}

// misc_functions.lua:376
Table *evaluate_poker_hand(Table *hand) {
    Table *results = TAB("Flush Five", T(tnew()), "Flush House", T(tnew()), "Five of a Kind", T(tnew()), "Straight Flush", T(tnew()),
                         "Four of a Kind", T(tnew()), "Full House", T(tnew()), "Flush", T(tnew()), "Straight", T(tnew()),
                         "Three of a Kind", T(tnew()), "Two Pair", T(tnew()), "Pair", T(tnew()), "High Card", T(tnew()));

    // :394 the constructor's order is the order of the calls (Stone cards' get_id draws from the generator)
    Table *_5 = get_X_same(5, hand);
    Table *_4 = get_X_same(4, hand);
    Table *_3 = get_X_same(3, hand);
    Table *_2 = get_X_same(2, hand);
    Table *_flush = get_flush(hand);
    Table *_straight = get_straight(hand);
    Table *_highest = get_highest(hand);

    // :404
    if (!tempty(_5) && !tempty(_flush)) {
        set_result(results, "Flush Five", _5);
    }

    // :409
    if (!tempty(_3) && !tempty(_2) && !tempty(_flush)) {
        Table *fh_hand = tnew();
        Table *fh_3 = ttabi(_3, 1);
        Table *fh_2 = ttabi(_2, 1);
        append_all(fh_hand, fh_3);
        append_all(fh_hand, fh_2);
        tinsert(ttab(results, "Flush House"), T(fh_hand));
        if (!truthy(tget(results, "top"))) tset(results, "top", tget(results, "Flush House"));
    }

    // :423
    if (!tempty(_5)) {
        set_result(results, "Five of a Kind", _5);
    }

    // :428
    if (!tempty(_flush) && !tempty(_straight)) {
        Table *_s = _straight, *_f = _flush, *ret = tnew();
        Table *f1 = ttabi(_f, 1), *s1 = ttabi(_s, 1);
        for (int i = 1; truthy(tgeti(f1, i)); i++) {
            tseti(ret, tlen(ret) + 1, tgeti(f1, i));
        }
        for (int i = 1; truthy(tgeti(s1, i)); i++) {
            Val v = tgeti(s1, i);
            bool in_straight = false;
            for (int k = 1; truthy(tgeti(f1, k)); k++) {
                if (veq(tgeti(f1, k), v)) in_straight = true;
            }
            if (!in_straight) tseti(ret, tlen(ret) + 1, v);
        }

        set_result(results, "Straight Flush", ARR(T(ret)));
    }

    // :445
    if (!tempty(_4)) {
        set_result(results, "Four of a Kind", _4);
    }

    // :450
    if (!tempty(_3) && !tempty(_2)) {
        Table *fh_hand = tnew();
        Table *fh_3 = ttabi(_3, 1);
        Table *fh_2 = ttabi(_2, 1);
        append_all(fh_hand, fh_3);
        append_all(fh_hand, fh_2);
        tinsert(ttab(results, "Full House"), T(fh_hand));
        if (!truthy(tget(results, "top"))) tset(results, "top", tget(results, "Full House"));
    }

    // :464
    if (!tempty(_flush)) {
        set_result(results, "Flush", _flush);
    }

    // :469
    if (!tempty(_straight)) {
        set_result(results, "Straight", _straight);
    }

    // :474
    if (!tempty(_3)) {
        set_result(results, "Three of a Kind", _3);
    }

    // :479
    if ((tlen(_2) == 2) || (tlen(_3) == 1 && tlen(_2) == 1)) {
        Table *fh_hand = tnew();
        Table *r = _2;
        Table *fh_2a = ttabi(r, 1);
        Table *fh_2b = ttabi(r, 2);
        if (!fh_2b) {
            fh_2b = ttabi(_3, 1);
        }
        append_all(fh_hand, fh_2a);
        append_all(fh_hand, fh_2b);
        tinsert(ttab(results, "Two Pair"), T(fh_hand));
        if (!truthy(tget(results, "top"))) tset(results, "top", tget(results, "Two Pair"));
    }

    // :497
    if (!tempty(_2)) {
        set_result(results, "Pair", _2);
    }

    // :502
    if (!tempty(_highest)) {
        set_result(results, "High Card", _highest);
    }

    // :507 the indices are groups, not cards; top is not touched
    if (truthy(tgeti(ttab(results, "Five of a Kind"), 1))) {
        tsett(results, "Four of a Kind", first_n(ttab(results, "Five of a Kind"), 4));
    }

    // :511
    if (truthy(tgeti(ttab(results, "Four of a Kind"), 1))) {
        tsett(results, "Three of a Kind", first_n(ttab(results, "Four of a Kind"), 3));
    }

    // :515
    if (truthy(tgeti(ttab(results, "Three of a Kind"), 1))) {
        tsett(results, "Pair", first_n(ttab(results, "Three of a Kind"), 2));
    }

    return results;
}

// misc_functions.lua:522
Table *get_flush(Table *hand) {
    Table *ret = tnew();
    bool four_fingers = !tempty(find_joker("Four Fingers", false));
    static const char *suits[] = {"Spades", "Hearts", "Clubs", "Diamonds"};
    if (tlen(hand) > 5 || tlen(hand) < (5 - (four_fingers ? 1 : 0))) return ret;
    else {
        for (int j = 1; j <= 4; j++) {
            Table *t = tnew();
            const char *suit = suits[j - 1];
            int flush_count = 0;
            for (int i = 1; i <= tlen(hand); i++) {
                if (Card_is_suit(tobji(hand, i), suit, false, true)) {
                    flush_count = flush_count + 1;
                    tseti(t, tlen(t) + 1, tgeti(hand, i));
                }
            }
            if (flush_count >= (5 - (four_fingers ? 1 : 0))) {
                tinsert(ret, T(t));
                return ret;
            }
        }
        return tnew();
    }
}

// misc_functions.lua:548
Table *get_straight(Table *hand) {
    Table *ret = tnew();
    bool four_fingers = !tempty(find_joker("Four Fingers", false));
    if (tlen(hand) > 5 || tlen(hand) < (5 - (four_fingers ? 1 : 0))) return ret;
    else {
        Table *t = tnew();
        Table *IDS = tnew();
        for (int i = 1; i <= tlen(hand); i++) {
            num id = Card_get_id(tobji(hand, i));
            if (id > 1 && id < 15) {
                if (ttabi(IDS, (int)id)) {
                    Table *l = ttabi(IDS, (int)id);
                    tseti(l, tlen(l) + 1, tgeti(hand, i));
                } else {
                    tseti(IDS, (int)id, T(ARR(tgeti(hand, i))));
                }
            }
        }

        int straight_length = 0;
        bool straight = false;
        bool can_skip = !tempty(find_joker("Shortcut", false));
        bool skipped_rank = false;
        for (int j = 1; j <= 14; j++) {
            Table *ids = ttabi(IDS, j == 1 ? 14 : j);
            if (ids) {
                straight_length = straight_length + 1;
                skipped_rank = false;
                for (int k = 1; truthy(tgeti(ids, k)); k++) {
                    tseti(t, tlen(t) + 1, tgeti(ids, k));
                }
            } else if (can_skip && !skipped_rank && j != 14) {
                skipped_rank = true;
            } else {
                straight_length = 0;
                skipped_rank = false;
                if (!straight) t = tnew();
                if (straight) break;
            }
            if (straight_length >= (5 - (four_fingers ? 1 : 0))) straight = true;
        }
        if (!straight) return ret;
        tinsert(ret, T(t));
        return ret;
    }
}

// misc_functions.lua:592
Table *get_X_same(num num_, Table *hand) {
    Table *vals = tnew();
    for (int i = 1; i <= 14; i++) tseti(vals, i, T(tnew()));
    for (int i = tlen(hand); i >= 1; i--) {
        Table *curr = tnew();
        tinsert(curr, tgeti(hand, i));
        for (int j = 1; j <= tlen(hand); j++) {
            // both get_id calls are made each time, left first (a Stone Card's draws from the generator)
            num id_i = Card_get_id(tobji(hand, i));
            num id_j = Card_get_id(tobji(hand, j));
            if (id_i == id_j && i != j) {
                tinsert(curr, tgeti(hand, j));
            }
        }
        if (tlen(curr) == num_) {
            tsetv(vals, N(Card_get_id(tobji(curr, 1))), T(curr));
        }
    }
    Table *ret = tnew();
    for (int i = tlen(vals); i >= 1; i--) {
        if (!tempty(ttabi(vals, i))) tinsert(ret, tgeti(vals, i));
    }
    return ret;
}

// misc_functions.lua:613
Table *get_highest(Table *hand) {
    Card *highest = NULL;
    for (int k = 1; truthy(tgeti(hand, k)); k++) {
        Card *v = tobji(hand, k);
        if (!highest || Card_get_nominal(v, NULL) > Card_get_nominal(highest, NULL)) {
            highest = v;
        }
    }
    if (tlen(hand) > 0) return ARR(T(ARR(O(highest))));
    else return tnew();
}

// misc_functions.lua:623
void reset_drawhash(void) {
    G->DRAW_HASH = EMPTY(G->DRAW_HASH);
}
