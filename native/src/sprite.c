// engine/sprite.lua:1-216
#include "all.h"

static void Sprite_draw_default(void *self) { Sprite_draw((Sprite *)self, NULL); } // obj:draw() with no overlay

// sprite.lua:2 Sprite = Moveable:extend()
// note: the entries left NULL are inherited from Moveable by class_init. The casts only change self's pointer type.
Class Sprite_class = {
    .name = "Sprite",
    .super = &Moveable_class,
    .draw = Sprite_draw_default,
    .remove = (void (*)(void *))Sprite_remove,
    .draw_self = (void (*)(void *, const float *))Sprite_draw_self,
    .set_sprite_pos = (void (*)(void *, Table *))Sprite_set_sprite_pos,
    .reset = (void (*)(void *))Sprite_reset,
    .get_pos_pixel = (void (*)(void *, float *))Sprite_get_pos_pixel,
};

// note: node.h holds sprite_pos as a Vec2, but the Lua keeps the CALLER'S table by reference and draw_self re-quads
// when that table changes (sprite.lua:128; spec engine-core.md trap 22). The table itself is kept in dyn
// "sprite_pos"; the member mirrors its values.
static Table *sprite_pos_table(Sprite *self) {
    Table *dyn = self->moveable.node.object.dyn;
    Table *t = ttab(dyn, "sprite_pos");
    if (!t) {
        t = TAB("x", N(self->sprite_pos.x), "y", N(self->sprite_pos.y));
        tsett(dyn, "sprite_pos", t);
    }
    return t;
}

// sprite.lua:5
void Sprite_init(Sprite *self, float X, float Y, float W, float H, Atlas *new_sprite_atlas, Table *sprite_pos) {
    Moveable_init((Moveable *)self, X, Y, W, H);
    self->moveable.node.CT = &self->moveable.VT;
    self->atlas = new_sprite_atlas;
    self->scale = (Vec2){self->atlas->px, self->atlas->py};
    self->scale_mag = fminf(self->scale.x / W, self->scale.y / H);
    self->moveable.zoom = true;

    CALL(self, set_sprite_pos, sprite_pos);

    // getmetatable(self) == Sprite
    // note: a sprite whose class is swapped later for a private subclass (tag.h, UI_definitions.h) is a plain
    // Sprite in the Lua; it is listed here only if it still has Sprite_class when init runs.
    if (self->moveable.node.object.cls == &Sprite_class) {
        tinsert(G->I.SPRITE, O(self));
    }
}

// sprite.lua:20
void Sprite_reset(Sprite *self) {
    self->atlas = ASSET_ATLAS_get(self->atlas->name);
    CALL(self, set_sprite_pos, sprite_pos_table(self));
}

// sprite.lua:25
void Sprite_set_sprite_pos(Sprite *self, Table *sprite_pos) {
    Table *pos;
    if (sprite_pos && truthy(tget(sprite_pos, "v"))) {
        pos = TAB("x", N(math_random_to(tnum(sprite_pos, "v")) - 1), "y", tget(sprite_pos, "y"));
    } else {
        pos = sprite_pos ? sprite_pos : TAB("x", N(0), "y", N(0));
    }
    tsett(self->moveable.node.object.dyn, "sprite_pos", pos);
    self->sprite_pos = (Vec2){(float)tnum(pos, "x"), (float)tnum(pos, "y")};
    self->sprite_pos_copy = self->sprite_pos;

    float iw, ih;
    Image_getDimensions(self->atlas->image, &iw, &ih);
    // note: the Lua makes a new quad every time (collected later); there is no collector, so the old one is reused
    if (self->sprite) {
        Quad_setViewport(self->sprite,
            self->sprite_pos.x * self->atlas->px,
            self->sprite_pos.y * self->atlas->py,
            self->scale.x,
            self->scale.y);
    } else {
        self->sprite = love_graphics_newQuad(
            self->sprite_pos.x * self->atlas->px,
            self->sprite_pos.y * self->atlas->py,
            self->scale.x,
            self->scale.y, iw, ih);
    }

    self->image_dims[0] = iw, self->image_dims[1] = ih;
}

// sprite.lua:43
void Sprite_get_pos_pixel(Sprite *self, float out[4]) {
    out[0] = self->sprite_pos.x;
    out[1] = self->sprite_pos.y;
    out[2] = self->atlas->px; // self.scale.x
    out[3] = self->atlas->py; // self.scale.y
}

// sprite.lua:52
const float *Sprite_get_image_dims(Sprite *self) {
    return self->image_dims;
}

static Val or_nil(Val v) { return truthy(v) ? v : NIL; } // v or nil

// sprite.lua:56
void Sprite_define_draw_steps(Sprite *self, Table *draw_step_definitions) {
    self->draw_steps = EMPTY(self->draw_steps);
    for (int k = 1; truthy(tgeti(draw_step_definitions, k)); k++) {
        Table *v = ttabi(draw_step_definitions, k);
        Val shader = tget(v, "shader");
        tinsert(self->draw_steps, T(TAB(
            "shader", truthy(shader) ? shader : S("dissolve"),
            "shadow_height", or_nil(tget(v, "shadow_height")),
            "send", or_nil(tget(v, "send")),
            "no_tilt", or_nil(tget(v, "no_tilt")),
            "other_obj", or_nil(tget(v, "other_obj")),
            "ms", or_nil(tget(v, "ms")),
            "mr", or_nil(tget(v, "mr")),
            "mx", or_nil(tget(v, "mx")),
            "my", or_nil(tget(v, "my")))));
    }
}

// shader:send(name, value) for a value kept in a table: a number, a boolean, a colour (P(float *)) or an array of
// 2 to 4 numbers.
static void send_val(Shader *shader, const char *name, Val v) {
    if (v.t == V_NUM) Shader_send(shader, name, (float)v.n);
    else if (v.t == V_BOOL) Shader_send(shader, name, v.b ? 1 : 0);
    else if (v.t == V_PTR) Shader_send_colour(shader, name, (const float *)v.p);
    else if (v.t == V_TAB) {
        float f[4];
        int n = tlen(v.tab);
        if (n > 4) n = 4;
        for (int i = 0; i < n; i++) f[i] = (float)tnumi(v.tab, i + 1);
        Shader_send_vec(shader, name, f, n);
    } else ERROR("sprite.lua:85: shader:send('%s', %s)", name, lua_typename_of(v));
}

// sprite.lua:85 v.val or (v.func and v.func()) or v.ref_table[v.ref_value]
// note: game.h:20-28: an entry that pointed into a struct of G has ref_float / ref_colour instead of ref_table +
// ref_value; the read order is val, ref_num, ref_float, ref_colour, ref_table[ref_value]. func stays where the Lua has it.
static void send_custom(Shader *shader, Table *v) {
    const char *name = tstr(v, "name");
    Val val = tget(v, "val");
    if (truthy(val)) { send_val(shader, name, val); return; }
    Val func = tget(v, "func");
    if (truthy(func)) {
        val = fcall0(func);
        if (truthy(val)) { send_val(shader, name, val); return; }
    }
    Val ref_num = tget(v, "ref_num"); // a timer of G (game.h): a num
    if (truthy(ref_num)) { Shader_send(shader, name, (float)*(num *)ref_num.p); return; }
    Val ref_float = tget(v, "ref_float");
    if (truthy(ref_float)) { Shader_send(shader, name, *(float *)ref_float.p); return; }
    Val ref_colour = tget(v, "ref_colour");
    if (truthy(ref_colour)) { Shader_send_colour(shader, name, (const float *)ref_colour.p); return; }
    Val ref_table = tget(v, "ref_table");
    const char *ref_value = tstr(v, "ref_value");
    if (ref_table.t == V_OBJ) {
        // note: button_callbacks.lua:2010-2011 refer to a sprite's image_dims and RETS.get_pos_pixel, which are
        // not tables here; any other field of an object is read from its dyn.
        if (is_a(ref_table.obj, &Sprite_class) && streq(ref_value, "image_dims")) {
            Shader_send_vec(shader, name, Sprite_get_image_dims(ref_table.obj), 2);
            return;
        }
        if (is_a(ref_table.obj, &Sprite_class) && streq(ref_value, "get_pos_pixel")) {
            float out[4];
            CALL(ref_table.obj, get_pos_pixel, out);
            Shader_send_vec(shader, name, out, 4);
            return;
        }
        send_val(shader, name, tget_k(((Object *)ref_table.obj)->dyn, ref_value)); // (ref_value: a table's string, interned)
        return;
    }
    send_val(shader, name, tget_k(ref_table.tab, ref_value));
}

// sprite.lua:73
void Sprite_draw_shader(Sprite *self, const char *_shader, float _shadow_height, Table *_send, bool _no_tilt, Sprite *other_obj,
                        Val ms, Val mr, Val mx, Val my, bool custom_shader, float tilt_shadow) {
    if (SETTINGS_snap()->reduced_motion) _no_tilt = true;
    Moveable *_draw_major = self->moveable.role.draw_major ? self->moveable.role.draw_major : (Moveable *)self;
    bool has_shadow_height = !(_shadow_height < 0); // node.h: < 0 is nil
    bool has_tilt_shadow = !(tilt_shadow < 0);
    Transform *VT = &self->moveable.VT;
    if (has_shadow_height) {
        VT->y = VT->y - _draw_major->shadow_parrallax.y * _shadow_height;
        VT->x = VT->x - _draw_major->shadow_parrallax.x * _shadow_height;
        VT->scale = VT->scale * (1 - 0.2f * _shadow_height);
    }

    Shader *shader = SHADERS_get(_shader ? _shader : "dissolve");
    if (custom_shader) {
        if (_send) {
            for (int k = 1; truthy(tgeti(_send, k)); k++) {
                send_custom(SHADERS_get(_shader), ttabi(_send, k));
            }
        }
    } else if (streq(_shader, "vortex")) {
        Shader_send(SHADERS_get("vortex"), "vortex_amt", G->TIMERS.REAL - G->vortex_time); // vortex_time: 0 as nil
    } else {
        // note: tilt_var, hover_tilt, dissolve and dissolve_colours are members of a Card or a Blind
        // (docs/header-notes.md) and dyn fields of anything else (a tag's sprite, G.SPLASH_LOGO).
        Table *dm_dyn = _draw_major->node.object.dyn;
        bool has_tilt_var;
        float tilt_mx = 0, tilt_my = 0, hover_tilt, dissolve;
        Table *dissolve_colours;
        if (is_a(_draw_major, &Card_class)) {
            Card *c = (Card *)_draw_major;
            has_tilt_var = true, tilt_mx = c->tilt_var.mx, tilt_my = c->tilt_var.my;
            hover_tilt = c->hover_tilt, dissolve = c->dissolve, dissolve_colours = c->dissolve_colours;
        } else if (is_a(_draw_major, &Blind_class)) {
            Blind *b = (Blind *)_draw_major;
            has_tilt_var = true, tilt_mx = b->tilt_var.mx, tilt_my = b->tilt_var.my;
            hover_tilt = b->hover_tilt, dissolve = b->dissolve, dissolve_colours = b->dissolve_colours;
        } else {
            Table *tilt_var = ttab(dm_dyn, "tilt_var");
            has_tilt_var = tilt_var != NULL;
            if (has_tilt_var) tilt_mx = (float)tnum(tilt_var, "mx"), tilt_my = (float)tnum(tilt_var, "my");
            hover_tilt = (float)tnum(dm_dyn, "hover_tilt"), dissolve = (float)tnum(dm_dyn, "dissolve");
            dissolve_colours = ttab(dm_dyn, "dissolve_colours");
        }

        float cursor_pos[2];
        cursor_pos[0] = has_tilt_var ? tilt_mx * G->CANV_SCALE : G->CONTROLLER->cursor_position.x * G->CANV_SCALE;
        cursor_pos[1] = has_tilt_var ? tilt_my * G->CANV_SCALE : G->CONTROLLER->cursor_position.y * G->CANV_SCALE;

        Shader_send_vec(shader, "mouse_screen_pos", cursor_pos, 2);
        // mouse_damping: 0 as nil
        Shader_send(shader, "screen_scale", G->TILESCALE * G->TILESIZE * (_draw_major->mouse_damping != 0 ? _draw_major->mouse_damping : 1) * G->CANV_SCALE);
        Shader_send(shader, "hovering", ((has_shadow_height && !has_tilt_shadow) || _no_tilt) ? 0 : hover_tilt * (has_tilt_shadow ? tilt_shadow : 1));
        Shader_send(shader, "dissolve", fabsf(dissolve));
        if (_draw_major->shader_time_id != _draw_major->node.ID || !_draw_major->node.ID) { // (doubles: once per node)
            _draw_major->shader_time = (float)lmod(123.33412 * (_draw_major->node.ID / 1.14212), 3000);
            _draw_major->shader_time_id = _draw_major->node.ID;
        }
        Shader_send(shader, "time", _draw_major->shader_time);
        float texture_details[4];
        CALL(self, get_pos_pixel, texture_details);
        Shader_send_vec(shader, "texture_details", texture_details, 4);
        Shader_send_vec(shader, "image_details", Sprite_get_image_dims(self), 2);
        Val burn_1 = tgeti(dissolve_colours, 1), burn_2 = tgeti(dissolve_colours, 2);
        Shader_send_colour(shader, "burn_colour_1", truthy(burn_1) ? (const float *)burn_1.p : G->C.CLEAR);
        Shader_send_colour(shader, "burn_colour_2", truthy(burn_2) ? (const float *)burn_2.p : G->C.CLEAR);
        Shader_send(shader, "shadow", has_shadow_height ? 1 : 0);
        if (_send) send_val(shader, _shader, T(_send));
    }

    love_graphics_setShader(shader);

    if (other_obj) {
        Sprite_draw_from(self, other_obj, ms, mr, mx, my);
    } else {
        CALL(self, draw_self, NULL);
    }

    love_graphics_setShader(NULL);

    if (has_shadow_height) {
        VT->y = VT->y + _draw_major->shadow_parrallax.y * _shadow_height;
        VT->x = VT->x + _draw_major->shadow_parrallax.x * _shadow_height;
        VT->scale = VT->scale / (1 - 0.2f * _shadow_height);
    }
}

// sprite.lua:126
void Sprite_draw_self(Sprite *self, const float *overlay) {
    Table *dyn = self->moveable.node.object.dyn;
    if (!self->moveable.node.states.visible) return;
    // note: the caller's table (dyn "sprite_pos") is compared first, then the member in case it was written directly
    // (dyn and that table are read again only after a write to them: dyn_snap, node.h)
    if (self->dyn_snap.stamp != tstamp(dyn) || self->dyn_snap.pos_stamp != tstamp(self->dyn_snap.sprite_pos)) {
        Table *p = ttab(dyn, "sprite_pos");
        self->dyn_snap.sprite_pos = p;
        self->dyn_snap.x = (float)tnum(p, "x"), self->dyn_snap.y = (float)tnum(p, "y");
        self->dyn_snap.video = truthy(tget(dyn, "video")), self->dyn_snap.shader_tab = truthy(tget(dyn, "shader_tab"));
        self->dyn_snap.stamp = tstamp(dyn), self->dyn_snap.pos_stamp = tstamp(p);
    }
    Table *pos = self->dyn_snap.sprite_pos;
    if (pos && (self->dyn_snap.x != self->sprite_pos_copy.x || self->dyn_snap.y != self->sprite_pos_copy.y)) {
        CALL(self, set_sprite_pos, pos);
    } else if (self->sprite_pos.x != self->sprite_pos_copy.x || self->sprite_pos.y != self->sprite_pos_copy.y) {
        CALL(self, set_sprite_pos, TAB("x", N(self->sprite_pos.x), "y", N(self->sprite_pos.y)));
    }
    prep_draw(self, 1, 0, NULL);
    // note: with VT.w == 0 the Lua gives 1/inf = 0; the float division does the same (or 1/huge where there is no inf)
    love_graphics_scale(1 / (self->scale.x / self->moveable.VT.w), 1 / (self->scale.y / self->moveable.VT.h));
    love_graphics_setColorv(overlay ? overlay : G->BRUTE_OVERLAY ? G->BRUTE_OVERLAY : G->C.WHITE);
    if (self->dyn_snap.video) {
        TODO_PORT("sprite.lua:134 self.video: love.graphics.draw of a video (love.h has no videos)");
    } else {
        love_graphics_draw_quad(
            self->atlas->image,
            self->sprite,
            0, 0,
            0,
            self->moveable.VT.w / self->moveable.node.T->w,
            self->moveable.VT.h / self->moveable.node.T->h,
            0, 0);
    }
    love_graphics_pop();
    add_to_drawhash(self);
    Node_draw_boundingrect((Node *)self);
    if (self->dyn_snap.shader_tab) love_graphics_setShader(NULL);
}

// sprite.lua:162
void Sprite_draw(Sprite *self, const float *overlay) {
    if (!self->moveable.node.states.visible) return;
    if (self->draw_steps) {
        for (int k = 1; truthy(tgeti(self->draw_steps, k)); k++) {
            Table *v = ttabi(self->draw_steps, k);
            Val shadow_height = tget(v, "shadow_height");
            Sprite_draw_shader(self, tstr(v, "shader"), truthy(shadow_height) ? (float)shadow_height.n : -1, ttab(v, "send"),
                               tbool(v, "no_tilt"), tobj(v, "other_obj"), tget(v, "ms"), tget(v, "mr"), tget(v, "mx"), tget(v, "my"),
                               truthy(tget(v, "send")), -1);
        }
    } else {
        CALL(self, draw_self, overlay);
    }

    add_to_drawhash(self);
    // note: pairs(): the array part, then the named children in insertion order (hash order in the Lua)
    for (TIter it = titer(self->moveable.node.children); tnext(&it);) {
        if (!(it.key.t == V_STR && streq(it.key.s, "h_popup"))) CALL(it.val.obj, draw);
    }
    add_to_drawhash(self);
    Node_draw_boundingrect((Node *)self);
}

// sprite.lua:180
void Sprite_draw_from(Sprite *self, Sprite *other_obj, Val ms, Val mr, Val mx, Val my) {
    Vec2 draw_from_offset;
    draw_from_offset.x = truthy(mx) ? (float)mx.n : 0;
    draw_from_offset.y = truthy(my) ? (float)my.n : 0;
    prep_draw(other_obj, (1 + (truthy(ms) ? (float)ms.n : 0)), (truthy(mr) ? (float)mr.n : 0), &draw_from_offset);
    // note: other_obj is a Sprite (node.h), so scale_mag is never nil and the `or other_obj.VT.scale` is not reached
    love_graphics_scale(1 / other_obj->scale_mag, 1 / other_obj->scale_mag);
    love_graphics_setColorv(G->BRUTE_OVERLAY ? G->BRUTE_OVERLAY : G->C.WHITE);
    love_graphics_draw_quad(
        self->atlas->image,
        self->sprite,
        -(other_obj->moveable.node.T->w / 2 - other_obj->moveable.VT.w / 2) * 10,
        0,
        0,
        other_obj->moveable.VT.w / other_obj->moveable.node.T->w,
        other_obj->moveable.VT.h / other_obj->moveable.node.T->h,
        0, 0);
    Node_draw_boundingrect((Node *)self);
    love_graphics_pop();
}

// sprite.lua:204-213 for k, v in pairs(t) do if v == self then table.remove(t, k) end end
// note: the lists hold each object once, so stopping at the first match is the same as the Lua's loop with no break.
static void remove_from_list(Table *t, void *self) {
    for (TIter it = titer(t); tnext(&it);) {
        if (veq(it.val, O(self))) {
            tremove(t, (int)it.key.n);
            return;
        }
    }
}

// sprite.lua:200
void Sprite_remove(Sprite *self) {
    if (truthy(tget(self->moveable.node.object.dyn, "video"))) {
        TODO_PORT("sprite.lua:202 self.video:release() (love.h has no videos)");
    }
    remove_from_list(G->ANIMATIONS, self);
    remove_from_list(G->I.SPRITE, self);

    Moveable_remove((Moveable *)self);
}
