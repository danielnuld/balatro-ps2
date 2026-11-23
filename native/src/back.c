// back.lua (lines 1-288): Back, the deck the run is played with.
#include "all.h"

// back.lua:2 Back = Object:extend()
Class Back_class = {.name = "Back", .super = &Object_class};

// self.effect.config, read again at every use as the Lua does
#define CFG(self) ttab((self)->effect, "config")

// localize{type = 'name_text', set = <set>, key = <key>} (the Lua's key order: type, set, key or type, key, set does
// not matter to localize)
static Val loc_name_text(const char *set, Val key) {
    return localize(T(TAB("type", S("name_text"), "set", S(set), "key", key)), NULL);
}

// back.lua:5
void Back_init(Back *self, Table *selected_back) {
    if (!selected_back) selected_back = ttab(G->P_CENTERS, "b_red");
    Val name = tget(selected_back, "name");
    self->name = truthy(name) ? vstr(name) : "Red Deck";

    self->effect = TAB("center", T(selected_back), "text_UI", S(""), "config", T(tcopy(ttab(selected_back, "config"))));
    // note: loc_name is a const char *: a name of several lines (a table) would be NULL; no Back has one
    self->loc_name = vstr(loc_name_text("Back", tget(ttab(self->effect, "center"), "key")));

    Table *center = ttab(self->effect, "center");
    Table *pos = tbool(center, "unlocked") ? ttab(center, "pos") : NULL;
    if (!pos) pos = TAB("x", N(4), "y", N(0));
    if (!self->pos) self->pos = tnew();
    tset(self->pos, "x", tget(pos, "x"));
    tset(self->pos, "y", tget(pos, "y"));
}

// back.lua:22
const char *Back_get_name(Back *self) {
    if (tbool(ttab(self->effect, "center"), "unlocked")) return self->loc_name;
    else return localize_s("k_locked", NULL);
}

// back.lua:26
Table *Back_generate_UI(Back *self, Table *other, float ui_scale, float min_dims, const char *challenge) {
    if (min_dims < 0) min_dims = 0.7f;
    if (ui_scale < 0) ui_scale = 0.9f;
    Table *back_config = other ? other : ttab(self->effect, "center");
    // note: `other and other.name or self.name`: a nil other.name falls back to self.name
    const char *name_to_check = (other && tbool(other, "name")) ? tstr(other, "name") : self->name;
    Table *effect_config = (other && tbool(other, "config")) ? ttab(other, "config") : CFG(self);
    // note: get_challenge_int_from_id gives 0 for the Lua's nil; G.CHALLENGES[0] is nil like G.CHALLENGES['']
    Table *challenge_t = ttabi(G->CHALLENGES, get_challenge_int_from_id(challenge ? challenge : ""));
    if (!challenge_t) challenge_t = TAB("name", S("ERROR"));

    Table *loc_args = NULL, *loc_nodes = tnew();

    if (!tbool(back_config, "unlocked")) {
        Table *unlock_condition = ttab(back_config, "unlock_condition");
        if (!tbool(back_config, "unlock_condition")) {
            localize(T(TAB("type", S("descriptions"), "key", S("demo_locked"), "set", S("Other"), "nodes", T(loc_nodes),
                           "vars", T(loc_args))), NULL);
        } else if (streq(tstr(unlock_condition, "type"), "win_deck")) {
            Val other_name = localize(S("k_unknown"), NULL);
            if (tbool(ttab(G->P_CENTERS, tstr(unlock_condition, "deck")), "unlocked")) {
                other_name = loc_name_text("Back", tget(unlock_condition, "deck"));
            }
            loc_args = ARR(other_name);
            localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_win"), "set", S("Other"), "nodes", T(loc_nodes),
                           "vars", T(loc_args))), NULL);
        } else if (streq(tstr(unlock_condition, "type"), "discover_amount")) {
            loc_args = ARR(S(tostring(tget(unlock_condition, "amount"))));
            localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_discover"), "set", S("Other"), "nodes",
                           T(loc_nodes), "vars", T(loc_args))), NULL);
        } else if (streq(tstr(unlock_condition, "type"), "win_stake")) {
            int stake = (int)tnum(unlock_condition, "stake");
            Val other_name = loc_name_text("Stake", tget(ttabi(ttab(G->P_CENTER_POOLS, "Stake"), stake), "key"));
            loc_args = ARR(other_name);
            tsett(loc_args, "colours", ARR(P(get_stake_col(stake))));
            localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_stake"), "set", S("Other"), "nodes",
                           T(loc_nodes), "vars", T(loc_args))), NULL);
        }
    } else {
        if (streq(name_to_check, "Blue Deck")) loc_args = ARR(tget(effect_config, "hands"));
        else if (streq(name_to_check, "Red Deck")) loc_args = ARR(tget(effect_config, "discards"));
        else if (streq(name_to_check, "Yellow Deck")) loc_args = ARR(tget(effect_config, "dollars"));
        else if (streq(name_to_check, "Green Deck"))
            loc_args = ARR(tget(effect_config, "extra_hand_bonus"), tget(effect_config, "extra_discard_bonus"));
        else if (streq(name_to_check, "Black Deck"))
            loc_args = ARR(tget(effect_config, "joker_slot"), N(-tnum(effect_config, "hands")));
        else if (streq(name_to_check, "Magic Deck")) {
            Val a = loc_name_text("Voucher", S("v_crystal_ball"));
            Val b = loc_name_text("Tarot", S("c_fool"));
            loc_args = ARR(a, b);
        } else if (streq(name_to_check, "Nebula Deck")) loc_args = ARR(loc_name_text("Voucher", S("v_telescope")), N(-1));
        else if (streq(name_to_check, "Ghost Deck")) {
        } else if (streq(name_to_check, "Abandoned Deck")) {
        } else if (streq(name_to_check, "Checkered Deck")) {
        } else if (streq(name_to_check, "Zodiac Deck")) {
            Val a = loc_name_text("Voucher", S("v_tarot_merchant"));
            Val b = loc_name_text("Voucher", S("v_planet_merchant"));
            Val c = loc_name_text("Voucher", S("v_overstock_norm"));
            loc_args = ARR(a, b, c);
        } else if (streq(name_to_check, "Painted Deck"))
            loc_args = ARR(tget(effect_config, "hand_size"), tget(effect_config, "joker_slot"));
        else if (streq(name_to_check, "Anaglyph Deck")) loc_args = ARR(loc_name_text("Tag", S("tag_double")));
        else if (streq(name_to_check, "Plasma Deck")) loc_args = ARR(tget(effect_config, "ante_scaling"));
        else if (streq(name_to_check, "Erratic Deck")) {
        }
        localize(T(TAB("type", S("descriptions"), "key", tget(back_config, "key"), "set", S("Back"), "nodes", T(loc_nodes),
                       "vars", T(loc_args))), NULL);
    }

    // back.lua:77 (the config is built before the node, as the Lua's constructor does)
    Table *config = TAB("align", S("cm"), "minw", N(min_dims * 5), "minh", N(min_dims * 2.5f), "id", S(self->name),
                        "colour", P(G->C.CLEAR));
    Table *node;
    if (streq(name_to_check, "Challenge Deck")) {
        // note: id is the challenge TABLE, as the Lua passes it
        Val label = localize(tget(challenge_t, "id"), "challenge_names");
        node = UIBox_button(TAB("button", S("deck_view_challenge"), "label", T(ARR(label)), "minw", N(2.2), "minh", N(1),
                                "scale", N(0.6), "id", T(challenge_t)));
    } else {
        node = desc_from_rows(loc_nodes, true, N(min_dims * 5));
    }
    return TAB("n", N(UIT_ROOT), "config", T(config), "nodes", T(ARR(T(node))));
}

// back.lua:83
void Back_change_to(Back *self, Table *new_back) {
    if (!new_back) new_back = ttab(G->P_CENTERS, "b_red");
    Val name = tget(new_back, "name");
    self->name = truthy(name) ? vstr(name) : "Red Deck";
    self->effect = TAB("center", T(new_back), "text_UI", S(""), "config", T(tcopy(ttab(new_back, "config"))));
    self->loc_name = vstr(loc_name_text("Back", tget(ttab(self->effect, "center"), "key")));
    Table *pos = tbool(ttab(self->effect, "center"), "unlocked") ? tcopy(ttab(new_back, "pos")) : NULL;
    if (!pos) pos = TAB("x", N(4), "y", N(0));
    tset(self->pos, "x", tget(pos, "x"));
    tset(self->pos, "y", tget(pos, "y"));
}

// back.lua:97
Table *Back_save(Back *self) {
    Val key = tget(ttab(self->effect, "center"), "key");
    Table *backTable = TAB("name", S(self->name), "pos", T(self->pos), "effect", T(self->effect), "key",
                           truthy(key) ? key : S("b_red"));

    return backTable;
}

// back.lua:113
static bool ev_anaglyph_tag(void *ctx) {
    Tag *tag = NEW(Tag);
    Tag_init(tag, "tag_double", false, NULL);
    add_tag(tag);
    play_sound("generic1", 0.9 + math_random() * 0.1, 0.8f);
    play_sound("holo1", 1.2 + math_random() * 0.1, 0.4f);
    return true;
}

// back.lua:147
static bool ev_plasma_ease_back(void *ctx) {
    ease_colour(G->C.UI_CHIPS, G->C.BLUE, 2);
    ease_colour(G->C.UI_MULT, G->C.RED, 2);
    return true;
}

// back.lua:159
static bool ev_plasma_reset_colours(void *ctx) {
    for (int i = 0; i < 4; i++) G->C.UI_CHIPS[i] = G->C.BLUE[i];
    for (int i = 0; i < 4; i++) G->C.UI_MULT[i] = G->C.RED[i];
    return true;
}

// back.lua:132
static bool ev_plasma_balance(void *ctx) {
    static const float purple_chips[4] = {0.8f, 0.45f, 0.85f, 1}, purple_mult[4] = {0.8f, 0.45f, 0.85f, 1};
    Val text = localize(S("k_balanced"), NULL);
    play_sound("gong", 0.94f, 0.3f);
    play_sound("gong", 0.94 * 1.5, 0.2f);
    play_sound("tarot1", 1.5f, -1);
    ease_colour(G->C.UI_CHIPS, purple_chips, -1);
    ease_colour(G->C.UI_MULT, purple_mult, -1);
    attention_text(TAB("scale", N(1.4), "text", text, "hold", N(2), "align", S("cm"), "offset",
                       T(TAB("x", N(0), "y", N(-2.7))), "major", O(G->play)));
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .not_blockable = true, .not_blocking = true, .delay = 4.3,
                                             .func = ev_plasma_ease_back}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .not_blockable = true, .not_blocking = true,
                                             .no_delete = true, .delay = 6.3, .func = ev_plasma_reset_colours}), NULL, false);
    return true;
}

// back.lua:108
Val Back_trigger_effect(Back *self, Table *args, Val *mult) {
    if (mult) *mult = NIL;
    if (!args) return NIL;

    if (streq(self->name, "Anaglyph Deck") && streq(tstr(args, "context"), "eval") && tbool(G->GAME, "last_blind") &&
        tbool(ttab(G->GAME, "last_blind"), "boss")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_anaglyph_tag}), NULL, false);
    }
    if (streq(self->name, "Plasma Deck") && streq(tstr(args, "context"), "blind_amount")) {
        return NIL;
    }

    if (streq(self->name, "Plasma Deck") && streq(tstr(args, "context"), "final_scoring_step")) {
        num tot = tnum(args, "chips") + tnum(args, "mult");
        tsetn(args, "chips", floor(tot / 2));
        tsetn(args, "mult", floor(tot / 2));
        update_hand_text(TAB("delay", N(0)), TAB("mult", tget(args, "mult"), "chips", tget(args, "chips")));

        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_plasma_balance}), NULL, false);

        delay(0.6, NULL);
        if (mult) *mult = tget(args, "mult");
        return tget(args, "chips");
    }
    return NIL;
}

// back.lua:187 (captures self)
static bool ev_deck_consumables(void *ctx) {
    Back *self = ctx;
    Table *consumables = ttab(CFG(self), "consumables");
    for (int k = 1; tgeti(consumables, k).t != V_NIL; k++) {
        const char *v = tstri(consumables, k);
        Card *card = create_card("Tarot", G->consumeables, false, NIL, false, false, v, "deck");
        Card_add_to_deck(card, false);
        CardArea_emplace(G->consumeables, card, NULL, false);
    }
    return true;
}

// back.lua:219 (captures self)
static bool ev_deck_edition(void *ctx) {
    Back *self = ctx;
    num i = 0;
    while (i < tnum(CFG(self), "edition_count")) {
        Card *card = pseudorandom_element(G->playing_cards, N(pseudoseed("edition_deck", NULL)), NULL).obj;
        if (!card->edition) {
            i = i + 1;
            Table *edition = tnew();
            tsetv(edition, tget(CFG(self), "edition"), B(true));
            Card_set_edition(card, edition, false, true);
        }
    }
    return true;
}

// back.lua:241
static bool ev_deck_checkered(void *ctx) {
    // note: pairs over an array: 1..n
    for (TIter it = titer(G->playing_cards); tnext(&it);) {
        Card *v = it.val.obj;
        if (streq(tstr(v->base, "suit"), "Clubs")) {
            Card_change_suit(v, "Spades");
        }
        if (streq(tstr(v->base, "suit"), "Diamonds")) {
            Card_change_suit(v, "Hearts");
        }
    }
    return true;
}

// back.lua:174
void Back_apply_to_run(Back *self) {
    Table *starting_params = ttab(G->GAME, "starting_params");

    if (tbool(CFG(self), "voucher")) {
        tsetv(ttab(G->GAME, "used_vouchers"), tget(CFG(self), "voucher"), B(true));
        Val count = tget(G->GAME, "starting_voucher_count");
        tsetn(G->GAME, "starting_voucher_count", (truthy(count) ? vnum(count) : 0) + 1);
        Card_apply_to_run(NULL, tgetv(G->P_CENTERS, tget(CFG(self), "voucher")).tab);
    }
    if (tbool(CFG(self), "hands")) {
        tsetn(starting_params, "hands", tnum(starting_params, "hands") + tnum(CFG(self), "hands"));
    }
    if (tbool(CFG(self), "consumables")) {
        delay(0.4, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_deck_consumables, .ctx = self}), NULL, false);
    }

    if (tbool(CFG(self), "dollars")) {
        tsetn(starting_params, "dollars", tnum(starting_params, "dollars") + tnum(CFG(self), "dollars"));
    }
    if (tbool(CFG(self), "remove_faces")) {
        tsetb(starting_params, "no_faces", true);
    }

    if (tbool(CFG(self), "spectral_rate")) {
        tset(G->GAME, "spectral_rate", tget(CFG(self), "spectral_rate"));
    }
    if (tbool(CFG(self), "discards")) {
        tsetn(starting_params, "discards", tnum(starting_params, "discards") + tnum(CFG(self), "discards"));
    }
    if (tbool(CFG(self), "reroll_discount")) {
        tsetn(starting_params, "reroll_cost", tnum(starting_params, "reroll_cost") - tnum(CFG(self), "reroll_discount"));
    }

    if (tbool(CFG(self), "edition")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_deck_edition, .ctx = self}), NULL, false);
    }
    if (tbool(CFG(self), "vouchers")) {
        // note: pairs over an array: 1..n (it only orders the Card_apply_to_run calls)
        for (TIter it = titer(ttab(CFG(self), "vouchers")); tnext(&it);) {
            Val v = it.val;
            tsetv(ttab(G->GAME, "used_vouchers"), v, B(true));
            Val count = tget(G->GAME, "starting_voucher_count");
            tsetn(G->GAME, "starting_voucher_count", (truthy(count) ? vnum(count) : 0) + 1);
            Card_apply_to_run(NULL, tgetv(G->P_CENTERS, v).tab);
        }
    }
    if (streq(self->name, "Checkered Deck")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_deck_checkered}), NULL, false);
    }
    if (tbool(CFG(self), "randomize_rank_suit")) {
        tsetb(starting_params, "erratic_suits_and_ranks", true);
    }
    if (tbool(CFG(self), "joker_slot")) {
        tsetn(starting_params, "joker_slots", tnum(starting_params, "joker_slots") + tnum(CFG(self), "joker_slot"));
    }
    if (tbool(CFG(self), "hand_size")) {
        tsetn(starting_params, "hand_size", tnum(starting_params, "hand_size") + tnum(CFG(self), "hand_size"));
    }
    if (tbool(CFG(self), "ante_scaling")) {
        tset(starting_params, "ante_scaling", tget(CFG(self), "ante_scaling"));
    }
    if (tbool(CFG(self), "consumable_slot")) {
        tsetn(starting_params, "consumable_slots",
              tnum(starting_params, "consumable_slots") + tnum(CFG(self), "consumable_slot"));
    }
    if (tbool(CFG(self), "no_interest")) {
        tsetb(ttab(G->GAME, "modifiers"), "no_interest", true);
    }
    if (tbool(CFG(self), "extra_hand_bonus")) {
        tset(ttab(G->GAME, "modifiers"), "money_per_hand", tget(CFG(self), "extra_hand_bonus"));
    }
    if (tbool(CFG(self), "extra_discard_bonus")) {
        tset(ttab(G->GAME, "modifiers"), "money_per_discard", tget(CFG(self), "extra_discard_bonus"));
    }
}

// back.lua:280
void Back_load(Back *self, Table *backTable) {
    self->name = tstr(backTable, "name");
    self->pos = ttab(backTable, "pos");
    self->effect = ttab(backTable, "effect");
    Table *center = tgetv(G->P_CENTERS, tget(backTable, "key")).tab;
    if (tgetv(G->P_CENTERS, tget(backTable, "key")).t != V_TAB) center = ttab(G->P_CENTERS, "b_red");
    tsett(self->effect, "center", center);

    self->loc_name = vstr(loc_name_text("Back", tget(ttab(self->effect, "center"), "key")));
}
