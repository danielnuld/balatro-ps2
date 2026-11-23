// functions/UI_definitions.lua:5680-6307: the challenge description tabs, the run setup tab, the button pip, the
// main menu's boxes, the language selector and (second definition) create_UIBox_highlight.
#include "all.h"

// UI_definitions.lua:5976 the accidental Lua global `saved_game` (UI_definitions.h)
Table *saved_game;

#define LOC(key) localize(S(key), NULL)

// {n = n, config = config, nodes = nodes}
static Table *node3(UIType n, Table *config, Table *nodes) {
    return TAB("n", N(n), "config", T(config), "nodes", T(nodes));
}
// {n = G.UIT.R, config = config, nodes = localize{type = 'text', key = key, vars = vars, default_col = default_col}}
// (localize gives nil for an unknown key: 'nodes' is then absent, as in the Lua)
static Table *loc_text_row(Table *config, const char *key, Table *vars, const float *default_col) {
    Table *largs = TAB("type", S("text"), "key", S(key), "vars", T(vars));
    if (default_col) tset(largs, "default_col", P((void *)default_col));
    Table *t = TAB("n", N(UIT_R), "config", T(config));
    tset(t, "nodes", localize(T(largs), NULL));
    return t;
}
// {v}: an array of one value (empty when v is nil)
static Table *vars1(Val v) {
    Table *t = tnew();
    tseti(t, 1, v);
    return t;
}
// Moveable() with no arguments: T = {0, 0, 0, 0} (moveable.lua:14)
static Moveable *moveable_new0(void) {
    Moveable *m = NEW(Moveable);
    Moveable_init(m, 0, 0, 0, 0);
    return m;
}
static Sprite *sprite_new(float X, float Y, float W, float H, Atlas *atlas, Table *pos) {
    Sprite *s = NEW(Sprite);
    Sprite_init(s, X, Y, W, H, atlas, pos);
    return s;
}

// UI_definitions.lua:5711, :5801, :5821 function(a, b) return a.order < b.order end
static bool less_order(Val a, Val b, void *ctx) { return tnum(a.tab, "order") < tnum(b.tab, "order"); }
// UI_definitions.lua:5938 function(a, b) return a:get_nominal() > b:get_nominal() end
static bool more_nominal(Val a, Val b, void *ctx) {
    return Card_get_nominal((Card *)a.obj, NULL) > Card_get_nominal((Card *)b.obj, NULL);
}
// UI_definitions.lua:6270 function(a, b) return a.label < b.label end (bytes, as Lua in the C locale)
static bool less_label(Val a, Val b, void *ctx) {
    const char *la = tstr(a.tab, "label"), *lb = tstr(b.tab, "label");
    return strcmp(la ? la : "", lb ? lb : "") < 0;
}

// UI_definitions.lua:5682-5758 the 'Rules' tab
static Table *challenge_tab_rules(Table *args) {
    Table *challenge = ttabi(G->CHALLENGES, (int)tnum(args, "_id"));
    Table *start_rules = tnew();
    Table *modded_starts = NULL;
    Table *game_rules = tnew();
    Table *starting_params = get_starting_params();
    Table *base_modifiers = TAB(
        "dollars", T(TAB("value", tget(starting_params, "dollars"), "order", N(6))),
        "discards", T(TAB("value", tget(starting_params, "discards"), "order", N(2))),
        "hands", T(TAB("value", tget(starting_params, "hands"), "order", N(1))),
        "reroll_cost", T(TAB("value", tget(starting_params, "reroll_cost"), "order", N(7))),
        "joker_slots", T(TAB("value", tget(starting_params, "joker_slots"), "order", N(4))),
        "consumable_slots", T(TAB("value", tget(starting_params, "consumable_slots"), "order", N(5))),
        "hand_size", T(TAB("value", tget(starting_params, "hand_size"), "order", N(3))));
    num bonus_mods = 100;
    Table *rules = ttab(challenge, "rules");
    if (rules) {
        Table *modifiers = ttab(rules, "modifiers");
        if (modifiers) {
            for (int k = 1;; k++) { // :5700 ipairs
                Val vv = tgeti(modifiers, k);
                if (vv.t == V_NIL) break;
                Table *v = vv.tab;
                Val id = tget(v, "id");
                Val oldv = tgetv(base_modifiers, id);
                // :5701 old_val = base_modifiers[v.id].value raises in the Lua when v.id is not a base modifier
                if (oldv.t != V_TAB) ERROR("challenge modifier '%s' is not a starting parameter", vstr(id));
                Table *old = oldv.tab;
                Val order = tget(old, "order");
                Table *nu = TAB("value", tget(v, "value"), "order", truthy(order) ? order : N(bonus_mods), "custom", B(true));
                tset(nu, "old_val", tget(old, "value"));
                tsetv(base_modifiers, id, T(nu));
                bonus_mods = bonus_mods + 1;
            }
        }
    }
    Table *nu_base_modifiers = tnew();
    // note: pairs in insertion order; the orders are unique, so the sort gives the Lua's result
    for (TIter it = titer(base_modifiers); tnext(&it);) {
        tset(it.val.tab, "key", it.key);
        tseti(nu_base_modifiers, tlen(nu_base_modifiers) + 1, it.val);
    }
    tsort(nu_base_modifiers, less_order, NULL);
    for (int k = 1;; k++) { // :5712
        Val vv = tgeti(nu_base_modifiers, k);
        if (vv.t == V_NIL) break;
        Table *v = vv.tab;
        const char *key = fmt("ch_m_%s", vstr(tget(v, "key")));
        if (truthy(tget(v, "old_val"))) {
            if (!modded_starts) modded_starts = tnew();
            tseti(modded_starts, tlen(modded_starts) + 1,
                  T(loc_text_row(TAB("align", S("cl"), "maxw", N(3.5)), key, vars1(tget(v, "value")), G->C.L_BLACK)));
        } else {
            tseti(start_rules, tlen(start_rules) + 1,
                  T(loc_text_row(TAB("align", S("cl"), "maxw", N(3.5)), key, vars1(tget(v, "value")),
                                 !tbool(v, "custom") ? G->C.UI.TEXT_INACTIVE : NULL)));
        }
    }

    if (modded_starts) { // :5722
        start_rules = ARR(
            T(node3(UIT_R, TAB("align", S("cl"), "padding", N(0.05)), modded_starts)),
            T(node3(UIT_R, TAB("align", S("cl"), "padding", N(0.05), "colour", P(G->C.GREY)), tnew())),
            T(node3(UIT_R, TAB("align", S("cl"), "padding", N(0.05)), start_rules)));
    }

    if (rules) { // :5730
        Table *custom = ttab(rules, "custom");
        if (custom) {
            for (int k = 1;; k++) {
                Val vv = tgeti(custom, k);
                if (vv.t == V_NIL) break;
                Table *v = vv.tab;
                tseti(game_rules, tlen(game_rules) + 1,
                      T(loc_text_row(TAB("align", S("cl")), fmt("ch_c_%s", vstr(tget(v, "id"))), vars1(tget(v, "value")), NULL)));
            }
        }
    }
    if (!truthy(tgeti(start_rules, 1)) && !modded_starts)
        tseti(start_rules, tlen(start_rules) + 1, T(loc_text_row(TAB("align", S("cl")), "ch_m_none", tnew(), NULL)));
    if (!truthy(tgeti(game_rules, 1)))
        tseti(game_rules, tlen(game_rules) + 1, T(loc_text_row(TAB("align", S("cl")), "ch_c_none", tnew(), NULL)));

    Table *starting_rule_list = node3(UIT_C, TAB("align", S("cm"), "minw", N(3), "r", N(0.1), "colour", P(G->C.BLUE)), ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.08), "minh", N(0.6)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_game_modifiers"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
        T(node3(UIT_R, TAB("align", S("cm"), "minh", N(4.1), "minw", N(4.2), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE)), start_rules))));

    Table *override_rule_list = node3(UIT_C, TAB("align", S("cm"), "minw", N(3), "r", N(0.1), "colour", P(G->C.BLUE)), ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.08), "minh", N(0.6)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_custom_rules"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
        T(node3(UIT_R, TAB("align", S("cm"), "minh", N(4.1), "minw", N(6.8), "maxw", N(6.7), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE)), game_rules))));

    return node3(UIT_ROOT, TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR)), ARR(
        T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.L_BLACK), "r", N(0.1), "minw", N(3)), ARR(
            T(override_rule_list), T(starting_rule_list))))));
}

// UI_definitions.lua:5759-5890 the 'Restrictions' tab
static Table *challenge_tab_restrictions(Table *args) {
    Table *challenge = ttabi(G->CHALLENGES, (int)tnum(args, "_id"));

    Table *banned_cards = tnew(), *banned_tags = tnew(), *banned_other = tnew();

    Table *restrictions = ttab(challenge, "restrictions");
    if (restrictions) {
        Table *r_cards = ttab(restrictions, "banned_cards");
        if (r_cards) {
            Table *row_cards = tnew();
            num n = tlen(r_cards);
            // :5767 math.log(6, n) is the logarithm of 6 in base n (n = 1: +inf, so n_rows = 1; n = 0: n_rows = 2)
            num n_rows = lmax(1, floor(n / 10) + 2 - floor(log(6.0) / log(n)));
            num max_width = 1;
            for (int k = 1;; k++) {
                Val v = tgeti(r_cards, k);
                if (v.t == V_NIL) break;
                int _row = (int)floor((k - 1) * n_rows / n + 1);
                Table *row = tgeti(row_cards, _row).t == V_TAB ? tgeti(row_cards, _row).tab : NULL;
                if (!row) {
                    row = tnew();
                    tseti(row_cards, _row, T(row));
                }
                tseti(row, tlen(row) + 1, v);
                if (tlen(row) > max_width) max_width = tlen(row);
            }

            num card_size = lmax(0.3, 0.75 - 0.01 * (max_width * n_rows));

            for (int r = 1;; r++) { // :5778 ipairs(row_cards)
                Val rv = tgeti(row_cards, r);
                if (rv.t == V_NIL) break;
                Table *row_card = rv.tab;
                CardArea *banned_card_area = NEW(CardArea);
                CardArea_init(banned_card_area, 0, 0, 6.7, 3.3 / n_rows,
                              TAB("type", S("title_2"), "view_deck", B(true), "highlight_limit", N(0), "card_w", N(G->CARD_W * card_size)));
                tinsert(banned_cards, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(banned_card_area)))))))));
                for (int k = 1;; k++) {
                    Val vv = tgeti(row_card, k);
                    if (vv.t == V_NIL) break;
                    Card *card = NEW(Card);
                    Card_init(card, 0, 0, G->CARD_W * card_size, G->CARD_H * card_size, NULL,
                              ttab(G->P_CENTERS, tstr(vv.tab, "id")),
                              TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
                    CardArea_emplace(banned_card_area, card, NULL, false);
                }
            }
        }
        Table *r_tags = ttab(restrictions, "banned_tags");
        if (r_tags) {
            Table *tag_tab = tnew();
            for (TIter it = titer(r_tags); tnext(&it);) // :5797 pairs (an array: insertion order; sorted below)
                tseti(tag_tab, tlen(tag_tab) + 1, tget(G->P_TAGS, tstr(it.val.tab, "id")));

            tsort(tag_tab, less_order, NULL);

            for (int k = 1;; k++) {
                Val vv = tgeti(tag_tab, k);
                if (vv.t == V_NIL) break;
                Tag *temp_tag = NEW(Tag);
                Tag_init(temp_tag, tstr(vv.tab, "key"), false, NULL);
                // note: Tag_generate_UI takes a negative size as nil; that needs 20 banned tags (no challenge has them)
                Table *temp_tag_ui = Tag_generate_UI(temp_tag, 1.1 - 0.25 * sqrt((num)tlen(r_tags)), NULL);
                tinsert(banned_tags, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(T(temp_tag_ui)))));
            }
        }
        Table *r_other = ttab(restrictions, "banned_other");
        if (r_other) {
            Table *other_tab = tnew();
            for (TIter it = titer(r_other); tnext(&it);) { // :5815 pairs
                if (streq(tstr(it.val.tab, "type"), "blind"))
                    tseti(other_tab, tlen(other_tab) + 1, tget(G->P_BLINDS, tstr(it.val.tab, "id")));
            }

            tsort(other_tab, less_order, NULL);

            for (int k = 1;; k++) {
                Val vv = tgeti(other_tab, k);
                if (vv.t == V_NIL) break;
                Table *v = vv.tab;
                AnimatedSprite *temp_blind = NEW(AnimatedSprite);
                AnimatedSprite_init(temp_blind, 0, 0, 1, 1, ANIMATION_ATLAS_get("blind_chips"), ttab(v, "pos"));
                Sprite_define_draw_steps((Sprite *)temp_blind, ARR(
                    T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
                    T(TAB("shader", S("dissolve")))));
                Node *tb = (Node *)temp_blind;
                tsetb(tb->object.dyn, "float", true);
                tb->states.hover.can = true;
                tb->states.drag.can = false;
                tb->states.collide.can = true;
                tb->config = TAB("blind", T(v), "force_focus", B(true));
                // :5834-5847 hover / stop_hover: the class of UI_definitions.h, with its upvalue discovered = true
                if (!UIDEF_blind_sprite_class.ready) class_init(&UIDEF_blind_sprite_class);
                tb->object.cls = &UIDEF_blind_sprite_class;
                tsetb(tb->object.dyn, "discovered", true);

                tinsert(banned_other, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(temp_blind)))))))));
            }
        }
    }
    if (!truthy(tgeti(banned_cards, 1)))
        tseti(banned_cards, tlen(banned_cards) + 1, T(loc_text_row(TAB("align", S("cl")), "ch_m_none", tnew(), NULL)));
    if (!truthy(tgeti(banned_tags, 1)))
        tseti(banned_tags, tlen(banned_tags) + 1, T(loc_text_row(TAB("align", S("cl")), "ch_c_none", tnew(), NULL)));
    if (!truthy(tgeti(banned_other, 1)))
        tseti(banned_other, tlen(banned_other) + 1, T(loc_text_row(TAB("align", S("cl")), "ch_c_none", tnew(), NULL)));

    // :5861 (the Lua shadows the three lists with locals of the same names)
    Table *banned_cards_c = node3(UIT_C, TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.RED)), ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.08), "minh", N(0.6)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_banned_cards"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
        T(node3(UIT_R, TAB("align", S("cm"), "minh", N(4.1), "minw", N(7.33), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE)),
                banned_cards))));

    Table *banned_tags_c = node3(UIT_C, TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.RED)), ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.08), "minh", N(0.6), "maxw", N(1.48)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_banned_tags"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
        T(node3(UIT_R, TAB("align", S("cm"), "minh", N(4.1), "minw", N(1.48), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE)),
                banned_tags))));

    Table *banned_other_c = node3(UIT_C, TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.RED)), ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.08), "minh", N(0.6), "maxw", N(1.84)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_other"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
        T(node3(UIT_R, TAB("align", S("cm"), "minh", N(4.1), "minw", N(2), "padding", N(0.05), "r", N(0.1), "colour", P(G->C.WHITE)),
                banned_other))));

    return node3(UIT_ROOT, TAB("align", S("cm"), "padding", N(0.05), "colour", P(G->C.CLEAR)), ARR(
        T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "colour", P(G->C.L_BLACK), "r", N(0.1)), ARR(
            T(banned_cards_c), T(banned_tags_c), T(banned_other_c))))));
}

// UI_definitions.lua:5891-5957 the 'Deck' tab
static Table *challenge_tab_deck(Table *args) {
    Table *challenge = ttabi(G->CHALLENGES, (int)tnum(args, "_id"));
    Table *deck_tables = tnew();
    Table *SUITS = TAB("S", T(tnew()), "H", T(tnew()), "C", T(tnew()), "D", T(tnew()));
    static const char *suit_map[4] = {"S", "H", "C", "D"};
    Table *card_protos = NULL;
    Table *_de = NULL;
    if (challenge) _de = ttab(challenge, "deck");

    if (_de && truthy(tget(_de, "cards"))) card_protos = ttab(_de, "cards");

    if (!card_protos) {
        card_protos = tnew();
        // note: pairs(G.P_CARDS): insertion order here (hash order in the Lua); each suit is sorted afterwards
        for (TIter it = titer(G->P_CARDS); tnext(&it);) {
            const char *k = vstr(it.key);
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
                if (truthy(tget(_de, "seal"))) _g = tget(_de, "seal");
            }

            if (keep) {
                Table *p = TAB("s", S(_s), "r", S(_r));
                tset(p, "e", _e);
                tset(p, "d", _d);
                tset(p, "g", _g);
                tseti(card_protos, tlen(card_protos) + 1, T(p));
            }
        }
    }
    for (int k = 1;; k++) { // :5929
        Val vv = tgeti(card_protos, k);
        if (vv.t == V_NIL) break;
        Table *v = vv.tab;
        const char *e = truthy(tget(v, "e")) ? tstr(v, "e") : "c_base";
        Card *_card = NEW(Card);
        Card_init(_card, 0, 0, G->CARD_W * 0.45, G->CARD_H * 0.45,
                  ttab(G->P_CARDS, fmt("%s_%s", vstr(tget(v, "s")), vstr(tget(v, "r")))), ttab(G->P_CENTERS, e), NULL);
        if (truthy(tget(v, "d"))) Card_set_edition(_card, TAB(tstr(v, "d"), B(true)), true, true);
        if (truthy(tget(v, "g"))) Card_set_seal(_card, tstr(v, "g"), true, true);
        Table *suit = ttab(SUITS, tstr(v, "s"));
        tseti(suit, tlen(suit) + 1, O(_card));
    }

    for (int j = 1; j <= 4; j++) {
        Table *suit = ttab(SUITS, suit_map[j - 1]);
        if (truthy(tgeti(suit, 1))) {
            tsort(suit, more_nominal, NULL);
            CardArea *view_deck = NEW(CardArea);
            CardArea_init(view_deck, 0, 0, 5.5 * G->CARD_W, 0.42 * G->CARD_H,
                          TAB("card_limit", N(tlen(suit)), "type", S("title_2"), "view_deck", B(true), "highlight_limit", N(0),
                              "card_w", N(G->CARD_W * 0.5), "draw_layers", T(ARR(S("card")))));
            tinsert(deck_tables, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(view_deck)))))))));

            for (int i = 1, n = tlen(suit); i <= n; i++) {
                if (truthy(tgeti(suit, i))) CardArea_emplace(view_deck, (Card *)tobji(suit, i), NULL, false);
            }
        }
    }
    return node3(UIT_ROOT, TAB("align", S("cm"), "padding", N(0), "colour", P(G->C.BLACK), "r", N(0.1), "minw", N(11.4), "minh", N(4.2)),
                 deck_tables);
}

// UI_definitions.lua:5680
Table *UIDEF_challenge_description_tab(Table *args) {
    if (!args) args = tnew();
    const char *_tab = tstr(args, "_tab");
    if (streq(_tab, "Rules")) return challenge_tab_rules(args);
    else if (streq(_tab, "Restrictions")) return challenge_tab_restrictions(args);
    else if (streq(_tab, "Deck")) return challenge_tab_deck(args);
    return NULL;
}

// A row of the saved run's summary (:6041-6065): label, ': ', and the value's T node
static Table *run_setup_stat_row(const char *label_key, Table *value_node, num lwidth, num rwidth, num scale) {
    return node3(UIT_R, TAB("align", S("cm")), ARR(
        T(node3(UIT_C, TAB("align", S("cm"), "minw", N(lwidth), "maxw", N(lwidth)), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC(label_key), "colour", P(G->C.UI.TEXT_DARK), "scale", N(scale * 0.8)))))))),
        T(node3(UIT_C, TAB("align", S("cm")), ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(": "), "colour", P(G->C.UI.TEXT_DARK), "scale", N(scale * 0.8)))))))),
        T(node3(UIT_C, TAB("align", S("cl"), "minw", N(rwidth), "maxw", N(lwidth)), ARR(T(value_node))))));
}

// UI_definitions.lua:5961
Table *UIDEF_run_setup_option(const char *type) {
    if (!G->SAVED_GAME) {
        const char *packed = get_compressed(fmt("%s/save.jkr", vstr(tget(G->SETTINGS, "profile"))));
        if (packed != NULL) G->SAVED_GAME = STR_UNPACK(packed);
    }
    Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
    Table *MEMORY = ttab(profile, "MEMORY");

    tsets(G->SETTINGS, "current_setup", type);
    Back *viewed_back = NEW(Back);
    Back_init(viewed_back, get_deck_from_name(tstr(MEMORY, "deck")));
    tseto(G->GAME, "viewed_back", viewed_back);

    if (!truthy(tget(MEMORY, "stake"))) tsetn(MEMORY, "stake", 1);

    if (streq(type, "Continue")) {
        G->viewed_stake = 1;
        if (G->SAVED_GAME != NULL) {
            saved_game = G->SAVED_GAME;
            const char *viewed_deck = "b_red";
            Val back_name = tpath(saved_game, "BACK.name");
            // note: pairs(G.P_CENTERS), the last match wins: insertion order here (the names of the backs are unique)
            for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
                if (veq(tget(it.val.tab, "name"), back_name)) viewed_deck = vstr(it.key);
            }
            Back_change_to(viewed_back, ttab(G->P_CENTERS, viewed_deck));
            Val stake = tpath(saved_game, "GAME.stake");
            G->viewed_stake = truthy(stake) ? (int)vnum(stake) : 1;
        }
    }

    if (streq(type, "New Run")) {
        if (G->OVERLAY_MENU) {
            UIElement *seed_toggle = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("run_setup_seed"), NULL);
            if (seed_toggle) ((Node *)seed_toggle)->states.visible = true;
        }
        Val stake = tget(MEMORY, "stake");
        G->viewed_stake = truthy(stake) ? (int)vnum(stake) : 1;
        FUNCS_change_stake(TAB("to_key", N(G->viewed_stake)));
    } else {
        tset(G->object.dyn, "run_setup_seed", NIL);
        if (G->OVERLAY_MENU) {
            UIElement *seed_toggle = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("run_setup_seed"), NULL);
            if (seed_toggle) ((Node *)seed_toggle)->states.visible = false;
        }
    }

    CardArea *area = NEW(CardArea);
    CardArea_init(area, G->ROOM->T->x + 0.2 * G->ROOM->T->w / 2, G->ROOM->T->h, G->CARD_W, G->CARD_H,
                  TAB("card_limit", N(5), "type", S("deck"), "highlight_limit", N(0), "deck_height", N(0.75), "thin_draw", N(1)));

    for (int i = 1; i <= 10; i++) {
        Card *card = NEW(Card);
        Table *front = pseudorandom_element(G->P_CARDS, NIL, NULL).tab;
        Card_init(card, G->ROOM->T->x + 0.2 * G->ROOM->T->w / 2, G->ROOM->T->h, G->CARD_W, G->CARD_H, front,
                  ttab(G->P_CENTERS, "c_base"), TAB("playing_card", N(i), "viewed_back", B(true)));
        card->sprite_facing = "back";
        card->facing = "back";
        CardArea_emplace(area, card, NULL, false);
        if (i == 10) {
            G->sticker_card = card;
            card->sticker = get_deck_win_sticker(ttab(viewed_back->effect, "center"));
        }
    }

    Table *ordered_names = tnew();
    int viewed_deck = 1;
    Table *backs = ttab(G->P_CENTER_POOLS, "Back");
    for (int k = 1;; k++) {
        Val vv = tgeti(backs, k);
        if (vv.t == V_NIL) break;
        tseti(ordered_names, tlen(ordered_names) + 1, tget(vv.tab, "name"));
        if (streq(tstr(vv.tab, "name"), viewed_back->name)) viewed_deck = k;
    }

    num lwidth = 1.4, rwidth = 1.8;

    float *type_colour = G->C.BLUE; // (unused, as in the Lua)

    num scale = 0.39;
    tsets(G->object.dyn, "setup_seed", "");

    // note: the objects the Lua makes inside the table constructor are made here first, in the constructor's order
    // (C does not fix the order in which arguments are evaluated).
    const char *back_key = tstr(ttab(viewed_back->effect, "center"), "key");
    Table *top;
    if (streq(type, "Continue")) {
        // :6044 the Lua indexes the global saved_game: nil raises
        if (!saved_game) ERROR("attempt to index global 'saved_game' (a nil value)");
        Moveable *name_obj = moveable_new0();
        Table *sg = ttab(saved_game, "GAME");
        Val amt = tpath(sg, "round_scores.hand.amt");
        Table *rows = ARR(
            T(run_setup_stat_row("k_round", TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(tget(sg, "round"))), "colour", P(G->C.RED), "scale", N(0.8 * scale)))), lwidth, rwidth, scale)),
            T(run_setup_stat_row("k_ante", TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(tpath(sg, "round_resets.ante"))), "colour", P(G->C.BLUE), "scale", N(0.8 * scale)))), lwidth, rwidth, scale)),
            T(run_setup_stat_row("k_money", TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s%s", vstr(LOC("$")), tostring(tget(sg, "dollars")))), "colour", P(G->C.ORANGE), "scale", N(0.8 * scale)))), lwidth, rwidth, scale)),
            T(run_setup_stat_row("k_best_hand", TAB("n", N(UIT_T), "config", T(TAB("text", S(number_format(amt)), "colour", P(G->C.RED), "scale", N(scale_number(amt, 0.8 * scale, -1))))), lwidth, rwidth, scale)));
        if (tbool(sg, "seeded")) // :6061 (the last entry: nil leaves no hole)
            tseti(rows, 5, T(run_setup_stat_row("k_seed", TAB("n", N(UIT_T), "config", T(TAB("text", S(tostring(tpath(sg, "pseudorandom.seed"))), "colour", P(G->C.RED), "scale", N(0.8 * scale)))), lwidth, rwidth, scale)));
        UIBox *stake_col = UIBox_new(TAB("definition", T(UIDEF_deck_stake_column(back_key)),
                                         "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));
        top = node3(UIT_R, TAB("align", S("tm"), "minh", N(3.8), "padding", N(0.15)), ARR(
            T(node3(UIT_R, TAB("align", S("cm"), "minh", N(3.3), "minw", N(6.8)), ARR(
                T(node3(UIT_C, TAB("align", S("cm"), "colour", P(G->C.BLACK), "padding", N(0.15), "r", N(0.1), "emboss", N(0.05)), ARR(
                    T(node3(UIT_C, TAB("align", S("cm")), ARR(
                        T(node3(UIT_R, TAB("align", S("cm"), "shadow", B(false)), ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area))))))))))),
                    T(node3(UIT_C, TAB("align", S("cm"), "minw", N(4), "maxw", N(4), "minh", N(1.7), "r", N(0.1), "colour", P(G->C.L_BLACK), "padding", N(0.1)), ARR(
                        T(node3(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(4), "maxw", N(4), "minh", N(0.6)), ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_back_name"), "object", O(name_obj)))))))),
                        T(node3(UIT_R, TAB("align", S("cm"), "colour", P(G->C.WHITE), "padding", N(0.03), "minh", N(1.75), "r", N(0.1)), rows))))),
                    T(node3(UIT_C, TAB("align", S("cm")), ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("id", S(viewed_back->name), "func", S("RUN_SETUP_check_back_stake_column"), "object", O(stake_col))))))))))))))));
    } else {
        Moveable *name_obj = moveable_new0();
        UIBox *back_box = UIBox_new(TAB("definition", T(Back_generate_UI(viewed_back, NULL, -1, -1, NULL)),
                                        "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));
        UIBox *stake_col = UIBox_new(TAB("definition", T(UIDEF_deck_stake_column(back_key)),
                                         "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));
        Table *mid = node3(UIT_R, TAB("align", S("cm"), "minh", N(3.3), "minw", N(5)), ARR(
            T(node3(UIT_C, TAB("align", S("cm"), "colour", P(G->C.BLACK), "padding", N(0.15), "r", N(0.1), "emboss", N(0.05)), ARR(
                T(node3(UIT_C, TAB("align", S("cm")), ARR(
                    T(node3(UIT_R, TAB("align", S("cm"), "shadow", B(false)), ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area))))))))))),
                T(node3(UIT_C, TAB("align", S("cm"), "minh", N(1.7), "r", N(0.1), "colour", P(G->C.L_BLACK), "padding", N(0.1)), ARR(
                    T(node3(UIT_R, TAB("align", S("cm"), "r", N(0.1), "minw", N(4), "maxw", N(4), "minh", N(0.6)), ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_back_name"), "object", O(name_obj)))))))),
                    T(node3(UIT_R, TAB("align", S("cm"), "colour", P(G->C.WHITE), "minh", N(1.7), "r", N(0.1)), ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("id", S(viewed_back->name), "func", S("RUN_SETUP_check_back"), "object", O(back_box))))))))))),
                T(node3(UIT_C, TAB("align", S("cm")), ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("id", S(viewed_back->name), "func", S("RUN_SETUP_check_back_stake_column"), "object", O(stake_col)))))))))))));
        Table *cycle = create_option_cycle(TAB("options", T(ordered_names), "opt_callback", S("change_viewed_back"),
                                               "current_option", N(viewed_deck), "colour", P(G->C.RED), "w", N(3.5), "mid", T(mid)));
        top = node3(UIT_R, TAB("align", S("cm"), "minh", N(3.8)), ARR(T(cycle)));
    }

    Moveable *stake_obj = moveable_new0();
    Table *stake_row;
    if (streq(type, "Continue")) {
        stake_row = node3(UIT_R, TAB("align", S("cm"), "minh", N(2.2), "minw", N(5)), ARR(
            T(node3(UIT_R, TAB("align", S("cm"), "minh", N(0.17)), tnew())),
            T(node3(UIT_R, TAB("align", S("cm")), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_stake"), "insta_func", B(true), "object", O(stake_obj))))))))));
    } else {
        stake_row = node3(UIT_R, TAB("align", S("cm"), "minh", N(2.2), "minw", N(6.8)), ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_stake"), "insta_func", B(true), "object", O(stake_obj)))))));
    }

    Moveable *seeded_obj = moveable_new0();
    Table *seed_nodes = tnew(); // :6113 type == 'New Run' and create_toggle{...} or nil
    if (streq(type, "New Run"))
        tseti(seed_nodes, 1, T(create_toggle(TAB("col", B(true), "label", LOC("k_seeded_run"), "label_scale", N(0.25), "w", N(0),
                                                 "scale", N(0.7), "ref_table", O(G), "ref_value", S("run_setup_seed")))));

    Table *t = node3(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR), "minh", N(6.6), "minw", N(6)), ARR(
        T(top),
        T(node3(UIT_R, TAB("align", S("cm")), ARR(T(stake_row)))),
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.05), "minh", N(0.9)), ARR(
            T(node3(UIT_O, TAB("align", S("cm"), "func", S("toggle_seeded_run"), "object", O(seeded_obj)), tnew()))))),
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
            T(node3(UIT_C, TAB("align", S("cm"), "minw", N(2.4), "id", S("run_setup_seed")), seed_nodes)),
            T(node3(UIT_C, TAB("align", S("cm"), "minw", N(5), "minh", N(0.8), "padding", N(0.2), "r", N(0.1), "hover", B(true), "colour", P(G->C.BLUE), "button", S("start_setup_run"), "shadow", B(true), "func", S("can_start_run")), ARR(
                T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0)), ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("b_play_cap"), "scale", N(0.8), "colour", P(G->C.UI.TEXT_LIGHT), "func", S("set_button_pip"),
                                                         "focus_args", T(TAB("button", S("x"), "set_button_pip", B(true))))))))))))),
            T(node3(UIT_C, TAB("align", S("cm"), "minw", N(2.5)), tnew())))))));
    return t;
}

// UI_definitions.lua:6126
Table *create_button_binding_pip(Table *args) {
    static const struct { const char *name; int x; } button_sprite_map[] = {
        {"x", 2}, {"y", 3}, {"leftshoulder", 4}, {"rightshoulder", 5}, {"triggerleft", 6}, {"triggerright", 7},
        {"start", 8}, {"back", 9}, {"dpadup", 10}, {"dpadright", 11}, {"dpaddown", 12}, {"dpadleft", 13},
        {"left", 14}, {"right", 15}, {"leftstick", 16}, {"rightstick", 17}, {"guide", 19}};
    const char *button = tstr(args, "button");
    Table *pos = tnew();
    if (streq(button, "a")) tsetn(pos, "x", G->F_SWAP_AB_PIPS ? 1 : 0);
    else if (streq(button, "b")) tsetn(pos, "x", G->F_SWAP_AB_PIPS ? 0 : 1);
    else {
        for (size_t i = 0; i < sizeof button_sprite_map / sizeof button_sprite_map[0]; i++) {
            if (streq(button, button_sprite_map[i].name)) tsetn(pos, "x", button_sprite_map[i].x);
        }
    } // (an unknown button: x stays nil, as in the Lua)
    const char *console = G->CONTROLLER->GAMEPAD_CONSOLE;
    tsetn(pos, "y", streq(console, "Nintendo") ? 2 : streq(console, "Playstation") ? (G->F_PS4_PLAYSTATION_GLYPHS ? 3 : 1) : 0);
    Val sc = tget(args, "scale");
    float scale = truthy(sc) ? (float)vnum(sc) : 0.45f;
    Sprite *BUTTON_SPRITE = sprite_new(0, 0, scale, scale, ASSET_ATLAS_get("gamepad_ui"), pos);

    return node3(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(BUTTON_SPRITE)))))));
}

// UI_definitions.lua:6158
Table *create_UIBox_profile_button(void) {
    Table *letters = tnew();
    if (truthy(G->F_DISP_USERNAME)) {
        // note: utf8.chars of a non-string (F_DISP_USERNAME = true, a name still awaited) raises in the Lua
        if (G->F_DISP_USERNAME.t != V_STR) ERROR("create_UIBox_profile_button: G.F_DISP_USERNAME is not a string");
        for (Utf8Iter it = utf8_chars(G->F_DISP_USERNAME.s, false); utf8_next(&it);) {
            const char *_char = it.c;
            bool leng = Font_hasGlyphs(font_FONT(ttab(ttab(G->LANGUAGES, "all1"), "font")), it.c);
            tseti(letters, tlen(letters) + 1, T(TAB("n", N(UIT_T), "config", T(TAB(
                "lang", T(ttab(G->LANGUAGES, leng ? "all1" : "all2")), "text", S(_char), "scale", N(0.3),
                "colour", P(mix_colours(G->C.GREEN, G->C.WHITE, 0.7, NULL)), "shadow", B(true))))));
        }
    }

    Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
    if (!truthy(tget(profile, "name"))) tsets(profile, "name", fmt("P%s", vstr(tget(G->SETTINGS, "profile"))));

    Table *nodes = ARR(
        T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.1), "emboss", N(0.1), "colour", P(G->C.L_BLACK)), ARR(
            T(node3(UIT_R, TAB("align", S("cm")), ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_profile"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
            T(node3(UIT_R, TAB("align", S("cm")), ARR(
                T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.15), "minw", N(2), "minh", N(0.8), "maxw", N(2), "r", N(0.1), "hover", B(true),
                                   "colour", P(mix_colours(G->C.WHITE, G->C.GREY, 0.2, NULL)), "button", S("profile_select"), "shadow", B(true)), ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("ref_table", T(profile), "ref_value", S("name"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))))))))));
    if (truthy(G->F_DISP_USERNAME)) // :6184 (the last entry: nil leaves no hole)
        tseti(nodes, 2, T(node3(UIT_R, TAB("align", S("cm")), ARR(
            T(node3(UIT_R, TAB("align", S("cm")), ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", LOC("k_playing_as"), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true)))))))),
            T(node3(UIT_R, TAB("align", S("cm"), "minh", N(0.12)), tnew())),
            T(node3(UIT_R, TAB("align", S("cm"), "maxw", N(2)), letters))))));
    return node3(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), nodes);
}

// UI_definitions.lua:6194
Table *create_UIBox_main_menu_buttons(void) {
    num text_scale = 0.45;
    Sprite *language = NULL;
    if (!G->F_ENGLISH_ONLY) {
        language = sprite_new(0, 0, 0.6, 0.6, ASSET_ATLAS_get("icons"), TAB("x", N(2), "y", N(0)));
        language->moveable.node.states.drag.can = false;
    }
    Sprite *discord = NULL;
    Sprite *twitter = NULL;
    if (G->F_DISCORD) {
        discord = sprite_new(0, 0, 0.6, 0.6, ASSET_ATLAS_get("icons"), TAB("x", N(0), "y", N(0)));
        discord->moveable.node.states.drag.can = false;
        twitter = sprite_new(0, 0, 0.6, 0.6, ASSET_ATLAS_get("icons"), TAB("x", N(0), "y", N(1)));
        twitter->moveable.node.states.drag.can = false;
    }

    const char *quit_func = "quit";

    // note: the buttons are built first, in the table constructor's order
    Table *play = UIBox_button(TAB("id", S("main_menu_play"), "button", S(!tbool(G->SETTINGS, "tutorial_complete") ? "start_run" : "setup_run"),
                                   "colour", P(G->C.BLUE), "minw", N(3.65), "minh", N(1.55), "label", T(ARR(LOC("b_play_cap"))),
                                   "scale", N(text_scale * 2), "col", B(true)));
    Table *mid_nodes = ARR(T(UIBox_button(TAB("button", S("options"), "colour", P(G->C.ORANGE), "minw", N(2.65), "minh", N(1.35),
                                              "label", T(ARR(LOC("b_options_cap"))), "scale", N(text_scale * 1.2), "col", B(true)))));
    if (G->F_QUIT_BUTTON) {
        tseti(mid_nodes, 2, T(node3(UIT_C, TAB("align", S("cm"), "minw", N(0.2)), tnew())));
        tseti(mid_nodes, 3, T(UIBox_button(TAB("button", S(quit_func), "colour", P(G->C.RED), "minw", N(2.65), "minh", N(1.35),
                                               "label", T(ARR(LOC("b_quit_cap"))), "scale", N(text_scale * 1.2), "col", B(true)))));
    }
    Table *collection = UIBox_button(TAB("id", S("collection_button"), "button", S("your_collection"), "colour", P(G->C.PALE_GREEN),
                                         "minw", N(3.65), "minh", N(1.55), "label", T(ARR(LOC("b_collection_cap"))),
                                         "scale", N(text_scale * 1.5), "col", B(true)));

    Table *right_nodes = tnew(); // :6226 two conditional entries, each at ITS index
    if (G->F_DISCORD)
        tseti(right_nodes, 1, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.2)), ARR(
            T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(mix_colours(G->C.BLUE, G->C.GREY, 0.4, NULL)), "button", S("go_to_discord"), "shadow", B(true)), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(discord)))))))),
            T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "hover", B(true), "colour", P(G->C.BLACK), "button", S("go_to_twitter"), "shadow", B(true)), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(twitter))))))))))));
    if (!G->F_ENGLISH_ONLY)
        tseti(right_nodes, 2, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.1), "emboss", N(0.1), "colour", P(G->C.L_BLACK)), ARR(
            T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.15), "minw", N(1), "r", N(0.1), "hover", B(true), "colour", P(mix_colours(G->C.WHITE, G->C.GREY, 0.2, NULL)), "button", S("language_selection"), "shadow", B(true)), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(language))))),
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(G->LANG, "label"), "scale", N(0.4), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))))))));

    Table *t = node3(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(
        T(node3(UIT_C, TAB("align", S("bm")), ARR(
            T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.1), "emboss", N(0.1), "colour", P(G->C.L_BLACK), "mid", B(true)), ARR(
                T(play),
                T(node3(UIT_C, TAB("align", S("cm")), mid_nodes)),
                T(collection))))))),
        T(node3(UIT_C, TAB("align", S("br"), "minw", N(3.2), "padding", N(0.1)), right_nodes))));
    return t;
}

// UI_definitions.lua:6245
Table *create_UIBox_main_menu_competittion_name(void) {
    Table *COMP = ttab(G->SETTINGS, "COMP");
    tsets(COMP, "name", "");
    Table *input = create_text_input(TAB("w", N(4), "max_length", N(16), "prompt_text", S(lit("competition_name", 1)),
                                         "ref_table", T(COMP), "ref_value", S("name")));
    Table *confirm = UIBox_button(TAB("button", S("confirm_contest_name"), "colour", P(G->C.PALE_GREEN), "minw", N(2.65), "minh", N(1),
                                      "label", T(ARR(S(lit("competition_name", 2)))), "scale", N(0.5)));
    Table *t = node3(UIT_ROOT, TAB("align", S("cm"), "colour", P(G->C.CLEAR)), ARR(
        T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.2), "r", N(0.1), "emboss", N(0.1), "colour", P(G->C.L_BLACK), "mid", B(true)), ARR(
            T(node3(UIT_R, TAB("align", S("cm")), ARR(T(input)))),
            T(confirm))))));
    return t;
}

// UI_definitions.lua:6262
Table *UIDEF_language_selector(void) {
    Table *rows = tnew();
    Table *langs = tnew();
    // note: pairs(G.LANGUAGES): insertion order here; sorted by label below
    for (TIter it = titer(G->LANGUAGES); tnext(&it);) {
        if (!tbool(it.val.tab, "omit")) tseti(langs, tlen(langs) + 1, it.val);
    }
    tsort(langs, less_label, NULL);
    Table *_row = tnew();
    for (int k = 1;; k++) {
        Val vv = tgeti(langs, k);
        if (vv.t == V_NIL) break;
        Table *v = vv.tab;
        if (!G->F_HIDE_BETA_LANGS || !tbool(v, "beta")) {
            tseti(_row, tlen(_row) + 1, T(node3(UIT_C, TAB(
                "align", S("cm"), "func", S("beta_lang_alert"), "padding", N(0.05), "r", N(0.1), "minh", N(0.7), "minw", N(4.5),
                "button", S(tbool(v, "beta") ? "warn_lang" : "change_lang"), "ref_table", T(v),
                "colour", P(tbool(v, "beta") ? G->C.RED : G->C.BLUE), "hover", B(true), "shadow", B(true),
                "focus_args", T(TAB("snap_to", B(k == 1)))), ARR(
                T(node3(UIT_R, TAB("align", S("cm")), ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(v, "label"), "lang", T(v), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))))))));
        }
        if (truthy(tgeti(_row, 3)) || k == tlen(langs)) {
            tseti(rows, tlen(rows) + 1, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.1)), _row)));
            _row = tnew();
        }
    }

    Sprite *discord = NULL;
    discord = sprite_new(0, 0, 0.6, 0.6, ASSET_ATLAS_get("icons"), TAB("x", N(2), "y", N(0)));
    discord->moveable.node.states.drag.can = false;

    Table *contents = ARR(T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.05)), rows)));
    if (G->F_EXTERNAL_LINKS) // :6292 (the last entry: nil leaves no hole)
        tseti(contents, 2, T(node3(UIT_R, TAB("align", S("cm"), "padding", N(0.05)), ARR(
            T(node3(UIT_C, TAB("align", S("cm"), "padding", N(0.1), "minw", N(4), "maxw", N(4), "r", N(0.1), "minh", N(0.8), "hover", B(true), "colour", P(mix_colours(G->C.GREEN, G->C.GREY, 0.4, NULL)), "button", S("loc_survey"), "shadow", B(true)), ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(discord))))),
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(G->LANG, "button"), "scale", N(0.45), "colour", P(G->C.UI.TEXT_LIGHT), "shadow", B(true))))))))))));
    Table *t = create_UIBox_generic_options(TAB("contents", T(contents)));
    return t;
}

// UI_definitions.lua:6302 The second, identical definition of the file: the one in force (the one of :463 is weak).
Table *create_UIBox_highlight(Node *rect) {
    Table *t = node3(UIT_ROOT, TAB("align", S("cm"), "minh", N(rect->T->h + 0.1), "minw", N(rect->T->w + 0.15), "r", N(0.15),
                                   "colour", P(G->C.DARK_EDITION)), tnew());
    return t;
}
