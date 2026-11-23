// functions/common_events.lua:2397-2745: generate_card_ui, the text of a card's tooltip.
#include "all.h"

// {a, b, ...} with tseti, so a nil element stays a hole exactly as in the Lua's constructor
static Table *lv_of(int n, Val *v) {
    Table *t = tnew();
    for (int i = 0; i < n; i++) tseti(t, i + 1, v[i]);
    return t;
}
#define LV(...) lv_of((int)VA_COUNT(__VA_ARGS__), (Val[]){__VA_ARGS__})

// a or b
static Val vor(Val a, Val b) { return truthy(a) ? a : b; }
// a colour as a table value; NULL as nil
static Val colv(const float *c) { return c ? P((void *)c) : NIL; }
// G.P_CENTERS[key]; NULL as nil
static Table *center(const char *key) { return key ? ttab(G->P_CENTERS, key) : NULL; }
// info_queue[#info_queue+1] = v
static void queue(Table *info_queue, Table *v) { tseti(info_queue, tlen(info_queue) + 1, T(v)); }
// localize{...}
static Val loc(Table *args) { return localize(T(args), NULL); }
// v == 'literal'
static bool vis(Val v, const char *s) { return v.t == V_STR && streq(v.s, s); }

// common_events.lua:2533 / :2634 any joker with a negative edition, and the 'remove_negative' text exists
static bool has_negative_joker_text(void) {
    if (G->jokers && G->jokers->cards) {
        for (int k = 1; truthy(tgeti(G->jokers->cards, k)); k++) {
            Card *v = (Card *)tobji(G->jokers->cards, k);
            if ((v->edition && tbool(v->edition, "negative")) &&
                truthy(tpath(G->localization, "descriptions.Other.remove_negative")))
                return true;
        }
    }
    return false;
}

// common_events.lua:2535 / :2637 main_end = {}; localize{...}; main_end = main_end[1]
static Table *remove_negative_line(void) {
    Table *main_end = tnew();
    loc(TAB("type", S("other"), "key", S("remove_negative"), "nodes", T(main_end), "vars", T(tnew())));
    return ttabi(main_end, 1);
}

// common_events.lua:2397
Table *generate_card_ui(Table *_c, Table *full_UI_table, Table *specific_vars, const char *card_type, Table *badges,
                        bool hide_desc, Table *main_start, Table *main_end) {
    bool first_pass = false;
    if (!full_UI_table) {
        first_pass = true;
        full_UI_table = TAB("main", T(tnew()), "info", T(tnew()), "type", T(tnew()), "badges", T(badges ? badges : tnew()));
    }

    // (not full_UI_table.name and full_UI_table.main) or full_UI_table.info
    Table *desc_nodes = (!truthy(tget(full_UI_table, "name")) && truthy(tget(full_UI_table, "main")))
                            ? ttab(full_UI_table, "main") : ttab(full_UI_table, "info");
    const char *name_override = NULL;
    Table *info_queue = tnew();

    const char *name = tstr(_c, "name"); // _c.name
    const char *set = tstr(_c, "set");   // _c.set
    Table *config = ttab(_c, "config");  // _c.config
    Table *uc = ttab(_c, "unlock_condition");
#define NM(x) streq(name, x)

    if (truthy(tget(full_UI_table, "name"))) {
        Table *info = ttab(full_UI_table, "info");
        tseti(info, tlen(info) + 1, T(tnew()));
        desc_nodes = ttabi(info, tlen(info));
    }

    if (!truthy(tget(full_UI_table, "name"))) {
        if (specific_vars && tbool(specific_vars, "no_name")) {
            tset(full_UI_table, "name", B(true));
        } else if (streq(card_type, "Locked")) {
            tset(full_UI_table, "name", loc(TAB("type", S("name"), "set", S("Other"), "key", S("locked"), "nodes", T(tnew()))));
        } else if (streq(card_type, "Undiscovered")) {
            tset(full_UI_table, "name", loc(TAB("type", S("name"), "set", S("Other"),
                                                "key", S(fmt("undiscovered_%s", str_lower(set))), "name_nodes", T(tnew()))));
        } else if (specific_vars && (streq(card_type, "Default") || streq(card_type, "Enhanced"))) {
            if (NM("Stone Card")) tset(full_UI_table, "name", B(true));
            if (tbool(specific_vars, "playing_card") && !NM("Stone Card")) {
                Table *nm = tnew();
                tset(full_UI_table, "name", T(nm));
                Table *vars = LV(localize(tget(specific_vars, "value"), "ranks"), localize(tget(specific_vars, "suit"), "suits_plural"));
                tset(vars, "colours", T(LV(tget(specific_vars, "colour"))));
                loc(TAB("type", S("other"), "key", S("playing_card"), "set", S("Other"), "nodes", T(nm), "vars", T(vars)));
                tset(full_UI_table, "name", tgeti(nm, 1));
            }
        } else if (streq(card_type, "Booster")) {

        } else {
            tset(full_UI_table, "name", loc(TAB("type", S("name"), "set", tget(_c, "set"), "key", tget(_c, "key"),
                                                "nodes", tget(full_UI_table, "name"))));
        }
        tset(full_UI_table, "card_type", card_type ? S(card_type) : tget(_c, "set"));
    }

    Table *loc_vars = tnew();
    if (main_start) {
        tseti(desc_nodes, tlen(desc_nodes) + 1, T(main_start));
    }

    if (streq(set, "Other")) {
        loc(TAB("type", S("other"), "key", tget(_c, "key"), "nodes", T(desc_nodes),
                "vars", specific_vars ? T(specific_vars) : tget(_c, "vars")));
    } else if (streq(card_type, "Locked")) {
        if (tbool(_c, "wip")) loc(TAB("type", S("other"), "key", S("wip_locked"), "set", S("Other"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
        else if (tbool(_c, "demo") && specific_vars) loc(TAB("type", S("other"), "key", S("demo_shop_locked"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
        else if (tbool(_c, "demo")) loc(TAB("type", S("other"), "key", S("demo_locked"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
        else {
            // G.PROFILES[G.SETTINGS.profile].career_stats
            Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
            Table *cs = ttab(profile, "career_stats");
            Table *uce = ttab(uc, "extra"); // _c.unlock_condition.extra where the Lua indexes it
#define UC_EXTRA tget(uc, "extra")
#define NAME_ENH loc(TAB("type", S("name_text"), "key", tget(uce, "e_key"), "set", S("Enhanced")))
#define SUIT_SING localize(tget(uce, "suit"), "suits_singular")
            if (NM("Golden Ticket")) {
            } else if (NM("Mr. Bones")) loc_vars = LV(UC_EXTRA, tget(cs, "c_losses"));
            else if (NM("Acrobat")) loc_vars = LV(UC_EXTRA, tget(cs, "c_hands_played"));
            else if (NM("Sock and Buskin")) loc_vars = LV(UC_EXTRA, tget(cs, "c_face_cards_played"));
            else if (NM("Swashbuckler")) loc_vars = LV(UC_EXTRA, tget(cs, "c_jokers_sold"));
            else if (NM("Troubadour")) loc_vars = LV(UC_EXTRA);
            else if (NM("Certificate")) {
            } else if (NM("Smeared Joker")) loc_vars = LV(tget(uce, "count"), NAME_ENH);
            else if (NM("Throwback")) {
            } else if (NM("Hanging Chad")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("Rough Gem")) loc_vars = LV(tget(uce, "count"), SUIT_SING);
            else if (NM("Bloodstone")) loc_vars = LV(tget(uce, "count"), SUIT_SING);
            else if (NM("Arrowhead")) loc_vars = LV(tget(uce, "count"), SUIT_SING);
            else if (NM("Onyx Agate")) loc_vars = LV(tget(uce, "count"), SUIT_SING);
            else if (NM("Glass Joker")) loc_vars = LV(tget(uce, "count"), NAME_ENH);
            else if (NM("Showman")) loc_vars = LV(tget(uc, "ante"));
            else if (NM("Flower Pot")) loc_vars = LV(tget(uc, "ante"));
            else if (NM("Blueprint")) {
            } else if (NM("Wee Joker")) loc_vars = LV(tget(uc, "n_rounds"));
            else if (NM("Merry Andy")) loc_vars = LV(tget(uc, "n_rounds"));
            else if (NM("Oops! All 6s")) loc_vars = LV(S(number_format(tget(uc, "chips"))));
            else if (NM("The Idol")) loc_vars = LV(S(number_format(tget(uc, "chips"))));
            else if (NM("Seeing Double")) loc_vars = LV(localize(S("ph_4_7_of_clubs"), NULL));
            else if (NM("Matador")) {
            } else if (NM("Hit the Road")) {
            } else if (NM("The Duo")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("The Trio")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("The Family")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("The Order")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("The Tribe")) loc_vars = LV(localize(UC_EXTRA, "poker_hands"));
            else if (NM("Stuntman")) loc_vars = LV(S(number_format(tget(uc, "chips"))));
            else if (NM("Invisible Joker")) {
            } else if (NM("Brainstorm")) {
            } else if (NM("Satellite")) loc_vars = LV(UC_EXTRA);
            else if (NM("Shoot the Moon")) {
            } else if (NM("Driver's License")) loc_vars = LV(tget(uce, "count"));
            else if (NM("Cartomancer")) loc_vars = LV(tget(uc, "tarot_count"));
            else if (NM("Astronomer")) {
            } else if (NM("Burnt Joker")) loc_vars = LV(UC_EXTRA, tget(cs, "c_cards_sold"));
            else if (NM("Bootstraps")) loc_vars = LV(tget(uce, "count"));
            // Vouchers
            else if (NM("Overstock Plus")) loc_vars = LV(UC_EXTRA, tget(cs, "c_shop_dollars_spent"));
            else if (NM("Liquidation")) loc_vars = LV(UC_EXTRA);
            else if (NM("Tarot Tycoon")) loc_vars = LV(UC_EXTRA, tget(cs, "c_tarots_bought"));
            else if (NM("Planet Tycoon")) loc_vars = LV(UC_EXTRA, tget(cs, "c_planets_bought"));
            else if (NM("Glow Up")) loc_vars = LV(UC_EXTRA);
            else if (NM("Reroll Glut")) loc_vars = LV(UC_EXTRA, tget(cs, "c_shop_rerolls"));
            else if (NM("Omen Globe")) loc_vars = LV(UC_EXTRA, tget(cs, "c_tarot_reading_used"));
            else if (NM("Observatory")) loc_vars = LV(UC_EXTRA, tget(cs, "c_planetarium_used"));
            else if (NM("Nacho Tong")) loc_vars = LV(UC_EXTRA, tget(cs, "c_cards_played"));
            else if (NM("Recyclomancy")) loc_vars = LV(UC_EXTRA, tget(cs, "c_cards_discarded"));
            else if (NM("Money Tree")) loc_vars = LV(UC_EXTRA, tget(cs, "c_round_interest_cap_streak"));
            else if (NM("Antimatter")) {
                Table *v_blank = ttab(ttab(profile, "voucher_usage"), "v_blank");
                loc_vars = LV(UC_EXTRA, v_blank ? vor(tget(v_blank, "count"), N(0)) : N(0));
            } else if (NM("Illusion")) loc_vars = LV(UC_EXTRA, tget(cs, "c_playing_cards_bought"));
            else if (NM("Petroglyph")) loc_vars = LV(UC_EXTRA);
            else if (NM("Retcon")) loc_vars = LV(UC_EXTRA);
            else if (NM("Palette")) loc_vars = LV(UC_EXTRA);
#undef UC_EXTRA
#undef NAME_ENH
#undef SUIT_SING

            if (truthy(tget(_c, "rarity")) && veq(tget(_c, "rarity"), N(4)) && specific_vars && !tbool(specific_vars, "not_hidden")) {
                loc(TAB("type", S("unlocks"), "key", S("joker_locked_legendary"), "set", S("Other"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
            } else {
                loc(TAB("type", S("unlocks"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
            }
        }
    } else if (hide_desc) {
        loc(TAB("type", S("other"), "key", S(fmt("undiscovered_%s", str_lower(set))), "set", tget(_c, "set"), "nodes", T(desc_nodes)));
    } else if (specific_vars && tbool(specific_vars, "debuffed")) {
        loc(TAB("type", S("other"), "key", S(fmt("debuffed_%s", tbool(specific_vars, "playing_card") ? "playing_card" : "default")),
                "nodes", T(desc_nodes)));
    } else if (streq(set, "Joker")) {
        if (NM("Stone Joker") || NM("Marble Joker")) queue(info_queue, center("m_stone"));
        else if (NM("Steel Joker")) queue(info_queue, center("m_steel"));
        else if (NM("Glass Joker")) queue(info_queue, center("m_glass"));
        else if (NM("Golden Ticket")) queue(info_queue, center("m_gold"));
        else if (NM("Lucky Cat")) queue(info_queue, center("m_lucky"));
        else if (NM("Midas Mask")) queue(info_queue, center("m_gold"));
        else if (NM("Invisible Joker")) {
            if (has_negative_joker_text()) main_end = remove_negative_line();
        } else if (NM("Diet Cola")) queue(info_queue, TAB("key", S("tag_double"), "set", S("Tag")));
        else if (NM("Perkeo")) queue(info_queue, TAB("key", S("e_negative_consumable"), "set", S("Edition"), "config", T(TAB("extra", N(1)))));
        if (specific_vars && tbool(specific_vars, "pinned")) queue(info_queue, TAB("key", S("pinned_left"), "set", S("Other")));
        if (specific_vars && tbool(specific_vars, "sticker"))
            queue(info_queue, TAB("key", S(fmt("%s_sticker", str_lower(tstr(specific_vars, "sticker")))), "set", S("Other")));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes),
                "vars", T(specific_vars ? specific_vars : tnew())));
    } else if (streq(set, "Tag")) {
        if (NM("Negative Tag")) queue(info_queue, center("e_negative"));
        else if (NM("Foil Tag")) queue(info_queue, center("e_foil"));
        else if (NM("Holographic Tag")) queue(info_queue, center("e_holo"));
        else if (NM("Polychrome Tag")) queue(info_queue, center("e_polychrome"));
        else if (NM("Charm Tag")) queue(info_queue, center("p_arcana_mega_1"));
        else if (NM("Meteor Tag")) queue(info_queue, center("p_celestial_mega_1"));
        else if (NM("Ethereal Tag")) queue(info_queue, center("p_spectral_normal_1"));
        else if (NM("Standard Tag")) queue(info_queue, center("p_standard_mega_1"));
        else if (NM("Buffoon Tag")) queue(info_queue, center("p_buffoon_mega_1"));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", S("Tag"), "nodes", T(desc_nodes),
                "vars", T(specific_vars ? specific_vars : tnew())));
    } else if (streq(set, "Voucher")) {
        if (NM("Overstock") || NM("Overstock Plus")) {
        } else if (NM("Tarot Merchant") || NM("Tarot Tycoon")) loc_vars = LV(tget(config, "extra_disp"));
        else if (NM("Planet Merchant") || NM("Planet Tycoon")) loc_vars = LV(tget(config, "extra_disp"));
        else if (NM("Hone") || NM("Glow Up")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Reroll Surplus") || NM("Reroll Glut")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Grabber") || NM("Nacho Tong")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Wasteful") || NM("Recyclomancy")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Seed Money") || NM("Money Tree")) loc_vars = LV(N(tnum(config, "extra") / 5));
        else if (NM("Blank") || NM("Antimatter")) {
        } else if (NM("Hieroglyph") || NM("Petroglyph")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Director's Cut") || NM("Retcon")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Paint Brush") || NM("Palette")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Telescope") || NM("Observatory")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Clearance Sale") || NM("Liquidation")) loc_vars = LV(tget(config, "extra"));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    } else if (streq(set, "Edition")) {
        loc_vars = LV(tget(config, "extra"));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    } else if (streq(set, "Default") && specific_vars) {
        if (tbool(specific_vars, "nominal_chips")) {
            loc(TAB("type", S("other"), "key", S("card_chips"), "nodes", T(desc_nodes), "vars", T(LV(tget(specific_vars, "nominal_chips")))));
        }
        if (tbool(specific_vars, "bonus_chips")) {
            loc(TAB("type", S("other"), "key", S("card_extra_chips"), "nodes", T(desc_nodes), "vars", T(LV(tget(specific_vars, "bonus_chips")))));
        }
    } else if (streq(set, "Enhanced")) {
        const char *effect = tstr(_c, "effect");
        // (specific_vars and specific_vars.bonus_chips) or _c.config.bonus
        Val bonus = vor(tget(specific_vars, "bonus_chips"), tget(config, "bonus"));
        if (specific_vars && !NM("Stone Card") && tbool(specific_vars, "nominal_chips")) {
            loc(TAB("type", S("other"), "key", S("card_chips"), "nodes", T(desc_nodes), "vars", T(LV(tget(specific_vars, "nominal_chips")))));
        }
        if (streq(effect, "Mult Card")) loc_vars = LV(tget(config, "mult"));
        else if (streq(effect, "Wild Card")) {
        } else if (streq(effect, "Glass Card")) loc_vars = LV(tget(config, "Xmult"), tpath(G->GAME, "probabilities.normal"), tget(config, "extra"));
        else if (streq(effect, "Steel Card")) loc_vars = LV(tget(config, "h_x_mult"));
        else if (streq(effect, "Stone Card")) loc_vars = LV(bonus);
        else if (streq(effect, "Gold Card")) loc_vars = LV(tget(config, "h_dollars"));
        else if (streq(effect, "Lucky Card")) loc_vars = LV(tpath(G->GAME, "probabilities.normal"), tget(config, "mult"), N(5), tget(config, "p_dollars"), N(15));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
        if (!NM("Stone Card") && truthy(bonus)) {
            loc(TAB("type", S("other"), "key", S("card_extra_chips"), "nodes", T(desc_nodes), "vars", T(LV(bonus))));
        }
    } else if (streq(set, "Booster")) {
        const char *desc_override = "p_arcana_normal";
#define PACK(nm, key) if (NM(nm)) { desc_override = key; loc_vars = LV(tget(config, "choose"), tget(config, "extra")); }
        PACK("Arcana Pack", "p_arcana_normal")
        else PACK("Jumbo Arcana Pack", "p_arcana_jumbo")
        else PACK("Mega Arcana Pack", "p_arcana_mega")
        else PACK("Celestial Pack", "p_celestial_normal")
        else PACK("Jumbo Celestial Pack", "p_celestial_jumbo")
        else PACK("Mega Celestial Pack", "p_celestial_mega")
        else PACK("Spectral Pack", "p_spectral_normal")
        else PACK("Jumbo Spectral Pack", "p_spectral_jumbo")
        else PACK("Mega Spectral Pack", "p_spectral_mega")
        else PACK("Standard Pack", "p_standard_normal")
        else PACK("Jumbo Standard Pack", "p_standard_jumbo")
        else PACK("Mega Standard Pack", "p_standard_mega")
        else PACK("Buffoon Pack", "p_buffoon_normal")
        else PACK("Jumbo Buffoon Pack", "p_buffoon_jumbo")
        else PACK("Mega Buffoon Pack", "p_buffoon_mega")
#undef PACK
        name_override = desc_override;
        if (!truthy(tget(full_UI_table, "name")))
            tset(full_UI_table, "name", loc(TAB("type", S("name"), "set", S("Other"), "key", S(name_override),
                                                "nodes", tget(full_UI_table, "name"))));
        loc(TAB("type", S("other"), "key", S(desc_override), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    } else if (streq(set, "Spectral")) {
        if (NM("Familiar") || NM("Grim") || NM("Incantation")) loc_vars = LV(tget(config, "extra"));
        else if (NM("Immolate")) loc_vars = LV(tpath(config, "extra.destroy"), tpath(config, "extra.dollars"));
        else if (NM("Hex")) queue(info_queue, center("e_polychrome"));
        else if (NM("Talisman")) queue(info_queue, TAB("key", S("gold_seal"), "set", S("Other")));
        else if (NM("Deja Vu")) queue(info_queue, TAB("key", S("red_seal"), "set", S("Other")));
        else if (NM("Trance")) queue(info_queue, TAB("key", S("blue_seal"), "set", S("Other")));
        else if (NM("Medium")) queue(info_queue, TAB("key", S("purple_seal"), "set", S("Other")));
        else if (NM("Ankh")) {
            if (has_negative_joker_text()) {
                queue(info_queue, center("e_negative"));
                main_end = remove_negative_line();
            }
        } else if (NM("Cryptid")) loc_vars = LV(tget(config, "extra"));
        if (NM("Ectoplasm")) {
            queue(info_queue, center("e_negative"));
            loc_vars = LV(vor(tget(G->GAME, "ecto_minus"), N(1)));
        }
        if (NM("Aura")) {
            queue(info_queue, center("e_foil"));
            queue(info_queue, center("e_holo"));
            queue(info_queue, center("e_polychrome"));
        }
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    } else if (streq(set, "Planet")) {
        Table *hand = ttab(ttab(G->GAME, "hands"), tstr(config, "hand_type")); // G.GAME.hands[_c.config.hand_type]
        Val level = tget(hand, "level");
        // G.C.HAND_LEVELS[math.min(7, level)]: nil outside 0..7
        num li = lmin(7, vnum(level));
        Val colour = veq(level, N(1)) ? colv(G->C.UI.TEXT_DARK)
                     : (li >= 0 && li == floor(li)) ? colv(G->C.HAND_LEVELS[(int)li]) : NIL;
        loc_vars = LV(level, localize(tget(config, "hand_type"), "poker_hands"), tget(hand, "l_mult"), tget(hand, "l_chips"));
        tset(loc_vars, "colours", T(LV(colour)));
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    } else if (streq(set, "Tarot")) {
#define MOD_CONV { \
        loc_vars = LV(tget(config, "max_highlighted"), loc(TAB("type", S("name_text"), "set", S("Enhanced"), "key", tget(config, "mod_conv")))); \
        queue(info_queue, center(tstr(config, "mod_conv"))); }
#define SUIT_CONV { \
        loc_vars = LV(tget(config, "max_highlighted"), localize(tget(config, "suit_conv"), "suits_plural")); \
        tset(loc_vars, "colours", T(LV(colv(C_SUITS_get(tstr(config, "suit_conv")))))); }
        if (NM("The Fool")) {
            Table *fool_c = tbool(G->GAME, "last_tarot_planet") ? center(tstr(G->GAME, "last_tarot_planet")) : NULL;
            Val last_tarot_planet = fool_c ? loc(TAB("type", S("name_text"), "key", tget(fool_c, "key"), "set", tget(fool_c, "set")))
                                           : localize(S("k_none"), NULL);
            float *colour = (!fool_c || streq(tstr(fool_c, "name"), "The Fool")) ? G->C.RED : G->C.GREEN;
            main_end = ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("bm"), "padding", N(0.02))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("m"), "colour", P(colour), "r", N(0.05), "padding", N(0.05))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt(" %s ", vstr(last_tarot_planet))), "colour", P(G->C.UI.TEXT_LIGHT), "scale", N(0.3), "shadow", B(true)))))
                    ))))
                ))))
            );
            loc_vars = LV(last_tarot_planet);
            if (!(!fool_c || streq(tstr(fool_c, "name"), "The Fool"))) {
                queue(info_queue, fool_c);
            }
        } else if (NM("The Magician")) MOD_CONV
        else if (NM("The High Priestess")) loc_vars = LV(tget(config, "planets"));
        else if (NM("The Empress")) MOD_CONV
        else if (NM("The Emperor")) loc_vars = LV(tget(config, "tarots"));
        else if (NM("The Hierophant")) MOD_CONV
        else if (NM("The Lovers")) MOD_CONV
        else if (NM("The Chariot")) MOD_CONV
        else if (NM("Justice")) MOD_CONV
        else if (NM("The Hermit")) loc_vars = LV(tget(config, "extra"));
        else if (NM("The Wheel of Fortune")) {
            loc_vars = LV(tpath(G->GAME, "probabilities.normal"), tget(config, "extra"));
            queue(info_queue, center("e_foil"));
            queue(info_queue, center("e_holo"));
            queue(info_queue, center("e_polychrome"));
        } else if (NM("Strength")) loc_vars = LV(tget(config, "max_highlighted"));
        else if (NM("The Hanged Man")) loc_vars = LV(tget(config, "max_highlighted"));
        else if (NM("Death")) loc_vars = LV(tget(config, "max_highlighted"));
        else if (NM("Temperance")) {
            num _money = 0;
            if (G->jokers) {
                for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                    if (streq(tstr(CardArea_card(G->jokers, i)->ability, "set"), "Joker")) {
                        _money = _money + CardArea_card(G->jokers, i)->sell_cost;
                    }
                }
            }
            loc_vars = LV(tget(config, "extra"), N(lmin(tnum(config, "extra"), _money)));
        } else if (NM("The Devil")) MOD_CONV
        else if (NM("The Tower")) MOD_CONV
        else if (NM("The Star")) SUIT_CONV
        else if (NM("The Moon")) SUIT_CONV
        else if (NM("The Sun")) SUIT_CONV
        else if (NM("Judgement")) {
        } else if (NM("The World")) SUIT_CONV
#undef MOD_CONV
#undef SUIT_CONV
        loc(TAB("type", S("descriptions"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", T(desc_nodes), "vars", T(loc_vars)));
    }

    if (main_end) {
        tseti(desc_nodes, tlen(desc_nodes) + 1, T(main_end));
    }

    // Fill all remaining info if this is the main desc
    if (!((specific_vars && !tbool(specific_vars, "sticker")) && (streq(card_type, "Default") || streq(card_type, "Enhanced")))) {
        if (desc_nodes == ttab(full_UI_table, "main") && !truthy(tget(full_UI_table, "name"))) {
            loc(TAB("type", S("name"), "key", tget(_c, "key"), "set", tget(_c, "set"), "nodes", tget(full_UI_table, "name")));
            if (!truthy(tget(full_UI_table, "name"))) tset(full_UI_table, "name", T(tnew()));
        } else if (desc_nodes != ttab(full_UI_table, "main")) {
            tset(desc_nodes, "name", loc(TAB("type", S("name_text"), "key", name_override ? S(name_override) : tget(_c, "key"),
                                             "set", name_override ? S("Other") : tget(_c, "set"))));
        }
    }

    if (first_pass && !streq(set, "Edition") && badges) {
        for (int k = 1; truthy(tgeti(badges, k)); k++) {
            Val v = tgeti(badges, k);
            if (vis(v, "foil")) queue(info_queue, center("e_foil"));
            if (vis(v, "holographic")) queue(info_queue, center("e_holo"));
            if (vis(v, "polychrome")) queue(info_queue, center("e_polychrome"));
            if (vis(v, "negative")) queue(info_queue, center("e_negative"));
            if (vis(v, "negative_consumable")) queue(info_queue, TAB("key", S("e_negative_consumable"), "set", S("Edition"), "config", T(TAB("extra", N(1)))));
            if (vis(v, "gold_seal")) queue(info_queue, TAB("key", S("gold_seal"), "set", S("Other")));
            if (vis(v, "blue_seal")) queue(info_queue, TAB("key", S("blue_seal"), "set", S("Other")));
            if (vis(v, "red_seal")) queue(info_queue, TAB("key", S("red_seal"), "set", S("Other")));
            if (vis(v, "purple_seal")) queue(info_queue, TAB("key", S("purple_seal"), "set", S("Other")));
            if (vis(v, "eternal")) queue(info_queue, TAB("key", S("eternal"), "set", S("Other")));
            if (vis(v, "perishable")) {
                if (!specific_vars) ERROR("common_events.lua:2734: attempt to index local 'specific_vars' (a nil value)");
                queue(info_queue, TAB("key", S("perishable"), "set", S("Other"),
                                      "vars", T(LV(vor(tget(G->GAME, "perishable_rounds"), N(1)),
                                                   vor(tget(specific_vars, "perish_tally"), tget(G->GAME, "perishable_rounds"))))));
            }
            if (vis(v, "rental")) queue(info_queue, TAB("key", S("rental"), "set", S("Other"), "vars", T(LV(vor(tget(G->GAME, "rental_rate"), N(1))))));
            if (vis(v, "pinned_left")) queue(info_queue, TAB("key", S("pinned_left"), "set", S("Other")));
        }
    }

    for (int i = 1; truthy(tgeti(info_queue, i)); i++) {
        generate_card_ui(ttabi(info_queue, i), full_UI_table, NULL, NULL, NULL, false, NULL, NULL);
    }
#undef NM

    return full_UI_table;
}
