// engine/controller.lua:1108-1382: the focus of the gamepad (update_focus, capture_focused_input, navigate_focus).
#include "all.h"

// G.ARGS.focus_list (array of {node = O(Node), dist = N}), G.ARGS.focusables (array of O(Node)),
// G.ARGS.focus_cursor_pos, G.ARGS.focus_vec: scratch of update_focus only (controller.h: locals of controller.c).
static Table *focus_list, *focusables;
static Vec2 focus_cursor_pos, focus_vec;

// config.focus_args.funnel_to / funnel_from: nil/false -> NULL; O(UIElement) -> the node.
// note: B(true) (a definition not yet linked by set_values) is indexed by the Lua, which fails: same here.
static Node *funnel_node(Table *focus_args, const char *key) {
    Val v = tget(focus_args, key);
    if (!truthy(v)) return NULL;
    if (v.t != V_OBJ) ERROR("controller.lua: attempt to index focus_args.%s (a %s value)", key, lua_typename_of(v));
    return (Node *)v.obj;
}

// controller.lua:1235 function (a, b) return a.dist < b.dist end
static bool focus_dist_less(Val a, Val b, void *ctx) { return tnum(a.tab, "dist") < tnum(b.tab, "dist"); }

// controller.lua:1284, :1290 function (a, b) return a.rank < b.rank end
static bool card_rank_less(Val a, Val b, void *ctx) { return ((Card *)a.obj)->rank < ((Card *)b.obj)->rank; }

// controller.lua:1112
void Controller_update_focus(Controller *self, const char *dir) {
    self->focused.prev_target = self->focused.target;

    // Only needed when using a controller, hovering covers all KBM scenarios (:1116)
    if (!self->HID.controller || self->interrupt.focus || (self->locked && (!tbool(G->SETTINGS, "paused") || G->screenwipe))) {
        if (self->focused.target) self->focused.target->states.focus.is = false;
        self->focused.target = NULL;
        return;
    }

    focus_list = EMPTY(focus_list);
    focusables = EMPTY(focusables);

    // First, is there currently a focusable target that is still valid? (:1129)
    if (self->focused.target) {
        self->focused.target->states.focus.is = false;

        Node *cur = (Node *)G->CURSOR;
        if (!Controller_is_node_focusable(self, self->focused.target) ||
            !CALL(self->focused.target, collides_with_point, (Vec2){cur->T->x, cur->T->y}) || self->HID.axis_cursor) {
            self->focused.target = NULL;
        }
    }

    // :1142
    if (!dir && self->focused.target) {
        self->focused.target->states.focus.can = true;
        tseti(focusables, tlen(focusables) + 1, O(self->focused.target));
    } else {
        if (!dir) {
            for (int k = 1; truthy(tgeti(self->nodes_at_cursor, k)); k++) {
                Node *v = (Node *)tobji(self->nodes_at_cursor, k);
                v->states.focus.can = false;
                v->states.focus.is = false;
                if (tlen(focusables) == 0 && Controller_is_node_focusable(self, v)) {
                    v->states.focus.can = true;
                    tseti(focusables, tlen(focusables) + 1, O(v));
                }
            }
        } else {
            // note: pairs(G.MOVEABLES): an array, index order (titer gives the same)
            for (TIter it = titer(G->MOVEABLES); tnext(&it);) {
                Node *v = (Node *)it.val.obj;
                v->states.focus.can = false;
                v->states.focus.is = false;
                if (Controller_is_node_focusable(self, v)) {
                    v->states.focus.can = true;
                    tseti(focusables, tlen(focusables) + 1, O(v));
                }
            }
        }
    }

    // If there are any valid focusables (:1168)
    if (tlen(focusables) > 0) {
        if (dir) {
            // note: only a Card has .area (card.lua:55, :4092)
            if ((streq(dir, "L") || streq(dir, "R")) && self->focused.target && is_a(self->focused.target, &Card_class) &&
                ((Card *)self->focused.target)->area == G->hand && G->hand) {
                Card *ft_card = (Card *)self->focused.target;
                int nu_rank = ft_card->rank + (streq(dir, "L") ? -1 : 1);
                if (nu_rank > CardArea_n(G->hand)) nu_rank = 1;
                if (nu_rank == 0) nu_rank = CardArea_n(G->hand);
                if (nu_rank != ft_card->rank) tseti(focus_list, 1, T(TAB("node", O(CardArea_card(G->hand, nu_rank)))));
            } else {
                // set the cursor position to where it currently is on screen (:1178)
                Node *cur = (Node *)G->CURSOR;
                focus_cursor_pos.x = cur->T->x - G->ROOM->T->x;
                focus_cursor_pos.y = cur->T->y - G->ROOM->T->y;

                // if there is a focused target, set the cursor to the midpoint (:1182)
                if (self->focused.target) {
                    Node *_ft = self->focused.target; // note: the Lua's accidental global _ft
                    Table *fa = ttab(self->focused.target->config, "focus_args");
                    if (fa && truthy(tget(fa, "funnel_to"))) {
                        _ft = funnel_node(fa, "funnel_to");
                    }
                    focus_cursor_pos.x = _ft->T->x + 0.5f * _ft->T->w;
                    focus_cursor_pos.y = _ft->T->y + 0.5f * _ft->T->h;
                // if not but there is a focusable hovering target, put the cursor on it instead (:1189)
                } else if (self->hovering.target && self->hovering.target->states.focus.can) {
                    CALL(self->hovering.target, put_focused_cursor, &focus_cursor_pos.x, &focus_cursor_pos.y);
                    focus_cursor_pos.x = focus_cursor_pos.x / (G->TILESCALE * G->TILESIZE) - G->ROOM->T->x;
                    focus_cursor_pos.y = focus_cursor_pos.y / (G->TILESCALE * G->TILESIZE) - G->ROOM->T->y;
                }

                // set the list to be all the nodes in that direction sorted by the closest node (:1196)
                for (TIter it = titer(focusables); tnext(&it);) {
                    Node *v = (Node *)it.val.obj;
                    if (v != self->hovering.target && v != self->focused.target) {
                        bool eligible = false;

                        Table *fa = ttab(v->config, "focus_args");
                        if (fa && truthy(tget(fa, "funnel_to"))) {
                            v = funnel_node(fa, "funnel_to");
                        }

                        focus_vec.x = v->T->x + 0.5f * v->T->w - (focus_cursor_pos.x);
                        focus_vec.y = v->T->y + 0.5f * v->T->h - (focus_cursor_pos.y);

                        fa = ttab(v->config, "focus_args");
                        if (fa && truthy(tget(fa, "nav"))) {
                            const char *nav = tstr(fa, "nav");
                            if (streq(nav, "wide")) {
                                if (focus_vec.y > 0.1f && streq(dir, "D")) eligible = true;
                                else if (focus_vec.y < -0.1f && streq(dir, "U")) eligible = true;
                                else if (fabsf(focus_vec.y) < v->T->h / 2) eligible = true;
                            } else if (streq(nav, "tall")) {
                                if (focus_vec.x > 0.1f && streq(dir, "R")) eligible = true;
                                else if (focus_vec.x < -0.1f && streq(dir, "L")) eligible = true;
                                else if (fabsf(focus_vec.x) < v->T->w / 2) eligible = true;
                            }
                        } else if (fabsf(focus_vec.x) > fabsf(focus_vec.y)) {
                            if (focus_vec.x > 0 && streq(dir, "R")) eligible = true;
                            else if (focus_vec.x < 0 && streq(dir, "L")) eligible = true;
                        } else {
                            if (focus_vec.y > 0 && streq(dir, "D")) eligible = true;
                            else if (focus_vec.y < 0 && streq(dir, "U")) eligible = true;
                        }

                        if (eligible) {
                            tseti(focus_list, tlen(focus_list) + 1,
                                  T(TAB("node", O(v), "dist", N(fabsf(focus_vec.x) + fabsf(focus_vec.y)))));
                        }
                    }
                }
                if (tlen(focus_list) < 1) {
                    if (self->focused.target) self->focused.target->states.focus.is = true;
                    return;
                }
                tsort(focus_list, focus_dist_less, NULL);
            }
        } else {
            if (self->focused.target) {
                tseti(focus_list, tlen(focus_list) + 1, T(TAB("node", O(self->focused.target), "dist", N(0))));
            } else {
                // otherwise, get the focusable that collides
                tseti(focus_list, tlen(focus_list) + 1, T(TAB("node", tgeti(focusables, 1), "dist", N(0))));
            }
        }
    }

    // now with the lists created, set the focused target to be the first node in the list (:1248)
    if (truthy(tgeti(focus_list, 1))) {
        Node *node = (Node *)tobj(ttabi(focus_list, 1), "node");
        // note: node nil (G.hand.cards[nu_rank] missing at :1175) fails in the Lua too
        if (!node) ERROR("controller.lua:1249: attempt to index field 'node' (a nil value)");
        Table *fa = ttab(node->config, "focus_args"); // (node.config is never nil)
        if (fa && truthy(tget(fa, "funnel_from"))) {
            self->focused.target = funnel_node(fa, "funnel_from");
        } else {
            self->focused.target = node;
        }
        if (self->focused.target != self->focused.prev_target) G->VIBRATION = G->VIBRATION + 0.7f;
    } else {
        self->focused.target = NULL;
    }

    if (self->focused.target) self->focused.target->states.focus.is = true;
}

// controller.lua:1262
bool Controller_capture_focused_input(Controller *self, const char *button, const char *input_type, float dt) {
    int ret = 0; // 0 false, 1 true, -1 nil (:1327, :1336)
    Node *focused = self->focused.target;
    bool extern_button = false;
    self->no_holdcap = false;
    bool press = streq(input_type, "press");
    // note: only a Card has .area and .rank (card.lua:55, :4092)
    Card *fcard = (focused && is_a(focused, &Card_class)) ? (Card *)focused : NULL;
    CardArea *farea = fcard ? fcard->area : NULL;
    Val held_a = tget(self->held_button_times, "a");

    // 'coyote time' selection (:1270)
    if (press && (streq(button, "dpleft") || streq(button, "dpright")) &&
        focused && self->dragging.target &&
        (truthy(held_a) && held_a.n < 0.12) &&
        farea && CardArea_can_highlight(farea, fcard)) {
        Controller_L_cursor_release(self, NIL, NIL);
        Controller_navigate_focus(self, streq(button, "dpleft") ? "L" : "R");
        tset(self->held_button_times, "a", NIL);
        self->COYOTE_FOCUS = true;
        ret = 1;
    } else if (press && focused && farea && focused == self->dragging.target) {
        focused->states.drag.is = false;
        if (streq(button, "dpleft") && fcard->rank > 1) {
            fcard->rank = fcard->rank - 1;
            CardArea_card(farea, fcard->rank)->rank = fcard->rank + 1;
            tsort(farea->cards, card_rank_less, NULL);
            CardArea_align_cards(farea);
            Controller_update_cursor(self, NULL);
        } else if (streq(button, "dpright") && fcard->rank < CardArea_n(farea)) {
            fcard->rank = fcard->rank + 1;
            CardArea_card(farea, fcard->rank)->rank = fcard->rank - 1;
            tsort(farea->cards, card_rank_less, NULL);
            CardArea_align_cards(farea);
            Controller_update_cursor(self, NULL);
        }
        focused->states.drag.is = true;
        ret = 1;
    }

    // note: G.OVERLAY_MENU == true (OVERLAY_MENU_TRUE, only while FUNCS_overlay_menu builds the box) would fail in
    // the Lua on :get_UIE_by_ID; no input is processed then.
    if (G->OVERLAY_MENU && !self->screen_keyboard && press) {
        if (G->OVERLAY_MENU == OVERLAY_MENU_TRUE) ERROR("controller.lua:1298: attempt to index G.OVERLAY_MENU (a boolean value)");
    }
    // :1298
    if (G->OVERLAY_MENU && !self->screen_keyboard && press && UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("tab_shoulders"), NULL) &&
        (streq(button, "leftshoulder") || streq(button, "rightshoulder"))) {
        focused = (Node *)UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("tab_shoulders"), NULL);
        extern_button = true;
    }
    // :1302
    if (G->OVERLAY_MENU && !self->screen_keyboard && press && UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("cycle_shoulders"), NULL) &&
        (streq(button, "leftshoulder") || streq(button, "rightshoulder"))) {
        focused = (Node *)tobji(((Node *)UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("cycle_shoulders"), NULL))->children, 1);
        extern_button = true;
    }
    // :1306
    Table *focus_args = focused ? ttab(focused->config, "focus_args") : NULL;
    if (focused && focus_args) {
        const char *type = tstr(focus_args, "type");
        if (streq(type, "cycle") && press) {
            if ((extern_button && streq(button, "leftshoulder")) || (!extern_button && streq(button, "dpleft"))) {
                CALL(tobji(focused->children, 1), click);
                ret = 1;
            }
            if ((extern_button && streq(button, "rightshoulder")) || (!extern_button && streq(button, "dpright"))) {
                CALL(tobji(focused->children, 3), click);
                ret = 1;
            }
        }
        if (streq(type, "tab") && press) {
            // note: a node with focus_args.type 'tab' is a UIElement (focused.UIBox)
            Node *c1 = (Node *)tobji(focused->children, 1);
            Node *c11 = (Node *)tobji(c1->children, 1);
            Table *proto_choices = UIBox_get_group(((UIElement *)focused)->UIBox, NULL, tget(c11->config, "group"), NULL);
            Table *choices = tnew();
            for (int k = 1; truthy(tgeti(proto_choices, k)); k++) {
                Node *v = (Node *)tobji(proto_choices, k);
                if (tbool(v->config, "choice") && tbool(v->config, "button")) tseti(choices, tlen(choices) + 1, O(v));
            }
            for (int k = 1; truthy(tgeti(choices, k)); k++) {
                Node *v = (Node *)tobji(choices, k);
                if (tbool(v->config, "chosen")) {
                    if ((extern_button && streq(button, "leftshoulder")) || (!extern_button && streq(button, "dpleft"))) {
                        int next_i = k != 1 ? (k - 1) : tlen(choices);
                        if (tbool(focus_args, "no_loop") && next_i > k) ret = -1;
                        else {
                            CALL(tobji(choices, next_i), click);
                            Controller_snap_to(self, TAB("node", tgeti(choices, next_i)));
                            Controller_update_cursor(self, NULL);
                            ret = 1;
                        }
                    } else if ((extern_button && streq(button, "rightshoulder")) || (!extern_button && streq(button, "dpright"))) {
                        int next_i = k != tlen(choices) ? (k + 1) : (1);
                        if (tbool(focus_args, "no_loop") && next_i < k) ret = -1;
                        else {
                            CALL(tobji(choices, next_i), click);
                            Controller_snap_to(self, TAB("node", tgeti(choices, next_i)));
                            Controller_update_cursor(self, NULL);
                            ret = 1;
                        }
                    }
                    break;
                }
            }
        }
        if (streq(type, "slider")) {
            // note: held_button_times[button] nil would fail the Lua's comparison; tnum reads it as 0 (no slide)
            if (streq(button, "dpleft")) {
                self->no_holdcap = true;
                if (streq(input_type, "hold") && tnum(self->held_button_times, button) > 0.2) {
                    FUNCS_slider_descreet((UIElement *)tobji(focused->children, 1), -(num)dt * tnum(self->held_button_times, button) * 0.6);
                }
                if (press) {
                    FUNCS_slider_descreet((UIElement *)tobji(focused->children, 1), -0.01);
                }
                ret = 1;
            }
            if (streq(button, "dpright")) {
                self->no_holdcap = true;
                if (streq(input_type, "hold") && tnum(self->held_button_times, button) > 0.2) {
                    FUNCS_slider_descreet((UIElement *)tobji(focused->children, 1), (num)dt * tnum(self->held_button_times, button) * 0.6);
                }
                if (press) {
                    FUNCS_slider_descreet((UIElement *)tobji(focused->children, 1), 0.01);
                }
                ret = 1;
            }
        }
    }
    if (ret == 1) G->VIBRATION = G->VIBRATION + 1;
    return ret == 1;
}

// controller.lua:1375
void Controller_navigate_focus(Controller *self, const char *dir) {
    // Get the corresponding focus target first, with or without a direction
    Controller_update_focus(self, dir);

    // Set the cursor to be in the correct position for that target
    Controller_update_cursor(self, NULL);
}
