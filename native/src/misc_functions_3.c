// functions/misc_functions.lua:1259-1890: profile progress and discovery tallies, stop_use, the run save, the
// localization (parser, utf8, localize) and the small getters of stakes, fronts and challenges.
#include "all.h"
#include <ctype.h>

// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }

// t[rec][field] = t[rec][field] + by
static void tally_add(Table *t, const char *rec, const char *field, num by) {
    Table *r = ttab(t, rec);
    tsetn(r, field, tnum(r, field) + by);
}

// misc_functions.lua:1259
void set_profile_progress(void) {
    if (!G->PROGRESS)
        G->PROGRESS = TAB("joker_stickers", T(TAB("tally", N(0), "of", N(0))),
                          "deck_stakes", T(TAB("tally", N(0), "of", N(0))),
                          "challenges", T(TAB("tally", N(0), "of", N(0))));
    for (TIter it = titer(G->PROGRESS); tnext(&it);) {
        if (it.val.t == V_TAB) {
            tsetn(it.val.tab, "tally", 0);
            tsetn(it.val.tab, "of", 0);
        }
    }

    for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
        Table *v = it.val.tab;
        if (streq(tstr(v, "set"), "Back") && !tbool(v, "omit")) {
            tally_add(G->PROGRESS, "deck_stakes", "of", tlen(ttab(G->P_CENTER_POOLS, "Stake")));
            tally_add(G->PROGRESS, "deck_stakes", "tally", get_deck_win_stake(tstr(v, "key"), NULL));
        }
        if (streq(tstr(v, "set"), "Joker")) {
            tally_add(G->PROGRESS, "joker_stickers", "of", tlen(ttab(G->P_CENTER_POOLS, "Stake")));
            tally_add(G->PROGRESS, "joker_stickers", "tally", vnum(get_joker_win_sticker(v, true)));
        }
    }

    for (TIter it = titer(G->CHALLENGES); tnext(&it);) {
        Table *v = it.val.tab;
        tally_add(G->PROGRESS, "challenges", "of", 1);
        if (truthy(tgetv(ttab(ttab(cur_profile(), "challenge_progress"), "completed"), tget(v, "id"))))
            tally_add(G->PROGRESS, "challenges", "tally", 1);
    }

    Table *progress = ttab(cur_profile(), "progress");
    tsett(progress, "joker_stickers", copy_table(ttab(G->PROGRESS, "joker_stickers")));
    tsett(progress, "deck_stakes", copy_table(ttab(G->PROGRESS, "deck_stakes")));
    tsett(progress, "challenges", copy_table(ttab(G->PROGRESS, "challenges")));
}

// of + 1, and tally + 1 when counted
static void discover_count(const char *rec, bool counted) {
    tally_add(G->DISCOVER_TALLIES, rec, "of", 1);
    if (counted) tally_add(G->DISCOVER_TALLIES, rec, "tally", 1);
}

// misc_functions.lua:1295
void set_discover_tallies(void) {
    if (!G->DISCOVER_TALLIES)
        G->DISCOVER_TALLIES = TAB("blinds", T(TAB("tally", N(0), "of", N(0))),
                                  "tags", T(TAB("tally", N(0), "of", N(0))),
                                  "jokers", T(TAB("tally", N(0), "of", N(0))),
                                  "consumeables", T(TAB("tally", N(0), "of", N(0))),
                                  "tarots", T(TAB("tally", N(0), "of", N(0))),
                                  "planets", T(TAB("tally", N(0), "of", N(0))),
                                  "spectrals", T(TAB("tally", N(0), "of", N(0))),
                                  "vouchers", T(TAB("tally", N(0), "of", N(0))),
                                  "boosters", T(TAB("tally", N(0), "of", N(0))),
                                  "editions", T(TAB("tally", N(0), "of", N(0))),
                                  "backs", T(TAB("tally", N(0), "of", N(0))),
                                  "total", T(TAB("tally", N(0), "of", N(0))));
    for (TIter it = titer(G->DISCOVER_TALLIES); tnext(&it);) {
        tsetn(it.val.tab, "tally", 0);
        tsetn(it.val.tab, "of", 0);
    }

    for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
        Table *v = it.val.tab;
        if (!tbool(v, "omit")) {
            const char *set = tstr(v, "set"); // v.set: a string or nil
            bool discovered = tbool(v, "discovered");
            if (set && (streq(set, "Joker") || tbool(v, "consumeable") || streq(set, "Edition") || streq(set, "Voucher") ||
                        streq(set, "Back") || streq(set, "Booster")))
                discover_count("total", discovered);
            if (set && streq(set, "Joker")) discover_count("jokers", discovered);
            if (set && streq(set, "Back")) discover_count("backs", tbool(v, "unlocked"));
            if (set && tbool(v, "consumeable")) {
                discover_count("consumeables", discovered);
                if (streq(set, "Planet")) discover_count("planets", discovered);
                else if (streq(set, "Spectral")) discover_count("spectrals", discovered);
                else if (streq(set, "Tarot")) discover_count("tarots", discovered);
            }
            if (set && streq(set, "Voucher")) discover_count("vouchers", discovered);
            if (set && streq(set, "Booster")) discover_count("boosters", discovered);
            if (set && streq(set, "Edition")) discover_count("editions", discovered);
        }
    }
    for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
        tally_add(G->DISCOVER_TALLIES, "total", "of", 1);
        tally_add(G->DISCOVER_TALLIES, "blinds", "of", 1);
        if (tbool(it.val.tab, "discovered")) {
            tally_add(G->DISCOVER_TALLIES, "blinds", "tally", 1);
            tally_add(G->DISCOVER_TALLIES, "total", "tally", 1);
        }
    }
    for (TIter it = titer(G->P_TAGS); tnext(&it);) {
        tally_add(G->DISCOVER_TALLIES, "total", "of", 1);
        tally_add(G->DISCOVER_TALLIES, "tags", "of", 1);
        if (tbool(it.val.tab, "discovered")) {
            tally_add(G->DISCOVER_TALLIES, "tags", "tally", 1);
            tally_add(G->DISCOVER_TALLIES, "total", "tally", 1);
        }
    }
    Table *total = ttab(G->DISCOVER_TALLIES, "total");
    Table *collection = ttab(ttab(cur_profile(), "high_scores"), "collection");
    tsetn(collection, "amt", tnum(total, "tally"));
    tsetn(collection, "tot", tnum(total, "of"));
    tsett(ttab(cur_profile(), "progress"), "discovered", copy_table(total));

    // (`if check_for_unlock then`: the function always exists)
    check_for_unlock(TAB("type", S("discover_amount"),
                         "amount", N(tnum(total, "tally")),
                         "planet_count", N(tnum(ttab(G->DISCOVER_TALLIES, "planets"), "tally")),
                         "tarot_count", N(tnum(ttab(G->DISCOVER_TALLIES, "tarots"), "tally"))));
}

// misc_functions.lua:1404
void stop_use(void) {
    Val su = tget(G->GAME, "STOP_USE");
    tsetn(G->GAME, "STOP_USE", (truthy(su) ? su.n : 0) + 1);
    dec_stop_use(6);
}

// misc_functions.lua:1414 (ctx: _depth itself, as an integer)
static bool ev_dec_stop_use(void *ctx) {
    dec_stop_use((int)(intptr_t)ctx - 1);
    return true;
}
// misc_functions.lua:1421
static bool ev_dec_stop_use_end(void *ctx) {
    tsetn(G->GAME, "STOP_USE", lmax(tnum(G->GAME, "STOP_USE") - 1, 0));
    return true;
}

// misc_functions.lua:1409
void dec_stop_use(int _depth) {
    if (_depth > 0) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.not_blocking = true, .no_delete = true, .func = ev_dec_stop_use,
                                                 .ctx = (void *)(intptr_t)_depth}), NULL, false);
    } else {
        E_add(G->E_MANAGER, Event_new((EventCfg){.not_blocking = true, .no_delete = true, .func = ev_dec_stop_use_end}), NULL, false);
    }
}

// misc_functions.lua:1427
void inc_career_stat(const char *stat, num mod) {
    if (tbool(G->GAME, "seeded") || tbool(G->GAME, "challenge")) return;
    Table *career_stats = ttab(cur_profile(), "career_stats");
    if (!truthy(tget(career_stats, stat))) tsetn(career_stats, stat, 0);
    tsetn(career_stats, stat, tnum(career_stats, stat) + mod);
    Game_save_settings(G);
}

// misc_functions.lua:1434
// note: `type(v) == 'table'` covers V_TAB and V_OBJ; `v.is and v:is(Object)` is V_OBJ (every class extends Object).
// A colour kept as P(float *) is copied as a new table of its 4 numbers, as the Lua copies the colour's table:
// the only raw pointers in the tables that get saved are colours (card.base.colour); STR_PACK cannot write a pointer.
Table *recursive_table_cull(Table *t) {
    Table *ret_t = tnew();
    for (TIter it = titer(t); tnext(&it);) {
        if (it.val.t == V_OBJ) tsetv(ret_t, it.key, S("\"MANUAL_REPLACE\""));
        else if (it.val.t == V_TAB) tsetv(ret_t, it.key, T(recursive_table_cull(it.val.tab)));
        else if (it.val.t == V_PTR) {
            const float *c = it.val.p;
            tsetv(ret_t, it.key, T(ARR(N(c[0]), N(c[1]), N(c[2]), N(c[3]))));
        } else tsetv(ret_t, it.key, it.val);
    }
    return ret_t;
}

// misc_functions.lua:1446
void save_with_action(Table *action) {
    G->action = action;
    save_run();
    G->action = NULL;
}

// misc_functions.lua:1452
void save_run(void) {
    if (G->F_NO_SAVING == true) return;
    Table *cardAreas = tnew();
    // note: pairs(G) looking for CardAreas is game.h's list of the card area members, then title_top,
    // your_collection when it is one area, and G's dyn; the order does not matter (a map by name).
    for (int i = 0; i <= G_CARDAREA_COUNT; i++) {
        const char *k = i < G_CARDAREA_COUNT ? G_CARDAREA_NAMES[i] : "title_top";
        CardArea *v = *Game_cardarea(G, k);
        if (v) {
            Table *cardAreaSer = CardArea_save(v);
            if (cardAreaSer) tsett(cardAreas, k, cardAreaSer);
        }
    }
    if (G->your_collection.t == V_OBJ && is_a(G->your_collection.obj, &CardArea_class)) {
        Table *cardAreaSer = CardArea_save(G->your_collection.obj);
        if (cardAreaSer) tsett(cardAreas, "your_collection", cardAreaSer);
    }
    for (TIter it = titer(G->object.dyn); tnext(&it);) {
        if (it.val.t == V_OBJ && is_a(it.val.obj, &CardArea_class)) {
            Table *cardAreaSer = CardArea_save(it.val.obj);
            if (cardAreaSer) tsetv(cardAreas, it.key, T(cardAreaSer));
        }
    }

    Table *tags = tnew();
    Table *game_tags = ttab(G->GAME, "tags");
    for (int k = 1; tgeti(game_tags, k).t != V_NIL; k++) {
        Val v = tgeti(game_tags, k);
        if (v.t == V_OBJ && is_a(v.obj, &Tag_class)) {
            Table *tagSer = Tag_save(v.obj);
            if (tagSer) tseti(tags, k, T(tagSer));
        }
    }

    G->culled_table = recursive_table_cull(TAB("cardAreas", T(cardAreas),
                                               "tags", T(tags),
                                               "GAME", T(G->GAME),
                                               "STATE", N(G->STATE),
                                               "ACTION", T(G->action),
                                               "BLIND", T(Blind_save(tobj(G->GAME, "blind"))),
                                               "BACK", T(Back_save(tobj(G->GAME, "selected_back"))),
                                               "VERSION", S(G->VERSION)));
    // note: the culled copy of G.GAME.hands is filled key by key in the Lua, so its pairs order (the order the save
    // is written in, and so what the loaded table's order comes from) is not G.GAME.hands' own: see lua51_pairs_order
    Table *culled_GAME = ttab(G->culled_table, "GAME");
    if (ttab(culled_GAME, "hands")) tsett(culled_GAME, "hands", lua51_pairs_order(ttab(culled_GAME, "hands"), false));
    tsett(G->ARGS, "save_run", G->culled_table);

    if (!G->FILE_HANDLER) G->FILE_HANDLER = tnew();
    tsetb(G->FILE_HANDLER, "run", true);
    tsetb(G->FILE_HANDLER, "update_queued", true);
}

// misc_functions.lua:1487
void remove_save(void) {
    love_filesystem_remove(fmt("%s/save.jkr", vstr(tget(G->SETTINGS, "profile"))));
    G->SAVED_GAME = NULL;
    if (!G->FILE_HANDLER) ERROR("remove_save: attempt to index field 'FILE_HANDLER' (a nil value)");
    tset(G->FILE_HANDLER, "run", NIL);
}

// misc_functions.lua:1493
float *loc_colour(const char *_c, float *_default) {
    if (!ttab(G->ARGS, "LOC_COLOURS"))
        tsett(G->ARGS, "LOC_COLOURS",
              TAB("red", P(G->C.RED),
                  "mult", P(G->C.MULT),
                  "blue", P(G->C.BLUE),
                  "chips", P(G->C.CHIPS),
                  "green", P(G->C.GREEN),
                  "money", P(G->C.MONEY),
                  "gold", P(G->C.GOLD),
                  "attention", P(G->C.FILTER),
                  "purple", P(G->C.PURPLE),
                  "white", P(G->C.WHITE),
                  "inactive", P(G->C.UI.TEXT_INACTIVE),
                  "spades", P(G->C.SUITS.Spades),
                  "hearts", P(G->C.SUITS.Hearts),
                  "clubs", P(G->C.SUITS.Clubs),
                  "diamonds", P(G->C.SUITS.Diamonds),
                  "tarot", P(G->C.SECONDARY_SET.Tarot),
                  "planet", P(G->C.SECONDARY_SET.Planet),
                  "spectral", P(G->C.SECONDARY_SET.Spectral),
                  "edition", P(G->C.EDITION),
                  "dark_edition", P(G->C.DARK_EDITION),
                  "legendary", P(G->C.RARITY[4]),
                  "enhanced", P(G->C.SECONDARY_SET.Enhanced)));
    Val v = _c ? tget(ttab(G->ARGS, "LOC_COLOURS"), _c) : NIL;
    return v.t == V_PTR ? (float *)v.p : _default ? _default : G->C.UI.TEXT_DARK;
}

// {multi_line = true, [kk] = loc_parse_string(vv) for kk, vv in ipairs(v)} (multi_line false: without that key)
static Table *parse_lines(Table *v, bool multi_line) {
    Table *t = multi_line ? TAB("multi_line", B(true)) : tnew();
    for (int kk = 1; tgeti(v, kk).t != V_NIL; kk++) tseti(t, kk, T(loc_parse_string(vstr(tgeti(v, kk)))));
    return t;
}

// t[#t+1] = loc_parse_string(line) for every line of ipairs(lines)
static Table *parse_append(Table *lines) {
    Table *t = tnew();
    for (int i = 1; tgeti(lines, i).t != V_NIL; i++) {
        Table *parsed = loc_parse_string(vstr(tgeti(lines, i)));
        if (parsed) tinsert(t, T(parsed));
    }
    return t;
}

// misc_functions.lua:1521
void init_localization(void) {
    Table *misc = ttab(G->localization, "misc");
    Table *parsed = tnew();
    tsett(misc, "v_dictionary_parsed", parsed);
    for (TIter it = titer(ttab(misc, "v_dictionary")); tnext(&it);) {
        if (it.val.t == V_TAB) tsetv(parsed, it.key, T(parse_lines(it.val.tab, true)));
        else tsetv(parsed, it.key, T(loc_parse_string(vstr(it.val))));
    }
    parsed = tnew();
    tsett(misc, "v_text_parsed", parsed);
    for (TIter it = titer(ttab(misc, "v_text")); tnext(&it);) tsetv(parsed, it.key, T(parse_lines(it.val.tab, false)));
    parsed = tnew();
    tsett(G->localization, "tutorial_parsed", parsed);
    for (TIter it = titer(ttab(misc, "tutorial")); tnext(&it);) tsetv(parsed, it.key, T(parse_lines(it.val.tab, true)));
    parsed = tnew();
    tsett(G->localization, "quips_parsed", parsed);
    for (TIter it = titer(ttab(misc, "quips")); tnext(&it);) tsetv(parsed, it.key, T(parse_lines(it.val.tab, true)));
    // (for g_k, group in pairs(G.localization) do if g_k == 'descriptions')
    for (TIter set = titer(ttab(G->localization, "descriptions")); tnext(&set);) {
        for (TIter c = titer(set.val.tab); tnext(&c);) {
            Table *center = c.val.tab;
            tsett(center, "text_parsed", tnew());
            if (!truthy(tget(center, "text"))) {
            } else {
                tsett(center, "text_parsed", parse_append(ttab(center, "text")));
                Val name = tget(center, "name");
                tsett(center, "name_parsed", parse_append(name.t == V_TAB ? name.tab : ARR(name)));
                if (truthy(tget(center, "unlock"))) tsett(center, "unlock_parsed", parse_append(ttab(center, "unlock")));
            }
        }
    }
}

// misc_functions.lua:1580
void playing_card_joker_effects(Table *cards) {
    for (int i = 1, n = CardArea_n(G->jokers); i <= n; i++)
        Card_calculate_joker(CardArea_card(G->jokers, i), TAB("playing_card_added", B(true), "cards", T(cards)));
}

// misc_functions.lua:1593-1616 dst[key] = true for every line of the file, without its whitespace, that is not empty
static void meta_lines(Table *dst, const char *file) {
    if (!love_filesystem_getInfo(file, NULL)) return;
    const char *text = get_compressed(file);
    if (!text) text = "";
    char *key = malloc(strlen(text) + 1);
    for (const char *p = text;;) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p), n = 0;
        for (size_t i = 0; i < len; i++)
            if (!isspace((unsigned char)p[i])) key[n++] = p[i];
        key[n] = 0;
        if (n) tsetb(dst, key, true);
        if (!e) break;
        p = e + 1;
    }
    free(key);
}

// misc_functions.lua:1586
void convert_save_to_meta(void) {
    const char *profile = vstr(tget(G->SETTINGS, "profile"));
    if (love_filesystem_getInfo(fmt("%s/%s", profile, "unlocked_jokers.jkr"), NULL)) {
        Table *unlocked = tnew(), *alerted = tnew(), *discovered = tnew();
        Table *_meta = TAB("unlocked", T(unlocked), "alerted", T(alerted), "discovered", T(discovered));
        meta_lines(unlocked, fmt("%s/%s", profile, "unlocked_jokers.jkr"));
        meta_lines(discovered, fmt("%s/%s", profile, "discovered_jokers.jkr"));
        meta_lines(alerted, fmt("%s/%s", profile, "alerted_jokers.jkr"));
        love_filesystem_remove(fmt("%s/%s", profile, "unlocked_jokers.jkr"));
        love_filesystem_remove(fmt("%s/%s", profile, "discovered_jokers.jkr"));
        love_filesystem_remove(fmt("%s/%s", profile, "alerted_jokers.jkr"));

        compress_and_save(fmt("%s/%s", profile, "meta.jkr"), STR_PACK(_meta, false));
    }
}

// misc_functions.lua:1625
void card_from_control(Table *control) {
    G->playing_card = G->playing_card + 1; // 0 as nil: (G.playing_card and G.playing_card + 1) or 1
    Val e = tget(control, "e");
    Card *_card = NEW(Card);
    Card_init(_card, G->deck->moveable.node.T->x, G->deck->moveable.node.T->y, G->CARD_W, G->CARD_H,
              ttab(G->P_CARDS, fmt("%s_%s", vstr(tget(control, "s")), vstr(tget(control, "r")))),
              ttab(G->P_CENTERS, truthy(e) ? e.s : "c_base"), TAB("playing_card", N(G->playing_card)));
    if (truthy(tget(control, "d"))) Card_set_edition(_card, TAB(tstr(control, "d"), B(true)), true, true);
    if (truthy(tget(control, "g"))) Card_set_seal(_card, tstr(control, "g"), true, true);
    CardArea_emplace(G->deck, _card, NULL, false);
    tinsert(G->playing_cards, O(_card));
}

// misc_functions.lua:1634
// note: the strings the Lua grows one character at a time (_c_name, _c_val, _s_ref, str_parts[str_it]) are local
// buffers here, written to the tables when the Lua would next read them; `set` is "not nil".
Table *loc_parse_string(const char *line) {
    int len = (int)strlen(line);
    if (len == 0) return NULL; // the loop never runs: nil
    Table *parsed_line = tnew();
    Table *control = tnew();
    bool _c = false, _c_gather = false, _s_gather = false;
    Table *str_parts = tnew();
    int str_it = 1;
    // 4 buffers of len + 1: the current str_parts[str_it], _c_name, _c_val, _s_ref
    char *bufs = malloc(4 * ((size_t)len + 1));
    char *cur = bufs, *_c_name = cur + len + 1, *_c_val = _c_name + len + 1, *_s_ref = _c_val + len + 1;
    int cur_n = 0, name_n = 0, val_n = 0, ref_n = 0;
    bool cur_set = false, name_set = false, val_set = false, ref_set = false;
#define FLUSH_CUR() do { if (cur_set) { cur[cur_n] = 0; tseti(str_parts, str_it, S(cur)); } cur_set = false, cur_n = 0; } while (0)
// if str_parts[1] then parsed_line[#parsed_line+1] = {strings = str_parts, control = control or {}} end
#define EMIT() do { bool has = str_it > 1 || cur_set; FLUSH_CUR(); \
        if (has) tinsert(parsed_line, T(TAB("strings", T(str_parts), "control", T(control)))); } while (0)
    for (int i = 1; i <= len; i++) {
        char ch = line[i - 1];
        if (ch == '{') { // Start of a control section, extract all controls
            EMIT();
            str_parts = tnew(), str_it = 1;
            control = tnew(), _c = false, name_set = false, name_n = 0, val_set = false, val_n = 0, _c_gather = false;
            _s_gather = false, ref_set = false, ref_n = 0;
            _c = true;
        } else if (_c && !(ch == ':' || ch == '}') && !_c_gather) {
            _c_name[name_n++] = ch, name_set = true;
        } else if (_c && ch == ':') {
            _c_gather = true;
        } else if (_c && !(ch == ',' || ch == '}') && _c_gather) {
            _c_val[val_n++] = ch, val_set = true;
        } else if (_c && (ch == ',' || ch == '}')) {
            _c_gather = false;
            if (name_set) {
                _c_name[name_n] = 0, _c_val[val_n] = 0;
                tset(control, _c_name, val_set ? S(_c_val) : NIL);
            }
            name_set = false, name_n = 0;
            val_set = false, val_n = 0;
            if (ch == '}') _c = false;

        } else if (!_c && ch != '#' && !_s_gather) {
            // control['X'] and char:gsub("%s+", "") or char: a whitespace under X appends '' (the string still exists)
            if (!(truthy(tget(control, "X")) && isspace((unsigned char)ch))) cur[cur_n++] = ch;
            cur_set = true;
        } else if (!_c && ch == '#' && !_s_gather) {
            _s_gather = true;
            if (cur_set) {
                FLUSH_CUR();
                str_it = str_it + 1;
            }
        } else if (!_c && ch == '#' && _s_gather) {
            _s_gather = false;
            if (ref_set) {
                _s_ref[ref_n] = 0;
                tseti(str_parts, str_it, T(ARR(S(_s_ref))));
                str_it = str_it + 1;
                ref_set = false, ref_n = 0;
            }
        } else if (!_c && _s_gather) {
            _s_ref[ref_n++] = ch, ref_set = true;
        }
        if (i == len) {
            EMIT();
            free(bufs);
            return parsed_line;
        }
    }
#undef FLUSH_CUR
#undef EMIT
    return NULL; // not reached
}

// misc_functions.lua:1684 utf8.chars(s, no_subs)
Utf8Iter utf8_chars(const char *s, bool no_subs) {
    return (Utf8Iter){.s = s, .no_subs = no_subs};
}

// The next match of utf8.pattern: '[%z\1-\127\194-\244][\128-\191]*'
bool utf8_next(Utf8Iter *it) {
    const unsigned char *s = (const unsigned char *)it->s;
    while (s[it->at]) {
        unsigned char ch = s[it->at];
        if (ch <= 127 || (ch >= 194 && ch <= 244)) {
            int start = it->at;
            it->at++;
            while (s[it->at] >= 128 && s[it->at] <= 191) it->at++;
            it->i = it->i + 1;
            it->len = it->at - start;
            it->b = start + 1;
            it->c = it->no_subs ? NULL : str_sub(it->s, start + 1, it->at);
            return true;
        }
        it->at++; // not a lead byte: gmatch moves on
    }
    return false;
}

// misc_functions.lua:1667 utf8.map
void utf8_map(const char *s, Utf8Fn f, void *ctx, bool no_subs) {
    for (Utf8Iter it = utf8_chars(s, no_subs); utf8_next(&it);) f(it.i, it.c, it.len, it.b, ctx);
}

// ---------------------------------------------------------------- localize
// A string grown by `..`: a local buffer, interned once at the end (no string is left behind per step)
typedef struct { char *s; size_t n, cap; } SBuf;
static void sb_add(SBuf *b, Val v) {
    const char *s = vstr(v); // a number is written as tostring does
    if (!s) ERROR("localize: attempt to concatenate a %s value", lua_typename_of(v));
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        b->cap = (b->n + l + 1) * 2;
        b->s = realloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, s, l + 1);
    b->n += l;
}
static Val sb_done(SBuf *b) {
    Val v = S(b->s ? b->s : "");
    free(b->s);
    *b = (SBuf){0};
    return v;
}

// for _, subpart in ipairs(strings) do out = out..(type(subpart) == 'string' and subpart or
// args.vars[tonumber(subpart[1])] [or 'ERROR']) end
static void loc_assemble(SBuf *out, Table *args, Table *strings, bool or_error) {
    if (!strings) ERROR("localize: bad argument #1 to 'ipairs' (table expected, got nil)");
    for (int i = 1; tgeti(strings, i).t != V_NIL; i++) {
        Val subpart = tgeti(strings, i);
        Val v = subpart;
        if (subpart.t != V_STR) {
            Table *vars = ttab(args, "vars");
            if (!vars) ERROR("localize: attempt to index field 'vars' (a nil value)");
            bool ok;
            num n = tonumber(tgeti(subpart.tab, 1), &ok);
            v = ok ? tgetv(vars, N(n)) : NIL;
        }
        if (!truthy(v) && or_error) v = S("ERROR");
        sb_add(out, v);
    }
}

// part.control.V and args.vars.colours[tonumber(part.control.V)]: P(float *), or a falsy value
static Val loc_colV(Table *args, Table *control) {
    Val V = tget(control, "V");
    if (!truthy(V)) return V;
    Table *colours = ttab(ttab(args, "vars"), "colours");
    if (!colours) ERROR("localize: attempt to index args.vars.colours (a nil value)");
    bool ok;
    num n = tonumber(V, &ok);
    return ok ? tgetv(colours, N(n)) : NIL;
}

// The table of a value, NULL for anything else
static Table *vtab(Val v) { return v.t == V_TAB ? v.tab : NULL; }

// G.localization.descriptions[set][key]; NULL as nil. A set that does not exist is the Lua's error.
static Table *loc_description(Val set, Val key) {
    Val st = tgetv(ttab(G->localization, "descriptions"), set);
    if (st.t != V_TAB) ERROR("localize: attempt to index descriptions[%s] (a nil value)", tostring(set));
    Val entry = tgetv(st.tab, key);
    return entry.t == V_TAB ? entry.tab : NULL;
}

// misc_functions.lua:1689
Val localize(Val args_, const char *misc_cat) {
    Table *misc = ttab(G->localization, "misc");
    if (truthy(args_) && !(args_.t == V_TAB)) {
        if (misc_cat && truthy(tget(misc, misc_cat))) {
            Val v = tgetv(ttab(misc, misc_cat), args_);
            return truthy(v) ? v : S("ERROR");
        }
        Val v = tgetv(ttab(misc, "dictionary"), args_);
        return truthy(v) ? v : S("ERROR");
    }
    if (args_.t != V_TAB) ERROR("localize: attempt to index local 'args' (a %s value)", lua_typename_of(args_));
    Table *args = args_.tab;
    const char *type = tstr(args, "type");

    Table *loc_target = NULL;
    Val ret_string = NIL;
    if (streq(type, "other")) {
        loc_target = ttab(ttab(G->localization, "descriptions"), "Other");
        Val e = tgetv(loc_target, tget(args, "key"));
        loc_target = e.t == V_TAB ? e.tab : NULL;
    } else if (streq(type, "descriptions") || streq(type, "unlocks")) {
        loc_target = loc_description(tget(args, "set"), tget(args, "key"));
    } else if (streq(type, "tutorial")) {
        loc_target = vtab(tgetv(ttab(G->localization, "tutorial_parsed"), tget(args, "key")));
    } else if (streq(type, "quips")) {
        loc_target = vtab(tgetv(ttab(G->localization, "quips_parsed"), tget(args, "key")));
    } else if (streq(type, "raw_descriptions")) {
        loc_target = loc_description(tget(args, "set"), tget(args, "key"));
        Table *multi_line = tnew();
        if (loc_target) {
            // note: of the Lua's `unlocks and .. or name and .. or text and .. or loc_target.text_parsed` only the
            // last can hold here (args.type is 'raw_descriptions')
            Table *lines_all = ttab(loc_target, "text_parsed");
            if (!lines_all) ERROR("localize: bad argument #1 to 'ipairs' (table expected, got nil)");
            for (int li = 1; tgeti(lines_all, li).t != V_NIL; li++) {
                Table *lines = ttabi(lines_all, li);
                SBuf final_line = {0};
                for (int pi = 1; tgeti(lines, pi).t != V_NIL; pi++) {
                    SBuf assembled_string = {0};
                    loc_assemble(&assembled_string, args, ttab(ttabi(lines, pi), "strings"), true);
                    sb_add(&final_line, sb_done(&assembled_string));
                }
                tinsert(multi_line, sb_done(&final_line));
            }
        }
        return T(multi_line);
    } else if (streq(type, "text")) {
        loc_target = vtab(tgetv(ttab(misc, "v_text_parsed"), tget(args, "key")));
    } else if (streq(type, "variable")) {
        Val lt = tgetv(ttab(misc, "v_dictionary_parsed"), tget(args, "key"));
        if (!truthy(lt)) return S("ERROR");
        loc_target = lt.tab;
        if (tbool(loc_target, "multi_line")) {
            Table *assembled_strings = tnew();
            for (int k = 1; tgeti(loc_target, k).t != V_NIL; k++) {
                SBuf assembled_string = {0};
                loc_assemble(&assembled_string, args, ttab(ttabi(ttabi(loc_target, k), 1), "strings"), false);
                tseti(assembled_strings, k, sb_done(&assembled_string));
            }
            return T(assembled_strings);
        } else {
            SBuf assembled_string = {0};
            loc_assemble(&assembled_string, args, ttab(ttabi(loc_target, 1), "strings"), false);
            ret_string = sb_done(&assembled_string);
        }
    } else if (streq(type, "name_text")) {
        // the pcall: any nil on the way gives "ERROR"; a nil name falls through
        Node *node = tobj(args, "node");
        Table *center = node ? ttab(node->config, "center") : NULL;
        Val set = tget(args, "set"), key = tget(args, "key");
        bool ok = true;
        if (!truthy(set)) {
            if (center) set = tget(center, "set");
            else ok = false;
        }
        Val st = ok ? tgetv(ttab(G->localization, "descriptions"), set) : NIL;
        if (st.t != V_TAB) ok = false;
        if (ok && !truthy(key)) {
            if (center) key = tget(center, "key");
            else ok = false;
        }
        Val entry = ok ? tgetv(st.tab, key) : NIL;
        if (entry.t != V_TAB) ok = false;
        if (ok) ret_string = tget(entry.tab, "name");
        else ret_string = S("ERROR");
    } else if (streq(type, "name")) {
        Node *node = tobj(args, "node");
        Table *center = node ? ttab(node->config, "center") : NULL;
        Val set = tget(args, "set"), key = tget(args, "key");
        if ((!truthy(set) || !truthy(key)) && !center) ERROR("localize: attempt to index args.node.config.center (a nil value)");
        loc_target = loc_description(truthy(set) ? set : tget(center, "set"), truthy(key) ? key : tget(center, "key"));
    }

    if (truthy(ret_string)) return ret_string;

    if (loc_target) {
        Table *lines_all = NULL;
        if (streq(type, "unlocks")) lines_all = ttab(loc_target, "unlock_parsed");
        if (!lines_all && streq(type, "name")) lines_all = ttab(loc_target, "name_parsed");
        if (!lines_all && (streq(type, "text") || streq(type, "tutorial") || streq(type, "quips"))) lines_all = loc_target;
        if (!lines_all) lines_all = ttab(loc_target, "text_parsed");
        if (!lines_all) ERROR("localize: bad argument #1 to 'ipairs' (table expected, got nil)");
        for (int li = 1; tgeti(lines_all, li).t != V_NIL; li++) {
            Table *lines = ttabi(lines_all, li);
            Table *final_line = tnew();
            for (int pi = 1; tgeti(lines, pi).t != V_NIL; pi++) {
                Table *part = ttabi(lines, pi);
                Table *control = ttab(part, "control");
                SBuf sb = {0};
                loc_assemble(&sb, args, ttab(part, "strings"), true);
                const char *assembled_string = sb_done(&sb).s;
                num slen = (num)strlen(assembled_string); // #assembled_string: bytes
                num desc_scale = tnum(ttab(G->LANG, "font"), "DESCSCALE");
                if (G->F_MOBILE_UI) desc_scale = desc_scale * 1.5;
                // part.control.s and tonumber(part.control.s) or 1
                num s_scale = 1;
                if (truthy(tget(control, "s"))) {
                    bool ok;
                    num n = tonumber(tget(control, "s"), &ok);
                    if (ok) s_scale = n;
                }
                if (streq(type, "name")) {
                    Val colV = loc_colV(args, control);
                    Val col = truthy(colV) ? colV
                              : truthy(tget(control, "C")) ? P(loc_colour(tstr(control, "C"), NULL)) : P(G->C.UI.TEXT_LIGHT);
                    tinsert(final_line, T(TAB("n", N(UIT_O), "config", T(TAB(
                        "object", O(DynaText_new(TAB("string", T(ARR(S(assembled_string))),
                            "colours", T(ARR(col)),
                            "bump", B(true),
                            "silent", B(true),
                            "pop_in", N(0),
                            "pop_in_rate", N(4),
                            "maxw", N(5),
                            "shadow", B(true),
                            "y_offset", N(-0.6),
                            "spacing", N(lmax(0, 0.32 * (17 - slen))),
                            "scale", N((0.55 - 0.004 * slen) * s_scale)
                        )))
                    )))));
                } else if (truthy(tget(control, "E"))) {
                    bool _float = false, _silent = true, _pop_in = false, _bump = false, _spacing = false; // false: nil
                    if (streq(tstr(control, "E"), "1")) {
                        _float = true, _silent = true, _pop_in = true; // _pop_in = 0
                    } else if (streq(tstr(control, "E"), "2")) {
                        _bump = true, _spacing = true; // _spacing = 1
                    }
                    Val colV = loc_colV(args, control);
                    Val col = truthy(colV) ? colV : P(loc_colour(tstr(control, "C"), NULL));
                    tinsert(final_line, T(TAB("n", N(UIT_O), "config", T(TAB(
                        "object", O(DynaText_new(TAB("string", T(ARR(S(assembled_string))), "colours", T(ARR(col)),
                            "float", _float ? B(true) : NIL,
                            "silent", B(_silent),
                            "pop_in", _pop_in ? N(0) : NIL,
                            "bump", _bump ? B(true) : NIL,
                            "spacing", _spacing ? N(1) : NIL,
                            "scale", N(0.32 * s_scale * desc_scale))))
                    )))));
                } else if (truthy(tget(control, "X"))) {
                    tinsert(final_line, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("m"), "colour", P(loc_colour(tstr(control, "X"), NULL)), "r", N(0.05), "padding", N(0.03), "res", N(0.15))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB(
                            "text", S(assembled_string),
                            "colour", P(loc_colour(tstr(control, "C"), NULL)),
                            "scale", N(0.32 * s_scale * desc_scale)))))
                    )))));
                } else {
                    // part.control.T and (G.P_CENTERS[part.control.T] or G.P_TAGS[part.control.T]) or nil
                    Val detailed_tooltip = NIL, cT = tget(control, "T");
                    if (truthy(cT)) {
                        detailed_tooltip = tgetv(G->P_CENTERS, cT);
                        if (!truthy(detailed_tooltip)) detailed_tooltip = tgetv(G->P_TAGS, cT);
                        if (!truthy(detailed_tooltip)) detailed_tooltip = NIL;
                    }
                    Val colV = loc_colV(args, control);
                    Val col = truthy(colV) ? colV : P(loc_colour(tstr(control, "C"), UI_colour(args, "default_col")));
                    tinsert(final_line, T(TAB("n", N(UIT_T), "config", T(TAB(
                        "detailed_tooltip", detailed_tooltip,
                        "text", S(assembled_string),
                        "shadow", tget(args, "shadow"),
                        "colour", col,
                        "scale", N(0.32 * s_scale * desc_scale))))));
                }
            }
            if (streq(type, "name") || streq(type, "text")) return T(final_line);
            Table *nodes = ttab(args, "nodes");
            if (!nodes) ERROR("localize: attempt to index field 'nodes' (a nil value)");
            tinsert(nodes, T(final_line));
        }
    }
    return NIL;
}

// misc_functions.lua:1822 the draw of the stake 8 sprite
// note: _sprite.ARGS.send_to_shader is one scratch table of this file (made once, as the Lua's per sprite)
static void stake8_draw(void *self) {
    Sprite *_sprite = self;
    static Table *send_to_shader;
    if (!send_to_shader) send_to_shader = tnew();
    tseti(send_to_shader, 1, N(lmin(_sprite->moveable.VT.r * 3, 1) + G->TIMERS.REAL / (18) +
                               (_sprite->moveable.juice.on ? _sprite->moveable.juice.r * 20 : 0) + 1));
    tseti(send_to_shader, 2, N(G->TIMERS.REAL));

    Sprite_draw_shader(_sprite, "dissolve", -1, NULL, false, NULL, NIL, NIL, NIL, NIL, false, -1);
    Sprite_draw_shader(_sprite, "voucher", -1, send_to_shader, false, NULL, NIL, NIL, NIL, NIL, false, -1);
}
// The class of that one sprite (misc_functions.h): a Sprite whose draw is replaced
static Class stake8_sprite_class = {.name = "Sprite", .super = &Sprite_class, .draw = stake8_draw};

// misc_functions.lua:1816
Sprite *get_stake_sprite(int _stake, float _scale) {
    if (_stake == 0) _stake = 1;
    if (_scale <= 0) _scale = 1;
    Sprite *stake_sprite = NEW(Sprite);
    Sprite_init(stake_sprite, 0, 0, _scale * 1, _scale * 1, ASSET_ATLAS_get("chips"),
                ttab(ttabi(ttab(G->P_CENTER_POOLS, "Stake"), _stake), "pos"));
    stake_sprite->moveable.node.states.drag.can = false;
    if (_stake == 8) {
        if (!stake8_sprite_class.ready) class_init(&stake8_sprite_class);
        stake_sprite->moveable.node.object.cls = &stake8_sprite_class;
    }
    return stake_sprite;
}

// misc_functions.lua:1834
Atlas *get_front_spriteinfo(Table *_front, Table **pos) {
    int cb = tbool(G->SETTINGS, "colourblind_option") ? 2 : 1;
    const char *value = tstr(_front, "value");
    if (_front && truthy(tget(_front, "suit")) && (streq(value, "Jack") || streq(value, "Queen") || streq(value, "King"))) {
        Table *CUSTOM_DECK = ttab(G->SETTINGS, "CUSTOM_DECK");
        if (CUSTOM_DECK && truthy(tgetv(ttab(CUSTOM_DECK, "Collabs"), tget(_front, "suit")))) {
            const char *_collab = vstr(tgetv(ttab(CUSTOM_DECK, "Collabs"), tget(_front, "suit")));
            if (streq(_collab, "default") || !ASSET_ATLAS_get(fmt("%s_%d", _collab, cb))) {
            } else {
                *pos = ttab(ttab(G->COLLABS, "pos"), value);
                return ASSET_ATLAS_get(fmt("%s_%d", _collab, cb));
            }
        }
    }
    *pos = ttab(_front, "pos");
    Atlas *atlas = ASSET_ATLAS_get(tstr(_front, "atlas"));
    return atlas ? atlas : ASSET_ATLAS_get(fmt("cards_%d", cb));
}

// misc_functions.lua:1847
float *get_stake_col(int _stake) {
    if (!G->C.STAKES[1]) {
        G->C.STAKES[1] = G->C.WHITE;
        G->C.STAKES[2] = G->C.RED;
        G->C.STAKES[3] = G->C.GREEN;
        G->C.STAKES[4] = G->C.BLACK;
        G->C.STAKES[5] = G->C.BLUE;
        G->C.STAKES[6] = G->C.PURPLE;
        G->C.STAKES[7] = G->C.ORANGE;
        G->C.STAKES[8] = G->C.GOLD;
    }
    return _stake >= 1 && _stake <= 8 ? G->C.STAKES[_stake] : NULL;
}

// misc_functions.lua:1861
int get_challenge_int_from_id(const char *_id) {
    for (TIter it = titer(G->CHALLENGES); tnext(&it);) {
        if (streq(tstr(it.val.tab, "id"), _id)) return (int)it.key.n;
    }
    return 0;
}

// misc_functions.lua:1868
Table *get_starting_params(void) {
    return TAB("dollars", N(4),
               "hand_size", N(8),
               "discards", N(3),
               "hands", N(4),
               "reroll_cost", N(5),
               "joker_slots", N(5),
               "ante_scaling", N(1),
               "consumable_slots", N(2),
               "no_faces", B(false),
               "erratic_suits_and_ranks", B(false));
}

// misc_functions.lua:1883
Val get_challenge_rule(Table *_challenge, const char *_type, const char *_id) {
    Table *rules = _challenge ? ttab(ttab(_challenge, "rules"), _type) : NULL;
    if (rules) {
        for (int k = 1; tgeti(rules, k).t != V_NIL; k++) {
            Table *v = ttabi(rules, k);
            if (streq(_id, tstr(v, "id"))) return tget(v, "value");
        }
    }
    return NIL;
}
