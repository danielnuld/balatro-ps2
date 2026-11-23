// functions/common_events.lua:1772-2396: the achievement / unlock notifications, discoveries, the pools and the
// creation of cards, the tutorial's step, the per-round joker targets, the boss choice, a card type's colour.
#include "all.h"

// G.GAME.round_resets.ante as the Lua's `..` writes it
static const char *ante_str(void) { return number_tostring(tnum(ttab(G->GAME, "round_resets"), "ante")); }
// G.GAME.banned_keys[key] (a nil key reads nil)
static bool banned(const char *key) { return key && tbool(ttab(G->GAME, "banned_keys"), key); }
// G.GAME.used_jokers[key] and not next(find_joker("Showman"))
static bool used_no_showman(const char *key) {
    return key && tbool(ttab(G->GAME, "used_jokers"), key) && tempty(find_joker("Showman", false));
}

// ---------------------------------------------------------------- notify_alert
struct notify_ctx { const char *_achievement, *_type; };

// common_events.lua:1778
static bool ev_notify_create(void *ctx) {
    struct notify_ctx *c = ctx;
    if (G->achievement_notification) {
        CALL(G->achievement_notification, remove);
        G->achievement_notification = NULL;
    }
    if (!G->achievement_notification)
        G->achievement_notification = UIBox_new(TAB(
            "definition", T(create_UIBox_notify_alert(c->_achievement, c->_type)),
            "config", T(TAB("align", S("cr"), "offset", T(TAB("x", N(20), "y", N(0))), "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
    return true;
}
// common_events.lua:1796
static bool ev_notify_slide_in(void *ctx) {
    UIElement *c1 = tobji(G->achievement_notification->UIRoot->moveable.node.children, 1);
    UIElement *c11 = tobji(c1->moveable.node.children, 1);
    G->achievement_notification->moveable.alignment.offset.x = G->ROOM->T->x - c11->moveable.node.T->w - 0.8f;
    return true;
}
// common_events.lua:1807
static bool ev_notify_sound(void *ctx) {
    play_sound("highlight1", -1, 0.5f);
    play_sound("foil2", 0.5f, 0.4f);
    return true;
}
// common_events.lua:1819
static bool ev_notify_slide_out(void *ctx) {
    G->achievement_notification->moveable.alignment.offset.x = 20;
    return true;
}
// common_events.lua:1830
static bool ev_notify_remove(void *ctx) {
    if (G->achievement_notification) {
        CALL(G->achievement_notification, remove);
        G->achievement_notification = NULL;
    }
    return true;
}

// common_events.lua:1772
void notify_alert(const char *_achievement, const char *_type) {
    struct notify_ctx *c = NEWCTX(struct notify_ctx);
    c->_achievement = str(_achievement);
    c->_type = str(_type ? _type : "achievement");
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .pause_force = true, .timer = "UPTIME", .func = ev_notify_create, .ctx = c}),
          "achievement", false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .trigger = "after", .pause_force = true, .timer = "UPTIME", .delay = 0.1,
                                             .func = ev_notify_slide_in}),
          "achievement", false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .pause_force = true, .trigger = "after", .timer = "UPTIME", .delay = 0.1,
                                             .func = ev_notify_sound}),
          "achievement", false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .pause_force = true, .trigger = "after", .delay = 3, .timer = "UPTIME",
                                             .func = ev_notify_slide_out}),
          "achievement", false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.no_delete = true, .pause_force = true, .trigger = "after", .delay = 0.5, .timer = "UPTIME",
                                             .func = ev_notify_remove}),
          "achievement", false);
}

// common_events.lua:1840
void inc_steam_stat(const char *stat_name) {
    // Steam only (G.STEAM is nil): not translated
}

// common_events.lua:1849
void unlock_notify(void) {
    const char *path = fmt("%s/unlock_notify.jkr", vstr(tget(G->SETTINGS, "profile")));
    const char *_UN = get_compressed(path);
    if (_UN) {
        // string.gmatch(_UN .. "\n", "(.-)\n"): every line, the last one included (empty ones too)
        for (const char *p = _UN;;) {
            const char *q = strchr(p, '\n');
            if (!q) {
                create_unlock_overlay(str(p));
                break;
            }
            create_unlock_overlay(fmt("%.*s", (int)(q - p), p));
            p = q + 1;
        }
        love_filesystem_remove(path);
    }
}

// common_events.lua:1864
static bool ev_unlock_overlay(void *ctx) {
    const char *key = ctx;
    if (!G->OVERLAY_MENU) {
        tsetb(G->SETTINGS, "paused", true);
        Table *center = ttab(G->P_CENTERS, key);
        FUNCS_overlay_menu(TAB("definition",
                               T(streq(tstr(center, "set"), "Back") ? create_UIBox_deck_unlock(center) : create_UIBox_card_unlock(center))));
        play_sound("foil1", 0.7f, 0.3f);
        play_sound("gong", 1.4f, 0.15f);
        return true;
    }
    return false;
}

// common_events.lua:1859
void create_unlock_overlay(const char *key) {
    if (truthy(tget(G->P_CENTERS, key))) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .no_delete = true, .func = ev_unlock_overlay, .ctx = (void *)str(key)}),
              "unlock", false);
    }
}

// common_events.lua:1890
static bool ev_discover_save(void *ctx) {
    Game_save_progress(G);
    return true;
}

// common_events.lua:1879
void discover_card(Table *card) {
    if (tbool(G->GAME, "seeded") || tbool(G->GAME, "challenge")) return;
    if (!card) card = tnew();
    if (tbool(card, "discovered") || tbool(card, "wip")) return;
    if (card && !tbool(card, "discovered")) {
        tsetb(card, "alert", true);
        Table *new_collection = ttab(ttab(G->GAME, "round_scores"), "new_collection");
        tsetn(new_collection, "amt", tnum(new_collection, "amt") + 1);
    }
    tsetb(card, "discovered", true);
    set_discover_tallies();
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_discover_save}), NULL, false);
}

// common_events.lua:1895
Table *get_deck_from_name(const char *_name) {
    // note: pairs(G.P_CENTERS): the first match in the table's order (deck names are unique)
    for (TIter it = titer(G->P_CENTERS); tnext(&it);) {
        if (it.val.t == V_TAB && streq(tstr(it.val.tab, "name"), _name)) return it.val.tab;
    }
    return NULL;
}

// common_events.lua:1901
const char *get_next_voucher_key(bool _from_tag) {
    const char *_pool_key;
    Table *_pool = get_current_pool("Voucher", NIL, false, NULL, &_pool_key);
    if (_from_tag) _pool_key = "Voucher_fromtag";
    Val center = pseudorandom_element(_pool, N(pseudoseed(_pool_key, NULL)), NULL);
    int it = 1;
    while (streq(vstr(center), "UNAVAILABLE")) {
        it = it + 1;
        center = pseudorandom_element(_pool, N(pseudoseed(fmt("%s_resample%d", _pool_key, it), NULL)), NULL);
    }
    return vstr(center);
}

// common_events.lua:1914
const char *get_next_tag_key(const char *append) {
    if (G->FORCE_TAG) return G->FORCE_TAG; // debug: the host test hooks set it
    const char *_pool_key;
    Table *_pool = get_current_pool("Tag", NIL, false, append, &_pool_key);
    Val _tag = pseudorandom_element(_pool, N(pseudoseed(_pool_key, NULL)), NULL);
    int it = 1;
    while (streq(vstr(_tag), "UNAVAILABLE")) {
        it = it + 1;
        _tag = pseudorandom_element(_pool, N(pseudoseed(fmt("%s_resample%d", _pool_key, it), NULL)), NULL);
    }
    return vstr(_tag);
}

// common_events.lua:1927
Card *create_playing_card(Table *card_init, CardArea *area, bool skip_materialize, bool silent, Table *colours) {
    if (!card_init) card_init = tnew();
    if (!truthy(tget(card_init, "front"))) tset(card_init, "front", pseudorandom_element(G->P_CARDS, N(pseudoseed("front", NULL)), NULL));
    if (!truthy(tget(card_init, "center"))) tset(card_init, "center", tget(G->P_CENTERS, "c_base"));

    G->playing_card = G->playing_card + 1; // (G.playing_card and G.playing_card + 1) or 1, 0 as nil
    CardArea *_area = area ? area : G->hand;
    Card *card = NEW(Card);
    Card_init(card, _area->moveable.node.T->x, _area->moveable.node.T->y, G->CARD_W, G->CARD_H, ttab(card_init, "front"),
              ttab(card_init, "center"), TAB("playing_card", N(G->playing_card)));
    tinsert(G->playing_cards, O(card));
    card->playing_card = G->playing_card;

    if (area) CardArea_emplace(area, card, NULL, false);
    if (!skip_materialize) Card_start_materialize(card, colours, silent, -1);

    return card;
}

// v.weight or 1
static num pack_weight(Table *v) {
    Val w = tget(v, "weight");
    return truthy(w) ? w.n : 1;
}

// common_events.lua:1944
Table *get_pack(const char *_key, const char *_type) {
    if (!tbool(G->GAME, "first_shop_buffoon") && !banned("p_buffoon_normal_1")) {
        tsetb(G->GAME, "first_shop_buffoon", true);
        return ttab(G->P_CENTERS, fmt("p_buffoon_normal_%s", number_tostring(math_random_range(1, 2))));
    }
    num cume = 0, it = 0;
    Table *center = NULL;
    Table *boosters = ttab(G->P_CENTER_POOLS, "Booster");
    for (int k = 1; truthy(tgeti(boosters, k)); k++) {
        Table *v = ttabi(boosters, k);
        if ((!_type || streq(_type, tstr(v, "kind"))) && !banned(tstr(v, "key"))) cume = cume + pack_weight(v);
    }
    num poll = pseudorandom(N(pseudoseed(fmt("%s%s", _key ? _key : "pack_generic", ante_str()), NULL)), NIL, NIL) * cume;
    for (int k = 1; truthy(tgeti(boosters, k)); k++) {
        Table *v = ttabi(boosters, k);
        if (!banned(tstr(v, "key"))) {
            if (!_type || streq(_type, tstr(v, "kind"))) it = it + pack_weight(v);
            if (it >= poll && it - pack_weight(v) <= poll) {
                center = v;
                break;
            }
        }
    }
    return center;
}

// common_events.lua:1963
Table *get_current_pool(const char *_type, Val _rarity, bool _legendary, const char *_append, const char **_pool_key_out) {
    // create the pool
    tsett(G->ARGS, "TEMP_POOL", EMPTY(ttab(G->ARGS, "TEMP_POOL")));
    Table *_pool = ttab(G->ARGS, "TEMP_POOL"), *_starting_pool = NULL;
    const char *_pool_key = "";
    int _pool_size = 0;
    Table *GAME = G->GAME;

    if (streq(_type, "Joker")) {
        num rarity_roll = truthy(_rarity) ? vnum(_rarity) : pseudorandom(S(fmt("rarity%s%s", ante_str(), _append ? _append : "")), NIL, NIL);
        int rarity = _legendary ? 4 : (rarity_roll > 0.95) ? 3 : (rarity_roll > 0.7) ? 2 : 1;
        _starting_pool = ttabi(G->P_JOKER_RARITY_POOLS, rarity);
        _pool_key = fmt("Joker%d%s", rarity, (!_legendary && _append) ? _append : "");
    } else {
        _starting_pool = ttab(G->P_CENTER_POOLS, _type);
        _pool_key = fmt("%s%s", _type, _append ? _append : "");
    }
    if (!_starting_pool) ERROR("get_current_pool: no pool for type %s", _type ? _type : "nil"); // ipairs(nil)

    // cull the pool
    for (int k = 1; truthy(tgeti(_starting_pool, k)); k++) {
        Table *v = ttabi(_starting_pool, k);
        const char *v_key = tstr(v, "key");
        Val unlocked = tget(v, "unlocked");
        bool add = false; // nil, true or false in the Lua: only its truthiness is read
        if (streq(_type, "Enhanced")) {
            add = true;
        } else if (streq(_type, "Demo")) {
            if (truthy(tget(v, "pos")) && truthy(tget(v, "config"))) add = true;
        } else if (streq(_type, "Tag")) {
            const char *requires = tstr(v, "requires");
            Val min_ante = tget(v, "min_ante");
            if ((!requires || (ttab(G->P_CENTERS, requires) && tbool(ttab(G->P_CENTERS, requires), "discovered"))) &&
                (!truthy(min_ante) || min_ante.n <= tnum(ttab(GAME, "round_resets"), "ante"))) {
                add = true;
            }
        } else if (!used_no_showman(v_key) && (!(unlocked.t == V_BOOL && !unlocked.b) || veq(tget(v, "rarity"), N(4)))) {
            const char *v_set = tstr(v, "set");
            if (streq(v_set, "Voucher")) {
                if (!tbool(ttab(GAME, "used_vouchers"), v_key)) {
                    bool include = true;
                    if (truthy(tget(v, "requires"))) {
                        for (TIter kk = titer(ttab(v, "requires")); tnext(&kk);) {
                            if (!truthy(tgetv(ttab(GAME, "used_vouchers"), kk.val))) include = false;
                        }
                    }
                    if (G->shop_vouchers && G->shop_vouchers->cards) {
                        for (int kk = 1; truthy(tgeti(G->shop_vouchers->cards, kk)); kk++) {
                            Card *vv = tobji(G->shop_vouchers->cards, kk);
                            if (streq(tstr(ttab(vv->moveable.node.config, "center"), "key"), v_key)) include = false;
                        }
                    }
                    if (include) add = true;
                }
            } else if (streq(v_set, "Planet")) {
                Table *v_config = ttab(v, "config");
                if (!tbool(v_config, "softlock") || tnum(ttab(ttab(GAME, "hands"), tstr(v_config, "hand_type")), "played") > 0) add = true;
            } else if (truthy(tget(v, "enhancement_gate"))) {
                add = false;
                for (TIter kk = titer(G->playing_cards); tnext(&kk);) {
                    Card *vv = kk.val.obj;
                    if (veq(tget(ttab(vv->moveable.node.config, "center"), "key"), tget(v, "enhancement_gate"))) add = true;
                }
            } else {
                add = true;
            }
            if (streq(tstr(v, "name"), "Black Hole") || streq(tstr(v, "name"), "The Soul")) add = false;
        }

        if (truthy(tget(v, "no_pool_flag")) && truthy(tgetv(ttab(GAME, "pool_flags"), tget(v, "no_pool_flag")))) add = false;
        if (truthy(tget(v, "yes_pool_flag")) && !truthy(tgetv(ttab(GAME, "pool_flags"), tget(v, "yes_pool_flag")))) add = false;

        if (add && !banned(v_key)) {
            tseti(_pool, tlen(_pool) + 1, tget(v, "key"));
            _pool_size = _pool_size + 1;
        } else {
            tseti(_pool, tlen(_pool) + 1, S("UNAVAILABLE"));
        }
    }

    // if pool is empty
    if (_pool_size == 0) {
        _pool = EMPTY(ttab(G->ARGS, "TEMP_POOL"));
        if (streq(_type, "Tarot") || streq(_type, "Tarot_Planet")) tseti(_pool, tlen(_pool) + 1, S("c_strength"));
        else if (streq(_type, "Planet")) tseti(_pool, tlen(_pool) + 1, S("c_pluto"));
        else if (streq(_type, "Spectral")) tseti(_pool, tlen(_pool) + 1, S("c_incantation"));
        else if (streq(_type, "Joker")) tseti(_pool, tlen(_pool) + 1, S("j_joker"));
        else if (streq(_type, "Demo")) tseti(_pool, tlen(_pool) + 1, S("j_joker"));
        else if (streq(_type, "Voucher")) tseti(_pool, tlen(_pool) + 1, S("v_blank"));
        else if (streq(_type, "Tag")) tseti(_pool, tlen(_pool) + 1, S("tag_handy"));
        else tseti(_pool, tlen(_pool) + 1, S("j_joker"));
    }

    *_pool_key_out = fmt("%s%s", _pool_key, !_legendary ? ante_str() : "");
    return _pool;
}

// common_events.lua:2055
Table *poll_edition(const char *_key, num _mod, bool _no_neg, bool _guaranteed) {
    if (_mod < 0) _mod = 1;
    num edition_poll = pseudorandom(N(pseudoseed(_key ? _key : "edition_generic", NULL)), NIL, NIL);
    if (_guaranteed) {
        if (edition_poll > 1 - 0.003 * 25 && !_no_neg) {
            return TAB("negative", B(true));
        } else if (edition_poll > 1 - 0.006 * 25) {
            return TAB("polychrome", B(true));
        } else if (edition_poll > 1 - 0.02 * 25) {
            return TAB("holo", B(true));
        } else if (edition_poll > 1 - 0.04 * 25) {
            return TAB("foil", B(true));
        }
    } else {
        num edition_rate = tnum(G->GAME, "edition_rate");
        if (edition_poll > 1 - 0.003 * _mod && !_no_neg) {
            return TAB("negative", B(true));
        } else if (edition_poll > 1 - 0.006 * edition_rate * _mod) {
            return TAB("polychrome", B(true));
        } else if (edition_poll > 1 - 0.02 * edition_rate * _mod) {
            return TAB("holo", B(true));
        } else if (edition_poll > 1 - 0.04 * edition_rate * _mod) {
            return TAB("foil", B(true));
        }
    }
    return NULL;
}

// common_events.lua:2082
Card *create_card(const char *_type, CardArea *area, bool legendary, Val _rarity, bool skip_materialize, bool soulable,
                  const char *forced_key, const char *key_append) {
    if (!area) area = G->jokers;
    Table *center = ttab(G->P_CENTERS, "b_red");

    // should pool be skipped with a forced key
    if (!forced_key && soulable && !banned("c_soul")) {
        if ((streq(_type, "Tarot") || streq(_type, "Spectral") || streq(_type, "Tarot_Planet")) && !used_no_showman("c_soul")) {
            if (pseudorandom(S(fmt("soul_%s%s", _type, ante_str())), NIL, NIL) > 0.997) {
                forced_key = "c_soul";
            }
        }
        if ((streq(_type, "Planet") || streq(_type, "Spectral")) && !used_no_showman("c_black_hole")) {
            if (pseudorandom(S(fmt("soul_%s%s", _type, ante_str())), NIL, NIL) > 0.997) {
                forced_key = "c_black_hole";
            }
        }
    }

    if (streq(_type, "Base")) {
        forced_key = "c_base";
    }

    if (forced_key && !banned(forced_key)) {
        center = ttab(G->P_CENTERS, forced_key);
        const char *set = tstr(center, "set");
        _type = (!streq(set, "Default") && set) ? set : _type;
    } else {
        const char *_pool_key;
        Table *_pool = get_current_pool(_type, _rarity, legendary, key_append, &_pool_key);
        Val key = pseudorandom_element(_pool, N(pseudoseed(_pool_key, NULL)), NULL);
        int it = 1;
        while (streq(vstr(key), "UNAVAILABLE")) {
            it = it + 1;
            key = pseudorandom_element(_pool, N(pseudoseed(fmt("%s_resample%d", _pool_key, it), NULL)), NULL);
        }

        center = ttab(G->P_CENTERS, vstr(key));
    }

    Table *front = NULL;
    if (streq(_type, "Base") || streq(_type, "Enhanced")) {
        Val f = pseudorandom_element(G->P_CARDS, N(pseudoseed(fmt("front%s%s", key_append ? key_append : "", ante_str()), NULL)), NULL);
        front = f.t == V_TAB ? f.tab : NULL;
    }

    // note: `(G.shop_demo and area==G.shop_demo)` is nil without a G.shop_demo: where it is the last term of the
    // `or` chain (bypass_discovery_ui) the key is then absent, not false
    bool in_shop = area == G->shop_jokers || area == G->pack_cards || area == G->shop_vouchers;
    Table *params = tnew();
    tset(params, "bypass_discovery_center",
         B(in_shop || (G->shop_demo && area == G->shop_demo) || area == G->jokers || area == G->consumeables));
    tset(params, "bypass_discovery_ui", in_shop ? B(true) : G->shop_demo ? B(area == G->shop_demo) : NIL);
    tset(params, "discover", B(area == G->jokers || area == G->consumeables));
    tset(params, "bypass_back", T(((Back *)tobj(G->GAME, "selected_back"))->pos));
    Card *card = NEW(Card);
    Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, front, center,
              params);
    if (truthy(tget(card->ability, "consumeable")) && !skip_materialize) Card_start_materialize(card, NULL, false, -1);

    if (streq(_type, "Joker")) {
        Table *modifiers = ttab(G->GAME, "modifiers");
        if (tbool(modifiers, "all_eternal")) {
            Card_set_eternal(card, true);
        }
        if ((area == G->shop_jokers) || (area == G->pack_cards)) {
            num eternal_perishable_poll = pseudorandom(S(fmt("%s%s", area == G->pack_cards ? "packetper" : "etperpoll", ante_str())), NIL, NIL);
            if (tbool(modifiers, "enable_eternals_in_shop") && eternal_perishable_poll > 0.7) {
                Card_set_eternal(card, true);
            } else if (tbool(modifiers, "enable_perishables_in_shop") && ((eternal_perishable_poll > 0.4) && (eternal_perishable_poll <= 0.7))) {
                Card_set_perishable(card, true);
            }
            if (tbool(modifiers, "enable_rentals_in_shop") &&
                pseudorandom(S(fmt("%s%s", area == G->pack_cards ? "packssjr" : "ssjr", ante_str())), NIL, NIL) > 0.7) {
                Card_set_rental(card, true);
            }
        }

        Table *edition = poll_edition(fmt("edi%s%s", key_append ? key_append : "", ante_str()), -1, false, false);
        Card_set_edition(card, edition, false, false);
        check_for_unlock(TAB("type", S("have_edition")));
    }
    return card;
}

// common_events.lua:2156
Card *copy_card(Card *other, Card *new_card, float card_scale, Val playing_card, bool strip_edition) {
    if (!new_card) {
        float s = card_scale < 0 ? 1 : card_scale;
        new_card = NEW(Card);
        // G.P_CARDS.empty is nil in the Lua too
        Card_init(new_card, other->moveable.node.T->x, other->moveable.node.T->y, G->CARD_W * s, G->CARD_H * s, ttab(G->P_CARDS, "empty"),
                  ttab(G->P_CENTERS, "c_base"), TAB("playing_card", playing_card));
    }
    Card_set_ability(new_card, ttab(other->moveable.node.config, "center"), false, false);
    tset(new_card->ability, "type", tget(other->ability, "type"));
    Card_set_base(new_card, ttab(other->moveable.node.config, "card"), false);
    for (TIter it = titer(other->ability); tnext(&it);) {
        if (it.val.t == V_TAB) {
            tsetv(new_card->ability, it.key, T(copy_table(it.val.tab)));
        } else {
            tsetv(new_card->ability, it.key, it.val);
        }
    }

    if (!strip_edition) {
        Card_set_edition(new_card, other->edition ? other->edition : tnew(), false, true);
    }
    check_for_unlock(TAB("type", S("have_edition")));
    Card_set_seal(new_card, other->seal, true, false);
    if (other->params) {
        new_card->params = other->params; // shared with other, as in the Lua
        tset(new_card->params, "playing_card", playing_card);
    }
    new_card->debuff = other->debuff;
    new_card->pinned = other->pinned;
    return new_card;
}

// ---------------------------------------------------------------- tutorial_info
struct tutorial_ctx {
    Table *args, *pos, *attach, *button;
    const char *align;
    num step;
};

// common_events.lua:2242
static bool ev_tutorial_snap(void *ctx) {
    Table *args = ctx;
    Card_Character *Jimbo = G->OVERLAY_TUTORIAL ? tobj(G->OVERLAY_TUTORIAL->moveable.node.object.dyn, "Jimbo") : NULL;
    if (G->OVERLAY_TUTORIAL && Jimbo && !Jimbo->talking) {
        Val _snap_to = fcall0(tget(args, "snap_to"));
        if (truthy(_snap_to)) {
            G->CONTROLLER->interrupt.focus = false;
            Controller_snap_to(G->CONTROLLER, TAB("node", fcall0(tget(args, "snap_to"))));
        }
        return true;
    }
    return false;
}

// common_events.lua:2211
static bool ev_tutorial_step(void *ctx) {
    struct tutorial_ctx *c = ctx;
    Table *args = c->args;
    Table *OT = G->OVERLAY_TUTORIAL ? G->OVERLAY_TUTORIAL->moveable.node.object.dyn : NULL; // its dyn fields
    if (G->OVERLAY_TUTORIAL && veq(tget(OT, "step"), N(c->step)) && !tbool(OT, "step_complete")) {
        G->CONTROLLER->interrupt.focus = true;
        Card_Character *Jimbo = tobj(OT, "Jimbo");
        if (!Jimbo) {
            Jimbo = NEW(Card_Character);
            Card_Character_init(Jimbo, c->pos);
            tseto(OT, "Jimbo", Jimbo);
        }
        if (tget(args, "highlight").t == V_FN) tset(args, "highlight", fcall0(tget(args, "highlight")));
        Table *highlight = ttab(args, "highlight");
        tseti(highlight, tlen(highlight) + 1, O(Jimbo));
        Card_Character_add_speech_bubble(Jimbo, tget(args, "text_key"), c->align, ttab(args, "loc_vars"));
        Moveable_set_alignment((Moveable *)Jimbo, c->attach);
        if (tbool(args, "hard_set")) Card_Character_hard_set_VT(Jimbo);
        tset(OT, "button_listen", NIL);
        // note: the removed box stays in .content until the next step with a content replaces it, as in the Lua
        if (tobj(OT, "content")) CALL(tobj(OT, "content"), remove);
        if (truthy(tget(args, "content"))) {
            Table *cc = ttab(args, "content_config");
            Val cc_align = tget(cc, "align"), cc_offset = tget(cc, "offset"), cc_major = tget(cc, "major");
            UIBox *content = UIBox_new(TAB(
                "definition", fcall0(tget(args, "content")),
                "config", T(TAB("align", truthy(cc_align) ? cc_align : S("cm"),
                                "offset", truthy(cc_offset) ? cc_offset : T(TAB("x", N(0), "y", N(0))),
                                "major", truthy(cc_major) ? cc_major : O(Jimbo),
                                "bond", S("Weak")))));
            tseto(OT, "content", content);
            tseti(highlight, tlen(highlight) + 1, O(content));
        }
        if (truthy(tget(args, "button_listen"))) tset(OT, "button_listen", tget(args, "button_listen"));
        if (!tbool(args, "no_button"))
            Card_Character_add_button(Jimbo, vstr(tget(c->button, "button")), tstr(c->button, "func"), UI_colour(c->button, "colour"),
                                      tstr(c->button, "update_func"), true, NAN);
        Val text_key = tget(args, "text_key");
        Val lines = truthy(text_key) ? tgetv(ttab(ttab(G->localization, "misc"), "tutorial"), text_key) : NIL;
        Card_Character_say_stuff(Jimbo, 2 * (lines.t == V_TAB ? tlen(lines.tab) : 0) + 1, false);
        if (truthy(tget(args, "snap_to"))) {
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.trigger = "immediate", .not_blocking = true, .not_blockable = true, .func = ev_tutorial_snap, .ctx = args}),
                  "tutorial", false);
        }
        if (truthy(tget(args, "highlight"))) tset(OT, "highlights", tget(args, "highlight"));
        tsetb(OT, "step_complete", true);
    }
    if (!G->OVERLAY_TUTORIAL) return true;
    OT = G->OVERLAY_TUTORIAL->moveable.node.object.dyn;
    return tnum(OT, "step") > c->step || tbool(OT, "skip_steps");
}

// common_events.lua:2183
num tutorial_info(Table *args) {
    // note: {0.32,0.36,0.41,0} is a new table on every call; only the first call's is bound to the box
    float *overlay_colour = malloc(4 * sizeof(float));
    overlay_colour[0] = 0.32f, overlay_colour[1] = 0.36f, overlay_colour[2] = 0.41f, overlay_colour[3] = 0;
    ease_value_f(&overlay_colour[3], 0.6, false, "REAL", 1, 0.4, NULL);
    if (!G->OVERLAY_TUTORIAL) {
        Table *skip_button = UIBox_button(TAB("label", T(ARR(S(fmt("%s >", vstr(localize(S("b_skip"), NULL)))))),
                                              "button", S("skip_tutorial_section"),
                                              "minw", N(1.3), "scale", N(0.45), "colour", P(G->C.JOKER_GREY)));
        Table *row = TAB("n", N(UIT_R),
                         "config", T(TAB("align", S("tr"), "minh", N(G->ROOM->T->h), "minw", N(G->ROOM->T->w))),
                         "nodes", T(ARR(T(skip_button))));
        Table *definition = TAB("n", N(UIT_ROOT),
                                "config", T(TAB("align", S("cm"), "padding", N(32.05), "r", N(0.1), "colour", P(overlay_colour),
                                                "emboss", N(0.05))),
                                "nodes", T(ARR(T(row))));
        G->OVERLAY_TUTORIAL = UIBox_new(TAB(
            "definition", T(definition),
            "config", T(TAB("align", S("cm"), "offset", T(TAB("x", N(0), "y", N(3.2))), "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
    }
    Table *OT = G->OVERLAY_TUTORIAL->moveable.node.object.dyn; // step, step_complete... are dyn fields (game.h)
    if (!truthy(tget(OT, "step"))) tsetn(OT, "step", 1);
    tsetb(OT, "step_complete", false);
    UIElement *row_dollars_chips = UIBox_get_UIE_by_ID(G->HUD, S("row_dollars_chips"), NULL);
    struct tutorial_ctx *c = NEWCTX(struct tutorial_ctx);
    c->args = args;
    c->align = tstr(args, "align") ? tstr(args, "align") : "tm";
    c->step = truthy(tget(args, "step")) ? tnum(args, "step") : 1;
    c->attach = ttab(args, "attach");
    if (!c->attach) c->attach = TAB("major", O(row_dollars_chips), "type", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.5))));
    c->pos = ttab(args, "pos");
    if (!c->pos) {
        Transform *mT = ((Node *)tobj(c->attach, "major"))->T;
        c->pos = TAB("x", N(mT->x + mT->w / 2), "y", N(mT->y + mT->h / 2));
    }
    c->button = ttab(args, "button");
    if (!c->button) c->button = TAB("button", localize(S("b_next"), NULL), "func", S("tut_next"));
    if (!truthy(tget(args, "highlight"))) tsett(args, "highlight", tnew());
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.3, .func = ev_tutorial_step, .ctx = c}), "tutorial", false);
    return c->step + 1;
}

// common_events.lua:2263
void calculate_reroll_cost(bool skip_increment) {
    Table *current_round = ttab(G->GAME, "current_round"), *round_resets = ttab(G->GAME, "round_resets");
    if (tnum(current_round, "free_rerolls") < 0) tsetn(current_round, "free_rerolls", 0);
    if (tnum(current_round, "free_rerolls") > 0) {
        tsetn(current_round, "reroll_cost", 0);
        return;
    }
    if (!truthy(tget(current_round, "reroll_cost_increase"))) tsetn(current_round, "reroll_cost_increase", 0);
    if (!skip_increment) tsetn(current_round, "reroll_cost_increase", tnum(current_round, "reroll_cost_increase") + 1);
    Val temp = tget(round_resets, "temp_reroll_cost");
    tsetn(current_round, "reroll_cost",
          (truthy(temp) ? temp.n : tnum(round_resets, "reroll_cost")) + tnum(current_round, "reroll_cost_increase"));
}

// The cards of G.playing_cards (ipairs) whose ability.effect is not 'Stone Card': the list three functions below build
static Table *non_stone_cards(void) {
    Table *valid = tnew();
    for (int k = 1; truthy(tgeti(G->playing_cards, k)); k++) {
        Card *v = tobji(G->playing_cards, k);
        if (!streq(tstr(v->ability, "effect"), "Stone Card")) {
            tseti(valid, tlen(valid) + 1, O(v));
        }
    }
    return valid;
}

// common_events.lua:2271
void reset_idol_card(void) {
    Table *idol = ttab(ttab(G->GAME, "current_round"), "idol_card");
    tsets(idol, "rank", "Ace");
    tsets(idol, "suit", "Spades");
    Table *valid_idol_cards = non_stone_cards();
    if (truthy(tgeti(valid_idol_cards, 1))) {
        Card *idol_card = pseudorandom_element(valid_idol_cards, N(pseudoseed(fmt("idol%s", ante_str()), NULL)), NULL).obj;
        tset(idol, "rank", tget(idol_card->base, "value"));
        tset(idol, "suit", tget(idol_card->base, "suit"));
        tset(idol, "id", tget(idol_card->base, "id"));
    }
}

// common_events.lua:2288
void reset_mail_rank(void) {
    Table *mail = ttab(ttab(G->GAME, "current_round"), "mail_card");
    tsets(mail, "rank", "Ace");
    Table *valid_mail_cards = non_stone_cards();
    if (truthy(tgeti(valid_mail_cards, 1))) {
        Card *mail_card = pseudorandom_element(valid_mail_cards, N(pseudoseed(fmt("mail%s", ante_str()), NULL)), NULL).obj;
        tset(mail, "rank", tget(mail_card->base, "value"));
        tset(mail, "id", tget(mail_card->base, "id"));
    }
}

// common_events.lua:2303
void reset_ancient_card(void) {
    Table *ancient = ttab(ttab(G->GAME, "current_round"), "ancient_card");
    Table *ancient_suits = tnew();
    static const char *const suits[4] = {"Spades", "Hearts", "Clubs", "Diamonds"};
    for (int k = 0; k < 4; k++) {
        if (!streq(suits[k], tstr(ancient, "suit"))) tseti(ancient_suits, tlen(ancient_suits) + 1, S(suits[k]));
    }
    Val ancient_card = pseudorandom_element(ancient_suits, N(pseudoseed(fmt("anc%s", ante_str()), NULL)), NULL);
    tset(ancient, "suit", ancient_card);
}

// common_events.lua:2312
void reset_castle_card(void) {
    Table *castle = ttab(ttab(G->GAME, "current_round"), "castle_card");
    tsets(castle, "suit", "Spades");
    Table *valid_castle_cards = non_stone_cards();
    if (truthy(tgeti(valid_castle_cards, 1))) {
        Card *castle_card = pseudorandom_element(valid_castle_cards, N(pseudoseed(fmt("cas%s", ante_str()), NULL)), NULL).obj;
        tset(castle, "suit", tget(castle_card->base, "suit"));
    }
}

// common_events.lua:2326
void reset_blinds(void) {
    Table *round_resets = ttab(G->GAME, "round_resets");
    if (!truthy(tget(round_resets, "blind_states")))
        tsett(round_resets, "blind_states", TAB("Small", S("Select"), "Big", S("Upcoming"), "Boss", S("Upcoming")));
    Table *blind_states = ttab(round_resets, "blind_states");
    if (streq(tstr(blind_states, "Boss"), "Defeated")) {
        tsets(blind_states, "Small", "Upcoming");
        tsets(blind_states, "Big", "Upcoming");
        tsets(blind_states, "Boss", "Upcoming");
        tsets(G->GAME, "blind_on_deck", "Small");
        tsets(ttab(round_resets, "blind_choices"), "Boss", get_new_boss());
        tsetb(round_resets, "boss_rerolled", false);
    }
}

// common_events.lua:2338
const char *get_new_boss(void) {
    Table *GAME = G->GAME;
    if (!truthy(tget(GAME, "perscribed_bosses"))) tsett(GAME, "perscribed_bosses", tnew());
    Table *perscribed_bosses = ttab(GAME, "perscribed_bosses"), *bosses_used = ttab(GAME, "bosses_used");
    num ante = tnum(ttab(GAME, "round_resets"), "ante"), win_ante = tnum(GAME, "win_ante");
    if (perscribed_bosses && truthy(tgetv(perscribed_bosses, N(ante)))) {
        const char *ret_boss = vstr(tgetv(perscribed_bosses, N(ante)));
        tsetv(perscribed_bosses, N(ante), NIL);
        tsetn(bosses_used, ret_boss, tnum(bosses_used, ret_boss) + 1);
        return ret_boss;
    }
    if (G->FORCE_BOSS) return G->FORCE_BOSS; // debug: the host test hooks set it

    Table *eligible_bosses = tnew();
    for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
        Table *boss = ttab(it.val.tab, "boss");
        if (!boss) {

        } else if (!tbool(boss, "showdown") && (tnum(boss, "min") <= lmax(1, ante) && (lmod(lmax(1, ante), win_ante) != 0 || ante < 2))) {
            tsetv(eligible_bosses, it.key, B(true));
        } else if (tbool(boss, "showdown") && lmod(ante, win_ante) == 0 && ante >= 2) {
            tsetv(eligible_bosses, it.key, B(true));
        }
    }
    for (TIter it = titer(ttab(GAME, "banned_keys")); tnext(&it);) {
        if (truthy(tgetv(eligible_bosses, it.key))) tsetv(eligible_bosses, it.key, NIL);
    }

    num min_use = 100;
    for (TIter it = titer(bosses_used); tnext(&it);) {
        if (truthy(tgetv(eligible_bosses, it.key))) {
            tsetv(eligible_bosses, it.key, it.val);
            if (vnum(tgetv(eligible_bosses, it.key)) <= min_use) {
                min_use = vnum(tgetv(eligible_bosses, it.key));
            }
        }
    }
    // note: the Lua clears the entries while it walks the table with pairs; here the keys to clear are collected
    // first (each test is independent of the others), so that the table is not changed under its own iterator
    Table *over = tnew();
    for (TIter it = titer(eligible_bosses); tnext(&it);) {
        if (truthy(it.val)) {
            // (an eligible boss missing from bosses_used still holds `true`: the Lua's comparison would fail)
            if (it.val.t != V_NUM) ERROR("get_new_boss: boss %s is not in G.GAME.bosses_used", vstr(it.key));
            if (it.val.n > min_use) {
                tinsert(over, it.key);
            }
        }
    }
    for (int k = 1; k <= tlen(over); k++) tsetv(eligible_bosses, tgeti(over, k), NIL);
    Val boss = NIL;
    pseudorandom_element(eligible_bosses, N(pseudoseed("boss", NULL)), &boss);
    tsetn(bosses_used, vstr(boss), tnum(bosses_used, vstr(boss)) + 1);

    return vstr(boss);
}

// common_events.lua:2385
float *get_type_colour(Table *_c, Card *card) {
    static float fallback[4] = {0, 1, 1, 1};
    Val unlocked = tget(_c, "unlocked"), area = tget(_c, "area");
    bool is_locked = unlocked.t == V_BOOL && !unlocked.b; // _c.unlocked == false
    const char *set = tstr(_c, "set");
    bool area_is_jokers = area.t == V_OBJ && G->jokers && area.obj == (void *)G->jokers;
    bool area_is_consumeables = area.t == V_OBJ && G->consumeables && area.obj == (void *)G->consumeables;
    if (is_locked && !(card && card->bypass_lock)) return G->C.BLACK;
    // (`discoveredand` is the Lua's own typo: always nil)
    if (!is_locked && (streq(set, "Joker") || truthy(tget(_c, "consumeable")) || streq(set, "Voucher")) && !truthy(tget(_c, "discoveredand")) &&
        !((!area_is_jokers && !area_is_consumeables && truthy(area)) || !truthy(area)))
        return G->C.JOKER_GREY;
    if (card && card->debuff) return mix_colours(G->C.RED, G->C.GREY, 0.7f, NULL);
    if (streq(set, "Joker")) {
        Val rarity = tget(_c, "rarity");
        if (rarity.t == V_NUM && (rarity.n == 1 || rarity.n == 2 || rarity.n == 3 || rarity.n == 4)) return G->C.RARITY[(int)rarity.n];
    }
    if (streq(set, "Edition")) return G->C.DARK_EDITION;
    if (streq(set, "Booster")) return G->C.BOOSTER;
    if (C_SECONDARY_SET_get(set)) return C_SECONDARY_SET_get(set);
    return fallback;
}
