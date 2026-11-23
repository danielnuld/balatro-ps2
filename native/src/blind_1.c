// blind.lua:1-623: the Blind class, part 1 (init .. stay_flipped). Part 2 (blind.lua:624-751) has debuff_card,
// move, change_dim, align, save, load.
#include "all.h"

// The sprite's states are the blind's own table in the Lua (blind.lua:18): see blind.h.
#define STATES(self) ((self)->moveable.node.states)
#define CONFIG(self) ((self)->moveable.node.config)
#define CHILDREN(self) ((self)->moveable.node.children)

// what several closures capture: self and the loop variable i (by value)
struct self_i_ctx { Blind *self; int i; };

// ---------------------------------------------------------------- class (blind.lua:2 Blind = Moveable:extend())
static void cls_move(void *self, float dt) { Blind_move(self, dt); }
static void cls_draw(void *self) { Blind_draw(self); }
static void cls_hover(void *self) { Blind_hover(self); }
static void cls_stop_hover(void *self) { Blind_stop_hover(self); }
static void cls_juice_up(void *self, float amount, Val rot_amt) { Blind_juice_up(self, amount, rot_amt); }
// note: the class object is defined here (the Lua's line 2); Blind_move is in part 2.
Class Blind_class = {
    .name = "Blind",
    .super = &Moveable_class,
    .move = cls_move,
    .draw = cls_draw,
    .hover = cls_hover,
    .stop_hover = cls_stop_hover,
    .juice_up = cls_juice_up,
};

// blind.lua:5
void Blind_init(Blind *self, float X, float Y, float W, float H) {
    Moveable_init(&self->moveable, X, Y, W, H);

    CHILDREN(self) = tnew();
    CONFIG(self) = tnew();
    self->tilt_var.mx = 0, self->tilt_var.my = 0, self->tilt_var.amt = 0;
    self->ambient_tilt = 0.3f;
    self->chips = 0;
    self->moveable.zoom = true;
    STATES(self).collide.can = true;
    memcpy(self->colour, G->C.BLACK, sizeof(Colour));
    darken(self->colour, 0.2f, self->dark_colour);
    Transform *T = self->moveable.node.T;
    AnimatedSprite *sprite = NEW(AnimatedSprite);
    AnimatedSprite_init(sprite, T->x, T->y, T->w, T->h, ANIMATION_ATLAS_get("blind_chips"),
                        ttab(ttab(G->P_BLINDS, "bl_small"), "pos"));
    tseto(CHILDREN(self), "animatedSprite", sprite);
    // blind.lua:18-22 the sprite's states are the blind's
    STATES(self).visible = false;
    STATES(self).drag.can = true;
    STATES(self).collide.can = true;
    STATES(self).drag.can = true;
    Blind_sync_states(self);
    self->loc_debuff_lines = ARR(S(""), S(""));

    self->moveable.shadow_height = 0;

    if (self->moveable.node.object.cls == &Blind_class) {
        tinsert(G->I.CARD, O(self));
    }
}

// blind.lua:32
void Blind_change_colour(Blind *self, const float *blind_col) {
    if (!blind_col) {
        const char *key = tstr(ttab(CONFIG(self), "blind"), "key");
        blind_col = get_blind_main_colour(key ? key : "");
    }
    const float *dark_col = mix_colours(blind_col, G->C.BLACK, 0.4f, NULL);
    ease_colour(G->C.DYN_UI.MAIN, blind_col, -1);
    ease_colour(G->C.DYN_UI.DARK, dark_col, -1);

    if (!self->boss && self->name) { // note: '' is truthy in Lua, so "no blind" takes this branch too
        blind_col = darken(G->C.BLACK, 0.05f, NULL);
        dark_col = lighten(G->C.BLACK, 0.07f, NULL);
    } else {
        dark_col = mix_colours(blind_col, G->C.BLACK, 0.2f, NULL);
    }
    ease_colour(G->C.DYN_UI.BOSS_MAIN, blind_col, -1);
    ease_colour(G->C.DYN_UI.BOSS_DARK, dark_col, -1);
}

// blind.lua:51, :62 self.name == '' and self.name or localize{type = 'name_text', key = ..., set = 'Blind'}
static const char *loc_name_of(Blind *self) {
    if (streq(self->name, "")) return self->name;
    return vstr(localize(T(TAB("type", S("name_text"), "key", tget(ttab(CONFIG(self), "blind"), "key"), "set", S("Blind"))), NULL));
}

// blind.lua:48
void Blind_set_text(Blind *self) {
    Table *blind = ttab(CONFIG(self), "blind");
    if (blind) {
        if (self->disabled) {
            self->loc_name = loc_name_of(self);
            self->loc_debuff_text = "";
            tseti(self->loc_debuff_lines, 1, S(""));
            tseti(self->loc_debuff_lines, 2, S(""));
        } else {
            Table *loc_vars = NULL;
            if (streq(self->name, "The Ox")) {
                loc_vars = ARR(localize(tget(ttab(G->GAME, "current_round"), "most_played_poker_hand"), "poker_hands"));
            }
            Val loc_target_v = localize(T(TAB("type", S("raw_descriptions"), "key", tget(blind, "key"), "set", S("Blind"), "vars",
                                              loc_vars ? T(loc_vars) : tget(blind, "vars"))),
                                        NULL);
            if (truthy(loc_target_v)) {
                Table *loc_target = loc_target_v.tab;
                self->loc_name = loc_name_of(self);
                self->loc_debuff_text = "";
                for (int k = 1; tgeti(loc_target, k).t != V_NIL; k++) {
                    self->loc_debuff_text = fmt("%s%s%s", self->loc_debuff_text, vstr(tgeti(loc_target, k)), k <= tlen(loc_target) ? " " : "");
                }
                Val l1 = tgeti(loc_target, 1), l2 = tgeti(loc_target, 2);
                tseti(self->loc_debuff_lines, 1, truthy(l1) ? l1 : S(""));
                tseti(self->loc_debuff_lines, 2, truthy(l2) ? l2 : S(""));
            } else {
                self->loc_name = "";
                self->loc_debuff_text = "";
                tseti(self->loc_debuff_lines, 1, S(""));
                tseti(self->loc_debuff_lines, 2, S(""));
            }
        }
    }
}

struct set_blind_ctx { Blind *self; Table *blind; bool reset, silent; };

// blind.lua:131
static bool ev_set_blind_show(void *ctx) {
    struct set_blind_ctx *c = ctx;
    Blind *self = c->self;
    UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_name"), NULL)->moveable.node.states.visible = true;
    UIBox_get_UIE_by_ID(G->HUD_blind, S("dollars_to_be_earned"), NULL)->moveable.node.parent->parent->states.visible = true;
    DynaText_pop_in(tobj(UIBox_get_UIE_by_ID(G->HUD_blind, S("dollars_to_be_earned"), NULL)->moveable.node.config, "object"), 0);
    DynaText_pop_in(tobj(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_name"), NULL)->moveable.node.config, "object"), 0);
    UIElement_juice_up(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_count"), NULL), -1, NIL);
    CALL(Blind_sprite(self), set_sprite_pos, ttab(ttab(CONFIG(self), "blind"), "pos"));
    self->blind_set = true;
    tsetn(G->ROOM->object.dyn, "jiggle", tnum(G->ROOM->object.dyn, "jiggle") + 3);
    if (!c->reset && !c->silent) {
        Blind_juice_up(self, -1, NIL);
        if (c->blind) {
            play_sound("chips1", (float)(math_random() * 0.1 + 0.55), 0.42f);
            play_sound("gold_seal", (float)(math_random() * 0.1 + 1.85), 0.26f);
        }
    }
    return true;
}

// blind.lua:123
static bool ev_set_blind_hide(void *ctx) {
    UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_name"), NULL)->moveable.node.states.visible = false;
    UIBox_get_UIE_by_ID(G->HUD_blind, S("dollars_to_be_earned"), NULL)->moveable.node.parent->parent->states.visible = false;
    G->HUD_blind->moveable.alignment.offset.y = 0;
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.15, .not_blockable = true, .func = ev_set_blind_show, .ctx = ctx}),
          NULL, false);
    return true;
}

// blind.lua:197, :199, :201 (ctx: the pitch, as a float *)
static bool ev_acorn_shuffle(void *ctx) {
    CardArea_shuffle(G->jokers, "aajk");
    play_sound("cardSlide1", *(float *)ctx, -1);
    return true;
}

// blind.lua:196
static bool ev_acorn(void *ctx) {
    static float per[3] = {0.85f, 1.15f, 1};
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_acorn_shuffle, .ctx = &per[0]}), NULL, false);
    delay(0.15, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_acorn_shuffle, .ctx = &per[1]}), NULL, false);
    delay(0.15, NULL);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_acorn_shuffle, .ctx = &per[2]}), NULL, false);
    delay(0.5, NULL);
    return true;
}

// blind.lua:78
void Blind_set_blind(Blind *self, Table *blind, bool reset, bool silent) {
    if (!reset) {
        tsett(CONFIG(self), "blind", blind ? blind : tnew());
        const char *bname = tstr(blind, "name");
        self->name = bname ? bname : "";
        Val bdollars = tget(blind, "dollars");
        self->dollars = truthy(bdollars) ? vnum(bdollars) : 0;
        self->sound_pings = self->dollars + 2;
        Table *no_blind_reward = ttab(ttab(G->GAME, "modifiers"), "no_blind_reward");
        const char *type = Blind_get_type(self);
        if (no_blind_reward && type && tbool(no_blind_reward, type)) self->dollars = 0;
        Table *bdebuff = ttab(blind, "debuff");
        self->debuff = bdebuff ? bdebuff : tnew();
        self->pos = ttab(blind, "pos");
        Val bmult = tget(blind, "mult");
        self->mult = truthy(bmult) ? vnum(bmult) : 0;
        self->disabled = false;
        self->discards_sub = NIL;
        self->hands_sub = NIL;
        self->boss = blind && tbool(blind, "boss");
        self->blind_set = false;
        self->triggered = false;
        self->prepped = true;
        Blind_set_text(self);

        if (!ttab(G->GAME, "last_blind")) tsett(G->GAME, "last_blind", tnew());
        Table *last_blind = ttab(G->GAME, "last_blind");
        tset(last_blind, "boss", blind ? B(self->boss) : NIL); // note: self.boss is nil with no blind
        tsets(last_blind, "name", self->name);

        if (blind && truthy(tget(blind, "name"))) {
            Blind_change_colour(self, NULL);
        } else {
            Blind_change_colour(self, G->C.BLACK);
        }

        self->chips = get_blind_amount(tnum(ttab(G->GAME, "round_resets"), "ante")) * self->mult * tnum(ttab(G->GAME, "starting_params"), "ante_scaling");
        self->chip_text = number_format(N(self->chips));

        if (!blind) self->chips = 0;

        tsets(ttab(G->GAME, "current_round"), "dollars_to_be_earned",
              self->dollars > 0 ? str_rep(localize_s("$", NULL), (int)self->dollars) : "");
        G->HUD_blind->moveable.alignment.offset.y = -10;
        UIBox_recalculate(G->HUD_blind);

        if (blind && bname && !streq(bname, "")) {
            Blind_alert_debuff(self, true);

            struct set_blind_ctx *c = NEWCTX(struct set_blind_ctx);
            c->self = self, c->blind = blind, c->reset = reset, c->silent = silent;
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.05, .not_blockable = true, .func = ev_set_blind_hide, .ctx = c}),
                  NULL, false);
        }

        tsett(CONFIG(self), "h_popup_config", TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.1))), "parent", O(self)));
    }

    if (streq(self->name, "The Eye") && !reset) {
        self->hands = TAB("Flush Five", B(false), "Flush House", B(false), "Five of a Kind", B(false), "Straight Flush", B(false),
                          "Four of a Kind", B(false), "Full House", B(false), "Flush", B(false), "Straight", B(false),
                          "Three of a Kind", B(false), "Two Pair", B(false), "Pair", B(false), "High Card", B(false));
    }
    if (streq(self->name, "The Mouth") && !reset) {
        self->only_hand = NULL;
    }
    if (streq(self->name, "The Fish") && !reset) {
        self->prepped = false;
    }
    if (streq(self->name, "The Water") && !reset) {
        self->discards_sub = tget(ttab(G->GAME, "current_round"), "discards_left");
        ease_discard(-vnum(self->discards_sub), false, false);
    }
    if (streq(self->name, "The Needle") && !reset) {
        self->hands_sub = N(tnum(ttab(G->GAME, "round_resets"), "hands") - 1);
        ease_hands_played(-vnum(self->hands_sub), false);
    }
    if (streq(self->name, "The Manacle") && !reset) {
        CardArea_change_size(G->hand, -1);
    }
    if (streq(self->name, "Amber Acorn") && !reset && CardArea_n(G->jokers) > 0) {
        CardArea_unhighlight_all(G->jokers);
        for (int k = 1; CardArea_card(G->jokers, k); k++) {
            Card_flip(CardArea_card(G->jokers, k));
        }
        if (CardArea_n(G->jokers) > 1) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_acorn}), NULL, false);
        }
    }

    // add new debuffs
    for (int i = 1; tobji(G->playing_cards, i); i++) {
        Blind_debuff_card(self, tobji(G->playing_cards, i), false);
    }
    for (int i = 1; CardArea_card(G->jokers, i); i++) {
        if (!reset) Blind_debuff_card(self, CardArea_card(G->jokers, i), true);
    }

    Val boss = tget(ttab(CONFIG(self), "blind"), "boss");
    tsetn(ttab(G->ARGS, "spin"), "real",
          (tbool(G->SETTINGS, "reduced_motion") ? 0 : 1) * (truthy(boss) ? (boss.t == V_TAB && tbool(boss.tab, "showdown") ? 0.5 : 0.25) : 0));
}

// blind.lua:236
static bool ev_alert_ping(void *ctx) {
    struct self_i_ctx *c = ctx;
    if (c->i == 1) Blind_juice_up(c->self, -1, NIL);
    play_sound("cancel", (float)(0.7 + 0.05 * c->i), 0.7f);
    return true;
}

// blind.lua:251
static bool ev_alert_unblock(void *ctx) {
    Blind *self = ctx;
    self->block_play = false;
    if (G->buttons) {
        UIElement *_buttons = UIBox_get_UIE_by_ID(G->buttons, S("play_button"), NULL);
        _buttons->disable_button = false;
    }
    return true;
}

// blind.lua:231
static bool ev_alert_show(void *ctx) {
    Blind *self = ctx;
    num GAMESPEED = tnum(G->SETTINGS, "GAMESPEED");
    play_sound("whoosh1", 0.55f, 0.62f);
    for (int i = 1; i <= 4; i++) {
        num wait_time = (0.1 * (i - 1));
        struct self_i_ctx *c = NEWCTX(struct self_i_ctx);
        c->self = self, c->i = i;
        E_add(G->E_MANAGER,
              Event_new((EventCfg){.not_blockable = true, .trigger = "after", .delay = GAMESPEED * wait_time, .func = ev_alert_ping, .ctx = c}), NULL,
              false);
    }
    num hold_time = GAMESPEED * (strlen(self->loc_debuff_text) * 0.035 + 1.3); // #string: bytes
    const char *disp_text = Blind_get_loc_debuff_text(self);
    attention_text(TAB("scale", N(0.7), "text", S(disp_text), "maxw", N(12), "hold", N(hold_time), "align", S("cm"), "offset",
                       T(TAB("x", N(0), "y", N(-1))), "major", O(G->play)));
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = 1, .not_blocking = true, .not_blockable = true, .func = ev_alert_unblock, .ctx = self}),
          NULL, false);
    return true;
}

// blind.lua:224
static bool ev_alert_poll(void *ctx) {
    Blind *self = ctx;
    if (self->disabled) {
        self->block_play = false;
        return true;
    }
    if (G->STATE == STATES_SELECTING_HAND) {
        E_add(G->E_MANAGER,
              Event_new((EventCfg){.trigger = "after", .delay = tnum(G->SETTINGS, "GAMESPEED") * 0.05, .not_blockable = true, .func = ev_alert_show, .ctx = self}),
              NULL, false);
        return true;
    }
    return false;
}

// blind.lua:218
void Blind_alert_debuff(Blind *self, bool first) {
    if (self->loc_debuff_text && !streq(self->loc_debuff_text, "")) {
        self->block_play = true;
        E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .not_blocking = true, .func = ev_alert_poll, .ctx = self}), NULL, false);
    }
}

// blind.lua:270
const char *Blind_get_loc_debuff_text(Blind *self) {
    const char *bname = tstr(ttab(CONFIG(self), "blind"), "name");
    const char *disp_text =
        fmt("%s%s", streq(bname, "The Wheel") ? tostring(tget(ttab(G->GAME, "probabilities"), "normal")) : "", self->loc_debuff_text);
    if (streq(bname, "The Mouth") && self->only_hand) {
        disp_text = fmt("%s [%s]", disp_text, vstr(localize(S(self->only_hand), "poker_hands")));
    }
    return disp_text;
}

// blind.lua:300
static bool ev_defeat_particles(void *ctx) {
    Blind *self = ctx;
    ((Particles *)tobj(CHILDREN(self), "particles"))->max = 0;
    return true;
}

// blind.lua:306 (self.sound_pings is read when it fires)
static bool ev_defeat_ping(void *ctx) {
    struct self_i_ctx *c = ctx;
    num pings = c->self->sound_pings ? c->self->sound_pings : 3;
    play_sound("cancel", (float)(0.8 - 0.05 * c->i), 1.7f);
    if (c->i == lmin(pings + 1, 6)) play_sound("whoosh2", 0.7f, 0.42f);
    return true;
}

// blind.lua:325
static bool ev_defeat_hud(void *ctx) {
    G->HUD_blind->moveable.alignment.offset.y = -10;
    return true;
}

// blind.lua:334
static bool ev_defeat_done(void *ctx) {
    Blind *self = ctx;
    self->dissolve = 0;
    Blind_set_blind(self, NULL, false, true);
    return true;
}

// blind.lua:276
void Blind_defeat(Blind *self, bool silent) {
    num dissolve_time = 1.3;
    num extra_time = 0;
    self->dissolve = 0;
    self->dissolve_colours = ARR(P(G->C.BLACK), P(G->C.RED));
    Blind_juice_up(self, -1, NIL);
    Particles *particles = NEW(Particles);
    Particles_init(particles, 0, 0, 0, 0,
                   TAB("timer_type", S("TOTAL"), "timer", N(0.01 * dissolve_time), "scale", N(0.1), "speed", N(1.5), "lifespan",
                       N(0.7 * dissolve_time), "attach", O(self), "colours", T(self->dissolve_colours), "fill", B(true)));
    tseto(CHILDREN(self), "particles", particles);

    DynaText *blind_name_dynatext = tobj(UIBox_get_UIE_by_ID(G->HUD_blind, S("HUD_blind_name"), NULL)->moveable.node.config, "object");
    DynaText_pop_out(blind_name_dynatext, 2);

    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .not_blockable = true, .delay = 0.5 * dissolve_time, .func = ev_defeat_particles, .ctx = self}),
          NULL, false);
    if (!silent) {
        num pings = self->sound_pings ? self->sound_pings : 3; // self.sound_pings or 3
        for (int i = 1; i <= lmin(pings, 7); i++) {
            extra_time = extra_time + (0.4 + 0.15 * i) * dissolve_time;
            struct self_i_ctx *c = NEWCTX(struct self_i_ctx);
            c->self = self, c->i = i;
            E_add(G->E_MANAGER,
                  Event_new((EventCfg){.not_blockable = true, .trigger = "after", .delay = (0.15 - 0.01 * pings) * i * dissolve_time, .func = ev_defeat_ping, .ctx = c}),
                  NULL, false);
        }
    }
    // note: the blind is never removed during a run's events, so the pointer to its member stays valid
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "ease", .not_blockable = true, .ref_float = &self->dissolve, .ease_to = 1, .delay = 0.7 * dissolve_time}),
          NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .not_blockable = true, .delay = 0.8 * dissolve_time, .func = ev_defeat_hud}), NULL,
          false);
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .not_blockable = true, .delay = 0.95 * dissolve_time, .func = ev_defeat_done, .ctx = self}), NULL,
          false);
    for (int k = 1; CardArea_card(G->jokers, k); k++) {
        Card *v = CardArea_card(G->jokers, k);
        if (streq(v->facing, "back")) Card_flip(v);
    }
    if (streq(self->name, "The Manacle") && !self->disabled) {
        CardArea_change_size(G->hand, 1);
    }
}

// blind.lua:346
const char *Blind_get_type(Blind *self) {
    if (streq(self->name, "Small Blind")) {
        return "Small";
    } else if (streq(self->name, "Big Blind")) {
        return "Big";
    } else if (self->name && !streq(self->name, "")) {
        return "Boss";
    }
    return NULL;
}

// blind.lua:399
static bool ev_disable_check(void *ctx) {
    Blind *self = ctx;
    if (self->boss && tnum(G->GAME, "chips") - ((Blind *)tobj(G->GAME, "blind"))->chips >= 0) {
        G->STATE = STATES_NEW_ROUND;
        G->STATE_COMPLETE = false;
    }
    return true;
}

// blind.lua:356
void Blind_disable(Blind *self) {
    self->disabled = true;
    for (int k = 1; CardArea_card(G->jokers, k); k++) {
        Card *v = CardArea_card(G->jokers, k);
        if (streq(v->facing, "back")) Card_flip(v);
    }
    if (streq(self->name, "The Water")) {
        ease_discard(vnum(self->discards_sub), false, false);
    }
    if (streq(self->name, "The Wheel") || streq(self->name, "The House") || streq(self->name, "The Mark") || streq(self->name, "The Fish")) {
        for (int i = 1; i <= CardArea_n(G->hand); i++) {
            if (streq(CardArea_card(G->hand, i)->facing, "back")) {
                Card_flip(CardArea_card(G->hand, i));
            }
        }
        for (TIter it = titer(G->playing_cards); tnext(&it);) {
            tset(((Card *)it.val.obj)->ability, "wheel_flipped", NIL);
        }
    }
    if (streq(self->name, "The Needle")) {
        ease_hands_played(vnum(self->hands_sub), false);
    }
    if (streq(self->name, "The Wall")) {
        self->chips = self->chips / 2;
        self->chip_text = number_format(N(self->chips));
    }
    if (streq(self->name, "Cerulean Bell")) {
        for (int k = 1; tobji(G->playing_cards, k); k++) {
            tset(((Card *)tobji(G->playing_cards, k))->ability, "forced_selection", NIL);
        }
    }
    if (streq(self->name, "The Manacle")) {
        CardArea_change_size(G->hand, 1);

        FUNCS_draw_from_deck_to_hand(N(1));
    }
    if (streq(self->name, "The Serpent")) {
    }
    if (streq(self->name, "Violet Vessel")) {
        self->chips = self->chips / 3;
        self->chip_text = number_format(N(self->chips));
    }
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_disable_check, .ctx = self}), NULL, false);
    for (int i = 1; tobji(G->playing_cards, i); i++) {
        Blind_debuff_card(self, tobji(G->playing_cards, i), false);
    }
    for (int i = 1; CardArea_card(G->jokers, i); i++) {
        Blind_debuff_card(self, CardArea_card(G->jokers, i), false);
    }
    Blind_set_text(self);
    Blind_wiggle(self);
}

// blind.lua:419
static bool ev_wiggle_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}

// blind.lua:417
void Blind_wiggle(Blind *self) {
    CALL(Blind_sprite(self), juice_up, 0.3f, NIL);
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"), .not_blockable = true, .not_blocking = true, .func = ev_wiggle_sound}),
          NULL, false);
    play_sound("tarot2", 1, 0.4f);
}

// blind.lua:424
void Blind_juice_up(Blind *self, float _a, Val _b) {
    CALL(Blind_sprite(self), juice_up, _a < 0 ? 0.2f : _a, truthy(_b) ? _b : N(0.2));
}

// blind.lua:428
void Blind_hover(Blind *self) {
    if (!G->CONTROLLER->dragging.target || tbool(G->CONTROLLER->object.dyn, "using_touch")) {
        // note: the sprite's states.visible is the blind's (shared table)
        if (!self->hovering && STATES(self).visible && STATES(self).visible) {
            self->hovering = true;
            self->hover_tilt = 2;
            CALL(Blind_sprite(self), juice_up, 0.05f, N(0.02));
            play_sound("chips1", (float)(math_random() * 0.1 + 0.55), 0.12f);
            Node_hover((Node *)self);
        }
    }
}

// blind.lua:440
void Blind_stop_hover(Blind *self) {
    self->hovering = false;
    self->hover_tilt = 0;
    Node_stop_hover((Node *)self);
}

// blind.lua:446
void Blind_draw(Blind *self) {
    if (!STATES(self).visible) return;
    self->tilt_var.mx = G->CONTROLLER->cursor_position.x, self->tilt_var.my = G->CONTROLLER->cursor_position.y;

    AnimatedSprite *sprite = Blind_sprite(self);
    sprite->sprite.moveable.role.draw_major = (Moveable *)self;
    Sprite_draw_shader(&sprite->sprite, "dissolve", 0.1f, NULL, false, NULL, NIL, NIL, NIL, NIL, false, -1);
    Sprite_draw_shader(&sprite->sprite, "dissolve", -1, NULL, false, NULL, NIL, NIL, NIL, NIL, false, -1);

    for (TIter it = titer(CHILDREN(self)); tnext(&it);) {
        if (!veq(it.key, S("animatedSprite"))) {
            Moveable *v = it.val.obj;
            v->VT.scale = self->moveable.VT.scale;
            CALL(v, draw);
        }
    }
    add_to_drawhash(self);
}

// blind.lua:467
static bool ev_hook(void *ctx) {
    bool any_selected = false;
    Table *_cards = tnew();
    for (int k = 1; CardArea_card(G->hand, k); k++) {
        tseti(_cards, tlen(_cards) + 1, O(CardArea_card(G->hand, k)));
    }
    for (int i = 1; i <= 2; i++) {
        if (CardArea_card(G->hand, i)) {
            Val card_key;
            Val selected_card = pseudorandom_element(_cards, N(pseudoseed("hook", NULL)), &card_key);
            CardArea_add_to_highlighted(G->hand, selected_card.obj, true);
            tremove(_cards, (int)card_key.n);
            any_selected = true;
            play_sound("card1", 1, -1);
        }
    }
    if (any_selected) FUNCS_discard_cards_from_highlighted(NULL, true);
    return true;
}

// blind.lua:500 (G.play.cards[i] is read when it fires)
static bool ev_tooth_juice(void *ctx) {
    Card_juice_up(CardArea_card(G->play, (int)(intptr_t)ctx), -1, NIL);
    return true;
}

// blind.lua:498
static bool ev_tooth(void *ctx) {
    for (int i = 1; i <= CardArea_n(G->play); i++) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_tooth_juice, .ctx = (void *)(intptr_t)i}), NULL, false);
        ease_dollars(-1, false);
        delay(0.23, NULL);
    }
    return true;
}

// blind.lua:464
bool Blind_press_play(Blind *self) {
    if (self->disabled) return false;
    if (streq(self->name, "The Hook")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_hook}), NULL, false);
        self->triggered = true;
        delay(0.7, NULL);
        return true;
    }
    if (streq(self->name, "Crimson Heart")) {
        if (CardArea_card(G->jokers, 1)) {
            self->triggered = true;
            self->prepped = true;
        }
    }
    if (streq(self->name, "The Fish")) {
        self->prepped = true;
    }
    if (streq(self->name, "The Tooth")) {
        E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_tooth}), NULL, false);
        self->triggered = true;
        return true;
    }
    return false;
}

// blind.lua:510
num Blind_modify_hand(Blind *self, Table *cards, Table *poker_hands, const char *text, num mult, num hand_chips, num *out_hand_chips,
                      bool *modded) {
    if (self->disabled) {
        *out_hand_chips = hand_chips, *modded = false;
        return mult;
    }
    if (streq(self->name, "The Flint")) {
        self->triggered = true;
        *out_hand_chips = lmax(floor(hand_chips * 0.5 + 0.5), 0), *modded = true;
        return lmax(floor(mult * 0.5 + 0.5), 1);
    }
    *out_hand_chips = hand_chips, *modded = false;
    return mult;
}

// blind.lua:519
bool Blind_debuff_hand(Blind *self, Table *cards, Table *hand, const char *handname, bool check) {
    if (self->disabled) return false;
    if (self->debuff) {
        self->triggered = false;
        const char *dhand = tstr(self->debuff, "hand");
        if (dhand && !tempty(ttab(hand, dhand))) {
            self->triggered = true;
            return true;
        }
        if (truthy(tget(self->debuff, "h_size_ge")) && tlen(cards) < tnum(self->debuff, "h_size_ge")) {
            self->triggered = true;
            return true;
        }
        if (truthy(tget(self->debuff, "h_size_le")) && tlen(cards) > tnum(self->debuff, "h_size_le")) {
            self->triggered = true;
            return true;
        }
        if (streq(self->name, "The Eye")) {
            if (tbool(self->hands, handname)) {
                self->triggered = true;
                return true;
            }
            if (!check) tsetb(self->hands, handname, true);
        }
        if (streq(self->name, "The Mouth")) {
            if (self->only_hand && !streq(self->only_hand, handname)) {
                self->triggered = true;
                return true;
            }
            if (!check) self->only_hand = str(handname);
        }
    }
    if (streq(self->name, "The Arm")) {
        self->triggered = false;
        if (tnum(ttab(ttab(G->GAME, "hands"), handname), "level") > 1) {
            self->triggered = true;
            if (!check) {
                level_up_hand(Blind_sprite(self), handname, false, N(-1));
                Blind_wiggle(self);
            }
        }
    }
    if (streq(self->name, "The Ox")) {
        self->triggered = false;
        if (streq(handname, tstr(ttab(G->GAME, "current_round"), "most_played_poker_hand"))) {
            self->triggered = true;
            if (!check) {
                ease_dollars(-tnum(G->GAME, "dollars"), true);
                Blind_wiggle(self);
            }
        }
    }
    return false;
}

// blind.lua:572
void Blind_drawn_to_hand(Blind *self) {
    if (!self->disabled) {
        if (streq(self->name, "Cerulean Bell")) {
            bool any_forced = false;
            for (int k = 1; CardArea_card(G->hand, k); k++) {
                if (tbool(CardArea_card(G->hand, k)->ability, "forced_selection")) {
                    any_forced = true;
                }
            }
            if (!any_forced) {
                CardArea_unhighlight_all(G->hand);
                Card *forced_card = pseudorandom_element(G->hand->cards, N(pseudoseed("cerulean_bell", NULL)), NULL).obj;
                tsetb(forced_card->ability, "forced_selection", true);
                CardArea_add_to_highlighted(G->hand, forced_card, false);
            }
        }
        if (streq(self->name, "Crimson Heart") && self->prepped && CardArea_card(G->jokers, 1)) {
            Table *jokers = tnew();
            for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                if (!CardArea_card(G->jokers, i)->debuff || CardArea_n(G->jokers) < 2) tseti(jokers, tlen(jokers) + 1, O(CardArea_card(G->jokers, i)));
                Card_set_debuff(CardArea_card(G->jokers, i), false);
            }
            Val _card = pseudorandom_element(jokers, N(pseudoseed("crimson_heart", NULL)), NULL);
            if (truthy(_card)) {
                Card_set_debuff(_card.obj, true);
                Card_juice_up(_card.obj, -1, NIL);
                Blind_wiggle(self);
            }
        }
    }
    self->prepped = false;
}

// blind.lua:605
bool Blind_stay_flipped(Blind *self, CardArea *area, Card *card) {
    if (!self->disabled) {
        if (area == G->hand) {
            if (streq(self->name, "The Wheel") &&
                pseudorandom(N(pseudoseed("wheel", NULL)), NIL, NIL) < tnum(ttab(G->GAME, "probabilities"), "normal") / 7) {
                return true;
            }
            if (streq(self->name, "The House") && tnum(ttab(G->GAME, "current_round"), "hands_played") == 0 &&
                tnum(ttab(G->GAME, "current_round"), "discards_used") == 0) {
                return true;
            }
            if (streq(self->name, "The Mark") && Card_is_face(card, true)) {
                return true;
            }
            if (streq(self->name, "The Fish") && self->prepped) {
                return true;
            }
        }
    }
    return false;
}
