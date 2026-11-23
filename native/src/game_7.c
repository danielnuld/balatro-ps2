// game.lua:3072-3629: the per-state update handlers of Game (shop, hand played, draw to hand, new round, blind
// select, round eval, the five booster packs, game over, menu).
#include "all.h"

// G.ROOM.jiggle = G.ROOM.jiggle + 3 (jiggle lives in the room's dyn: game.h)
static void room_jiggle3(void) {
    tsetn(G->ROOM->object.dyn, "jiggle", tnum(G->ROOM->object.dyn, "jiggle") + 3);
}

// for i = 1, #G.GAME.tags do G.GAME.tags[i]:apply_to_run({type = type}) end (the bound is taken once, as the Lua's for)
static void tags_apply(const char *type) {
    int n = tlen(ttab(G->GAME, "tags"));
    for (int i = 1; i <= n; i++) Tag_apply_to_run((Tag *)tobji(ttab(G->GAME, "tags"), i), TAB("type", S(type)));
}

// UIBox{definition = definition, config = {align = align, offset = {x = 0, y = G.ROOM.T.y + yoff}, major = G.hand, bond = 'Weak'}}
// note: the callers evaluate the definition first, as the Lua's constructor does (C argument order is unspecified).
static UIBox *hand_box(Table *definition, const char *align, float yoff) {
    return UIBox_new(TAB(
        "definition", T(definition),
        "config", T(TAB("align", S(align), "offset", T(TAB("x", N(0), "y", N(G->ROOM->T->y + yoff))), "major", O(G->hand), "bond", S("Weak")))));
}

// if self.buttons then self.buttons:remove(); self.buttons = nil end
static void remove_buttons(Game *self) {
    if (self->buttons) { UIBox_remove(self->buttons); self->buttons = NULL; }
}
// if self.shop then self.shop:remove(); self.shop = nil end
static void remove_shop(Game *self) {
    if (self->shop) { UIBox_remove(self->shop); self->shop = NULL; }
}

// game.lua:3171, :3265 func = function() save_run(); return true end
static bool ev_save_run(void *ctx) {
    save_run();
    return true;
}

// ---------------------------------------------------------------- shop
struct shop_ctx { bool shop_exists; };

// game.lua:3089
static bool ev_shop_fill(void *ctx) {
    struct shop_ctx *c = ctx;
    if (fabsf(G->shop->moveable.node.T->y - G->shop->moveable.VT.y) < 3) {
        room_jiggle3();
        play_sound("cardFan2", -1, -1);
        tags_apply("shop_start");
        bool nosave_shop = false; // nil
        if (!c->shop_exists) {
            if (G->load_shop_jokers) {
                nosave_shop = true;
                CardArea_load(G->shop_jokers, G->load_shop_jokers);
                for (int k = 1;; k++) {
                    Card *v = CardArea_card(G->shop_jokers, k);
                    if (!v) break;
                    create_shop_card_ui(v, NULL, NULL);
                    if (tbool(v->ability, "consumeable")) Card_start_materialize(v, NULL, false, -1);
                    for (int _kk = 1;; _kk++) {
                        Tag *vvv = (Tag *)tobji(ttab(G->GAME, "tags"), _kk);
                        if (!vvv) break;
                        if (truthy(Tag_apply_to_run(vvv, TAB("type", S("store_joker_modify"), "card", O(v))))) break;
                    }
                }
                G->load_shop_jokers = NULL;
            } else {
                num n = tnum(ttab(G->GAME, "shop"), "joker_max") - CardArea_n(G->shop_jokers); // bound taken once
                for (num i = 1; i <= n; i++) {
                    CardArea_emplace(G->shop_jokers, create_card_for_shop(G->shop_jokers), NULL, false);
                }
            }

            if (G->load_shop_vouchers) {
                nosave_shop = true;
                CardArea_load(G->shop_vouchers, G->load_shop_vouchers);
                for (int k = 1;; k++) {
                    Card *v = CardArea_card(G->shop_vouchers, k);
                    if (!v) break;
                    create_shop_card_ui(v, NULL, NULL);
                    Card_start_materialize(v, NULL, false, -1);
                }
                G->load_shop_vouchers = NULL;
            } else {
                Val voucher = tget(ttab(G->GAME, "current_round"), "voucher");
                if (truthy(voucher) && ttab(G->P_CENTERS, vstr(voucher))) {
                    Transform *AT = G->shop_vouchers->moveable.node.T;
                    Card *card = NEW(Card);
                    // G.P_CARDS.empty is nil
                    Card_init(card, AT->x + AT->w / 2, AT->y, G->CARD_W, G->CARD_H, ttab(G->P_CARDS, "empty"), ttab(G->P_CENTERS, vstr(voucher)),
                              TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
                    tsetb(card->moveable.node.object.dyn, "shop_voucher", true);
                    create_shop_card_ui(card, "Voucher", G->shop_vouchers);
                    Card_start_materialize(card, NULL, false, -1);
                    CardArea_emplace(G->shop_vouchers, card, NULL, false);
                }
            }

            if (G->load_shop_booster) {
                nosave_shop = true;
                CardArea_load(G->shop_booster, G->load_shop_booster);
                for (int k = 1;; k++) {
                    Card *v = CardArea_card(G->shop_booster, k);
                    if (!v) break;
                    create_shop_card_ui(v, NULL, NULL);
                    Card_start_materialize(v, NULL, false, -1);
                }
                G->load_shop_booster = NULL;
            } else {
                for (int i = 1; i <= 2; i++) {
                    Table *current_round = ttab(G->GAME, "current_round");
                    if (!truthy(tget(current_round, "used_packs"))) tsett(current_round, "used_packs", tnew());
                    Table *used_packs = ttab(current_round, "used_packs");
                    if (!truthy(tgeti(used_packs, i))) {
                        tseti(used_packs, i, tget(get_pack("shop_pack", NULL), "key"));
                    }

                    if (!veq(tgeti(used_packs, i), S("USED"))) {
                        Transform *AT = G->shop_booster->moveable.node.T;
                        Card *card = NEW(Card);
                        Card_init(card, AT->x + AT->w / 2, AT->y, G->CARD_W * 1.27f, G->CARD_H * 1.27f, ttab(G->P_CARDS, "empty"),
                                  ttab(G->P_CENTERS, vstr(tgeti(used_packs, i))),
                                  TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
                        create_shop_card_ui(card, "Booster", G->shop_booster);
                        tsetn(card->ability, "booster_pos", i);
                        Card_start_materialize(card, NULL, false, -1);
                        CardArea_emplace(G->shop_booster, card, NULL, false);
                    }
                }

                tags_apply("voucher_add");
                tags_apply("shop_final_pass");
            }
        }

        Controller_snap_to(G->CONTROLLER, TAB("node", O(UIBox_get_UIE_by_ID(G->shop, S("next_round_button"), NULL))));
        if (!nosave_shop) E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_save_run}), NULL, false);
        return true;
    }
    return false; // no return in the Lua: called again
}

// game.lua:3082
static bool ev_shop_slide(void *ctx) {
    G->shop->moveable.alignment.offset.y = -5.3f;
    G->shop->moveable.alignment.offset.x = 0;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .not_blockable = true, .func = ev_shop_fill, .ctx = ctx}), NULL, false);
    return true;
}

// game.lua:3072 Game:update_shop(dt)
void Game_update_shop(Game *self, float dt) {
    if (!G->STATE_COMPLETE) {
        stop_use();
        ease_background_colour_blind(STATES_SHOP, NULL);
        struct shop_ctx *c = NEWCTX(struct shop_ctx);
        c->shop_exists = G->shop != NULL;
        if (!G->shop) {
            Table *definition = UIDEF_shop();
            G->shop = hand_box(definition, "tmi", 11);
        }
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_shop_slide, .ctx = c}), NULL, false);
        G->STATE_COMPLETE = true;
    }
    remove_buttons(self);
}

// game.lua:3183 Game:update_play_tarot(dt)
void Game_update_play_tarot(Game *self, float dt) {
    remove_buttons(self);
}

// ---------------------------------------------------------------- hand played
// game.lua:3195
static bool ev_hand_played(void *ctx) {
    Blind *blind = (Blind *)tobj(G->GAME, "blind");
    if (tnum(G->GAME, "chips") - blind->chips >= 0 || tnum(ttab(G->GAME, "current_round"), "hands_left") < 1) {
        G->STATE = STATES_NEW_ROUND;
    } else {
        G->STATE = STATES_DRAW_TO_HAND;
    }
    G->STATE_COMPLETE = false;
    return true;
}

// game.lua:3187 Game:update_hand_played(dt)
void Game_update_hand_played(Game *self, float dt) {
    remove_buttons(self);
    remove_shop(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_hand_played}), NULL, false);
    }
}

// ---------------------------------------------------------------- draw to hand
// game.lua:3235
static bool ev_drawn_to_hand(void *ctx) {
    G->STATE = STATES_SELECTING_HAND;
    G->STATE_COMPLETE = false;
    Blind_drawn_to_hand((Blind *)tobj(G->GAME, "blind"));
    return true;
}

// game.lua:3221
static bool ev_draw_to_hand(void *ctx) {
    if (FUNCS_draw_from_deck_to_hand(NIL)) {
        return true;
    }

    Table *current_round = ttab(G->GAME, "current_round");
    if (veq(tget(current_round, "hands_played"), N(0)) && veq(tget(current_round, "discards_used"), N(0)) && tbool(G->GAME, "facing_blind")) {
        int n = CardArea_n(G->jokers);
        for (int i = 1; i <= n; i++) {
            Card_calculate_joker(CardArea_card(G->jokers, i), TAB("first_hand_drawn", B(true)));
        }
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_drawn_to_hand}), NULL, false);
    return true;
}

// game.lua:3208 Game:update_draw_to_hand(dt)
void Game_update_draw_to_hand(Game *self, float dt) {
    remove_buttons(self);
    remove_shop(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        tags_apply("round_start_bonus");
        ease_background_colour_blind(STATES_DRAW_TO_HAND, NULL);

        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_draw_to_hand}), NULL, false);
    }
}

// game.lua:3248 Game:update_new_round(dt)
void Game_update_new_round(Game *self, float dt) {
    remove_buttons(self);
    remove_shop(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        end_round();
    }
}

// ---------------------------------------------------------------- blind select
// game.lua:3271
static bool ev_blind_select_box(void *ctx) {
    play_sound("cancel", -1, -1);
    Table *definition = create_UIBox_blind_select();
    G->blind_select = hand_box(definition, "bmi", 29);
    G->blind_select->moveable.alignment.offset.y =
        0.8f - (G->hand->moveable.node.T->y - G->jokers->moveable.node.T->y) + G->blind_select->moveable.node.T->h;
    room_jiggle3();
    G->blind_select->moveable.alignment.offset.x = 0;
    tsetb(G->CONTROLLER->object.dyn, "lock_input", false);
    tags_apply("immediate");
    int n = tlen(ttab(G->GAME, "tags"));
    for (int i = 1; i <= n; i++) {
        if (truthy(Tag_apply_to_run((Tag *)tobji(ttab(G->GAME, "tags"), i), TAB("type", S("new_blind_choice"))))) break;
    }
    return true;
}

// game.lua:3268
static bool ev_blind_select_queue(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_blind_select_box}), NULL, false);
    return true;
}

// game.lua:3258 Game:update_blind_select(dt)
void Game_update_blind_select(Game *self, float dt) {
    remove_buttons(self);
    remove_shop(self);

    if (!G->STATE_COMPLETE) {
        stop_use();
        ease_background_colour_blind(STATES_BLIND_SELECT, NULL);
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_save_run}), NULL, false);
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_blind_select_queue}), NULL, false);
    }
}

// ---------------------------------------------------------------- round eval
// game.lua:3322
static bool ev_round_eval_poll(void *ctx) {
    // note: float against a float literal, so the compare is exact once assigned (spec game-b.md, trap 3)
    if (G->round_eval->moveable.alignment.offset.y != -7.8f) {
        G->round_eval->moveable.alignment.offset.y = -7.8f;
    } else {
        if (fabsf(G->round_eval->moveable.node.T->y - G->round_eval->moveable.VT.y) < 3) {
            room_jiggle3();
            play_sound("cardFan2", -1, -1);
            delay(0.1, NULL);
            FUNCS_evaluate_round();
            return true;
        }
    }
    return false; // no return in the Lua: called again
}

// game.lua:3311
static bool ev_round_eval(void *ctx) {
    tset(G->GAME, "facing_blind", NIL);
    save_run();
    ease_background_colour_blind(STATES_ROUND_EVAL, NULL);
    Table *definition = create_UIBox_round_evaluation();
    G->round_eval = hand_box(definition, "bm", 19);
    G->round_eval->moveable.alignment.offset.x = 0;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_round_eval_poll}), NULL, false);
    return true;
}

// game.lua:3302 Game:update_round_eval(dt)
void Game_update_round_eval(Game *self, float dt) {
    remove_buttons(self);
    remove_shop(self);

    if (!G->STATE_COMPLETE) {
        stop_use();
        G->STATE_COMPLETE = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_round_eval}), NULL, false);
    }
}

// ---------------------------------------------------------------- booster packs
// note: the Lua repeats these closures in every pack handler with identical bodies; they are shared here.

// game.lua:3379, :3430, :3479, :3515, :3571
static bool ev_pack_recall_focus(void *ctx) {
    Controller_recall_cardarea_focus(G->CONTROLLER, S("pack_cards"));
    return true;
}

// game.lua:3373, :3424 (arcana, spectral)
static bool ev_pack_draw(void *ctx) {
    FUNCS_draw_from_deck_to_hand(NIL);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.5, .func = ev_pack_recall_focus}), NULL, false);
    return true;
}

// game.lua:3475, :3511 (standard, buffoon)
static bool ev_pack_nodraw(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.5, .func = ev_pack_recall_focus}), NULL, false);
    return true;
}

// Every frame, all packs: buttons removed, the shop slid off screen (not removed)
static void pack_every_frame(Game *self) {
    remove_buttons(self);
    if (self->shop) G->shop->moveable.alignment.offset.y = G->ROOM->T->y + 11;
}

// G.booster_pack_sparkles = Particles(1, 1, 0,0, {...}); fade_alpha = 1; :fade(1, 0)
static void pack_sparkles(num scale, num lifespan, num speed, Table *colours) {
    G->booster_pack_sparkles = NEW(Particles);
    Particles_init(G->booster_pack_sparkles, 1, 1, 0, 0, TAB(
        "timer", N(0.015),
        "scale", N(scale),
        "initialize", B(true),
        "lifespan", N(lifespan),
        "speed", N(speed),
        "padding", N(-1),
        "attach", O(G->ROOM_ATTACH),
        "colours", T(colours),
        "fill", B(true)));
    G->booster_pack_sparkles->fade_alpha = 1;
    Particles_fade(G->booster_pack_sparkles, 1, 0);
}

// G.booster_pack = UIBox{definition = definition, config = {align="tmi", offset = {x=0,y=G.ROOM.T.y + 9},major = G.hand, bond = 'Weak'}}
// G.booster_pack.alignment.offset.y = -2.2; G.ROOM.jiggle = G.ROOM.jiggle + 3
static void pack_box(Table *definition) {
    G->booster_pack = hand_box(definition, "tmi", 9);
    G->booster_pack->moveable.alignment.offset.y = -2.2f;
    room_jiggle3();
}

// game.lua:3350
static bool ev_arcana_pack(void *ctx) {
    float *c2 = lighten(G->C.PURPLE, 0.4f, NULL);
    float *c3 = lighten(G->C.PURPLE, 0.2f, NULL);
    float *c4 = lighten(G->C.GOLD, 0.2f, NULL);
    pack_sparkles(0.2, 1, 1.1, ARR(P(G->C.WHITE), P(c2), P(c3), P(c4)));
    pack_box(create_UIBox_arcana_pack());
    ease_background_colour_blind(STATES_TAROT_PACK, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_pack_draw}), NULL, false);
    return true;
}

// game.lua:3341 Game:update_arcana_pack(dt)
void Game_update_arcana_pack(Game *self, float dt) {
    pack_every_frame(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_arcana_pack}), NULL, false);
    }
}

// game.lua:3401
static bool ev_spectral_pack(void *ctx) {
    pack_sparkles(0.1, 3, 0.2, ARR(P(G->C.WHITE), P(lighten(G->C.GOLD, 0.2f, NULL))));
    pack_box(create_UIBox_spectral_pack());
    ease_background_colour_blind(STATES_SPECTRAL_PACK, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_pack_draw}), NULL, false);
    return true;
}

// game.lua:3392 Game:update_spectral_pack(dt)
void Game_update_spectral_pack(Game *self, float dt) {
    pack_every_frame(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_spectral_pack}), NULL, false);
    }
}

// game.lua:3452
static bool ev_standard_pack(void *ctx) {
    pack_sparkles(0.3, 3, 0.2, ARR(P(G->C.BLACK), P(G->C.RED)));
    pack_box(create_UIBox_standard_pack());
    ease_background_colour_blind(STATES_STANDARD_PACK, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_pack_nodraw}), NULL, false);
    return true;
}

// game.lua:3443 Game:update_standard_pack(dt)
void Game_update_standard_pack(Game *self, float dt) {
    pack_every_frame(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_standard_pack}), NULL, false);
    }
}

// game.lua:3501
static bool ev_buffoon_pack(void *ctx) {
    pack_box(create_UIBox_buffoon_pack());
    ease_background_colour_blind(STATES_BUFFOON_PACK, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_pack_nodraw}), NULL, false);
    return true;
}

// game.lua:3492 Game:update_buffoon_pack(dt)
void Game_update_buffoon_pack(Game *self, float dt) {
    pack_every_frame(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_buffoon_pack}), NULL, false);
    }
}

// game.lua:3537
static bool ev_celestial_pack(void *ctx) {
    ease_background_colour_blind(STATES_PLANET_PACK, NULL);
    float *c2 = HEX("a7d6e0");
    float *c3 = HEX("fddca0");
    G->booster_pack_stars = NEW(Particles);
    Particles_init(G->booster_pack_stars, 1, 1, 0, 0, TAB(
        "timer", N(0.07),
        "scale", N(0.1),
        "initialize", B(true),
        "lifespan", N(15),
        "speed", N(0.1),
        "padding", N(-4),
        "attach", O(G->ROOM_ATTACH),
        "colours", T(ARR(P(G->C.WHITE), P(c2), P(c3))),
        "fill", B(true)));
    G->booster_pack_meteors = NEW(Particles);
    Particles_init(G->booster_pack_meteors, 1, 1, 0, 0, TAB(
        "timer", N(2),
        "scale", N(0.05),
        "lifespan", N(1.5),
        "speed", N(4),
        "attach", O(G->ROOM_ATTACH),
        "colours", T(ARR(P(G->C.WHITE))),
        "fill", B(true)));
    pack_box(create_UIBox_celestial_pack());
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_pack_recall_focus}), NULL, false);
    return true;
}

// game.lua:3528 Game:update_celestial_pack(dt)
void Game_update_celestial_pack(Game *self, float dt) {
    pack_every_frame(self);

    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        G->CONTROLLER->interrupt.focus = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_celestial_pack}), NULL, false);
    }
}

// ---------------------------------------------------------------- game over
// game.lua:3609
static bool ev_game_over_jimbo(void *ctx) {
    // note: G.OVERLAY_MENU == true (still being built) would be the Lua's error on indexing a boolean
    if (G->OVERLAY_MENU == OVERLAY_MENU_TRUE) ERROR("game.lua:3610: attempt to index G.OVERLAY_MENU (a boolean value)");
    if (G->OVERLAY_MENU && UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL)) {
        Card_Character *Jimbo = NEW(Card_Character);
        Card_Character_init(Jimbo, TAB("x", N(0), "y", N(5)));
        UIElement *spot = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("jimbo_spot"), NULL);
        CALL(tobj(spot->moveable.node.config, "object"), remove);
        tseto(spot->moveable.node.config, "object", Jimbo);
        tsetb(Jimbo->moveable.node.object.dyn, "ui_object_updated", true);
        Card_Character_add_speech_bubble(Jimbo, S(fmt("lq_%d", (int)math_random_range(1, 10))), NULL, TAB("quip", B(true)));
        Card_Character_say_stuff(Jimbo, 5, false);
    }
    return true;
}

// game.lua:3581 Game:update_game_over(dt)
void Game_update_game_over(Game *self, float dt) {
    if (!G->STATE_COMPLETE) {
        remove_save();

        if (tnum(ttab(G->GAME, "round_resets"), "ante") <= tnum(G->GAME, "win_ante")) {
            if (!tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
                inc_career_stat("c_losses", 1);
                set_deck_loss();
                set_joker_loss();
            }
        }

        play_sound("negative", 0.5f, 0.7f);
        play_sound("whoosh2", 0.9f, 0.7f);

        tsetb(G->SETTINGS, "paused", true);
        Table *definition = create_UIBox_game_over();
        FUNCS_overlay_menu(TAB("definition", T(definition), "config", T(TAB("no_esc", B(true)))));
        room_jiggle3();

        if (tnum(ttab(G->GAME, "round_resets"), "ante") <= tnum(G->GAME, "win_ante")) { // Only add Jimbo to say a quip if the game over happens when the run is lost
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 2.5, .not_blocking = true, .func = ev_game_over_jimbo}), NULL, false);
        }

        G->STATE_COMPLETE = true;
    }
}

// game.lua:3628 Game:update_menu(dt)
void Game_update_menu(Game *self, float dt) {
}
