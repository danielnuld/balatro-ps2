// functions/UI_definitions.lua:4328-5205: G.UIDEF.credits.
#include "all.h"

// UI_definitions.lua:4329 local text_scale = 0.75, the only upvalue of the six tab closures (never assigned again).
static const num text_scale = 0.75;

// note: the Lua writes the same three node shapes hundreds of times; the helpers below build exactly those tables
// (a fresh config per node, same keys in the same order), so the definitions are the Lua's, entry by entry.
// note: every text of this screen is a string literal of the Lua function, not a table of the game: it comes from
// data/game.dat's LITERALS.credits (data.h), where each tab has its texts in the order of the Lua's source. X(i) is
// the i-th text of the tab being built; the layout, the scales and the colours are the code below.

// LITERALS.credits.tabs[tab].text, which must have n texts (another count is another version of the game's credits)
static Table *tab_text(int tab, int n) {
    Table *x = ttab(ttabi(lits("credits.tabs"), tab), "text");
    if (tlen(x) != n) ERROR("data/game.dat: tab %d of the credits has %d texts, not %d: another version of the game", tab, tlen(x), n);
    return x;
}
#define X(i) lit_at(x, i)

// {n=G.UIT.T, config={text = text, scale = scale, colour = colour, shadow = true}}
static Val txt(const char *text, num scale, float *colour) {
    return T(TAB("n", N(UIT_T), "config", T(TAB("text", S(text), "scale", N(scale), "colour", P(colour), "shadow", B(true)))));
}
// {n=G.UIT.R, config={align = align, padding = padding}, nodes=nodes}
static Val row(const char *align, num padding, Table *nodes) {
    return T(TAB("n", N(UIT_R), "config", T(TAB("align", S(align), "padding", N(padding))), "nodes", T(nodes)));
}
// {n=n, config={align = align, padding = padding, outline_colour = G.C.JOKER_GREY, r = 0.1, outline = 1}, nodes=nodes}
static Val box(UIType n, const char *align, num padding, Table *nodes) {
    return T(TAB("n", N(n), "config", T(TAB("align", S(align), "padding", N(padding), "outline_colour", P(G->C.JOKER_GREY),
                                            "r", N(0.1), "outline", N(1))),
                 "nodes", T(nodes)));
}
// {n=G.UIT.C, config={align = "tl", padding = padding[, minw = minw]}, nodes={ one R{cl, padding 0}[T] per name }}:
// the n texts of x from `from` on
static Val names(num padding, Val minw, num scale, float *colour, Table *x, int from, int n) {
    Table *nodes = tnew();
    for (int i = 0; i < n; i++) tinsert(nodes, row("cl", 0, ARR(txt(X(from + i), scale, colour))));
    Table *config = TAB("align", S("tl"), "padding", N(padding));
    if (truthy(minw)) tset(config, "minw", minw);
    return T(TAB("n", N(UIT_C), "config", T(config), "nodes", T(nodes)));
}
// {n=G.UIT.ROOT, config={align = "cm", padding = 0.2, colour = colour, r = 0.1, emboss = 0.05, minh = 6, minw = minw}, nodes=nodes}
static Val tab_root(float *colour, num minw, Table *nodes) {
    return T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.2), "colour", P(colour), "r", N(0.1),
                                                   "emboss", N(0.05), "minh", N(6), "minw", N(minw))),
                 "nodes", T(nodes)));
}

// UI_definitions.lua:4337 the first tab
static Val credits_production_fn(void *ctx, int argc, Val *argv) {
    float *LIGHT = G->C.UI.TEXT_LIGHT;
    Table *x = tab_text(1, 55);
    // :4339 the soundtrack column; the link's row is a nil hole at index 3 without G.F_EXTERNAL_LINKS
    Table *ost = tnew();
    tseti(ost, 1, row("cm", 0, ARR(txt(X(1), text_scale * 0.8, LIGHT))));
    tseti(ost, 2, row("cm", 0, ARR(txt(X(2), text_scale * 0.8, LIGHT), txt(X(3), text_scale * 0.8, G->C.BLUE))));
    if (G->F_EXTERNAL_LINKS)
        tseti(ost, 3, row("cm", 0, ARR(T(UIBox_button(TAB("label", T(ARR(S(X(4)))), "button", S("louisf_insta")))))));
    tseti(ost, 4, row("cm", 0.2, ARR(txt(X(5), text_scale * 0.5, LIGHT))));

    // :4354 porting: the heading, then five times a platform and a column of one name
    Table *porting = ARR(row("cm", 0, ARR(txt(X(6), text_scale * 0.6, G->C.WHITE))));
    for (int i = 0; i < 5; i++) {
        tinsert(porting, row("cm", 0, ARR(txt(X(7 + 2 * i), text_scale * 0.45, G->C.GOLD))));
        tinsert(porting, row("tm", 0, ARR(names(0.03, NIL, text_scale * 0.35, LIGHT, x, 8 + 2 * i, 1))));
    }

    // :4409 localization: two headings, a column of 14 languages and a column of their 14 teams
    Table *loc = ARR(
        row("cm", 0, ARR(txt(X(17), text_scale * 0.6, G->C.WHITE))),
        row("cm", 0, ARR(txt(X(18), text_scale * 0.6, G->C.FILTER))),
        row("tm", 0, ARR(
            names(0.05, NIL, text_scale * 0.35, G->C.FILTER, x, 19, 14),
            names(0.05, NIL, text_scale * 0.35, LIGHT, x, 33, 14))));

    // :4507 testing: the heading and 8 names
    Table *qa = ARR(
        row("cm", 0, ARR(txt(X(47), text_scale * 0.6, G->C.WHITE))),
        row("tm", 0, ARR(names(0.03, NIL, text_scale * 0.35, LIGHT, x, 48, 8))));

    return tab_root(G->C.BLACK, 10, ARR(box(UIT_C, "cm", 0.2, ost), box(UIT_C, "tm", 0.1, porting), box(UIT_C, "cm", 0.1, loc),
                                        box(UIT_C, "tm", 0.1, qa)));
}

// {n=G.UIT.B, config={align = "tl", w=0.25, h=0}, nodes={}}
static Val publishing_gap(void) {
    return T(TAB("n", N(UIT_B), "config", T(TAB("align", S("tl"), "w", N(0.25), "h", N(0))), "nodes", T(tnew())));
}

// UI_definitions.lua:4547 the second tab: the publisher, then three pairs of columns (names, roles) of 13, 13 and 12
static Val credits_publishing_fn(void *ctx, int argc, Val *argv) {
    float *LIGHT = G->C.UI.TEXT_LIGHT, *GOLD = G->C.GOLD;
    num ns = text_scale * 0.42, rs = text_scale * 0.35;
    Table *x = tab_text(2, 77);
    return tab_root(G->C.BLACK, 10, ARR(box(UIT_C, "cm", 0.1, ARR(
        row("cm", 0, ARR(txt(X(1), text_scale * 0.6, G->C.RED))),
        row("tm", 0, ARR(
            names(0.05, NIL, ns, LIGHT, x, 2, 13),
            names(0.093, NIL, rs, GOLD, x, 15, 13),
            publishing_gap(),
            names(0.05, NIL, ns, LIGHT, x, 28, 13),
            names(0.093, NIL, rs, GOLD, x, 41, 13),
            publishing_gap(),
            names(0.05, NIL, ns, LIGHT, x, 54, 12),
            names(0.093, NIL, rs, GOLD, x, 66, 12)))))));
}

// UI_definitions.lua:4803 the third tab
static Val credits_collabs_fn(void *ctx, int argc, Val *argv) {
    tab_text(3, 0);
    // :4804 G.collab_credits = G.collab_credits or {...}: [name] = {artist = , artist_link = , ml_text = {...}, art = },
    // from LITERALS.credits.collabs, which has the entries in the order of the Lua's source (artist, artist_link and
    // ml_text absent where the Lua has nil or nothing)
    if (!G->collab_credits) {
        Table *cc = tnew(), *collabs = lits("credits.collabs");
        for (int i = 1; truthy(tgeti(collabs, i)); i++) {
            Table *v = ttabi(collabs, i), *c = tnew();
            if (truthy(tget(v, "artist"))) tset(c, "artist", tget(v, "artist"));
            if (truthy(tget(v, "artist_link"))) tset(c, "artist_link", tget(v, "artist_link"));
            if (truthy(tget(v, "ml_text"))) tsett(c, "ml_text", tcopy(ttab(v, "ml_text")));
            tset(c, "art", tget(v, "art"));
            tsett(cc, tstr(v, "name"), c);
        }
        G->collab_credits = cc;
    }

    // :4997 (id = nil: not set)
    Moveable *object = NEW(Moveable);
    Moveable_init(object, 0, 0, 0, 0); // Moveable()
    Table *middle = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minh", N(4.8), "minw", N(12.3))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("func", S("CREDITS_check_collab"), "object", O(object))))))));

    // :5001 note: pairs order does not matter, the keys are distinct and sorted right after (byte-wise, as LOVE's "C" locale)
    Table *collab_options = tnew();
    for (TIter it = titer(G->collab_credits); tnext(&it);) tinsert(collab_options, it.key);

    tsort(collab_options, NULL, NULL);

    G->viewed_collab = tstri(collab_options, 1);

    return tab_root(G->C.L_BLACK, 10, ARR(box(UIT_C, "cm", 0.2, ARR(
        T(create_option_cycle(TAB("options", T(collab_options), "opt_callback", S("change_viewed_collab"), "current_option", N(1),
                                  "colour", P(G->C.RED), "w", N(4), "mid", T(middle))))))));
}

// One licence box of the fourth tab (:5028, :5038, :5048, :5058): four texts from `from` on (the sound, the words
// before the licence, the licence, the end of the sentence)
static Val sound_box(Table *x, int from, float *licence_colour) {
    float *LIGHT = G->C.UI.TEXT_LIGHT;
    return box(UIT_R, "cm", 0.1, ARR(
        row("cm", 0, ARR(txt(X(from), text_scale * 0.5, LIGHT))),
        row("cm", 0, ARR(txt(X(from + 1), text_scale * 0.5, LIGHT), txt(X(from + 2), text_scale * 0.5, licence_colour),
                         txt(X(from + 3), text_scale * 0.5, LIGHT)))));
}

// UI_definitions.lua:5022 the fourth tab
static Val credits_sounds_fn(void *ctx, int argc, Val *argv) {
    Table *x = tab_text(4, 18);
    return tab_root(G->C.BLACK, 10, ARR(
        row("cm", 0, ARR(txt(X(1), text_scale * 0.6, G->C.UI.TEXT_LIGHT), txt(X(2), text_scale * 0.6, G->C.BLUE))),
        sound_box(x, 3, G->C.GOLD),
        sound_box(x, 7, G->C.ORANGE),
        sound_box(x, 11, G->C.ORANGE),
        sound_box(x, 15, G->C.ORANGE)));
}

// UI_definitions.lua:5073 the fifth tab
static Val credits_imagery_fn(void *ctx, int argc, Val *argv) {
    float *LIGHT = G->C.UI.TEXT_LIGHT;
    num s = text_scale * 0.5;
    Table *x = tab_text(5, 13);
    Table *font = ARR(
        row("cm", 0, ARR(txt(X(1), s, LIGHT))),
        row("cm", 0, ARR(txt(X(2), s, LIGHT), txt(X(3), s, G->C.GOLD), txt(X(4), s, LIGHT))));
    // :5084 the last entry, nil with G.F_BASIC_CREDITS
    if (!G->F_BASIC_CREDITS)
        tseti(font, 3, row("cm", 0, ARR(txt(X(5), s, LIGHT))));
    return tab_root(G->C.BLACK, 10, ARR(
        box(UIT_R, "cm", 0.1, font),
        box(UIT_R, "cm", 0.1, ARR(row("cm", 0, ARR(txt(X(6), s, LIGHT))))),
        box(UIT_R, "cm", 0.1, ARR(
            row("cm", 0, ARR(txt(X(7), s, LIGHT))),
            row("cm", 0, ARR(txt(X(8), s, LIGHT), txt(X(9), s, G->C.GOLD), txt(X(10), s, LIGHT))))),
        row("cm", 0, ARR(txt(X(11), text_scale * 0.6, LIGHT))),
        row("cm", 0, ARR(txt(X(12), text_scale * 0.6, LIGHT))),
        row("cm", 0, ARR(txt(X(13), text_scale * 0.6, G->C.BLUE)))));
}

// UI_definitions.lua:5118 the sixth tab: two headings and two columns of 10 names
static Val credits_misc_fn(void *ctx, int argc, Val *argv) {
    float *LIGHT = G->C.UI.TEXT_LIGHT;
    Table *x = tab_text(6, 22);
    return tab_root(G->C.BLACK, 6, ARR(
        box(UIT_R, "cm", 0.1, ARR(row("cm", 0, ARR(txt(X(1), text_scale * 0.6, G->C.WHITE))))),
        box(UIT_R, "cm", 0.1, ARR(
            row("cm", 0, ARR(txt(X(2), text_scale * 0.6, G->C.GREEN))),
            row("tm", 0, ARR(
                names(0.05, N(2.5), text_scale * 0.5, LIGHT, x, 3, 10),
                names(0.05, N(2.5), text_scale * 0.5, LIGHT, x, 13, 10)))))));
}

// UI_definitions.lua:4328
Table *UIDEF_credits(void) {
    Table *tabs = lits("credits.tabs"); // the label of each tab
#define LABEL(i) tget(ttabi(tabs, i), "label")
    Table *t = create_UIBox_generic_options(TAB("contents", T(ARR(
        row("cm", 0, ARR(
            T(create_tabs(TAB(
                "tabs", T(ARR(
                    T(TAB("label", LABEL(1), "chosen", B(true), "tab_definition_function", FN(credits_production_fn, NULL))),
                    T(TAB("label", LABEL(2), "tab_definition_function", FN(credits_publishing_fn, NULL))),
                    T(TAB("label", LABEL(3), "tab_definition_function", FN(credits_collabs_fn, NULL))),
                    T(TAB("label", LABEL(4), "tab_definition_function", FN(credits_sounds_fn, NULL))),
                    T(TAB("label", LABEL(5), "tab_definition_function", FN(credits_imagery_fn, NULL))),
                    T(TAB("label", LABEL(6), "tab_definition_function", FN(credits_misc_fn, NULL))))),
                "snap_to_nav", B(true))))))))));
    return t;
}
