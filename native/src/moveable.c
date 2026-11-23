// engine/moveable.lua:1-517: Moveable, a Node whose visible transform VT eases towards its transform T.
#include "all.h"

// moveable.lua:2 Moveable = Node:extend()
// note: the entries left NULL are inherited from Node by class_init. The casts only change self's pointer type.
Class Moveable_class = {
    .name = "Moveable",
    .super = &Node_class,
    .move = (void (*)(void *, float))Moveable_move,
    .draw = (void (*)(void *))Moveable_draw,
    .remove = (void (*)(void *))Moveable_remove,
    .drag = (void (*)(void *, const Vec2 *))Moveable_drag,
    .hard_set_T = (void (*)(void *, float, float, float, float))Moveable_hard_set_T,
    .hard_set_VT = (void (*)(void *))Moveable_hard_set_VT,
    .juice_up = (void (*)(void *, float, Val))Moveable_juice_up,
};

// node.h: the role and bond strings of the Lua ('Major', 'Minor', 'Glued'; 'Strong', 'Weak')
// note: the game uses no other string; an unknown one reads as 'Major' / 'Strong'.
RoleType role_type_of(const char *name) {
    if (streq(name, "Minor")) return ROLE_MINOR;
    if (streq(name, "Glued")) return ROLE_GLUED;
    return ROLE_MAJOR;
}
Bond bond_of(const char *name) { return streq(name, "Weak") ? BOND_WEAK : BOND_STRONG; }
static const char *bond_name(Bond b) { return b == BOND_WEAK ? "Weak" : "Strong"; }

// moveable.lua:107, :466 `if args.offset and (type(args.offset)=='table' and not (args.offset.y and args.offset.x))
// or type(args.offset) ~= 'table' then args.offset = nil end` (it writes the caller's table, as the Lua)
static void args_check_offset(Table *args) {
    Val off = tget(args, "offset");
    if ((truthy(off) && (off.t == V_TAB && !(truthy(tget(off.tab, "y")) && truthy(tget(off.tab, "x"))))) || off.t != V_TAB) {
        tset(args, "offset", NIL);
    }
}

// moveable.lua:17-80, what follows Node.init
static void moveable_init_rest(Moveable *self) {
    Transform *T = self->node.T;
    self->VT = (Transform){.x = T->x, .y = T->y, .w = T->w, .h = T->h, .r = T->r, .scale = T->scale};

    self->velocity.x = 0, self->velocity.y = 0, self->velocity.r = 0, self->velocity.scale = 0, self->velocity.mag = 0;

    self->role.role_type = ROLE_MAJOR;
    self->role.offset = (Vec2){0, 0};
    self->role.major = NULL;
    self->role.draw_major = self;
    self->role.xy_bond = BOND_STRONG;
    self->role.wh_bond = BOND_STRONG;
    self->role.r_bond = BOND_STRONG;
    self->role.scale_bond = BOND_STRONG;

    self->alignment.type = "a";
    self->alignment.offset = (Vec2){0, 0};
    self->alignment.prev_type = "";
    self->alignment.prev_offset = (Vec2){0, 0};

    self->pinch.x = false, self->pinch.y = false;

    self->last_moved = -1;
    self->last_aligned = -1;

    self->static_rotation = false;

    self->offset = (Vec2){0, 0};
    self->Mid = self;

    self->shadow_parrallax = (Vec2){0, -1.5f};
    self->layered_parallax = (Vec2){0, 0};
    self->shadow_height = 0.2f;

    Moveable_calculate_parrallax(self);

    tinsert(G->MOVEABLES, O(self));
    if (self->node.object.cls == &Moveable_class) { // getmetatable(self) == Moveable
        tinsert(G->I.MOVEABLE, O(self));
    }
}

// moveable.lua:13 Moveable(X, Y, W, H)
void Moveable_init(Moveable *self, float X, float Y, float W, float H) {
    Table *args = TAB("T", T(ARR(N(X), N(Y), N(W), N(H))));
    Node_init(&self->node, args);
    moveable_init_rest(self);
}

// moveable.lua:13 Moveable(args): type(X) == 'table'
// note: args NULL is the Lua's Moveable() with no argument: T = {0, 0, 0, 0}.
void Moveable_init_args(Moveable *self, Table *args) {
    if (!args) {
        Moveable_init(self, 0, 0, 0, 0);
        return;
    }
    Node_init(&self->node, args);
    moveable_init_rest(self);
}

// moveable.lua:82
void Moveable_draw(Moveable *self) {
    Node_draw(&self->node);
    Node_draw_boundingrect(&self->node);
}

// moveable.lua:94
void Moveable_set_alignment(Moveable *self, Table *args) {
    if (!args) args = tnew();
    if (truthy(tget(args, "major"))) {
        Val xy_bond = tget(args, "bond");
        if (!truthy(xy_bond)) xy_bond = tget(args, "xy_bond");
        if (!truthy(xy_bond)) xy_bond = S("Weak");
        Val wh_bond = tget(args, "wh_bond");
        if (!truthy(wh_bond)) wh_bond = S(bond_name(self->role.wh_bond));
        Val r_bond = tget(args, "r_bond");
        if (!truthy(r_bond)) r_bond = S(bond_name(self->role.r_bond));
        Val scale_bond = tget(args, "scale_bond");
        if (!truthy(scale_bond)) scale_bond = S(bond_name(self->role.scale_bond));
        Moveable_set_role(self, TAB("role_type", S("Minor"),
                                    "major", tget(args, "major"),
                                    "xy_bond", xy_bond,
                                    "wh_bond", wh_bond,
                                    "r_bond", r_bond,
                                    "scale_bond", scale_bond));
    }
    if (truthy(tget(args, "type"))) self->alignment.type = str(tstr(args, "type"));
    args_check_offset(args);
    // note: the Lua keeps the caller's offset table by reference; alignment.offset is a Vec2, so it is copied here
    // (a caller that later changes its own table must write alignment.offset too).
    Table *offset = ttab(args, "offset");
    if (offset) self->alignment.offset = (Vec2){(float)tnum(offset, "x"), (float)tnum(offset, "y")};
    self->alignment.lr_clamp = tbool(args, "lr_clamp");
}

// moveable.lua:114
void Moveable_align_to_major(Moveable *self) {
    if (!streq(self->alignment.type, self->alignment.prev_type)) {
        const char *type = self->alignment.type;
        self->alignment.type_list.a = streq(type, "a");
        self->alignment.type_list.m = strchr(type, 'm') != NULL;
        self->alignment.type_list.c = strchr(type, 'c') != NULL;
        self->alignment.type_list.b = strchr(type, 'b') != NULL;
        self->alignment.type_list.t = strchr(type, 't') != NULL;
        self->alignment.type_list.l = strchr(type, 'l') != NULL;
        self->alignment.type_list.r = strchr(type, 'r') != NULL;
        self->alignment.type_list.i = strchr(type, 'i') != NULL;
    }

    if (self->alignment.prev_offset.x == self->alignment.offset.x &&
        self->alignment.prev_offset.y == self->alignment.offset.y &&
        streq(self->alignment.prev_type, self->alignment.type)) return;

    self->NEW_ALIGNMENT = true;

    if (!streq(self->alignment.type, self->alignment.prev_type)) {
        self->alignment.prev_type = self->alignment.type;
    }

    if (self->alignment.type_list.a || !self->role.major) return;

    Transform *T = self->node.T;
    if (self->alignment.type_list.m) {
        self->role.offset.x = 0.5f * self->role.major->node.T->w - (self->Mid->node.T->w) / 2 + self->alignment.offset.x - self->Mid->node.T->x + T->x;
    }

    if (self->alignment.type_list.c) {
        self->role.offset.y = 0.5f * self->role.major->node.T->h - (self->Mid->node.T->h) / 2 + self->alignment.offset.y - self->Mid->node.T->y + T->y;
    }

    if (self->alignment.type_list.b) {
        if (self->alignment.type_list.i) {
            self->role.offset.y = self->alignment.offset.y + self->role.major->node.T->h - T->h;
        } else {
            self->role.offset.y = self->alignment.offset.y + self->role.major->node.T->h;
        }
    }

    if (self->alignment.type_list.r) {
        if (self->alignment.type_list.i) {
            self->role.offset.x = self->alignment.offset.x + self->role.major->node.T->w - T->w;
        } else {
            self->role.offset.x = self->alignment.offset.x + self->role.major->node.T->w;
        }
    }

    if (self->alignment.type_list.t) {
        if (self->alignment.type_list.i) {
            self->role.offset.y = self->alignment.offset.y;
        } else {
            self->role.offset.y = self->alignment.offset.y - T->h;
        }
    }

    if (self->alignment.type_list.l) {
        if (self->alignment.type_list.i) {
            self->role.offset.x = self->alignment.offset.x;
        } else {
            self->role.offset.x = self->alignment.offset.x - T->w;
        }
    }

    // moveable.lua:180-181 `role.offset.x = role.offset.x or 0`: a Vec2 is never nil

    T->x = self->role.major->node.T->x + self->role.offset.x;
    T->y = self->role.major->node.T->y + self->role.offset.y;

    self->alignment.prev_offset.x = self->alignment.offset.x, self->alignment.prev_offset.y = self->alignment.offset.y;
}

// moveable.lua:190
// note: the arguments are never nil here (a class whose hard_set_T takes nil, Card or CardArea, resolves it first).
void Moveable_hard_set_T(Moveable *self, float X, float Y, float W, float H) {
    self->node.T->x = X;
    self->node.T->y = Y;
    self->node.T->w = W;
    self->node.T->h = H;

    self->velocity.x = 0;
    self->velocity.y = 0;
    self->velocity.r = 0;
    self->velocity.scale = 0;

    self->VT.x = X;
    self->VT.y = Y;
    self->VT.w = W;
    self->VT.h = H;
    self->VT.r = self->node.T->r;
    self->VT.scale = self->node.T->scale;
    Moveable_calculate_parrallax(self);
}

// moveable.lua:210
void Moveable_hard_set_VT(Moveable *self) {
    self->VT.x = self->node.T->x;
    self->VT.y = self->node.T->y;
    self->VT.w = self->node.T->w;
    self->VT.h = self->node.T->h;
}

// moveable.lua:217
void Moveable_drag(Moveable *self, const Vec2 *offset) {
    if (self->node.states.drag.can || offset) {
        Vec2 _p, _t; // self.ARGS.drag_cursor_trans, self.ARGS.drag_translation
        _p.x = G->CONTROLLER->cursor_position.x / (G->TILESCALE * G->TILESIZE);
        _p.y = G->CONTROLLER->cursor_position.y / (G->TILESCALE * G->TILESIZE);

        Transform *CT = self->node.container->T;
        _t.x = -CT->w / 2, _t.y = -CT->h / 2;
        point_translate(&_p, _t);

        point_rotate(&_p, CT->r);

        _t.x = CT->w / 2 - CT->x, _t.y = CT->h / 2 - CT->y;
        point_translate(&_p, _t);

        if (!offset) {
            offset = &self->node.click_offset;
        }

        self->node.T->x = _p.x - offset->x;
        self->node.T->y = _p.y - offset->y;
        self->NEW_ALIGNMENT = true;
        // note: pairs(self.children): the array part, then the named children in insertion order
        for (TIter it = titer(self->node.children); tnext(&it);) {
            CALL(it.val.obj, drag, offset);
        }
    }
    if (self->node.states.drag.can) {
        Node_drag(&self->node);
    }
}

// moveable.lua:250 amount < 0: the default 0.4; rot_amt NIL (or false): a random sign
void Moveable_juice_up(Moveable *self, float amount, Val rot_amt) {
    if (SETTINGS_snap()->reduced_motion) return;
    if (amount < 0) amount = 0.4f;

    float end_time = (float)G->TIMERS.REAL + 0.4f;
    float start_time = (float)G->TIMERS.REAL;
    Val r_amt = rot_amt;
    if (!truthy(r_amt)) r_amt = pseudorandom_element(ARR(N(0.6f * amount), N(-0.6f * amount)), NIL, NULL);
    self->juice.on = true;
    self->juice.handled_elsewhere = false; // a new table
    self->juice.scale = 0;
    self->juice.scale_amt = amount;
    self->juice.r = 0;
    self->juice.r_amt = truthy(r_amt) ? (float)vnum(r_amt) : 0;
    self->juice.start_time = start_time;
    self->juice.end_time = end_time;
    self->VT.scale = 1 - 0.6f * amount;
}

// moveable.lua:267
void Moveable_move_juice(Moveable *self, float dt) {
    if (self->juice.on && !self->juice.handled_elsewhere) {
        float REAL = (float)G->TIMERS.REAL; // (the juice is float: node.h)
        if (self->juice.end_time < REAL) {
            memset(&self->juice, 0, sizeof self->juice); // self.juice = nil
        } else {
            self->juice.scale = self->juice.scale_amt * sinf(50.8f * (REAL - self->juice.start_time)) * fmaxf(0, powf((self->juice.end_time - REAL) / (self->juice.end_time - self->juice.start_time), 3));
            self->juice.r = self->juice.r_amt * sinf(40.8f * (REAL - self->juice.start_time)) * fmaxf(0, powf((self->juice.end_time - REAL) / (self->juice.end_time - self->juice.start_time), 2));
        }
    }
}

// node.h Moveable_refresh_movement: reads config.refresh_movement again
void Moveable_cfg_snap_fill(Moveable *self) {
    Val v = tget(self->node.config, "refresh_movement");
    self->cfg_snap.refresh_movement = truthy(v);
    self->cfg_snap.is_true = v.t == V_BOOL && v.b;
    self->cfg_snap.config = self->node.config, self->cfg_snap.stamp = tstamp(self->node.config);
}

// moveable.lua:278
void Moveable_move(Moveable *self, float dt) {
    if (self->node.FRAME.MOVE >= G->FRAMES.MOVE) return;
    // FRAME.OLD_MAJOR = FRAME.MAJOR (the Lua recycles the table); FRAME.MAJOR = nil
    self->frame_major.have = false;
    self->node.FRAME.MOVE = G->FRAMES.MOVE;
    if (!self->node.created_on_pause && SETTINGS_snap()->paused) return;

    Moveable_align_to_major(self);

    self->node.CALCING = false;
    if (self->role.role_type == ROLE_GLUED) {
        if (self->role.major) Moveable_glue_to_major(self, self->role.major);
    } else if (self->role.role_type == ROLE_MINOR && self->role.major) {
        if (self->role.major->node.FRAME.MOVE < G->FRAMES.MOVE) CALL(self->role.major, move, dt);
        self->STATIONARY = self->role.major->STATIONARY;
        if ((!self->STATIONARY) || self->NEW_ALIGNMENT ||
            Moveable_refresh_movement(self) ||
            self->juice.on ||
            self->role.xy_bond == BOND_WEAK ||
            self->role.r_bond == BOND_WEAK) {
            self->node.CALCING = true;
            Moveable_move_with_major(self, dt);
        }
    } else if (self->role.role_type == ROLE_MAJOR) {
        self->STATIONARY = true;
        Moveable_move_juice(self, dt);
        Moveable_move_xy(self, dt);
        Moveable_move_r(self, dt, self->velocity.x);
        Moveable_move_scale(self, dt);
        Moveable_move_wh(self, dt);
        Moveable_calculate_parrallax(self);
    }
    if (self->alignment.lr_clamp) {
        Moveable_lr_clamp(self);
    }

    self->NEW_ALIGNMENT = false;
}

// moveable.lua:322
void Moveable_lr_clamp(Moveable *self) {
    Transform *T = self->node.T;
    if (T->x < 0) T->x = 0;
    if (self->VT.x < 0) self->VT.x = 0;
    if ((T->x + T->w) > G->ROOM->T->w) T->x = G->ROOM->T->w - T->w;
    if ((self->VT.x + self->VT.w) > G->ROOM->T->w) self->VT.x = G->ROOM->T->w - self->VT.w;
}

// moveable.lua:329
void Moveable_glue_to_major(Moveable *self, Moveable *major_tab) {
    self->node.T = major_tab->node.T; // the same table from now on

    self->VT.x = major_tab->VT.x + (0.5f * (1 - major_tab->VT.w / (major_tab->node.T->w)) * self->node.T->w);
    self->VT.y = major_tab->VT.y;
    self->VT.w = major_tab->VT.w;
    self->VT.h = major_tab->VT.h;
    self->VT.r = major_tab->VT.r;
    self->VT.scale = major_tab->VT.scale;

    // note: the Lua shares the major's pinch and shadow_parrallax tables; they are values here, copied at every
    // glue (once a frame from Moveable_move), so a change of the major's is seen at the glued node's next move.
    self->pinch.x = major_tab->pinch.x, self->pinch.y = major_tab->pinch.y;
    self->shadow_parrallax = major_tab->shadow_parrallax;
}

// moveable.lua:343 MWM: the global scratch of move_with_major. note: every field is written before it is read in
// each call and nothing else uses it, so it is locals of Moveable_move_with_major.

// moveable.lua:350
void Moveable_move_with_major(Moveable *self, float dt) {
    if (self->role.role_type != ROLE_MINOR) return;
    MajorTab major_tab = Moveable_get_major(self->role.major);
    Transform *T = self->node.T;
    Vec2 rotated_offset;

    Moveable_move_juice(self, dt);

    if (self->role.r_bond == BOND_WEAK) {
        rotated_offset.x = self->role.offset.x + major_tab.offset.x, rotated_offset.y = self->role.offset.y + major_tab.offset.y;
    } else {
        if (major_tab.major->VT.r < 0.0001f && major_tab.major->VT.r > -0.0001f) {
            rotated_offset.x = self->role.offset.x + major_tab.offset.x;
            rotated_offset.y = self->role.offset.y + major_tab.offset.y;
        } else {
            float angles_cos = cosf(major_tab.major->VT.r), angles_sin = sinf(major_tab.major->VT.r);
            float WH_w = -T->w / 2 + major_tab.major->node.T->w / 2, WH_h = -T->h / 2 + major_tab.major->node.T->h / 2;
            float offs_x = self->role.offset.x + major_tab.offset.x - WH_w, offs_y = self->role.offset.y + major_tab.offset.y - WH_h;

            rotated_offset.x = offs_x * angles_cos - offs_y * angles_sin + WH_w;
            rotated_offset.y = offs_x * angles_sin + offs_y * angles_cos + WH_h;
        }
    }

    T->x = major_tab.major->node.T->x + rotated_offset.x;
    T->y = major_tab.major->node.T->y + rotated_offset.y;

    if (self->role.xy_bond == BOND_STRONG) {
        self->VT.x = major_tab.major->VT.x + rotated_offset.x;
        self->VT.y = major_tab.major->VT.y + rotated_offset.y;
    } else if (self->role.xy_bond == BOND_WEAK) {
        Moveable_move_xy(self, dt);
    }

    if (self->role.r_bond == BOND_STRONG) {
        self->VT.r = T->r + major_tab.major->VT.r + (self->juice.on ? self->juice.r : 0);
    } else if (self->role.r_bond == BOND_WEAK) {
        Moveable_move_r(self, dt, self->velocity.x);
    }

    if (self->role.scale_bond == BOND_STRONG) {
        self->VT.scale = T->scale * (major_tab.major->VT.scale / major_tab.major->node.T->scale) + (self->juice.on ? self->juice.scale : 0);
    } else if (self->role.scale_bond == BOND_WEAK) {
        Moveable_move_scale(self, dt);
    }

    if (self->role.wh_bond == BOND_STRONG) {
        self->VT.x = self->VT.x + (0.5f * (1 - major_tab.major->VT.w / (major_tab.major->node.T->w)) * T->w);
        self->VT.w = (T->w) * (major_tab.major->VT.w / major_tab.major->node.T->w);
        self->VT.h = (T->h) * (major_tab.major->VT.h / major_tab.major->node.T->h);
    } else if (self->role.wh_bond == BOND_WEAK) {
        Moveable_move_wh(self, dt);
    }

    Moveable_calculate_parrallax(self);
}

// moveable.lua:405
void Moveable_move_xy(Moveable *self, float dt) {
    Transform *T = self->node.T;
    if ((T->x != self->VT.x || fabsf(self->velocity.x) > 0.01f) ||
        (T->y != self->VT.y || fabsf(self->velocity.y) > 0.01f)) {
        self->velocity.x = G->exp_times.xy * self->velocity.x + (1 - G->exp_times.xy) * (T->x - self->VT.x) * 35 * dt;
        self->velocity.y = G->exp_times.xy * self->velocity.y + (1 - G->exp_times.xy) * (T->y - self->VT.y) * 35 * dt;
        if (self->velocity.x * self->velocity.x + self->velocity.y * self->velocity.y > G->exp_times.max_vel * G->exp_times.max_vel) {
            float actual_vel = sqrtf(self->velocity.x * self->velocity.x + self->velocity.y * self->velocity.y);
            self->velocity.x = G->exp_times.max_vel * self->velocity.x / actual_vel;
            self->velocity.y = G->exp_times.max_vel * self->velocity.y / actual_vel;
        }
        self->STATIONARY = false;
        self->VT.x = self->VT.x + self->velocity.x;
        self->VT.y = self->VT.y + self->velocity.y;
        if (fabsf(self->VT.x - T->x) < 0.01f && fabsf(self->velocity.x) < 0.01f) { self->VT.x = T->x; self->velocity.x = 0; }
        if (fabsf(self->VT.y - T->y) < 0.01f && fabsf(self->velocity.y) < 0.01f) { self->VT.y = T->y; self->velocity.y = 0; }
    }
}

// moveable.lua:423
void Moveable_move_scale(Moveable *self, float dt) {
    float des_scale = self->node.T->scale + (self->zoom ? ((self->node.states.drag.is ? 0.1f : 0) + (self->node.states.hover.is ? 0.05f : 0)) : 0) + (self->juice.on ? self->juice.scale : 0);

    if (des_scale != self->VT.scale ||
        fabsf(self->velocity.scale) > 0.001f) {
        self->STATIONARY = false;
        self->velocity.scale = G->exp_times.scale * self->velocity.scale + (1 - G->exp_times.scale) * (des_scale - self->VT.scale);
        self->VT.scale = self->VT.scale + self->velocity.scale;
    }
}

// moveable.lua:434
void Moveable_move_wh(Moveable *self, float dt) {
    Transform *T = self->node.T;
    if ((T->w != self->VT.w && !self->pinch.x) ||
        (T->h != self->VT.h && !self->pinch.y) ||
        (self->VT.w > 0 && self->pinch.x) ||
        (self->VT.h > 0 && self->pinch.y)) {
        self->STATIONARY = false;
        self->VT.w = self->VT.w + (8 * dt) * (self->pinch.x ? -1 : 1) * T->w;
        self->VT.h = self->VT.h + (8 * dt) * (self->pinch.y ? -1 : 1) * T->h;
        self->VT.w = fmaxf(fminf(self->VT.w, T->w), 0);
        self->VT.h = fmaxf(fminf(self->VT.h, T->h), 0);
    }
}

// moveable.lua:447 vel_x: vel.x of the Lua's vel argument
void Moveable_move_r(Moveable *self, float dt, float vel_x) {
    float des_r = self->node.T->r + 0.015f * vel_x / dt + (self->juice.on ? self->juice.r * 2 : 0);

    if (des_r != self->VT.r ||
        fabsf(self->velocity.r) > 0.001f) {
        self->STATIONARY = false;
        self->velocity.r = G->exp_times.r * self->velocity.r + (1 - G->exp_times.r) * (des_r - self->VT.r);
        self->VT.r = self->VT.r + self->velocity.r;
    }
    if (fabsf(self->VT.r - self->node.T->r) < 0.001f && fabsf(self->velocity.r) < 0.001f) { self->VT.r = self->node.T->r; self->velocity.r = 0; }
}

// moveable.lua:459
void Moveable_calculate_parrallax(Moveable *self) {
    if (!G->ROOM) return;
    self->shadow_parrallax.x = (self->node.T->x + self->node.T->w / 2 - G->ROOM->T->w / 2) / (G->ROOM->T->w / 2) * 1.5f;
}

// moveable.lua:464
void Moveable_set_role(Moveable *self, Table *args) {
    Val major = tget(args, "major");
    // `args.major and not args.major.set_role`: a major that is not a Moveable
    if (truthy(major) && !(major.t == V_OBJ && is_a(major.obj, &Moveable_class))) return;
    args_check_offset(args);
    // note: the Lua keeps the caller's offset table by reference; role.offset is a Vec2, so it is copied.
    Table *offset = ttab(args, "offset");
    if (truthy(tget(args, "role_type"))) self->role.role_type = role_type_of(tstr(args, "role_type"));
    if (offset) self->role.offset = (Vec2){(float)tnum(offset, "x"), (float)tnum(offset, "y")};
    if (truthy(major)) self->role.major = major.obj;
    if (truthy(tget(args, "xy_bond"))) self->role.xy_bond = bond_of(tstr(args, "xy_bond"));
    if (truthy(tget(args, "wh_bond"))) self->role.wh_bond = bond_of(tstr(args, "wh_bond"));
    if (truthy(tget(args, "r_bond"))) self->role.r_bond = bond_of(tstr(args, "r_bond"));
    if (truthy(tget(args, "scale_bond"))) self->role.scale_bond = bond_of(tstr(args, "scale_bond"));
    if (truthy(tget(args, "draw_major"))) self->role.draw_major = tobj(args, "draw_major");
    if (self->role.role_type == ROLE_MAJOR) self->role.major = NULL;
}

// moveable.lua:482
MajorTab Moveable_get_major(Moveable *self) {
    if ((self->role.role_type != ROLE_MAJOR && self->role.major != self) && (self->role.xy_bond != BOND_WEAK && self->role.r_bond != BOND_WEAK)) {
        // First, does the major already have their offset precalculated for this frame?
        if (!self->frame_major.have || G->REFRESH_FRAME_MAJOR_CACHE > 0) {
            MajorTab major = Moveable_get_major(self->role.major);
            self->frame_major.have = true;
            self->frame_major.tab.major = major.major;
            self->frame_major.tab.offset.x = major.offset.x + self->role.offset.x + self->layered_parallax.x;
            self->frame_major.tab.offset.y = major.offset.y + self->role.offset.y + self->layered_parallax.y;
        }
        return self->frame_major.tab;
    } else {
        return (MajorTab){.major = self, .offset = {0, 0}}; // self.ARGS.get_major
    }
}

// moveable.lua:503
void Moveable_remove(Moveable *self) {
    for (int k = 1, n = tlen(G->MOVEABLES); k <= n; k++) {
        if (tgeti(G->MOVEABLES, k).obj == self) {
            tremove(G->MOVEABLES, k);
            break;
        }
    }
    for (int k = 1, n = tlen(G->I.MOVEABLE); k <= n; k++) {
        if (tgeti(G->I.MOVEABLE, k).obj == self) {
            tremove(G->I.MOVEABLE, k);
            break;
        }
    }
    Node_remove(&self->node);
}
