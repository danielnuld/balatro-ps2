// functions/UI_definitions.lua:3183-3808: the run info's stake and voucher tabs, the deck view, tally_sprite and the
// collection's card pages (jokers ... seals).
// note: C does not fix the order in which the arguments of TAB / ARR are evaluated, so whatever a definition builds
// with side effects (sprites, DynaTexts, UIBoxes, widgets) is made in locals first, in the Lua's textual order.
#include "all.h"

// a colour as a table value; NULL as nil
static Val COL(const float *c) { return c ? P((void *)c) : NIL; }
// x of G.ROOM.T.x + 0.2*G.ROOM.T.w/2 (the y is G.ROOM.T.h): where every area of this file is created
static float room_x(void) { return G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2; }

// UI_definitions.lua:3183
Table *UIDEF_current_stake(void) {
    Table *other_col = NULL;
    int stake = (int)tnum(G->GAME, "stake");
    Table *Stake = ttab(G->P_CENTER_POOLS, "Stake");
    if (tnum(G->GAME, "stake") > 2) {
        Table *stake_desc_rows = ARR(T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_also_applied"), NULL), "scale", N(0.4), "colour", COL(G->C.WHITE)))))
        )))));
        for (int i = stake - 1; i >= 2; i--) {
            Table *_stake_desc = tnew();
            Table *_stake_center = ttabi(Stake, i);
            localize(T(TAB("type", S("descriptions"), "key", tget(_stake_center, "key"), "set", tget(_stake_center, "set"), "nodes", T(_stake_desc))), NULL);
            Table *_full_desc = tnew();
            for (int k = 1; truthy(tgeti(_stake_desc, k)); k++) {
                tinsert(_full_desc, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", tgeti(_stake_desc, k))));
            }
            tseti(_full_desc, tlen(_full_desc), NIL);
            tinsert(stake_desc_rows, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "colour", COL(get_stake_col(i)), "r", N(0.1), "minh", N(0.35), "minw", N(0.35), "emboss", N(0.05))), "nodes", T(tnew()))),
                    T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.1)))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.03), "colour", COL(G->C.WHITE), "r", N(0.1), "minh", N(0.7), "minw", N(4.8))), "nodes", T(_full_desc)))
            )))));
        }
        other_col = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "colour", COL(G->C.L_BLACK))), "nodes", T(stake_desc_rows));
    }

    Sprite *stake_sprite = get_stake_sprite(stake, 0.8f);
    Table *_stake_desc = tnew();
    Table *_stake_center = ttabi(Stake, stake);
    localize(T(TAB("type", S("descriptions"), "key", tget(_stake_center, "key"), "set", tget(_stake_center, "set"), "nodes", T(_stake_desc))), NULL);
    Table *_full_desc = tnew();
    for (int k = 1; truthy(tgeti(_stake_desc, k)); k++) {
        tinsert(_full_desc, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", tgeti(_stake_desc, k))));
    }
    tseti(_full_desc, tlen(_full_desc), NIL);
    Val name_text = localize(T(TAB("type", S("name_text"), "key", tget(ttabi(Stake, stake), "key"), "set", S("Stake"))), NULL);
    Table *current_col = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minw", N(4))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("colour", COL(G->C.BLUE), "object", O(stake_sprite), "hover", B(true), "can_collide", B(false))))),
            T(TAB("n", N(UIT_T), "config", T(TAB("text", name_text, "scale", N(0.45), "colour", COL(G->C.WHITE)))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", COL(get_stake_col(stake)), "r", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", COL(G->C.WHITE), "r", N(0.05))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.03), "minh", N(0.7), "minw", N(3.8))), "nodes", T(_full_desc)))
            ))))
        ))))
    )));

    // {current_col, other_col}: other_col may be nil (a hole at 2)
    Table *nodes = ARR(T(current_col));
    if (other_col) tseti(nodes, 2, T(other_col));
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", COL(G->C.BLACK), "r", N(0.1), "padding", N(0.1))), "nodes", T(nodes));
}

// UI_definitions.lua:3237 the sort's comparator
static bool view_deck_less(Val a, Val b, void *ctx) {
    return Card_get_nominal((Card *)a.obj, "suit") > Card_get_nominal((Card *)b.obj, "suit");
}

// {{string = ''..a, colour = flip_col}, {string = ''..b, colour = G.C.BLUE}} (UI_definitions.lua:3339, :3368-3378)
static Table *tally_pair(num a, const float *flip_col, num b) {
    return ARR(T(TAB("string", S(number_tostring(a)), "colour", COL(flip_col))),
               T(TAB("string", S(number_tostring(b)), "colour", COL(G->C.BLUE))));
}

// UI_definitions.lua:3233
Table *UIDEF_view_deck(bool unplayed_only) {
    Table *deck_tables = tnew();
    remove_nils(G->playing_cards); // (the Lua discards the result too)
    G->VIEWING_DECK = true;
    tsort(G->playing_cards, view_deck_less, NULL);
    Table *SUITS = TAB("Spades", T(tnew()), "Hearts", T(tnew()), "Clubs", T(tnew()), "Diamonds", T(tnew()));
    const char *suit_map[] = {"Spades", "Hearts", "Clubs", "Diamonds"};
    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        Table *suit = ttab(SUITS, tstr(v->base, "suit"));
        if (!suit) ERROR("UI_definitions.lua:3246: bad argument #1 to 'insert' (table expected, got nil)");
        tinsert(suit, O(v));
    }
    for (int j = 1; j <= 4; j++) {
        Table *suit = ttab(SUITS, suit_map[j - 1]);
        if (truthy(tgeti(suit, 1))) {
            CardArea *view_deck = NEW(CardArea);
            CardArea_init(view_deck,
                room_x(), G->ROOM->T->h,
                6.5f * G->CARD_W,
                0.6f * G->CARD_H,
                TAB("card_limit", N(tlen(suit)), "type", S("title"), "view_deck", B(true), "highlight_limit", N(0), "card_w", N(G->CARD_W * 0.7), "draw_layers", T(ARR(S("card")))));
            tinsert(deck_tables,
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(view_deck)))))
                )))));

            int n = tlen(suit);
            for (int i = 1; i <= n; i++) {
                Card *src = tobji(suit, i);
                if (src) {
                    bool greyed = false;
                    float _scale = 0.7f;
                    if (unplayed_only && !((src->area && src->area == G->deck) || tbool(src->ability, "wheel_flipped"))) {
                        greyed = true;
                    }
                    Card *copy = copy_card(src, NULL, _scale, NIL, false);
                    copy->greyed = greyed;
                    copy->moveable.node.T->x = view_deck->moveable.node.T->x + view_deck->moveable.node.T->w / 2;
                    copy->moveable.node.T->y = view_deck->moveable.node.T->y;

                    Card_hard_set_T(copy, NAN, NAN, NAN, NAN);
                    CardArea_emplace(view_deck, copy, NULL, false);
                }
            }
        }
    }

    const float *flip_col = G->C.WHITE;

    Table *suit_tallies = TAB("Spades", N(0), "Hearts", N(0), "Clubs", N(0), "Diamonds", N(0));
    Table *mod_suit_tallies = TAB("Spades", N(0), "Hearts", N(0), "Clubs", N(0), "Diamonds", N(0));
    num rank_tallies[14] = {0};     // [1..13], as the Lua
    num mod_rank_tallies[14] = {0};
    num face_tally = 0;
    num mod_face_tally = 0;
    num num_tally = 0;
    num mod_num_tally = 0;
    num ace_tally = 0;
    num mod_ace_tally = 0;
    num wheel_flipped = 0;

    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        if (!streq(tstr(v->ability, "name"), "Stone Card") && (!unplayed_only || ((v->area && v->area == G->deck) || tbool(v->ability, "wheel_flipped")))) {
            if (tbool(v->ability, "wheel_flipped") && unplayed_only) wheel_flipped = wheel_flipped + 1;
            // For the suits
            const char *suit = tstr(v->base, "suit");
            tsetn(suit_tallies, suit, tnum(suit_tallies, suit) + 1);
            tsetn(mod_suit_tallies, "Spades", tnum(mod_suit_tallies, "Spades") + (Card_is_suit(v, "Spades", false, false) ? 1 : 0));
            tsetn(mod_suit_tallies, "Hearts", tnum(mod_suit_tallies, "Hearts") + (Card_is_suit(v, "Hearts", false, false) ? 1 : 0));
            tsetn(mod_suit_tallies, "Clubs", tnum(mod_suit_tallies, "Clubs") + (Card_is_suit(v, "Clubs", false, false) ? 1 : 0));
            tsetn(mod_suit_tallies, "Diamonds", tnum(mod_suit_tallies, "Diamonds") + (Card_is_suit(v, "Diamonds", false, false) ? 1 : 0));

            // for face cards/numbered cards/aces
            num card_id = Card_get_id(v);
            face_tally = face_tally + ((card_id == 11 || card_id == 12 || card_id == 13) ? 1 : 0);
            mod_face_tally = mod_face_tally + (Card_is_face(v, false) ? 1 : 0);
            if (card_id > 1 && card_id < 11) {
                num_tally = num_tally + 1;
                if (!v->debuff) mod_num_tally = mod_num_tally + 1;
            }
            if (card_id == 14) {
                ace_tally = ace_tally + 1;
                if (!v->debuff) mod_ace_tally = mod_ace_tally + 1;
            }

            // ranks
            int r = (int)card_id - 1;
            if (card_id != floor(card_id) || r < 1 || r > 13) ERROR("UI_definitions.lua:3318: attempt to perform arithmetic on a nil value (rank_tallies[%g])", (double)card_id - 1);
            rank_tallies[r] = rank_tallies[r] + 1;
            if (!v->debuff) mod_rank_tallies[r] = mod_rank_tallies[r] + 1;
        }
    }

    bool modded = (face_tally != mod_face_tally) ||
        (tnum(mod_suit_tallies, "Spades") != tnum(suit_tallies, "Spades")) ||
        (tnum(mod_suit_tallies, "Hearts") != tnum(suit_tallies, "Hearts")) ||
        (tnum(mod_suit_tallies, "Clubs") != tnum(suit_tallies, "Clubs")) ||
        (tnum(mod_suit_tallies, "Diamonds") != tnum(suit_tallies, "Diamonds"));

    if (wheel_flipped > 0) flip_col = mix_colours(G->C.FILTER, G->C.WHITE, 0.7f, NULL);

    Table *rank_cols = tnew();
    for (int i = 13; i >= 1; i--) {
        bool mod_delta = mod_rank_tallies[i] != rank_tallies[i];
        // rank_name_mapping = {2, 3, 4, 5, 6, 7, 8, 9, 10, 'J', 'Q', 'K', 'A'}
        Val rank_name = i <= 9 ? N(i + 1) : S(i == 10 ? "J" : i == 11 ? "Q" : i == 12 ? "K" : "A");
        Table *tally_node;
        if (mod_delta) {
            tally_node = TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB(
                "string", T(tally_pair(rank_tallies[i], flip_col, mod_rank_tallies[i])),
                "colours", T(ARR(COL(G->C.RED))), "scale", N(0.4), "y_offset", N(-2), "silent", B(true), "shadow", B(true), "pop_in_rate", N(10), "pop_delay", N(4)))))));
        } else {
            tally_node = TAB("n", N(UIT_T), "config", T(TAB("text", N(rank_tallies[i]), "colour", COL(flip_col), "scale", N(0.45), "shadow", B(true))));
        }
        tinsert(rank_cols, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.04), "emboss", N(0.04), "minw", N(0.5), "colour", COL(G->C.L_BLACK))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", rank_name, "colour", COL(G->C.JOKER_GREY), "scale", N(0.35), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "minw", N(0.4))), "nodes", T(ARR(
                T(tally_node)
            ))))
        )))));
    }

    // the objects of t, in the Lua's order
    Back *selected_back = tobj(G->GAME, "selected_back");
    DynaText *name_text = DynaText_new(TAB("string", S(selected_back->loc_name), "colours", T(ARR(COL(G->C.WHITE))), "bump", B(true), "rotate", B(true), "shadow", B(true),
        "scale", N(0.6 - str_len(selected_back->loc_name) * 0.01))); // note: string.len: bytes
    Table *back_def = Back_generate_UI(selected_back, NULL, 0.7f, 0.5f, tstr(G->GAME, "challenge"));
    UIBox *back_box = UIBox_new(TAB(
        "definition", T(back_def),
        "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));
    Table *base_strings = ARR(T(TAB("string", localize(S("k_base_cards"), NULL), "colour", COL(G->C.RED))));
    if (modded) tseti(base_strings, 2, T(TAB("string", localize(S("k_effective"), NULL), "colour", COL(G->C.BLUE))));
    DynaText *base_text = DynaText_new(TAB("string", T(base_strings), "colours", T(ARR(COL(G->C.RED))), "silent", B(true), "scale", N(0.4), "pop_in_rate", N(10), "pop_delay", N(4)));
    Table *ts_aces = tally_sprite(TAB("x", N(1), "y", N(0)), T(tally_pair(ace_tally, flip_col, mod_ace_tally)), ARR(localize(S("k_aces"), NULL))); // Aces
    Table *ts_face = tally_sprite(TAB("x", N(2), "y", N(0)), T(tally_pair(face_tally, flip_col, mod_face_tally)), ARR(localize(S("k_face_cards"), NULL))); // Face
    Table *ts_num = tally_sprite(TAB("x", N(3), "y", N(0)), T(tally_pair(num_tally, flip_col, mod_num_tally)), ARR(localize(S("k_numbered_cards"), NULL))); // Numbers
    Table *ts_spades = tally_sprite(TAB("x", N(3), "y", N(1)), T(tally_pair(tnum(suit_tallies, "Spades"), flip_col, tnum(mod_suit_tallies, "Spades"))), ARR(localize(S("Spades"), "suits_plural")));
    Table *ts_hearts = tally_sprite(TAB("x", N(0), "y", N(1)), T(tally_pair(tnum(suit_tallies, "Hearts"), flip_col, tnum(mod_suit_tallies, "Hearts"))), ARR(localize(S("Hearts"), "suits_plural")));
    Table *ts_clubs = tally_sprite(TAB("x", N(2), "y", N(1)), T(tally_pair(tnum(suit_tallies, "Clubs"), flip_col, tnum(mod_suit_tallies, "Clubs"))), ARR(localize(S("Clubs"), "suits_plural")));
    Table *ts_diamonds = tally_sprite(TAB("x", N(1), "y", N(1)), T(tally_pair(tnum(suit_tallies, "Diamonds"), flip_col, tnum(mod_suit_tallies, "Diamonds"))), ARR(localize(S("Diamonds"), "suits_plural")));

    // the last row: {modded and {...} or nil, wheel_flipped > 0 and {...} or nil} (holes kept at their index)
    Table *legend = tnew();
    if (modded) {
        tseti(legend, 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.3), "r", N(0.1), "colour", COL(mix_colours(G->C.BLUE, G->C.WHITE, 0.7f, NULL)))), "nodes", T(tnew()))),
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s", vstr(localize(S("ph_deck_preview_effective"), NULL)))), "colour", COL(G->C.WHITE), "scale", N(0.3)))))
        )))));
    }
    if (wheel_flipped > 0) {
        tseti(legend, 2, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("padding", N(0.3), "r", N(0.1), "colour", COL(flip_col))), "nodes", T(tnew()))),
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s", vstr(wheel_flipped > 1 ?
                localize(T(TAB("type", S("variable"), "key", S("deck_preview_wheel_plural"), "vars", T(ARR(N(wheel_flipped))))), NULL) :
                localize(T(TAB("type", S("variable"), "key", S("deck_preview_wheel_singular"), "vars", T(ARR(N(wheel_flipped))))), NULL)))), "colour", COL(G->C.WHITE), "scale", N(0.3)))))
        )))));
    }

    Table *t =
    TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", COL(G->C.CLEAR))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(tnew()))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(1.5), "minh", N(2), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", COL(G->C.L_BLACK), "emboss", N(0.05), "padding", N(0.15))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(name_text)))))
                        )))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.1), "minw", N(2.5), "minh", N(1.3), "colour", COL(G->C.WHITE), "emboss", N(0.05))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(back_box)))))
                        ))))
                    )))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "outline_colour", COL(G->C.L_BLACK), "line_emboss", N(0.05), "outline", N(1.5))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05), "padding", N(0.07))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(base_text)))))
                        )))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05), "padding", N(0.1))), "nodes", T(ARR(
                            T(ts_aces),
                            T(ts_face),
                            T(ts_num)
                        )))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05), "padding", N(0.1))), "nodes", T(ARR(
                            T(ts_spades),
                            T(ts_hearts)
                        )))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.05), "padding", N(0.1))), "nodes", T(ARR(
                            T(ts_clubs),
                            T(ts_diamonds)
                        ))))
                    ))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(rank_cols))),
                T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.1), "h", N(0.1)))))
            )))),
            T(TAB("n", N(UIT_B), "config", T(TAB("w", N(0.2), "h", N(0.1))))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables)))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.8), "padding", N(0.05))), "nodes", T(legend)))
    )));
    return t;
}

// UI_definitions.lua:3404
Table *tally_sprite(Table *pos, Val value, Table *tooltip) {
    const float *text_colour = G->C.BLACK;
    if (value.t == V_TAB && veq(tget(ttabi(value.tab, 1), "string"), tget(ttabi(value.tab, 2), "string"))) {
        text_colour = UI_colour(ttabi(value.tab, 1), "colour");
        if (!text_colour) text_colour = G->C.WHITE;
        value = tget(ttabi(value.tab, 1), "string");
    }
    Val px = tget(pos, "x"), py = tget(pos, "y");
    Sprite *t_s = NEW(Sprite);
    Sprite_init(t_s, 0, 0, 0.5f, 0.5f, ASSET_ATLAS_get(fmt("ui_%d", tbool(G->SETTINGS, "colourblind_option") ? 2 : 1)),
                TAB("x", truthy(px) ? px : N(0), "y", truthy(py) ? py : N(0)));
    t_s->moveable.node.states.drag.can = false;
    t_s->moveable.node.states.hover.can = false;
    t_s->moveable.node.states.collide.can = false;
    Table *value_node;
    if (value.t == V_TAB) {
        value_node = TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB("string", value, "colours", T(ARR(COL(G->C.RED))), "scale", N(0.4), "silent", B(true), "shadow", B(true), "pop_in_rate", N(10), "pop_delay", N(4)))))));
    } else {
        value_node = TAB("n", N(UIT_T), "config", T(TAB("text", truthy(value) ? value : S("NIL"), "colour", COL(text_colour), "scale", N(0.4), "shadow", B(true))));
    }
    return
    TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.07), "force_focus", B(true), "focus_args", T(TAB("type", S("tally_sprite"))), "tooltip", T(TAB("text", T(tooltip))))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "padding", N(0.04), "emboss", N(0.05), "colour", COL(G->C.JOKER_GREY))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("w", N(0.5), "h", N(0.5), "can_collide", B(false), "object", O(t_s), "tooltip", T(TAB("text", T(tooltip)))))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(value_node)
        ))))
    )));
}

// UI_definitions.lua:3426
Table *UIDEF_used_vouchers(void) {
    bool silent = false;
    Table *keys_used = tnew();
    int area_count = 0;
    Table *voucher_areas = tnew();
    Table *voucher_tables = tnew();
    Table *voucher_table_rows = tnew();
    Table *Voucher = ttab(G->P_CENTER_POOLS, "Voucher");
    for (int k = 1; truthy(tgeti(Voucher, k)); k++) {
        Table *v = ttabi(Voucher, k);
        int key = 1 + (int)floor((k - 0.1) / 2);
        if (!truthy(tgeti(keys_used, key))) tseti(keys_used, key, T(tnew()));
        if (truthy(tgetv(ttab(G->GAME, "used_vouchers"), tget(v, "key")))) {
            tinsert(ttabi(keys_used, key), T(v));
        }
    }
    for (int k = 1; truthy(tgeti(keys_used, k)); k++) {
        if (!tempty(ttabi(keys_used, k))) {
            area_count = area_count + 1;
        }
    }
    for (int k = 1; truthy(tgeti(keys_used, k)); k++) {
        Table *v = ttabi(keys_used, k);
        if (!tempty(v)) {
            if (tlen(voucher_areas) == 5 || tlen(voucher_areas) == 10) {
                tinsert(voucher_table_rows,
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(voucher_tables))));
                voucher_tables = tnew();
            }
            CardArea *area = NEW(CardArea);
            CardArea_init(area,
                room_x(), G->ROOM->T->h,
                (tlen(v) == 1 ? 1 : 1.33f) * G->CARD_W,
                (area_count >= 10 ? 0.75f : 1.07f) * G->CARD_H,
                TAB("card_limit", N(2), "type", S("voucher"), "highlight_limit", N(0)));
            tinsert(voucher_areas, O(area));
            for (int kk = 1; truthy(tgeti(v, kk)); kk++) {
                Table *vv = ttabi(v, kk);
                Table *center = ttab(G->P_CENTERS, tstr(vv, "key"));
                Card *card = NEW(Card);
                Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center,
                          TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true), "bypass_lock", B(true)));
                tset(card->ability, "order", tget(vv, "order"));
                Card_start_materialize(card, NULL, silent, -1);
                silent = true;
                CardArea_emplace(area, card, NULL, false);
            }
            tinsert(voucher_tables,
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
                )))));
        }
    }
    tinsert(voucher_table_rows,
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(voucher_tables))));

    Table *t;
    if (silent) {
        DynaText *text = DynaText_new(TAB("string", T(ARR(localize(S("ph_vouchers_redeemed"), NULL))), "colours", T(ARR(COL(G->C.UI.TEXT_LIGHT))), "bump", B(true), "scale", N(0.6)));
        t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", COL(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(text)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(0.5))), "nodes", T(tnew()))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", COL(G->C.BLACK), "r", N(1), "padding", N(0.15), "emboss", N(0.05))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(voucher_table_rows)))
            ))))
        )));
    } else {
        DynaText *text = DynaText_new(TAB("string", T(ARR(localize(S("ph_no_vouchers"), NULL))), "colours", T(ARR(COL(G->C.UI.TEXT_LIGHT))), "bump", B(true), "scale", N(0.6)));
        t = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", COL(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(text)))))
        )));
    }
    return t;
}

// UI_definitions.lua:3501
static bool ev_refresh_alerts(void *ctx) {
    G->REFRESH_ALERTS = true;
    return true;
}

// UI_definitions.lua:3497
Table *create_UIBox_your_collection(void) {
    set_discover_tallies();
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .not_blockable = true,
        .func = ev_refresh_alerts}), NULL, false);
    Table *DT = G->DISCOVER_TALLIES;
    Table *b_jokers = UIBox_button(TAB("button", S("your_collection_jokers"), "label", T(ARR(localize(S("b_jokers"), NULL))), "count", tget(DT, "jokers"), "minw", N(5), "minh", N(1.7), "scale", N(0.6), "id", S("your_collection_jokers")));
    Table *b_decks = UIBox_button(TAB("button", S("your_collection_decks"), "label", T(ARR(localize(S("b_decks"), NULL))), "count", tget(DT, "backs"), "minw", N(5)));
    Table *b_vouchers = UIBox_button(TAB("button", S("your_collection_vouchers"), "label", T(ARR(localize(S("b_vouchers"), NULL))), "count", tget(DT, "vouchers"), "minw", N(5), "id", S("your_collection_vouchers")));
    Val cap_consumables = localize(S("k_cap_consumables"), NULL);
    Table *b_tarots = UIBox_button(TAB("button", S("your_collection_tarots"), "label", T(ARR(localize(S("b_tarot_cards"), NULL))), "count", tget(DT, "tarots"), "minw", N(4), "id", S("your_collection_tarots"), "colour", COL(G->C.SECONDARY_SET.Tarot)));
    Table *b_planets = UIBox_button(TAB("button", S("your_collection_planets"), "label", T(ARR(localize(S("b_planet_cards"), NULL))), "count", tget(DT, "planets"), "minw", N(4), "id", S("your_collection_planets"), "colour", COL(G->C.SECONDARY_SET.Planet)));
    Table *b_spectrals = UIBox_button(TAB("button", S("your_collection_spectrals"), "label", T(ARR(localize(S("b_spectral_cards"), NULL))), "count", tget(DT, "spectrals"), "minw", N(4), "id", S("your_collection_spectrals"), "colour", COL(G->C.SECONDARY_SET.Spectral)));
    Table *b_enhancements = UIBox_button(TAB("button", S("your_collection_enhancements"), "label", T(ARR(localize(S("b_enhanced_cards"), NULL))), "minw", N(5)));
    Table *b_seals = UIBox_button(TAB("button", S("your_collection_seals"), "label", T(ARR(localize(S("b_seals"), NULL))), "minw", N(5), "id", S("your_collection_seals")));
    Table *b_editions = UIBox_button(TAB("button", S("your_collection_editions"), "label", T(ARR(localize(S("b_editions"), NULL))), "count", tget(DT, "editions"), "minw", N(5), "id", S("your_collection_editions")));
    Table *b_boosters = UIBox_button(TAB("button", S("your_collection_boosters"), "label", T(ARR(localize(S("b_booster_packs"), NULL))), "count", tget(DT, "boosters"), "minw", N(5), "id", S("your_collection_boosters")));
    Table *b_tags = UIBox_button(TAB("button", S("your_collection_tags"), "label", T(ARR(localize(S("b_tags"), NULL))), "count", tget(DT, "tags"), "minw", N(5), "id", S("your_collection_tags")));
    Table *b_blinds = UIBox_button(TAB("button", S("your_collection_blinds"), "label", T(ARR(localize(S("b_blinds"), NULL))), "count", tget(DT, "blinds"), "minw", N(5), "minh", N(2.0), "id", S("your_collection_blinds"), "focus_args", T(TAB("snap_to", B(true)))));
    Table *t = create_UIBox_generic_options(TAB("back_func", S(G->STAGE == STAGES_RUN ? "options" : "exit_overlay_menu"), "contents", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.15))), "nodes", T(ARR(
            T(b_jokers),
            T(b_decks),
            T(b_vouchers),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1), "r", N(0.2), "colour", COL(G->C.BLACK))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "maxh", N(2.9))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", cap_consumables, "scale", N(0.45), "colour", COL(G->C.L_BLACK), "vert", B(true), "maxh", N(2.2)))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.15))), "nodes", T(ARR(
                    T(b_tarots),
                    T(b_planets),
                    T(b_spectrals)
                ))))
            ))))
        )))),
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.15))), "nodes", T(ARR(
            T(b_enhancements),
            T(b_seals),
            T(b_editions),
            T(b_boosters),
            T(b_tags),
            T(b_blinds)
        ))))
    ))));
    return t;
}

// localize('k_page')..' '..tostring(i)..'/'..tostring(n)
static Val page_label(int i, int n) { return S(fmt("%s %d/%d", vstr(localize(S("k_page"), NULL)), i, n)); }

// UI_definitions.lua:3535
Table *create_UIBox_your_collection_jokers(void) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 3; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            5 * G->CARD_W,
            0.95f * G->CARD_H,
            TAB("card_limit", N(5), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Joker = ttab(G->P_CENTER_POOLS, "Joker");
    Table *joker_options = tnew();
    int pages = (int)ceil((double)tlen(Joker) / (5 * tlen(yc)));
    for (int i = 1; i <= pages; i++) {
        tinsert(joker_options, page_label(i, pages));
    }

    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= tlen(yc); j++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Joker, i + (j - 1) * 5);
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            Val sticker = get_joker_win_sticker(center, false);
            card->sticker = sticker.t == V_STR ? sticker.s : NULL;
            CardArea_emplace(area, card, NULL, false);
        }
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *cycle = create_option_cycle(TAB("options", T(joker_options), "w", N(4.5), "cycle_shoulders", B(true), "opt_callback", S("your_collection_joker_page"), "current_option", N(1), "colour", COL(G->C.RED), "no_pips", B(true), "focus_args", T(TAB("snap_to", B(true), "nav", S("wide")))));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(cycle)
        ))))
    ))));
    return t;
}

// UI_definitions.lua:3577
Table *create_UIBox_your_collection_tarots(void) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            (4.25f + j) * G->CARD_W,
            1 * G->CARD_H,
            TAB("card_limit", N(4 + j), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Tarot = ttab(G->P_CENTER_POOLS, "Tarot");
    Table *tarot_options = tnew();
    int pages = (int)floor((double)tlen(Tarot) / 11);
    for (int i = 1; i <= pages; i++) {
        tinsert(tarot_options, page_label(i, pages));
    }

    for (int j = 1; j <= tlen(yc); j++) {
        for (int i = 1; i <= 4 + j; i++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Tarot, i + (j - 1) * (5));
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *cycle = create_option_cycle(TAB("options", T(tarot_options), "w", N(4.5), "cycle_shoulders", B(true), "opt_callback", S("your_collection_tarot_page"), "focus_args", T(TAB("snap_to", B(true), "nav", S("wide"))), "current_option", N(1), "colour", COL(G->C.RED), "no_pips", B(true)));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(cycle)
        ))))
    ))));
    return t;
}

// UI_definitions.lua:3619
Table *create_UIBox_your_collection_boosters(void) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            (5.25f) * G->CARD_W,
            1.3f * G->CARD_H,
            TAB("card_limit", N(4), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Booster = ttab(G->P_CENTER_POOLS, "Booster");
    Table *booster_options = tnew();
    int pages = (int)ceil((double)tlen(Booster) / 8);
    for (int i = 1; i <= pages; i++) {
        tinsert(booster_options, page_label(i, pages));
    }

    for (int j = 1; j <= tlen(yc); j++) {
        for (int i = 1; i <= 4; i++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Booster, i + (j - 1) * 4);
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W * 1.27f, G->CARD_H * 1.27f, NULL, center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *cycle = create_option_cycle(TAB("options", T(booster_options), "w", N(4.5), "cycle_shoulders", B(true), "opt_callback", S("your_collection_booster_page"), "focus_args", T(TAB("snap_to", B(true), "nav", S("wide"))), "current_option", N(1), "colour", COL(G->C.RED), "no_pips", B(true)));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(cycle)
        ))))
    ))));
    return t;
}

// UI_definitions.lua:3661
Table *create_UIBox_your_collection_planets(void) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            (6.25f) * G->CARD_W,
            1 * G->CARD_H,
            TAB("card_limit", N(6), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Planet = ttab(G->P_CENTER_POOLS, "Planet");
    for (int j = 1; j <= tlen(yc); j++) {
        for (int i = 1; i <= 6; i++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Planet, i + (j - 1) * (6));
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.7))), "nodes", T(tnew())))
    ))));
    return t;
}

// UI_definitions.lua:3696
Table *create_UIBox_your_collection_spectrals(void) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            (3.25f + j) * G->CARD_W,
            1 * G->CARD_H,
            TAB("card_limit", N(3 + j), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Spectral = ttab(G->P_CENTER_POOLS, "Spectral");
    for (int j = 1; j <= tlen(yc); j++) {
        for (int i = 1; i <= 3 + j; i++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Spectral, i + (j - 1) * 3 + j - 1);

            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    // note: the Lua counts the pages with the Tarot pool and labels them with the Spectral pool: kept
    Table *spectral_options = tnew();
    int loops = (int)floor((double)tlen(ttab(G->P_CENTER_POOLS, "Tarot")) / 9);
    for (int i = 1; i <= loops; i++) {
        tinsert(spectral_options, page_label(i, (int)floor((double)tlen(Spectral) / 9)));
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *cycle = create_option_cycle(TAB("options", T(spectral_options), "w", N(4.5), "cycle_shoulders", B(true), "opt_callback", S("your_collection_spectral_page"), "focus_args", T(TAB("snap_to", B(true), "nav", S("wide"))), "current_option", N(1), "colour", COL(G->C.RED), "no_pips", B(true)));
    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(cycle)
        ))))
    ))));
    return t;
}

// UI_definitions.lua:3739
Table *create_UIBox_your_collection_vouchers(const char *exit) {
    Table *deck_tables = tnew();

    Table *yc = tnew();
    G->your_collection = T(yc);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area,
            room_x(), G->ROOM->T->h,
            4.25f * G->CARD_W,
            1 * G->CARD_H,
            TAB("card_limit", N(4), "type", S("voucher"), "highlight_limit", N(0), "collection", B(true)));
        tseti(yc, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    Table *Voucher = ttab(G->P_CENTER_POOLS, "Voucher");
    Table *voucher_options = tnew();
    int pages = (int)ceil((double)tlen(Voucher) / (4 * tlen(yc)));
    for (int i = 1; i <= pages; i++) {
        tinsert(voucher_options, page_label(i, pages));
    }

    for (int i = 1; i <= 4; i++) {
        for (int j = 1; j <= tlen(yc); j++) {
            CardArea *area = tobji(yc, j);
            Table *center = ttabi(Voucher, i + (j - 1) * 4);
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            tsetn(card->ability, "order", i + (j - 1) * 4);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *cycle = create_option_cycle(TAB("options", T(voucher_options), "w", N(4.5), "cycle_shoulders", B(true), "opt_callback", S("your_collection_voucher_page"), "focus_args", T(TAB("snap_to", B(true), "nav", S("wide"))), "current_option", N(1), "colour", COL(G->C.RED), "no_pips", B(true)));
    Table *t = create_UIBox_generic_options(TAB("back_func", S(exit ? exit : "your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
            T(cycle)
        ))))
    ))));
    return t;
}

// UI_definitions.lua:3782
Table *create_UIBox_your_collection_seals(const char *exit) {
    Table *deck_tables = tnew();

    CardArea *area = NEW(CardArea);
    CardArea_init(area,
        room_x(), G->ROOM->T->h,
        4.25f * G->CARD_W,
        1.03f * G->CARD_H,
        TAB("card_limit", N(4), "type", S("title"), "highlight_limit", N(0)));
    G->your_collection = O(area); // ONE CardArea here, not an array
    tinsert(deck_tables,
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
        )))));

    Table *Seal = ttab(G->P_CENTER_POOLS, "Seal");
    for (int k = 1; truthy(tgeti(Seal, k)); k++) {
        Table *v = ttabi(Seal, k);
        Table *center = ttab(G->P_CENTERS, "c_base");
        Card *card = NEW(Card);
        // G.P_CARDS.empty does not exist: nil
        Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"), center, NULL);
        Card_set_seal(card, tstr(v, "key"), true, false);
        CardArea_emplace(area, card, NULL, false);
    }

    Val infotip = localize(S("ml_edition_seal_enhancement_explanation"), NULL);
    Table *t = create_UIBox_generic_options(TAB("infotip", infotip, "back_func", S(exit ? exit : "your_collection"), "snap_back", B(true), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", COL(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables)))
    ))));
    return t;
}
