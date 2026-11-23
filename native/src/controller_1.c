// engine/controller.lua, lines 1-573: the Controller's init, HID flags, the per-frame update, the button registry,
// the cursor context and the cursor snapping.
#include "all.h"
#include <ctype.h>

// controller.lua:2 Controller = Object:extend()
Class Controller_class = {.name = "Controller", .super = &Object_class};

// node.area / node.rank / node.click_timeout: members of Card (card.h), in dyn for any other class.
static CardArea *node_area(Node *n) {
    if (is_a(n, &Card_class)) return ((Card *)n)->area;
    return (CardArea *)tobj(n->object.dyn, "area");
}
static Val node_rank(Node *n) {
    if (is_a(n, &Card_class)) return ((Card *)n)->rank ? N(((Card *)n)->rank) : NIL; // 0 as nil (card.h)
    return tget(n->object.dyn, "rank");
}
static Val node_click_timeout(Node *n) {
    if (is_a(n, &Card_class)) return N(((Card *)n)->click_timeout);
    return tget(n->object.dyn, "click_timeout");
}

// controller.lua:5
void Controller_init(Controller *self) {
    // :8-12
    self->clicked = (ControllerTarget){.target = NULL, .handled = true, .prev_target = NULL};
    self->focused = (ControllerTarget){.target = NULL, .handled = true, .prev_target = NULL};
    self->dragging = (ControllerTarget){.target = NULL, .handled = true, .prev_target = NULL};
    self->hovering = (ControllerTarget){.target = NULL, .handled = true, .prev_target = NULL};
    self->released_on = (ControllerTarget){.target = NULL, .handled = true, .prev_target = NULL};

    self->collision_list = tnew(); // :14
    self->nodes_at_cursor = tnew(); // note: the Lua makes it on first use (:955, EMPTY(nil)); controller.h asks for it here

    // :17-21
    self->cursor_down = (ControllerCursor){.T = {0, 0}, .target = NULL, .time = 0, .handled = true};
    self->cursor_up = (ControllerCursor){.T = {0, 0}, .target = NULL, .time = 0.1, .handled = true};
    self->cursor_hover = (ControllerCursor){.T = {0, 0}, .target = NULL, .time = 0, .handled = true};
    self->cursor_collider = NULL;
    self->cursor_position = (Vec2){0, 0};

    // :24-27
    self->pressed_keys = tnew();
    self->held_keys = tnew();
    self->held_key_times = tnew();
    self->released_keys = tnew();

    // :30-33
    self->pressed_buttons = tnew();
    self->held_buttons = tnew();
    self->held_button_times = tnew();
    self->released_buttons = tnew();

    // :36
    self->interrupt.focus = false;
    self->interrupt.stack = false;

    // :41-42
    self->locks = tnew();
    self->locked = false;

    // :45-50
    self->axis_buttons.l_stick = (AxisButton){.current = "", .previous = ""};
    self->axis_buttons.r_stick = (AxisButton){.current = "", .previous = ""};
    self->axis_buttons.l_trig = (AxisButton){.current = "", .previous = ""};
    self->axis_buttons.r_trig = (AxisButton){.current = "", .previous = ""};

    self->axis_cursor_speed = 20; // :53

    self->button_registry = tnew(); // :56

    self->snap_cursor_to.set = false; // :59

    // :63-66
    self->cursor_context.layer = 1;
    self->cursor_context.stack = tnew();

    self->cardarea_context = tnew(); // :68

    // :71-79
    self->HID.last_type = "";
    self->HID.dpad = false;
    self->HID.pointer = true;
    self->HID.touch = false;
    self->HID.controller = false;
    self->HID.mouse = true;
    self->HID.axis_cursor = false;

    // :82-83
    self->GAMEPAD.object = NULL;
    self->GAMEPAD.mapping = NULL;
    self->GAMEPAD.name = NULL;
    self->GAMEPAD_CONSOLE = "";

    // :86 the emulated gamepad of the debug keybinds: not ported (controller.h)
    self->keyboard_controller = NULL;

    self->is_cursor_down = false; // :91
}

// controller.lua:96
void Controller_set_gamepad(Controller *self, Joystick *_gamepad) {
    if (self->GAMEPAD.object != _gamepad) {
        self->GAMEPAD.object = _gamepad;
        const char *mapping = Joystick_getGamepadMappingString(_gamepad);
        self->GAMEPAD.mapping = mapping ? mapping : "";
        // :100 mapping:match("^%x*,(.-),") or '': the hex digits, a comma, then the text up to the next comma
        const char *name = "";
        const char *p = self->GAMEPAD.mapping;
        while (isxdigit((unsigned char)*p)) p++;
        if (*p == ',') {
            const char *end = strchr(p + 1, ',');
            if (end) name = fmt("%.*s", (int)(end - (p + 1)), p + 1);
        }
        self->GAMEPAD.name = name;
        const char *temp_console = Controller_get_console_from_gamepad(self, self->GAMEPAD.name);
        if (!streq(self->GAMEPAD_CONSOLE, temp_console)) {
            self->GAMEPAD_CONSOLE = temp_console;
            for (TIter it = titer(G->I.SPRITE); tnext(&it);) {
                Sprite *v = (Sprite *)it.val.obj;
                if (v->atlas == ASSET_ATLAS_get("gamepad_ui")) {
                    v->sprite_pos.y = streq(G->CONTROLLER->GAMEPAD_CONSOLE, "Nintendo") ? 2
                                    : streq(G->CONTROLLER->GAMEPAD_CONSOLE, "Playstation") ? (G->F_PS4_PLAYSTATION_GLYPHS ? 3 : 1)
                                    : 0;
                    CALL(v, set_sprite_pos, TAB("x", N(v->sprite_pos.x), "y", N(v->sprite_pos.y)));
                }
            }
        }
        // :111 GAMEPAD.temp_console = nil: a local here
    }
}

// The Lua patterns of :119-120, by hand (core.h has no pattern matcher). A frontier %f[set] at the end of the
// string sees '\0', which is in every complemented class (%D, %W, %L, %U).
static bool pat_ps_digit(const char *s) { // "%f[%w]PS%d%f[%D]"
    for (const char *p = s; *p; p++)
        if ((p == s || !isalnum((unsigned char)p[-1])) && p[0] == 'P' && p[1] == 'S' && isdigit((unsigned char)p[2]) &&
            !isdigit((unsigned char)p[3]))
            return true;
    return false;
}
static bool pat_word_then(const char *s, const char *word, int (*not_after)(int)) { // "word%f[%X]"
    size_t n = strlen(word);
    for (const char *p = strstr(s, word); p; p = strstr(p + 1, word))
        if (!not_after((unsigned char)p[n])) return true;
    return false;
}
static bool pat_nes(const char *s, const char *nes, char opt, int (*cls)(int)) { // "%f[%u]S?NES%f[%U]" and the lower one
    for (const char *p = s; *p; p++) {
        if (!((p == s || !cls((unsigned char)p[-1])) && cls((unsigned char)*p))) continue;
        if (*p == opt && strncmp(p + 1, nes, 3) == 0 && !cls((unsigned char)p[4])) return true;
        if (strncmp(p, nes, 3) == 0 && !cls((unsigned char)p[3])) return true;
    }
    return false;
}
static bool pat_switch(const char *s) { // "%f[%u]Switch%f[%L]"
    for (const char *p = strstr(s, "Switch"); p; p = strstr(p + 1, "Switch"))
        if ((p == s || !isupper((unsigned char)p[-1])) && !islower((unsigned char)p[6])) return true;
    return false;
}
static bool pat_joycon(const char *s) { // "Joy[- ]Cons?%f[%L]"
    for (const char *p = strstr(s, "Joy"); p; p = strstr(p + 1, "Joy")) {
        if ((p[3] != '-' && p[3] != ' ') || strncmp(p + 4, "Con", 3) != 0) continue;
        const char *q = p + 7;
        if (*q == 's') q++;
        if (!islower((unsigned char)*q)) return true;
    }
    return false;
}

// controller.lua:116
const char *Controller_get_console_from_gamepad(Controller *self, const char *_gamepad) {
    // note: G.ARGS.gamepad_patterns is a constant scratch table: the patterns are code here. pairs() order of its two
    // keys is hash order in the Lua; Playstation is tested first here (it only matters for a name matching both).
    if (pat_ps_digit(_gamepad) || pat_word_then(_gamepad, "Sony", isalnum) || strstr(_gamepad, "Playstation") ||
        strstr(_gamepad, "PlayStation"))
        return "Playstation";
    if (pat_word_then(_gamepad, "Wii", islower) || pat_nes(_gamepad, "NES", 'S', isupper) ||
        pat_nes(_gamepad, "nes", 's', islower) || pat_switch(_gamepad) || pat_joycon(_gamepad))
        return "Nintendo";
    return "Xbox";
}

// controller.lua:136
void Controller_set_HID_flags(Controller *self, const char *HID_type, const char *button) {
    if (streq(HID_type, "axis")) {
        self->HID.controller = true;
        self->HID.last_type = "axis";
    } else if (HID_type && !streq(HID_type, self->HID.last_type)) {
        self->HID.dpad = streq(HID_type, "button");
        self->HID.pointer = streq(HID_type, "mouse") || streq(HID_type, "axis_cursor") || streq(HID_type, "touch");
        self->HID.controller = streq(HID_type, "button") || streq(HID_type, "axis_cursor");
        self->HID.mouse = streq(HID_type, "mouse");
        self->HID.touch = streq(HID_type, "touch");
        self->HID.axis_cursor = streq(HID_type, "axis_cursor");
        self->HID.last_type = str(HID_type);

        if (self->HID.mouse) {
            love_mouse_setVisible(true);
        } else {
            love_mouse_setVisible(false);
        }
    }
    if (!self->HID.controller) {
        self->GAMEPAD_CONSOLE = "";
        self->GAMEPAD.object = NULL;
        self->GAMEPAD.mapping = NULL;
        self->GAMEPAD.name = NULL;
    }
}

// controller.lua:166
void Controller_set_cursor_position(Controller *self) {
    if (self->HID.mouse || self->HID.touch) {
        self->interrupt.focus = false;
        if (self->focused.target) {
            self->focused.target->states.focus.is = false;
            self->focused.target = NULL;
        }

        love_mouse_getPosition(&self->cursor_position.x, &self->cursor_position.y);
        G->CURSOR->moveable.node.T->x = self->cursor_position.x / (G->TILESCALE * G->TILESIZE);
        G->CURSOR->moveable.node.T->y = self->cursor_position.y / (G->TILESCALE * G->TILESIZE);
        G->CURSOR->moveable.VT.x = G->CURSOR->moveable.node.T->x;
        G->CURSOR->moveable.VT.y = G->CURSOR->moveable.node.T->y;
    }
}

// controller.lua:206 (E1)
static bool ev_unlock_frame(void *ctx) {
    Controller *self = ctx;
    tset(self->locks, "frame", NIL);
    return true;
}

// controller.lua:405 (E2)
struct ev_touch_hover_ctx { Controller *self; int _ID; };
static bool ev_touch_hover(void *ctx) {
    struct ev_touch_hover_ctx *c = ctx;
    Controller *self = c->self;
    if (self->hovering.target && c->_ID == self->hovering.target->ID) {
        CALL(self->hovering.target, hover);
    }
    return true;
}

// controller.lua:186
void Controller_update(Controller *self, float dt) {
    // :189 parse all locks and set
    self->locked = false;
    if (G->screenwipe) tsetb(self->locks, "wipe", true); else tsetb(self->locks, "wipe", false);

    for (TIter it = titer(self->locks); tnext(&it);) {
        if (truthy(it.val)) self->locked = true;
    }

    if (tbool(self->locks, "frame_set")) {
        tset(self->locks, "frame_set", NIL);
        self->overlay_timer = 0;
        E_add(G->E_MANAGER, Event_new((EventCfg){
            .trigger = "after",
            .delay = 0.1,
            .timer = "UPTIME",
            .not_blocking = true,
            .not_blockable = true,
            .no_delete = true,
            .func = ev_unlock_frame, .ctx = self}), NULL, false);
    }

    // :213 overlay_timer or 0: nil is 0 (controller.h)
    if (G->OVERLAY_MENU) {
        self->overlay_timer = self->overlay_timer + dt;
    } else {
        self->overlay_timer = 0;
    }

    if (self->overlay_timer > 1.5) tset(self->locks, "frame", NIL);

    // :223 Remove anything from the registry that is no longer in game
    Controller_cull_registry(self);

    // :226 Calculate the axis update and set the HID flags if there is any axis input
    Controller_set_HID_flags(self, Controller_update_axis(self, dt), NULL);

    // :229
    if (self->HID.pointer && !(self->HID.mouse || self->HID.touch) && !self->interrupt.focus) {
        G->CURSOR->moveable.node.states.visible = true;
    } else {
        G->CURSOR->moveable.node.states.visible = false;
    }

    // :236
    Controller_set_cursor_position(self);

    // :239 Handle all the button updates and key updates
    // note: pairs() over these maps is hash order in the Lua; titer walks them in insertion order.
    if (!G->screenwipe) {
        for (TIter it = titer(self->pressed_keys); tnext(&it);) {
            if (truthy(it.val)) Controller_key_press_update(self, vstr(it.key), dt);
        }
        for (TIter it = titer(self->held_keys); tnext(&it);) {
            if (truthy(it.val)) Controller_key_hold_update(self, vstr(it.key), dt);
        }
        for (TIter it = titer(self->released_keys); tnext(&it);) {
            if (truthy(it.val)) Controller_key_release_update(self, vstr(it.key), dt);
        }

        for (TIter it = titer(self->pressed_buttons); tnext(&it);) {
            if (truthy(it.val)) Controller_button_press_update(self, vstr(it.key), dt);
        }
        for (TIter it = titer(self->held_buttons); tnext(&it);) {
            if (truthy(it.val)) Controller_button_hold_update(self, vstr(it.key), dt);
        }
        for (TIter it = titer(self->released_buttons); tnext(&it);) {
            if (truthy(it.val)) Controller_button_release_update(self, vstr(it.key), dt);
        }
    }

    self->frame_buttonpress = false;

    // :266 reset press and release lists
    self->pressed_keys = EMPTY(self->pressed_keys);
    self->released_keys = EMPTY(self->released_keys);
    self->pressed_buttons = EMPTY(self->pressed_buttons);
    self->released_buttons = EMPTY(self->released_buttons);

    // :273 If using controller, update context and snap tos
    if (self->HID.controller) {
        // :275 If there is a node/position to snap to from the cursor context layer
        if (truthy(tgeti(self->cursor_context.stack, self->cursor_context.layer))) {
            Table *_context = ttabi(self->cursor_context.stack, self->cursor_context.layer);
            Node *cnode = (Node *)tobj(_context, "node");
            // (_context.node and not _context.node.REMOVED and _context.node): nil/false is NULL
            Controller_snap_to(self, TAB("node", O(cnode && !cnode->REMOVED ? cnode : NULL), "T", tget(_context, "cursor_pos")));
            self->interrupt.stack = tbool(_context, "interrupt");
            tseti(self->cursor_context.stack, self->cursor_context.layer, NIL);
        }
        // :282 If there is a card the was being dragged but no longer is, snap to it
        // note: getmetatable(x) == Card is the exact class; Jimbo's card keeps Card as its metatable in the Lua (its
        // hover is replaced on the instance), so its C class Card_Character_card_class counts too.
        if (self->dragging.prev_target && !self->dragging.target &&
            (self->dragging.prev_target->object.cls == &Card_class ||
             self->dragging.prev_target->object.cls == &Card_Character_card_class) &&
            !self->dragging.prev_target->REMOVED) {
            if (!self->COYOTE_FOCUS) {
                Controller_snap_to(self, TAB("node", O(self->dragging.prev_target)));
            } else {
                self->COYOTE_FOCUS = false;
            }
        }
        // :291 If the cursor should snap to a location
        if (self->snap_cursor_to.set) {
            self->interrupt.focus = self->interrupt.stack;
            self->interrupt.stack = false;
            if (streq(self->snap_cursor_to.type, "node") && !self->snap_cursor_to.node->REMOVED) {
                self->focused.prev_target = self->focused.target;
                self->focused.target = self->snap_cursor_to.node;
                Controller_update_cursor(self, NULL);
            } else if (streq(self->snap_cursor_to.type, "transform")) {
                Controller_update_cursor(self, self->snap_cursor_to.has_T ? &self->snap_cursor_to.T : NULL);
            }
            if (self->focused.prev_target != self->focused.target && self->focused.prev_target) self->focused.prev_target->states.focus.is = false;
            self->snap_cursor_to.set = false;
        }
    }

    // :307 Reset all collision states, get every node that collides with the cursor, then update the focus and hover targets
    Controller_get_cursor_collision(self, (Vec2){G->CURSOR->moveable.node.T->x, G->CURSOR->moveable.node.T->y});
    Controller_update_focus(self, NULL);
    Controller_set_cursor_hover(self);
    if (self->L_cursor_queue.set) {
        Controller_L_cursor_press(self, self->L_cursor_queue.x, self->L_cursor_queue.y);
        self->L_cursor_queue.set = false;
    }

    self->dragging.prev_target = self->dragging.target;
    self->released_on.prev_target = self->released_on.target;
    self->clicked.prev_target = self->clicked.target;
    self->hovering.prev_target = self->hovering.target;

    // :321 Cursor is currently down
    if (!self->cursor_down.handled) {
        if (self->cursor_down.target->states.drag.can) {
            self->cursor_down.target->states.drag.is = true;
            Node_set_offset(self->cursor_down.target, self->cursor_down.T, "Click");
            self->dragging.target = self->cursor_down.target;
            self->dragging.handled = false;
        }
        self->cursor_down.handled = true;
    }

    if (!self->cursor_up.handled) {
        // :333 First, stop dragging
        if (self->dragging.target) {
            CALL(self->dragging.target, stop_drag);
            self->dragging.target->states.drag.is = false;
            self->dragging.target = NULL;
        }
        // :340 Was the Cursor release in the same location as the Cursor press and within Cursor timeout?
        if (self->cursor_down.target) {
            Val click_timeout = node_click_timeout(self->cursor_down.target);
            if (!truthy(click_timeout) || vnum(click_timeout) * G->SPEEDFACTOR > self->cursor_up.time - self->cursor_down.time) {
                if (Vector_Dist(self->cursor_down.T, self->cursor_up.T) < G->MIN_CLICK_DIST) {
                    if (self->cursor_down.target->states.click.can) {
                        self->clicked.target = self->cursor_down.target;
                        self->clicked.handled = false;
                    }
                // :348 if not, was the Cursor dragging some other thing?
                } else if (self->dragging.prev_target && self->cursor_up.target && self->cursor_up.target->states.release_on.can) {
                    self->released_on.target = self->cursor_up.target;
                    self->released_on.handled = false;
                }
            }
        }
        self->cursor_up.handled = true;
    }

    // :358 Cursor is currently hovering over something
    if (self->cursor_hover.target && self->cursor_hover.target->states.hover.can && (!self->HID.touch || self->is_cursor_down)) {
        self->hovering.target = self->cursor_hover.target;
        if (self->hovering.prev_target && self->hovering.prev_target != self->hovering.target) self->hovering.prev_target->states.hover.is = false;
        self->hovering.target->states.hover.is = true;
        Node_set_offset(self->hovering.target, self->cursor_hover.T, "Hover");
    } else if ((self->cursor_hover.target == NULL || (self->HID.touch && !self->is_cursor_down)) && self->hovering.target) {
        self->hovering.target->states.hover.is = false;
        self->hovering.target = NULL;
    }

    // -------- Sending all input updates to the game objects
    // :372 The clicked object
    if (!self->clicked.handled) {
        CALL(self->clicked.target, click);
        self->clicked.handled = true;
    }

    // :378 Process registry clicks
    Controller_process_registry(self);

    // :381 The object being dragged
    if (self->dragging.target) {
        CALL(self->dragging.target, drag, NULL);
    }

    // :386 The object released on
    if (!self->released_on.handled && self->dragging.prev_target) {
        if (self->dragging.prev_target == self->hovering.target) { CALL(self->hovering.target, stop_hover); self->hovering.target = NULL; }
        CALL(self->released_on.target, release, self->dragging.prev_target);
        self->released_on.handled = true;
    }

    // :393 The object being hovered over
    if (self->hovering.target) {
        Node_set_offset(self->hovering.target, self->cursor_hover.T, "Hover");
        if (self->hovering.prev_target != self->hovering.target) {
            if (self->hovering.target != self->dragging.target && !self->HID.touch) {
                CALL(self->hovering.target, hover);
            } else if (self->HID.touch) {
                struct ev_touch_hover_ctx *c = NEWCTX(struct ev_touch_hover_ctx);
                c->self = self;
                c->_ID = self->hovering.target->ID;
                E_add(G->E_MANAGER, Event_new((EventCfg){
                    .trigger = "after",
                    .not_blockable = true,
                    .not_blocking = true,
                    .delay = G->MIN_HOVER_TIME,
                    .func = ev_touch_hover, .ctx = c}), NULL, false);
                if (self->hovering.prev_target) {
                    CALL(self->hovering.prev_target, stop_hover);
                }
            }
            if (self->hovering.prev_target) {
                CALL(self->hovering.prev_target, stop_hover);
            }
        }
    } else if (self->hovering.prev_target) {
        CALL(self->hovering.prev_target, stop_hover);
    }
    if (self->hovering.target && self->hovering.target == self->dragging.target && !self->HID.touch) {
        CALL(self->hovering.target, stop_hover);
    }
}

// controller.lua:429
void Controller_cull_registry(Controller *self) {
    for (TIter it = titer(self->button_registry); tnext(&it);) {
        Table *registry = it.val.tab;
        for (int i = tlen(registry); i >= 1; i--) {
            if (((Node *)tobj(ttabi(registry, i), "node"))->REMOVED) {
                tremove(registry, i);
            }
        }
    }
}

// controller.lua:443
void Controller_add_to_registry(Controller *self, Node *node, const char *registry) {
    if (!truthy(tget(self->button_registry, registry))) tsett(self->button_registry, registry, tnew());

    tinsert_at(ttab(self->button_registry, registry), 1,
               T(TAB("node", O(node), "menu", B((G->OVERLAY_MENU != NULL) || tbool(G->SETTINGS, "paused")))));
}

// controller.lua:453
void Controller_process_registry(Controller *self) {
    for (TIter it = titer(self->button_registry); tnext(&it);) {
        Table *registry = it.val.tab;
        for (int i = 1, n = tlen(registry); i <= n; i++) {
            Table *entry = ttabi(registry, i);
            Node *node = (Node *)tobj(entry, "node");
            // note: `registry[i].node.click` (the method exists) is always true: every Class has click
            if (tbool(entry, "click") && node) {
                if (tbool(entry, "menu") == (G->OVERLAY_MENU != NULL) &&
                    node->T->x > -2 && node->T->x < G->ROOM->T->w + 2 &&
                    node->T->y > -2 && node->T->y < G->ROOM->T->h + 2) {
                    CALL(node, click);
                }
                tset(entry, "click", NIL);
            }
        }
    }
}

// controller.lua:472
void Controller_mod_cursor_context_layer(Controller *self, int delta) {
    if (delta == 1) {
        // :475 Add a layer to the context
        Table *prev_cursor_context = TAB("node", O(self->focused.target),
                                         "cursor_pos", T(TAB("x", N(G->CURSOR->moveable.node.T->x), "y", N(G->CURSOR->moveable.node.T->y))),
                                         "interrupt", B(self->interrupt.focus));
        tseti(self->cursor_context.stack, self->cursor_context.layer, T(prev_cursor_context));
        self->cursor_context.layer = self->cursor_context.layer + 1;
    } else if (delta == -1) {
        // :480 remove the top layer from the stack
        tseti(self->cursor_context.stack, self->cursor_context.layer, NIL);
        self->cursor_context.layer = self->cursor_context.layer - 1;
    } else if (delta == -1000) {
        // :485 remove all but the base layer from the stack
        self->cursor_context.layer = 1;
        Table *stack = tnew();
        tseti(stack, 1, tgeti(self->cursor_context.stack, 1));
        self->cursor_context.stack = stack;
    } else if (delta == -2000) {
        // :490 remove all layers
        self->cursor_context.layer = 1;
        self->cursor_context.stack = tnew();
    }

    // :496 Navigate focus, will default to the top layer on the stack
    Controller_navigate_focus(self, NULL);
}

// controller.lua:500
void Controller_snap_to(Controller *self, Table *args) {
    // note: T is copied (the Lua keeps the caller's table; nobody changes it before the next update)
    Table *T_ = ttab(args, "T");
    self->snap_cursor_to.set = true;
    self->snap_cursor_to.node = (Node *)tobj(args, "node");
    self->snap_cursor_to.has_T = T_ != NULL;
    self->snap_cursor_to.T = (Vec2){(float)tnum(T_, "x"), (float)tnum(T_, "y")};
    self->snap_cursor_to.type = self->snap_cursor_to.node ? "node" : "transform";
}

// controller.lua:505
bool Controller_save_cardarea_focus(Controller *self, const char *_cardarea) {
    CardArea **ca = Game_cardarea(G, _cardarea);
    if (ca && *ca) {
        if (self->focused.target && node_area(self->focused.target) && node_area(self->focused.target) == *ca) {
            tset(self->cardarea_context, _cardarea, node_rank(self->focused.target));
            return true;
        } else {
            tset(self->cardarea_context, _cardarea, NIL);
        }
    }
    return false;
}

// controller.lua:517
void Controller_recall_cardarea_focus(Controller *self, Val _cardarea) {
    const char *ca_string = NULL;
    CardArea *area = _cardarea.t == V_OBJ ? (CardArea *)_cardarea.obj : NULL;
    if (_cardarea.t == V_STR) {
        ca_string = _cardarea.s;
        CardArea **ca = Game_cardarea(G, ca_string);
        area = ca ? *ca : NULL;
    }

    if (area && (!self->focused.target ||
        self->interrupt.focus ||
        (!self->interrupt.focus && node_area(self->focused.target) && node_area(self->focused.target) == area))) {
        if (ca_string && truthy(tget(self->cardarea_context, ca_string))) {
            for (int i = (int)tnum(self->cardarea_context, ca_string); i >= 1; i--) {
                if (CardArea_card(area, i)) {
                    Controller_snap_to(self, TAB("node", O(CardArea_card(area, i))));
                    self->interrupt.focus = false;
                    break;
                }
            }
        } else if (area->cards && CardArea_card(area, 1)) {
            Controller_snap_to(self, TAB("node", O(CardArea_card(area, 1))));
            self->interrupt.focus = false;
        }
    }
    if (ca_string) tset(self->cardarea_context, ca_string, NIL);
}

// controller.lua:541
void Controller_update_cursor(Controller *self, const Vec2 *hard_set_T) {
    if (hard_set_T) {
        G->CURSOR->moveable.node.T->x = hard_set_T->x;
        G->CURSOR->moveable.node.T->y = hard_set_T->y;
        self->cursor_position.x = G->CURSOR->moveable.node.T->x * (G->TILESCALE * G->TILESIZE);
        self->cursor_position.y = G->CURSOR->moveable.node.T->y * (G->TILESCALE * G->TILESIZE);
        G->CURSOR->moveable.VT.x = G->CURSOR->moveable.node.T->x;
        G->CURSOR->moveable.VT.y = G->CURSOR->moveable.node.T->y;
        return;
    }
    if (self->focused.target) {
        // note: a put_focused_cursor that returns nothing (a tab row with no chosen tab) is an arithmetic error in
        // the Lua; here the position keeps its value if the callee does not write it.
        CALL(self->focused.target, put_focused_cursor, &self->cursor_position.x, &self->cursor_position.y);
        G->CURSOR->moveable.node.T->x = self->cursor_position.x / (G->TILESCALE * G->TILESIZE);
        G->CURSOR->moveable.node.T->y = self->cursor_position.y / (G->TILESCALE * G->TILESIZE);
        G->CURSOR->moveable.VT.x = G->CURSOR->moveable.node.T->x;
        G->CURSOR->moveable.VT.y = G->CURSOR->moveable.node.T->y;
    }
}

// controller.lua:561 (the Lua uses G.CONTROLLER, not self)
void Controller_handle_axis_buttons(Controller *self) {
    // note: pairs() order of the four entries is hash order in the Lua; struct order here (controller.h)
    AxisButton *all[4] = {&G->CONTROLLER->axis_buttons.l_stick, &G->CONTROLLER->axis_buttons.r_stick,
                          &G->CONTROLLER->axis_buttons.l_trig, &G->CONTROLLER->axis_buttons.r_trig};
    for (int i = 0; i < 4; i++) {
        AxisButton *v = all[i];
        // :564 Button is no longer being pressed
        if (!streq(v->previous, "") && (streq(v->current, "") || !streq(v->previous, v->current))) {
            Controller_button_release(G->CONTROLLER, v->previous);
        }
        // :568 New button is being pressed
        if (!streq(v->current, "") && !streq(v->previous, v->current)) {
            Controller_button_press(G->CONTROLLER, v->current);
        }
    }
}
