// engine/controller.lua:574-1107: axis input, button and key handlers, cursor collision / hover / press / release,
// is_node_focusable.
#include "all.h"

// node.area: a member of Card, a dyn field on anything else
static CardArea *node_area(Node *node) {
    if (is_a(node, &Card_class)) return ((Card *)node)->area;
    return (CardArea *)tobj(node->object.dyn, "area");
}

// node.UIBox: a member of UIElement, a dyn field on anything else
static UIBox *node_UIBox(Node *node) {
    if (is_a(node, &UIElement_class)) return ((UIElement *)node)->UIBox;
    return (UIBox *)tobj(node->object.dyn, "UIBox");
}

// controller.lua:575
// Handles all axis input for left stick, right stick and triggers. Treats them as buttons or cursors depending on context
const char *Controller_update_axis(Controller *self, float dt) {
    // Keep track of if there is any cursor movement from the axis changes
    const char *axis_interpretation = NULL;

    // Advance all the axis buttons to determine if there were any changes
    self->axis_buttons.l_stick.previous = self->axis_buttons.l_stick.current; self->axis_buttons.l_stick.current = "";
    self->axis_buttons.r_stick.previous = self->axis_buttons.r_stick.current; self->axis_buttons.r_stick.current = "";
    self->axis_buttons.l_trig.previous = self->axis_buttons.l_trig.current; self->axis_buttons.l_trig.current = "";
    self->axis_buttons.r_trig.previous = self->axis_buttons.r_trig.current; self->axis_buttons.r_trig.current = "";

    if (self->HID.controller) {
        Transform *CT = G->CURSOR->moveable.node.T;
        Transform *CVT = &G->CURSOR->moveable.VT;
        // ---- Left Thumbstick
        float l_stick_x = Joystick_getGamepadAxis(self->GAMEPAD.object, "leftx");
        float l_stick_y = Joystick_getGamepadAxis(self->GAMEPAD.object, "lefty");
        // If there is something being dragged, we want to treat the left stick as a cursor input
        if (self->dragging.target && fabsf(l_stick_x) + fabsf(l_stick_y) > 0.1) {
            axis_interpretation = "axis_cursor"; // There is some cursor movement

            // deadzone of 10% for each axis of l_stick
            if (fabsf(l_stick_x) < 0.1) l_stick_x = 0;
            if (fabsf(l_stick_y) < 0.1) l_stick_y = 0;
            l_stick_x = l_stick_x + (l_stick_x > 0 ? -0.1f : 0) + (l_stick_x < 0 ? 0.1f : 0);
            l_stick_y = l_stick_y + (l_stick_y > 0 ? -0.1f : 0) + (l_stick_y < 0 ? 0.1f : 0);

            // Modify the cursor position according to the l_stick axis values
            CT->x = CT->x + dt * l_stick_x * self->axis_cursor_speed;
            CT->y = CT->y + dt * l_stick_y * self->axis_cursor_speed;
            CVT->x = CT->x;
            CVT->y = CT->y;

            self->cursor_position.x = CT->x * (G->TILESCALE * G->TILESIZE);
            self->cursor_position.y = CT->y * (G->TILESCALE * G->TILESIZE);

        // If nothing is being dragged, we want to treat the left stick as a dpad input
        } else {
            self->axis_buttons.l_stick.current = self->axis_buttons.l_stick.previous;
            if (fabsf(l_stick_x) + fabsf(l_stick_y) > 0.5) {
                axis_interpretation = "button"; // left stick is no longer a cursor, can be set below from the right stick though

                self->axis_buttons.l_stick.current = fabsf(l_stick_x) > fabsf(l_stick_y)
                    ? (l_stick_x > 0 ? "dpright" : "dpleft")
                    : (l_stick_y > 0 ? "dpdown" : "dpup");
            } else if (fabsf(l_stick_x) + fabsf(l_stick_y) < 0.3) {
                self->axis_buttons.l_stick.current = "";
            }
        }

        // ---- Right Thumbstick
        float r_stick_x = Joystick_getGamepadAxis(self->GAMEPAD.object, "rightx");
        float r_stick_y = Joystick_getGamepadAxis(self->GAMEPAD.object, "righty");
        G->DEADZONE = 0.2f;
        float mag = sqrtf(powf(fabsf(r_stick_x), 2) + powf(fabsf(r_stick_y), 2));
        if (mag > G->DEADZONE) {
            axis_interpretation = "axis_cursor"; // There is some cursor movement

            // deadzone of 20% for each axis of r_stick
            if (fabsf(r_stick_x) < G->DEADZONE) r_stick_x = 0;
            if (fabsf(r_stick_y) < G->DEADZONE) r_stick_y = 0;
            r_stick_x = r_stick_x + (r_stick_x > 0 ? -G->DEADZONE : 0) + (r_stick_x < 0 ? G->DEADZONE : 0);
            r_stick_y = r_stick_y + (r_stick_y > 0 ? -G->DEADZONE : 0) + (r_stick_y < 0 ? G->DEADZONE : 0);

            // Modify the cursor position according to the r_stick axis values
            CT->x = CT->x + dt * r_stick_x * self->axis_cursor_speed;
            CT->y = CT->y + dt * r_stick_y * self->axis_cursor_speed;
            CVT->x = CT->x;
            CVT->y = CT->y;

            self->cursor_position.x = CT->x * (G->TILESCALE * G->TILESIZE);
            self->cursor_position.y = CT->y * (G->TILESCALE * G->TILESIZE);
        }

        // ---- Triggers: they are just buttons
        float l_trig = Joystick_getGamepadAxis(self->GAMEPAD.object, "triggerleft");
        float r_trig = Joystick_getGamepadAxis(self->GAMEPAD.object, "triggerright");

        // Set the current to be the same as previous, only set to '' if the trigger value is low enough
        self->axis_buttons.l_trig.current = self->axis_buttons.l_trig.previous;
        self->axis_buttons.r_trig.current = self->axis_buttons.r_trig.previous;

        // (controller.lua:662-664: the tilt effect's body is commented out in the Lua)

        if (l_trig > 0.5) {
            self->axis_buttons.l_trig.current = "triggerleft";
        } else if (l_trig < 0.3) {
            self->axis_buttons.l_trig.current = "";
        }
        if (r_trig > 0.5) {
            self->axis_buttons.r_trig.current = "triggerright";
        } else if (r_trig < 0.3) {
            self->axis_buttons.r_trig.current = "";
        }

        // return 'button' if trigger is being used and axis is not
        if (!streq(self->axis_buttons.r_trig.current, "") || !streq(self->axis_buttons.l_trig.current, "")) {
            if (!axis_interpretation) axis_interpretation = "button";
        }

        Controller_handle_axis_buttons(self);
    }

    if (axis_interpretation) self->interrupt.focus = false;

    return axis_interpretation;
}

// controller.lua:690
void Controller_button_press_update(Controller *self, const char *button, float dt) {
    if (tbool(self->locks, "frame")) return;
    tsetn(self->held_button_times, button, 0);
    self->interrupt.focus = false;

    if (!Controller_capture_focused_input(self, button, "press", dt)) {
        if (streq(button, "dpup")) {
            Controller_navigate_focus(self, "U");
        }
        if (streq(button, "dpdown")) {
            Controller_navigate_focus(self, "D");
        }
        if (streq(button, "dpleft")) {
            Controller_navigate_focus(self, "L");
        }
        if (streq(button, "dpright")) {
            Controller_navigate_focus(self, "R");
        }
    }

    if ((self->locked && !tbool(G->SETTINGS, "paused")) || tbool(self->locks, "frame") || self->frame_buttonpress) return;
    self->frame_buttonpress = true;

    Table *reg = ttab(self->button_registry, button);
    Table *reg1 = ttabi(reg, 1);
    if (reg && reg1 && !((Node *)tobj(reg1, "node"))->under_overlay) {
        tsetb(reg1, "click", true);
    } else {
        if (streq(button, "start")) {
            if (G->STATE == STATES_SPLASH) {
                Game_delete_run(G);
                Game_main_menu(G, NULL);
            }
        }
        if (streq(button, "a")) {
            Table *focus_args = self->focused.target ? ttab(self->focused.target->config, "focus_args") : NULL;
            if (self->focused.target && focus_args && streq(tstr(focus_args, "type"), "slider") &&
                (!G->CONTROLLER->HID.mouse && !G->CONTROLLER->HID.axis_cursor)) {
            } else {
                Controller_L_cursor_press(self, NIL, NIL);
            }
        }
        if (streq(button, "b")) {
            if (G->hand && self->focused.target && node_area(self->focused.target) == G->hand) {
                Controller_queue_R_cursor_press(self, NIL, NIL);
            } else {
                self->interrupt.focus = true;
            }
        }
    }
}

// controller.lua:742
void Controller_button_hold_update(Controller *self, const char *button, float dt) {
    if ((self->locked && !tbool(G->SETTINGS, "paused")) || tbool(self->locks, "frame") || self->frame_buttonpress) return;
    self->frame_buttonpress = true;
    if (truthy(tget(self->held_button_times, button))) {
        tsetn(self->held_button_times, button, tnum(self->held_button_times, button) + dt);
        Controller_capture_focused_input(self, button, "hold", dt);
    }
    if ((streq(button, "dpleft") || streq(button, "dpright") || streq(button, "dpup") || streq(button, "dpdown")) && !self->no_holdcap) {
        if (self->repress_timer == 0) self->repress_timer = 0.3f; // 0 as nil (controller.h)
        // note: repress_timer is a float member, so the threshold is 0.3f / 0.1f, not the Lua's doubles
        if (truthy(tget(self->held_button_times, button)) && (tnum(self->held_button_times, button) > self->repress_timer)) {
            self->repress_timer = 0.1f;
            tsetn(self->held_button_times, button, 0);
            Controller_button_press_update(self, button, dt);
        }
    }
}

// controller.lua:759
void Controller_button_release_update(Controller *self, const char *button, float dt) {
    if (!truthy(tget(self->held_button_times, button))) return;
    self->repress_timer = 0.3f;
    tset(self->held_button_times, button, NIL);
    if (streq(button, "a")) {
        Controller_L_cursor_release(self, NIL, NIL);
    }
}

// controller.lua:768
void Controller_key_press_update(Controller *self, const char *key, float dt) {
    if (tbool(self->locks, "frame")) return;
    if (streq(str_sub(key, 1, 2), "kp")) key = str_sub(key, 3, -1);
    if (streq(key, "enter")) key = "return";

    if (self->text_input_hook) {
        if (streq(key, "escape")) {
            self->text_input_hook = NULL;
        } else if (streq(key, "capslock")) {
            self->capslock = !self->capslock;
        } else {
            Val caps = tget(self->held_keys, "lshift");
            if (!truthy(caps)) caps = tget(self->held_keys, "rshift");
            Table *args = TAB("e", O(self->text_input_hook), "key", S(key));
            tset(args, "caps", caps);
            FUNCS_text_input_key(args);
        }
        return;
    }

    if (streq(key, "escape")) {
        if (G->STATE == STATES_SPLASH) {
            Game_delete_run(G);
            Game_main_menu(G, NULL);
        } else {
            if (!G->OVERLAY_MENU) {
                FUNCS_options(NULL); // note: the Lua's G.FUNCS:options() passes G.FUNCS as e, which options does not use
            } else if (!tbool(G->OVERLAY_MENU->moveable.node.config, "no_esc")) {
                FUNCS_exit_overlay_menu();
            }
        }
    }

    if ((self->locked && !tbool(G->SETTINGS, "paused")) || tbool(self->locks, "frame") || self->frame_buttonpress) return;
    self->frame_buttonpress = true;
    tsetn(self->held_key_times, key, 0);

    // controller.lua:806-890: the debug keys (`if not _RELEASE_MODE`), not translated
}

// controller.lua:893
void Controller_key_hold_update(Controller *self, const char *key, float dt) {
    if ((self->locked && !tbool(G->SETTINGS, "paused")) || tbool(self->locks, "frame") || self->frame_buttonpress) return;
    // self.frame_buttonpress = true (commented out in the Lua)
    if (truthy(tget(self->held_key_times, key))) {
        if (streq(key, "r") && !tbool(G->SETTINGS, "paused")) {
            if (tnum(self->held_key_times, key) > 0.7) {
                if (!tbool(G->GAME, "won") && !tbool(G->GAME, "seeded") && !tbool(G->GAME, "challenge")) {
                    tsetn(ttab(ttab(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "high_scores"), "current_streak"), "amt", 0);
                }
                Game_save_settings(G);
                tset(self->held_key_times, key, NIL);
                tsets(G->SETTINGS, "current_setup", "New Run");
                tset(G->GAME, "viewed_back", NIL);
                tset(G->object.dyn, "run_setup_seed", tget(G->GAME, "seeded")); // G.run_setup_seed is in dyn (game.h)
                G->challenge_tab = (G->GAME && tbool(G->GAME, "challenge")) ? ttab(G->GAME, "challenge_tab") : NULL;
                G->forced_seed = NULL; tset(G->object.dyn, "setup_seed", NIL);
                if (tbool(G->GAME, "seeded")) G->forced_seed = tstr(ttab(G->GAME, "pseudorandom"), "seed");
                G->forced_stake = tnum(G->GAME, "stake");
                if (G->STAGE == STAGES_RUN) FUNCS_start_setup_run(NULL);
                G->forced_stake = 0; // 0 as nil
                G->challenge_tab = NULL;
                G->forced_seed = NULL;
            } else {
                tsetn(self->held_key_times, key, tnum(self->held_key_times, key) + dt);
            }
        }
    }
}

// controller.lua:922
void Controller_key_release_update(Controller *self, const char *key, float dt) {
    if ((self->locked && !tbool(G->SETTINGS, "paused")) || tbool(self->locks, "frame") || self->frame_buttonpress) return;
    self->frame_buttonpress = true;
    // controller.lua:925-927: the G.DEBUG toggle (`not _RELEASE_MODE`), not translated
    if (streq(key, "tab") && G->debug_tools) {
        CALL(G->debug_tools, remove);
        G->debug_tools = NULL;
    }
}

// controller.lua:934
void Controller_key_press(Controller *self, const char *key) {
    tsetb(self->pressed_keys, key, true);
    tsetb(self->held_keys, key, true);
}

// controller.lua:939
void Controller_key_release(Controller *self, const char *key) {
    tset(self->held_keys, key, NIL);
    tsetb(self->released_keys, key, true);
}

// controller.lua:944
void Controller_button_press(Controller *self, const char *button) {
    tsetb(self->pressed_buttons, button, true);
    tsetb(self->held_buttons, button, true);
}

// controller.lua:949
void Controller_button_release(Controller *self, const char *button) {
    tset(self->held_buttons, button, NIL);
    tsetb(self->released_buttons, button, true);
}

// controller.lua:954
void Controller_get_cursor_collision(Controller *self, Vec2 cursor_trans) {
    tclear(self->collision_list);
    tclear(self->nodes_at_cursor);

    if (self->COYOTE_FOCUS) return;
    if (self->dragging.target) {
        self->dragging.target->states.collide.is = true;
        tinsert(self->nodes_at_cursor, O(self->dragging.target));
        tinsert(self->collision_list, O(self->dragging.target));
    }

    if (!truthy(tgeti(G->DRAW_HASH, 1)) ||
        cursor_trans.x - G->ROOM->T->x < -G->DRAW_HASH_BUFF || cursor_trans.x - G->ROOM->T->x > G->TILE_W + G->DRAW_HASH_BUFF ||
        cursor_trans.y - G->ROOM->T->y < -G->DRAW_HASH_BUFF || cursor_trans.y - G->ROOM->T->y > G->TILE_H + G->DRAW_HASH_BUFF) {
        return;
    }

    Table *DRAW_HASH_SQUARE = G->DRAW_HASH;
    for (int i = tlen(DRAW_HASH_SQUARE); i >= 1; i--) {
        Node *v = (Node *)tobji(DRAW_HASH_SQUARE, i);
        if (CALL(v, collides_with_point, cursor_trans) && !v->REMOVED) {
            tinsert(self->nodes_at_cursor, O(v));
            if (v->states.collide.can) {
                v->states.collide.is = true;
                tinsert(self->collision_list, O(v));
            }
        }
    }
}

// controller.lua:984
void Controller_set_cursor_hover(Controller *self) {
    self->cursor_hover.T.x = G->CURSOR->moveable.node.T->x; self->cursor_hover.T.y = G->CURSOR->moveable.node.T->y;
    self->cursor_hover.time = G->TIMERS.TOTAL;

    self->cursor_hover.prev_target = self->cursor_hover.target;
    self->cursor_hover.target = NULL;

    if (self->interrupt.focus || (self->locked && (!tbool(G->SETTINGS, "paused") || G->screenwipe)) || tbool(self->locks, "frame") ||
        self->COYOTE_FOCUS) {
        self->cursor_hover.target = G->ROOM;
        return;
    }

    if (self->HID.controller && self->focused.target && self->focused.target->states.hover.can) {
        if ((self->HID.dpad || self->HID.axis_cursor) && self->focused.target->states.collide.is) {
            self->cursor_hover.target = self->focused.target;
        } else {
            for (int i = 1; truthy(tgeti(self->collision_list, i)); i++) {
                Node *v = (Node *)tobji(self->collision_list, i);
                if (v->states.hover.can) {
                    self->cursor_hover.target = v;
                    break;
                }
            }
        }
    } else {
        for (int i = 1; truthy(tgeti(self->collision_list, i)); i++) {
            Node *v = (Node *)tobji(self->collision_list, i);
            if (v->states.hover.can && (!v->states.drag.is || self->HID.touch)) {
                self->cursor_hover.target = v;
                break;
            }
        }
    }

    if (!self->cursor_hover.target || (self->dragging.target && !self->HID.touch)) self->cursor_hover.target = G->ROOM;
    if (self->cursor_hover.target != self->cursor_hover.prev_target) self->cursor_hover.handled = false;
}

// controller.lua:1018
void Controller_queue_L_cursor_press(Controller *self, Val x, Val y) {
    if (tbool(self->locks, "frame")) return;
    if (G->STATE == STATES_SPLASH) {
        Controller_key_press(self, "escape");
    }
    self->L_cursor_queue.set = true;
    self->L_cursor_queue.x = x;
    self->L_cursor_queue.y = y;
}

// controller.lua:1026
void Controller_queue_R_cursor_press(Controller *self, Val x, Val y) {
    if (tbool(self->locks, "frame")) return;
    if (!tbool(G->SETTINGS, "paused") && G->hand && truthy(tgeti(G->hand->highlighted, 1))) {
        Val STOP_USE = tget(G->GAME, "STOP_USE");
        if ((G->play && tlen(G->play->cards) > 0) ||
            self->locked ||
            tbool(self->locks, "frame") ||
            (truthy(STOP_USE) && vnum(STOP_USE) > 0)) return;
        CardArea_unhighlight_all(G->hand);
    }
}

// controller.lua:1037
void Controller_L_cursor_press(Controller *self, Val x, Val y) {
    float xf = truthy(x) ? (float)vnum(x) : self->cursor_position.x;
    float yf = truthy(y) ? (float)vnum(y) : self->cursor_position.y;

    if ((self->locked && (!tbool(G->SETTINGS, "paused") || G->screenwipe)) || tbool(self->locks, "frame")) return;

    self->cursor_down.T = (Vec2){xf / (G->TILESCALE * G->TILESIZE), yf / (G->TILESCALE * G->TILESIZE)};
    self->cursor_down.time = G->TIMERS.TOTAL;
    self->cursor_down.handled = false;
    self->cursor_down.target = NULL;
    self->is_cursor_down = true;

    Node *press_node = (self->HID.touch && self->cursor_hover.target) ? self->cursor_hover.target
                       : self->hovering.target ? self->hovering.target : self->focused.target;

    if (press_node) {
        self->cursor_down.target = press_node->states.click.can ? press_node : (Node *)CALL(press_node, can_drag);
    }

    if (self->cursor_down.target == NULL) {
        self->cursor_down.target = G->ROOM;
    }
}

// controller.lua:1060
void Controller_L_cursor_release(Controller *self, Val x, Val y) {
    float xf = truthy(x) ? (float)vnum(x) : self->cursor_position.x;
    float yf = truthy(y) ? (float)vnum(y) : self->cursor_position.y;

    if ((self->locked && (!tbool(G->SETTINGS, "paused") || G->screenwipe)) || tbool(self->locks, "frame")) return;

    self->cursor_up.T = (Vec2){xf / (G->TILESCALE * G->TILESIZE), yf / (G->TILESCALE * G->TILESIZE)};
    self->cursor_up.time = G->TIMERS.TOTAL;
    self->cursor_up.handled = false;
    self->cursor_up.target = NULL;
    self->is_cursor_down = false;

    self->cursor_up.target = self->hovering.target ? self->hovering.target : self->focused.target;

    if (self->cursor_up.target == NULL) {
        self->cursor_up.target = G->ROOM;
    }
}

// controller.lua:1079
bool Controller_is_node_focusable(Controller *self, Node *node) {
    bool ret_val = false;
    if (node->T->y > G->ROOM->T->h + 3) return false;
    UIBox *box = node_UIBox(node);
    if (!node->REMOVED && !node->under_overlay && ((node->states.hover.can && !self->dragging.target) || self->dragging.target == node) &&
        node->created_on_pause == tbool(G->SETTINGS, "paused") &&
        node->states.visible && (!box || box->moveable.node.states.visible)) {
        if (self->screen_keyboard) {
            if (box && box == self->screen_keyboard && truthy(tget(node->config, "button"))) {
                ret_val = true;
            }
        } else {
            if (is_a(node, &Card_class) &&
                (streq(((Card *)node)->facing, "front") || ((Card *)node)->area == G->hand || ((Card *)node)->area == G->jokers ||
                 node == (Node *)G->deck) &&
                node->states.hover.can && !truthy(tget(node->object.dyn, "jimbo"))) {
                ret_val = true;
            }
            if (node->config && truthy(tget(node->config, "force_focus"))) ret_val = true;
            if (node->config && truthy(tget(node->config, "button"))) ret_val = true;
            if (node->config && truthy(tget(node->config, "focus_args"))) {
                Table *focus_args = ttab(node->config, "focus_args");
                if (streq(tstr(focus_args, "type"), "none") || truthy(tget(focus_args, "funnel_from"))) {
                    ret_val = false;
                } else {
                    ret_val = true;
                }
            }
        }
    }
    return ret_val;
}
