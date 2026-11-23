// card.lua:4064-4658: is_suit, area links, flip, update, hard_set_T, move, the hover popup, juice_up, draw, release,
// highlight, click, save.
#include "all.h"

#define CFG(obj) (((Node *)(obj))->config)
#define CENTER(card) ttab(CFG(card), "center")
#define DYN(obj) (((Object *)(obj))->dyn)

// card.lua:4072 / 4084 next(find_joker('Smeared Joker')) and (red base suit) == (red suit)
static bool smeared_match(Card *self, const char *suit) {
    const char *base_suit = tstr(self->base, "suit");
    return !tempty(find_joker("Smeared Joker", false)) &&
           (streq(base_suit, "Hearts") || streq(base_suit, "Diamonds")) == (streq(suit, "Hearts") || streq(suit, "Diamonds"));
}

// card.lua:4064
bool Card_is_suit(Card *self, const char *suit, bool bypass_debuff, bool flush_calc) {
    if (flush_calc) {
        if (streq(tstr(self->ability, "effect"), "Stone Card")) {
            return false;
        }
        if (streq(tstr(self->ability, "name"), "Wild Card") && !self->debuff) {
            return true;
        }
        if (smeared_match(self, suit)) {
            return true;
        }
        return streq(tstr(self->base, "suit"), suit);
    } else {
        if (self->debuff && !bypass_debuff) return false; // nil
        if (streq(tstr(self->ability, "effect"), "Stone Card")) {
            return false;
        }
        if (streq(tstr(self->ability, "name"), "Wild Card")) {
            return true;
        }
        if (smeared_match(self, suit)) {
            return true;
        }
        return streq(tstr(self->base, "suit"), suit);
    }
}

// card.lua:4091
void Card_set_card_area(Card *self, CardArea *area) {
    self->area = area;
    self->moveable.node.parent = (Node *)area;
    // note: the Lua shares the area's {x, y} table; layered_parallax is a Vec2 by value, so this is a copy
    self->moveable.layered_parallax = area->moveable.layered_parallax;
}

// card.lua:4097
void Card_remove_from_area(Card *self) {
    self->area = NULL;
    self->moveable.node.parent = NULL;
    self->moveable.layered_parallax = (Vec2){0, 0};
}

// card.lua:4103
void Card_align(Card *self) {
    Sprite *floating_sprite = Card_child(self, "floating_sprite");
    if (floating_sprite) {
        floating_sprite->moveable.node.T->y = self->moveable.node.T->y;
        floating_sprite->moveable.node.T->x = self->moveable.node.T->x;
        floating_sprite->moveable.node.T->r = self->moveable.node.T->r;
    }

    // focused_ui:set_alignment() is the closure of UI_definitions.lua:325 (UI_definitions.h)
    if (Card_child(self, "focused_ui")) UIDEF_card_focus_ui_set_alignment(self);
}

// card.lua:4113
void Card_flip(Card *self) {
    if (streq(self->facing, "front")) {
        self->flipping = "f2b";
        self->facing = "back";
        self->moveable.pinch.x = true;
    } else if (streq(self->facing, "back")) {
        tset(self->ability, "wheel_flipped", NIL);
        self->flipping = "b2f";
        self->facing = "front";
        self->moveable.pinch.x = true;
    }
}

// card.h Card_kids / Card_ab / Card_cen: read the tables again
void Card_kids_fill(Card *self) {
    Table *t = self->moveable.node.children;
    struct CardKids *k = &self->kids;
    k->center = tobj(t, "center"), k->front = tobj(t, "front"), k->back = tobj(t, "back");
    k->focused_ui = tobj(t, "focused_ui"), k->particles = tobj(t, "particles"), k->price = tobj(t, "price");
    k->buy_button = tobj(t, "buy_button"), k->buy_and_use_button = tobj(t, "buy_and_use_button");
    k->use_button = tobj(t, "use_button"), k->floating_sprite = tobj(t, "floating_sprite");
    k->h_popup = tobj(t, "h_popup"), k->alert = tobj(t, "alert");
    k->t = t, k->stamp = tstamp(t); // (last: the reads above write nothing)
}
void Card_ab_fill(Card *self) {
    Table *t = self->ability;
    struct CardAb *a = &self->ab;
    a->name = tstr(t, "name"), a->set = tstr(t, "set"), a->effect = tstr(t, "effect");
    a->consumeable = ttab(t, "consumeable"), a->has_consumeable = tbool(t, "consumeable");
    a->eternal = tbool(t, "eternal"), a->rental = tbool(t, "rental"), a->perishable = tbool(t, "perishable");
    a->perma_debuff = tbool(t, "perma_debuff");
    a->t = t, a->stamp = tstamp(t);
}
void Card_cen_fill(Card *self) {
    struct CardCen *c = &self->cen;
    c->config = CFG(self);
    c->center = ttab(c->config, "center");
    c->discovered = tbool(c->center, "discovered"), c->unlocked = tbool(c->center, "unlocked");
    c->demo = tbool(c->center, "demo"), c->alerted = tbool(c->center, "alerted");
    c->soul_pos = truthy(tget(c->center, "soul_pos"));
    c->config_stamp = tstamp(c->config), c->stamp = tstamp(c->center);
}

// card.lua:4126
void Card_update(Card *self, float dt) {
    Table *ability = self->ability;
    const char *name = Card_ab(self)->name;
    if (streq(self->flipping, "f2b")) {
        // (self.sprite_facing == 'front' or true)
        if (self->moveable.VT.w <= 0) {
            self->sprite_facing = "back";
            self->moveable.pinch.x = false;
        }
    }
    if (streq(self->flipping, "b2f")) {
        // (self.sprite_facing == 'back' or true)
        if (self->moveable.VT.w <= 0) {
            self->sprite_facing = "front";
            self->moveable.pinch.x = false;
        }
    }

    if (!self->moveable.node.states.focus.is && Card_kids(self)->focused_ui) {
        CALL(Card_kids(self)->focused_ui, remove);
        tset(self->moveable.node.children, "focused_ui", NIL);
    }

    Card_update_alert(self);
    if (streq(Card_ab(self)->set, "Joker") && !self->sticker_run) {
        Val sticker = get_joker_win_sticker(Card_cen(self)->center, false);
        self->sticker_run = truthy(sticker) ? vstr(sticker) : "NONE";
    }

    Table *consumeable = Card_ab(self)->consumeable;
    if (consumeable && truthy(tget(consumeable, "max_highlighted"))) {
        tsetn(consumeable, "mod_num", lmin(5, tnum(consumeable, "max_highlighted")));
    }
    if (G->STAGE == STAGES_RUN) {
        if (ability && Card_ab(self)->perma_debuff) self->debuff = true;

        if (self->area && ((self->area == G->jokers) || (self->area == G->consumeables))) {
            self->bypass_lock = true;
            self->bypass_discovery_center = true;
            self->bypass_discovery_ui = true;
        }
        static const char *question; // S("?") interned once (a static: the collector keeps it)
        if (!question) question = str("?");
        self->sell_cost_label = streq(self->facing, "back") ? (Val){.t = V_STR, .s = question} : N(self->sell_cost);

        if (streq(name, "Temperance")) {
            tsetn(ability, "money", 0);
            for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                if (streq(tstr(CardArea_card(G->jokers, i)->ability, "set"), "Joker")) {
                    tsetn(ability, "money", tnum(ability, "money") + CardArea_card(G->jokers, i)->sell_cost);
                }
            }
            tsetn(ability, "money", lmin(tnum(ability, "money"), tnum(ability, "extra")));
        }
        if (streq(name, "Throwback")) {
            tsetn(ability, "x_mult", 1 + tnum(G->GAME, "skips") * tnum(ability, "extra"));
        }
        if (streq(name, "Driver's License")) {
            tsetn(ability, "driver_tally", 0);
            for (TIter it = titer(G->playing_cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (CENTER(v) != ttab(G->P_CENTERS, "c_base")) tsetn(ability, "driver_tally", tnum(ability, "driver_tally") + 1);
            }
        }
        if (streq(name, "Steel Joker")) {
            tsetn(ability, "steel_tally", 0);
            for (TIter it = titer(G->playing_cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (CENTER(v) == ttab(G->P_CENTERS, "m_steel")) tsetn(ability, "steel_tally", tnum(ability, "steel_tally") + 1);
            }
        }
        if (streq(name, "Cloud 9")) {
            tsetn(ability, "nine_tally", 0);
            for (TIter it = titer(G->playing_cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (Card_get_id(v) == 9) tsetn(ability, "nine_tally", tnum(ability, "nine_tally") + 1);
            }
        }
        if (streq(name, "Stone Joker")) {
            tsetn(ability, "stone_tally", 0);
            for (TIter it = titer(G->playing_cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (CENTER(v) == ttab(G->P_CENTERS, "m_stone")) tsetn(ability, "stone_tally", tnum(ability, "stone_tally") + 1);
            }
        }
        if (streq(name, "Joker Stencil")) {
            tsetn(ability, "x_mult", tnum(CFG(G->jokers), "card_limit") - CardArea_n(G->jokers));
            for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                if (streq(tstr(CardArea_card(G->jokers, i)->ability, "name"), "Joker Stencil")) tsetn(ability, "x_mult", tnum(ability, "x_mult") + 1);
            }
        }
        if (streq(name, "The Wheel of Fortune")) {
            self->eligible_strength_jokers = EMPTY(self->eligible_strength_jokers);
            for (TIter it = titer(G->jokers->cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (streq(tstr(v->ability, "set"), "Joker") && (!v->edition)) {
                    tinsert(self->eligible_strength_jokers, O(v));
                }
            }
        }
        if (streq(name, "Ectoplasm") || streq(name, "Hex")) {
            self->eligible_editionless_jokers = EMPTY(self->eligible_editionless_jokers);
            for (TIter it = titer(G->jokers->cards); tnext(&it);) {
                Card *v = it.val.obj;
                if (streq(tstr(v->ability, "set"), "Joker") && (!v->edition)) {
                    tinsert(self->eligible_editionless_jokers, O(v));
                }
            }
        }
        if (streq(name, "Blueprint") || streq(name, "Brainstorm")) {
            Card *other_joker = NULL;
            if (streq(name, "Brainstorm")) {
                other_joker = CardArea_card(G->jokers, 1);
            } else if (streq(name, "Blueprint")) {
                for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                    if (CardArea_card(G->jokers, i) == self) other_joker = CardArea_card(G->jokers, i + 1);
                }
            }
            if (other_joker && other_joker != self && tbool(CENTER(other_joker), "blueprint_compat")) {
                tsets(ability, "blueprint_compat", "compatible");
            } else {
                tsets(ability, "blueprint_compat", "incompatible");
            }
        }
        if (streq(name, "Swashbuckler")) {
            num sell_cost = 0;
            for (int i = 1; i <= CardArea_n(G->jokers); i++) {
                Card *c = CardArea_card(G->jokers, i);
                if (c != self && (c->area && c->area == G->jokers)) {
                    sell_cost = sell_cost + c->sell_cost;
                }
            }
            tsetn(ability, "mult", sell_cost);
        }
    } else {
        if (streq(name, "Temperance")) {
            tsetn(ability, "money", 0);
        }
    }
}

// card.lua:4256 NAN as nil
void Card_hard_set_T(Card *self, float X, float Y, float W, float H) {
    float x = isnan(X) ? self->moveable.node.T->x : X;
    float y = isnan(Y) ? self->moveable.node.T->y : Y;
    float w = isnan(W) ? self->moveable.node.T->w : W;
    float h = isnan(H) ? self->moveable.node.T->h : H;
    Moveable_hard_set_T((Moveable *)self, x, y, w, h);
    if (Card_kids(self)->front) CALL(Card_kids(self)->front, hard_set_T, x, y, w, h);
    CALL(Card_kids(self)->back, hard_set_T, x, y, w, h);
    CALL(Card_kids(self)->center, hard_set_T, x, y, w, h);
}

// card.lua:4267
void Card_move(Card *self, float dt) {
    Moveable_move((Moveable *)self, dt);
    //self:align()
    if (Card_kids(self)->h_popup) {
        Moveable_set_alignment(Card_kids(self)->h_popup, Card_align_h_popup(self));
    }
}

// card.lua:4275
Table *Card_align_h_popup(Card *self) {
    void *focused_ui_child = Card_kids(self)->focused_ui;
    bool focused_ui = focused_ui_child ? true : false;
    const char *popup_direction = (Card_kids(self)->buy_button || (self->area && tbool(CFG(self->area), "view_deck")) ||
                                   (self->area && streq(CardArea_type(self->area), "shop"))) ? "cl" :
                                  (self->moveable.node.T->y < G->CARD_H * 0.8f) ? "bm" :
                                  "tm";
    // note: the Lua's and/or chains; a number (0.0 included) is truthy and ends its chain
    num x = !streq(popup_direction, "cl") ? 0 :
            focused_ui ? -0.05 :
            ttab(self->ability, "consumeable") ? 0.0 :
            streq(tstr(self->ability, "set"), "Voucher") ? 0.0 :
            -0.05;
    num y = focused_ui ? (
                streq(popup_direction, "tm") ? (self->area && self->area == G->hand ? -0.08 : -0.15) :
                streq(popup_direction, "bm") ? 0.12 :
                0
            ) :
            streq(popup_direction, "tm") ? -0.13 :
            streq(popup_direction, "bm") ? 0.1 :
            0;
    return TAB(
        "major", O(focused_ui_child ? focused_ui_child : (void *)self),
        "parent", O(self),
        "xy_bond", S("Strong"),
        "r_bond", S("Weak"),
        "wh_bond", S("Weak"),
        "offset", T(TAB("x", N(x), "y", N(y))),
        "type", S(popup_direction)
        //lr_clamp = true
    );
}

// card.lua:4306
void Card_hover(Card *self) {
    Card_juice_up(self, 0.05f, N(0.03));
    play_sound("paper1", math_random() * 0.2 + 0.9, 0.35f);

    //if this is the focused card
    if (self->moveable.node.states.focus.is && !Card_kids(self)->focused_ui) {
        tseto(self->moveable.node.children, "focused_ui", UIDEF_card_focus_ui(self));
    }

    if (streq(self->facing, "front") && (!self->moveable.node.states.drag.is || G->CONTROLLER->HID.touch) && !self->no_ui && !G->debug_tooltip_toggle) {
        if (Card_kids(self)->alert && !tbool(CENTER(self), "alerted")) {
            tsetb(CENTER(self), "alerted", true);
            Game_save_progress(G);
        }

        tset(DYN(self), "ability_UIBox_table", T(Card_generate_UIBox_ability_table(self)));
        tset(CFG(self), "h_popup", T(UIDEF_card_h_popup((Node *)self)));
        tset(CFG(self), "h_popup_config", T(Card_align_h_popup(self)));

        Node_hover((Node *)self);
    }
}

// card.lua:4329
void Card_stop_hover(Card *self) {
    Node_stop_hover((Node *)self);
}

// card.lua:4333 scale < 0: nil; rot_amount NIL: nil. One math.random() in either path.
void Card_juice_up(Card *self, float scale, Val rot_amount) {
    //G.VIBRATION = G.VIBRATION + 0.4
    float rot_amt = truthy(rot_amount) ? 0.4 * (math_random() > 0.5 ? 1 : -1) * vnum(rot_amount) : (math_random() > 0.5 ? 1 : -1) * 0.16;
    scale = scale >= 0 ? scale * 0.4f : 0.11f;
    Moveable_juice_up((Moveable *)self, scale, N(rot_amt));
}

// sprite:draw_shader(shader, nil, send) / sprite:draw_shader(shader, nil, send, nil, other_obj)
static void ds(Sprite *s, const char *shader, Table *send, Sprite *other_obj) {
    Sprite_draw_shader(s, shader, -1, send, false, other_obj, NIL, NIL, NIL, NIL, false, -1);
}

// the shared sticker pair of card.lua:4479-4500: draw_major, 'dissolve', then 'voucher' with the send pair
static void draw_sticker(Card *self, Sprite *s, Table *send, Sprite *center) {
    s->moveable.role.draw_major = (Moveable *)self;
    ds(s, "dissolve", NULL, center);
    ds(s, "voucher", send, center);
}

// card.lua:4340
void Card_draw(Card *self, const char *layer) {
    Node *node = &self->moveable.node;
    Table *ability = self->ability;
    Table *center_cfg;
    const char *ability_name = Card_ab(self)->name, *ability_set = Card_ab(self)->set;
    if (!layer) layer = "both";

    self->hover_tilt = 1;

    if (!node->states.visible) return;

    bool shadow_layer = streq(layer, "shadow") || streq(layer, "both");
    bool card_layer = streq(layer, "card") || streq(layer, "both");

    // note: self.ARGS.send_to_shader is a member (card.h), not a local: a 'card' pass reuses the pair the 'shadow'
    // pass wrote (and has nil if there never was one), as in the Lua
    if (shadow_layer) {
        Table *s = self->send_to_shader;
        if (!s) s = self->send_to_shader = tnew();
        tseti(s, 1, N(fminf(self->moveable.VT.r * 3, 1) + (float)G->TIMERS.REAL / (28) + (self->moveable.juice.on ? self->moveable.juice.r * 20 : 0) + self->tilt_var.amt));
        tseti(s, 2, N(G->TIMERS.REAL));

        for (TIter it = titer(node->children); tnext(&it);) {
            ((Moveable *)it.val.obj)->VT.scale = self->moveable.VT.scale;
        }
    }
    Table *send = self->send_to_shader;
    Sprite *center = Card_kids(self)->center, *front = Card_kids(self)->front, *back = Card_kids(self)->back;
    bool stone = streq(Card_ab(self)->effect, "Stone Card");

    G->shared_shadow = streq(self->sprite_facing, "front") ? center : back;

    //Draw the shadow
    if (!self->no_shadow && SETTINGS_snap()->shadows_on && (shadow_layer && (!streq(Card_ab(self)->effect, "Glass Card") && !self->greyed) && ((self->area && self->area != G->discard && !streq(CardArea_type(self->area), "deck")) || !self->area || node->states.drag.is))) {
        self->moveable.shadow_height = 0 * (0.08f + 0.4f * sqrtf(self->moveable.velocity.x * self->moveable.velocity.x)) + ((((self->highlighted && self->area == G->play) || node->states.drag.is) ? 0.35f : (self->area && streq(CardArea_type(self->area), "title_2")) ? 0.04f : 0.1f));
        Sprite_draw_shader(G->shared_shadow, "dissolve", self->moveable.shadow_height, NULL, false, NULL, NIL, NIL, NIL, NIL, false, -1);
    }

    if (card_layer && self->area != G->hand) {
        if (Card_kids(self)->focused_ui) CALL(Card_kids(self)->focused_ui, draw);
    }

    if (card_layer) {
        // for all hover/tilting: (self.tilt_var is never nil)
        float tilt_factor = 0.3f;
        if (node->states.focus.is) {
            self->tilt_var.mx = G->CONTROLLER->cursor_position.x + self->tilt_var.dx * node->T->w * G->TILESCALE * G->TILESIZE;
            self->tilt_var.my = G->CONTROLLER->cursor_position.y + self->tilt_var.dy * node->T->h * G->TILESCALE * G->TILESIZE;
            self->tilt_var.amt = fabsf(node->hover_offset.y + node->hover_offset.x - 1 + self->tilt_var.dx + self->tilt_var.dy - 1) * tilt_factor;
        } else if (node->states.hover.is) {
            self->tilt_var.mx = G->CONTROLLER->cursor_position.x;
            self->tilt_var.my = G->CONTROLLER->cursor_position.y;
            self->tilt_var.amt = fabsf(node->hover_offset.y + node->hover_offset.x - 1) * tilt_factor;
        } else { // elseif self.ambient_tilt: never nil (card.h), and 0 is truthy
            float tilt_angle = wrap_angle((float)G->TIMERS.REAL * (1.56f + lmodf(node->ID / 1.14212f, 1)) + node->ID / 1.35122f);
            self->tilt_var.mx = ((0.5f + 0.5f * self->ambient_tilt * cosf(tilt_angle)) * self->moveable.VT.w + self->moveable.VT.x + G->ROOM->T->x) * G->TILESIZE * G->TILESCALE;
            self->tilt_var.my = ((0.5f + 0.5f * self->ambient_tilt * sinf(tilt_angle)) * self->moveable.VT.h + self->moveable.VT.y + G->ROOM->T->y) * G->TILESIZE * G->TILESCALE;
            self->tilt_var.amt = self->ambient_tilt * (0.5f + cosf(tilt_angle)) * tilt_factor;
        }
        //Any particles
        if (Card_kids(self)->particles) CALL(Card_kids(self)->particles, draw);

        //Draw any tags/buttons
        if (Card_kids(self)->price) CALL(Card_kids(self)->price, draw);
        Node *buy_button = Card_kids(self)->buy_button;
        if (buy_button) {
            if (self->highlighted) {
                buy_button->states.visible = true;
                CALL(buy_button, draw);
                if (Card_kids(self)->buy_and_use_button) {
                    CALL(Card_kids(self)->buy_and_use_button, draw);
                }
            } else {
                buy_button->states.visible = false;
            }
        }
        if (Card_kids(self)->use_button && self->highlighted) CALL(Card_kids(self)->use_button, draw);

        center_cfg = Card_cen(self)->center;
        const struct CardCen *cen = Card_cen(self); // (nothing drawn below writes the center or the ability)
        const struct CardAb *ab = Card_ab(self);
        bool disc = cen->discovered || self->bypass_discovery_center;
        float t = (float)G->TIMERS.REAL;
        if (self->vortex) {
            if (streq(self->facing, "back")) {
                ds(back, "vortex", NULL, NULL);
            } else {
                ds(center, "vortex", NULL, NULL);
                if (front) {
                    ds(front, "vortex", NULL, NULL);
                }
            }

            love_graphics_setShader(NULL);
        } else if (streq(self->sprite_facing, "front")) {
            //Draw the main part of the card
            if ((self->edition && tbool(self->edition, "negative")) || (streq(ability_name, "Antimatter") && disc)) {
                ds(center, "negative", send, NULL);
                if (front && !stone) {
                    ds(front, "negative", send, NULL);
                }
            } else if (!self->greyed) {
                ds(center, "dissolve", NULL, NULL);
                //If the card has a front, draw that next
                if (front && !stone) {
                    ds(front, "dissolve", NULL, NULL);
                }
            }

            //If the card is not yet discovered
            if (!cen->discovered && (ab->consumeable || cen->unlocked) && !cen->demo && !self->bypass_discovery_center) {
                Sprite *shared_sprite = (streq(ability_set, "Edition") || streq(ability_set, "Joker")) ? G->shared_undiscovered_joker : G->shared_undiscovered_tarot;
                float scale_mod = -0.05f + 0.05f * sinf(wrap_angle(1.8f * t));
                float rotate_mod = 0.03f * sinf(wrap_angle(1.219f * t));

                shared_sprite->moveable.role.draw_major = (Moveable *)self;
                Sprite_draw_shader(shared_sprite, "dissolve", -1, NULL, false, center, N(scale_mod), N(rotate_mod), NIL, NIL, false, -1);
            }

            if (streq(ability_name, "Invisible Joker") && disc) {
                ds(center, "voucher", send, NULL);
            }

            //If the card has any edition/seal, add that here
            if (self->edition || self->seal || ab->eternal || ab->rental || ab->perishable || self->sticker || ((self->sticker_run && !streq(self->sticker_run, "NONE")) && tbool(G->SETTINGS, "run_stake_stickers")) || streq(ability_set, "Spectral") || self->debuff || self->greyed || streq(ability_name, "The Soul") || streq(ability_set, "Voucher") || streq(ability_set, "Booster") || cen->soul_pos || cen->demo) {

                if ((streq(ability_set, "Voucher") || cen->demo) && (!streq(ability_name, "Antimatter") || !disc)) {
                    ds(center, "voucher", send, NULL);
                }
                if (streq(ability_set, "Booster") || streq(ability_set, "Spectral")) {
                    ds(center, "booster", send, NULL);
                }
                if (self->edition && tbool(self->edition, "holo")) {
                    ds(center, "holo", send, NULL);
                    if (front && !stone) {
                        ds(front, "holo", send, NULL);
                    }
                }
                if (self->edition && tbool(self->edition, "foil")) {
                    ds(center, "foil", send, NULL);
                    if (front && !stone) {
                        ds(front, "foil", send, NULL);
                    }
                }
                if (self->edition && tbool(self->edition, "polychrome")) {
                    ds(center, "polychrome", send, NULL);
                    if (front && !stone) {
                        ds(front, "polychrome", send, NULL);
                    }
                }
                if ((self->edition && tbool(self->edition, "negative")) || (streq(ability_name, "Antimatter") && disc)) {
                    ds(center, "negative_shine", send, NULL);
                }
                if (self->seal) {
                    Sprite *seal = tobj(G->shared_seals, self->seal);
                    seal->moveable.role.draw_major = (Moveable *)self;
                    ds(seal, "dissolve", NULL, center);
                    if (streq(self->seal, "Gold")) ds(seal, "voucher", send, center);
                }
                if (ab->eternal) {
                    draw_sticker(self, G->shared_sticker_eternal, send, center);
                }
                if (ab->perishable) {
                    draw_sticker(self, G->shared_sticker_perishable, send, center);
                }
                if (ab->rental) {
                    draw_sticker(self, G->shared_sticker_rental, send, center);
                }
                if (self->sticker && tobj(G->shared_stickers, self->sticker)) {
                    draw_sticker(self, tobj(G->shared_stickers, self->sticker), send, center);
                } else if ((self->sticker_run && tobj(G->shared_stickers, self->sticker_run)) && tbool(G->SETTINGS, "run_stake_stickers")) {
                    draw_sticker(self, tobj(G->shared_stickers, self->sticker_run), send, center);
                }

                if (streq(ability_name, "The Soul") && disc) {
                    float fr = t - floorf(t);
                    float scale_mod = 0.05f + 0.05f * sinf(wrap_angle(1.8f * t)) + 0.07f * sinf(fr * (float)M_PI * 14) * powf(1 - fr, 3);
                    float rotate_mod = 0.1f * sinf(wrap_angle(1.219f * t)) + 0.07f * sinf(wrap_angle(t * (float)M_PI * 5)) * powf(1 - fr, 2);

                    G->shared_soul->moveable.role.draw_major = (Moveable *)self;
                    Sprite_draw_shader(G->shared_soul, "dissolve", 0, NULL, false, center, N(scale_mod), N(rotate_mod), NIL, N(0.1f + 0.03f * sinf(wrap_angle(1.8f * t))), false, 0.6f);
                    Sprite_draw_shader(G->shared_soul, "dissolve", -1, NULL, false, center, N(scale_mod), N(rotate_mod), NIL, NIL, false, -1);
                }

                if (cen->soul_pos && disc) {
                    float fr = t - floorf(t);
                    float scale_mod = 0.07f + 0.02f * sinf(wrap_angle(1.8f * t)) + 0.00f * sinf(fr * (float)M_PI * 14) * powf(1 - fr, 3);
                    float rotate_mod = 0.05f * sinf(wrap_angle(1.219f * t)) + 0.00f * sinf(wrap_angle(t * (float)M_PI * 5)) * powf(1 - fr, 2);
                    Sprite *floating_sprite = Card_kids(self)->floating_sprite;

                    if (streq(ability_name, "Hologram")) {
                        self->hover_tilt = self->hover_tilt * 1.5f;
                        Sprite_draw_shader(floating_sprite, "hologram", -1, send, false, center, N(2 * scale_mod), N(2 * rotate_mod), NIL, NIL, false, -1);
                        self->hover_tilt = self->hover_tilt / 1.5f;
                    } else {
                        Sprite_draw_shader(floating_sprite, "dissolve", 0, NULL, false, center, N(scale_mod), N(rotate_mod), NIL, N(0.1f + 0.03f * sinf(wrap_angle(1.8f * t))), false, 0.6f);
                        Sprite_draw_shader(floating_sprite, "dissolve", -1, NULL, false, center, N(scale_mod), N(rotate_mod), NIL, NIL, false, -1);
                    }
                }
                if (self->debuff) {
                    ds(center, "debuff", send, NULL);
                    if (front && !stone) {
                        ds(front, "debuff", send, NULL);
                    }
                }
                if (self->greyed) {
                    ds(center, "played", send, NULL);
                    if (front && !stone) {
                        ds(front, "played", send, NULL);
                    }
                }
            }
        } else if (streq(self->sprite_facing, "back")) {
            const float *overlay = G->C.WHITE;
            if (self->area && streq(CardArea_type(self->area), "deck") && self->rank > 3) {
                self->back_overlay[0] = 0.5f + lmodf(CardArea_n(self->area) - self->rank, 7) / 50;
                self->back_overlay[1] = 0.5f + lmodf(CardArea_n(self->area) - self->rank, 7) / 50;
                self->back_overlay[2] = 0.5f + lmodf(CardArea_n(self->area) - self->rank, 7) / 50;
                self->back_overlay[3] = 1;
                overlay = self->back_overlay;
            }

            if (self->area && streq(CardArea_type(self->area), "deck")) {
                Sprite_draw(back, overlay);
            } else {
                ds(back, "dissolve", NULL, NULL);
            }

            if (self->sticker && tobj(G->shared_stickers, self->sticker)) {
                Sprite *sticker = tobj(G->shared_stickers, self->sticker);
                sticker->moveable.role.draw_major = (Moveable *)self;
                Sprite_draw_shader(sticker, "dissolve", -1, NULL, true, center, NIL, NIL, NIL, NIL, false, -1);
                if (streq(self->sticker, "Gold")) Sprite_draw_shader(sticker, "voucher", -1, send, true, center, NIL, NIL, NIL, NIL, false, -1);
            }
        }

        for (TIter it = titer(node->children); tnext(&it);) {
            const char *k = it.key.t == V_STR ? it.key.s : NULL;
            if (!streq(k, "focused_ui") && !streq(k, "front") && !streq(k, "back") && !streq(k, "soul_parts") && !streq(k, "center") && !streq(k, "floating_sprite") && !streq(k, "shadow") && !streq(k, "use_button") && !streq(k, "buy_button") && !streq(k, "buy_and_use_button") && !streq(k, "debuff") && !streq(k, "price") && !streq(k, "particles") && !streq(k, "h_popup")) CALL(it.val.obj, draw);
        }

        if (card_layer && self->area == G->hand) {
            if (Card_kids(self)->focused_ui) CALL(Card_kids(self)->focused_ui, draw);
        }

        add_to_drawhash(self);
        Node_draw_boundingrect(node);
    }
}

// card.lua:4576
void Card_release(Card *self, void *dragged) {
    if (is_a(dragged, &Card_class) && self->area) {
        CALL(self->area, release, dragged);
    }
}

// card.lua:4582
void Card_highlight(Card *self, bool is_higlighted) {
    Table *ability = self->ability;
    self->highlighted = is_higlighted;
    if (ttab(ability, "consumeable") || streq(tstr(ability, "set"), "Joker") || (self->area && self->area == G->pack_cards)) {
        if (self->highlighted && self->area && !streq(tstr(CFG(self->area), "type"), "shop")) {
            num x_off = (ttab(ability, "consumeable") ? -0.1 : 0);
            bool owned = (self->area == G->jokers) || (self->area == G->consumeables);
            tseto(self->moveable.node.children, "use_button", UIBox_new(TAB(
                "definition", T(UIDEF_use_and_sell_buttons(self)),
                "config", T(TAB("align",
                        owned ? S("cr") :
                        S("bmi")
                    , "offset",
                        owned ? T(TAB("x", N(x_off - 0.4), "y", N(0))) :
                        T(TAB("x", N(0), "y", N(0.65))),
                    "parent", O(self)))
            )));
        } else if (Card_child(self, "use_button")) {
            CALL(Card_child(self, "use_button"), remove);
            tset(self->moveable.node.children, "use_button", NIL);
        }
    }
    if (ttab(ability, "consumeable") || streq(tstr(ability, "set"), "Joker")) {
        if (!self->highlighted && self->area && streq(tstr(CFG(self->area), "type"), "joker") &&
            (CardArea_n(G->jokers) >= tnum(CFG(G->jokers), "card_limit") || (self->edition && tbool(self->edition, "negative")))) {
            if (G->shop_jokers) CardArea_unhighlight_all(G->shop_jokers);
        }
    }
}

// card.lua:4610
void Card_click(Card *self) {
    if (self->area && CardArea_can_highlight(self->area, self)) {
        if ((self->area == G->hand) && (G->STATE == STATES_HAND_PLAYED)) return;
        if (self->highlighted != true) {
            CardArea_add_to_highlighted(self->area, self, false);
        } else {
            CardArea_remove_from_highlighted(self->area, self, false);
            play_sound("cardSlide2", -1, 0.3f);
        }
    }
    if (self->area && self->area == G->deck && CardArea_card(self->area, 1) == self) {
        FUNCS_deck_info(NULL);
    }
}

// a bool member that is nil or true in the Lua: true, or absent
static Val true_or_nil(bool b) { return b ? B(true) : NIL; }

// card.lua:4625
Table *Card_save(Card *self) {
    // note: no_ui, added_to_deck, pinned and the bypass_* flags are bool members (nil and false merged): saved as
    // true or absent. rank and playing_card: 0 is nil.
    Table *cardTable = TAB(
        "sort_id", N(self->sort_id),
        "save_fields", T(TAB(
            "center", S(tstr(CFG(self), "center_key")),
            "card", S(tstr(CFG(self), "card_key"))
        )),
        "params", T(self->params),
        "no_ui", true_or_nil(self->no_ui),
        "base_cost", N(self->base_cost),
        "extra_cost", N(self->extra_cost),
        "cost", N(self->cost),
        "sell_cost", N(self->sell_cost),
        "facing", S(self->facing),
        "sprite_facing", S(self->facing),
        // flipping = nil
        "highlighted", tget(DYN(self), "highligted"), // the Lua's typo: always nil
        "debuff", B(self->debuff),
        "rank", self->rank ? N(self->rank) : NIL,
        "added_to_deck", true_or_nil(self->added_to_deck),
        "label", S(self->label),
        "playing_card", self->playing_card ? N(self->playing_card) : NIL,
        "base", T(self->base),
        "ability", T(self->ability),
        "pinned", true_or_nil(self->pinned),
        "edition", T(self->edition),
        "seal", S(self->seal),
        "bypass_discovery_center", true_or_nil(self->bypass_discovery_center),
        "bypass_discovery_ui", true_or_nil(self->bypass_discovery_ui),
        "bypass_lock", true_or_nil(self->bypass_lock)
    );
    return cardTable;
}
