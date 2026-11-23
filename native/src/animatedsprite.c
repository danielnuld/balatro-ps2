// engine/animatedsprite.lua:1-107
#include "all.h"

// animatedsprite.lua:2 AnimatedSprite = Sprite:extend()
// note: the entries left NULL are inherited from Sprite by class_init. The casts only change self's pointer type.
Class AnimatedSprite_class = {
    .name = "AnimatedSprite",
    .super = &Sprite_class,
    .animate = (void (*)(void *, float))AnimatedSprite_animate,
    .remove = (void (*)(void *))AnimatedSprite_remove,
    .draw_self = (void (*)(void *, const float *))AnimatedSprite_draw_self,
    .set_sprite_pos = (void (*)(void *, Table *))AnimatedSprite_set_sprite_pos,
    .reset = (void (*)(void *))AnimatedSprite_reset,
    .get_pos_pixel = (void (*)(void *, float *))AnimatedSprite_get_pos_pixel,
};

// animatedsprite.lua:5
void AnimatedSprite_init(AnimatedSprite *self, float X, float Y, float W, float H, Atlas *new_sprite_atlas, Table *sprite_pos) {
    Sprite_init((Sprite *)self, X, Y, W, H, new_sprite_atlas, sprite_pos);
    self->sprite.moveable.offset = (Vec2){0, 0};

    tinsert(G->ANIMATIONS, O(self));
    // getmetatable(self) == AnimatedSprite
    if (self->sprite.moveable.node.object.cls == &AnimatedSprite_class) {
        tinsert(G->I.SPRITE, O(self));
    }
}

// animatedsprite.lua:15
void AnimatedSprite_rescale(AnimatedSprite *self) {
    Sprite *s = &self->sprite;
    Transform *T_ = s->moveable.node.T;
    s->scale_mag = fminf(s->scale.x / T_->w, s->scale.y / T_->h);
}

// animatedsprite.lua:19
void AnimatedSprite_reset(AnimatedSprite *self) {
    self->sprite.atlas = ANIMATION_ATLAS_get(self->sprite.atlas->name);
    CALL(self, set_sprite_pos, TAB("x", N(self->animation.x), "y", N(self->animation.y)));
}

// animatedsprite.lua:24
void AnimatedSprite_set_sprite_pos(AnimatedSprite *self, Table *sprite_pos) {
    Sprite *s = &self->sprite;
    // sprite_pos and sprite_pos.x or 0
    Val px = tget(sprite_pos, "x"), py = tget(sprite_pos, "y");
    self->animation.x = truthy(px) ? (float)vnum(px) : 0;
    self->animation.y = truthy(py) ? (float)vnum(py) : 0;
    self->animation.frames = s->atlas->frames;
    self->animation.current = 0;
    self->animation.w = s->scale.x;
    self->animation.h = s->scale.y;

    self->frame_offset = 0;

    self->current_animation.current = 0;
    self->current_animation.frames = self->animation.frames;
    self->current_animation.w = self->animation.w;
    self->current_animation.h = self->animation.h;

    Image_getDimensions(s->atlas->image, &s->image_dims[0], &s->image_dims[1]);

    s->sprite = love_graphics_newQuad(
        0,
        self->animation.h * self->animation.y,
        self->animation.w,
        self->animation.h,
        s->image_dims[0], s->image_dims[1]);
    // note: offset_seconds is not a member: it lives in dyn
    tsetn(s->moveable.node.object.dyn, "offset_seconds", G->TIMERS.REAL);
}

// animatedsprite.lua:51
void AnimatedSprite_get_pos_pixel(AnimatedSprite *self, float out[4]) {
    out[0] = self->current_animation.current;
    out[1] = self->animation.y;
    out[2] = self->animation.w;
    out[3] = self->animation.h;
}

// animatedsprite.lua:60
// note: the Lua takes no overlay argument; it is ignored.
void AnimatedSprite_draw_self(AnimatedSprite *self, const float *overlay) {
    Sprite *s = &self->sprite;
    if (!s->moveable.node.states.visible) return;

    prep_draw(self, 1, 0, NULL);
    love_graphics_scale(1 / s->scale_mag, 1 / s->scale_mag);
    love_graphics_setColorv(G->C.WHITE);
    love_graphics_draw_quad(
        s->atlas->image,
        s->sprite,
        0, 0,
        0,
        s->moveable.VT.w / s->moveable.node.T->w,
        s->moveable.VT.h / s->moveable.node.T->h,
        0, 0);
    love_graphics_pop();
}

// animatedsprite.lua:77
// note: the Lua takes no dt argument; it is ignored.
void AnimatedSprite_animate(AnimatedSprite *self, float dt) {
    Sprite *s = &self->sprite;
    Table *dyn = s->moveable.node.object.dyn;
    num REAL = G->TIMERS.REAL;
    // note: a 0-frame animation gives NaN in the Lua (never equal, viewport set to NaN); no atlas has 0 frames.
    if (self->anim_snap.stamp != tstamp(dyn)) { // (dyn is read again only after a write to it)
        self->anim_snap.offset_seconds = tnum(dyn, "offset_seconds"), self->anim_snap.float_ = tbool(dyn, "float");
        self->anim_snap.stamp = tstamp(dyn);
    }
    int new_frame = (int)lmod(floor(G->ANIMATION_FPS * (REAL - self->anim_snap.offset_seconds)), self->current_animation.frames);
    if (new_frame != self->current_animation.current) {
        self->current_animation.current = new_frame;
        self->frame_offset = floorf(self->animation.w * self->current_animation.current);
        Quad_setViewport(s->sprite,
            self->frame_offset,
            self->animation.h * self->animation.y,
            self->animation.w,
            self->animation.h);
    }
    // note: self.float is set by callers (tag.lua:507, UI_definitions.lua:3995, 5829); not a member: dyn "float"
    if (self->anim_snap.float_) {
        Transform *T_ = s->moveable.node.T;
        float REALf = (float)G->TIMERS.REAL; // (float, as every animation: docs/conventions.md; wrap_angle: core.h)
        T_->r = 0.02f * sinf(wrap_angle(2 * REALf + T_->x));
        s->moveable.offset.y = -(1 + 0.3f * sinf(wrap_angle(0.666f * REALf + T_->y))) * s->moveable.shadow_parrallax.y;
        s->moveable.offset.x = -(0.7f + 0.2f * sinf(wrap_angle(0.666f * REALf + T_->x))) * s->moveable.shadow_parrallax.x;
    }
}

// animatedsprite.lua:96-105: `k` is an undefined global (nil), so table.remove(t, k) pops the LAST entry when self is
// in the list, whatever that entry is. Kept as the Lua does it (spec engine-core.md, trap 6); Sprite_remove then
// removes self itself if it is still there.
// note: the lists hold each object once, so the loop pops at most one entry.
static void pop_last_if_listed(Table *t, void *self) {
    bool found = false;
    for (TIter it = titer(t); tnext(&it);) {
        if (veq(it.val, O(self))) found = true;
    }
    if (found) tremove(t, 0);
}

// animatedsprite.lua:95
void AnimatedSprite_remove(AnimatedSprite *self) {
    pop_last_if_listed(G->ANIMATIONS, self);
    pop_last_if_listed(G->I.SPRITE, self);
    Sprite_remove((Sprite *)self);
}
