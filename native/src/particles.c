// engine/particles.lua:1-177
#include "all.h"

static void Particles_draw_default(void *self) { Particles_draw((Particles *)self, -1); } // self:draw()

// particles.lua:2 Particles = Moveable:extend()
// note: the entries left NULL are inherited from Moveable by class_init. The casts only change self's pointer type.
Class Particles_class = {
    .name = "Particles",
    .super = &Moveable_class,
    .update = (void (*)(void *, float))Particles_update,
    .move = (void (*)(void *, float))Particles_move,
    .draw = Particles_draw_default,
    .remove = (void (*)(void *))Particles_remove,
};

// config.key or def
static float cfg_or(Table *config, const char *key, float def) {
    Val v = tget(config, key);
    return truthy(v) ? (float)v.n : def;
}

// particles.lua:5
void Particles_init(Particles *self, float X, float Y, float W, float H, Table *config) {
    Moveable *m = &self->moveable;
    Node *n = &m->node;
    if (!config) config = tnew();

    Moveable_init(m, X, Y, W, H);

    self->fill = tbool(config, "fill");
    self->padding = cfg_or(config, "padding", 0);

    if (truthy(tget(config, "attach"))) {
        Moveable_set_alignment(m, TAB("major", tget(config, "attach"), "type", S("cm"), "bond", S("Strong")));
        tinsert(m->role.major->node.children, O(self));
        n->parent = (Node *)m->role.major;
        n->T->x = m->role.major->node.T->x + self->padding;
        n->T->y = m->role.major->node.T->y + self->padding;
        if (self->fill) {
            n->T->w = m->role.major->node.T->w - self->padding;
            n->T->h = m->role.major->node.T->h - self->padding;
        }
    }

    n->states.hover.can = false;
    n->states.click.can = false;
    n->states.collide.can = false;
    n->states.drag.can = false;
    n->states.release_on.can = false;

    self->timer = cfg_or(config, "timer", 0.5f);
    {
        const char *timer_type = tstr(config, "timer_type");
        self->timer_type = n->created_on_pause ? "REAL" : timer_type ? timer_type : "REAL";
    }
    self->last_real_time = *TIMERS_get(self->timer_type) - self->timer;
    self->last_drawn = 0;
    self->lifespan = cfg_or(config, "lifespan", 1);
    self->fade_alpha = 0;
    self->speed = cfg_or(config, "speed", 1);
    {
        Val v = tget(config, "max");
        self->max = truthy(v) ? v.n : 1000000000000000.0;
        v = tget(config, "pulse_max");
        self->pulse_max = (int)lmin(20, truthy(v) ? v.n : 0);
    }
    self->pulsed = 0;
    self->vel_variation = cfg_or(config, "vel_variation", 1);
    // self.particles = {}
    self->particles = NULL;
    self->nparticles = 0;
    self->room = 0;
    self->scale = cfg_or(config, "scale", 1);
    self->colours = ttab(config, "colours");
    if (!self->colours) self->colours = ARR(P(G->C.BACKGROUND.D));

    if (truthy(tget(config, "initialize"))) {
        for (int i = 1; i <= 60; i++) {
            self->last_real_time = self->last_real_time - 15.0f / 60;
            CALL(self, update, 15.0f / 60);
            CALL(self, move, 15.0f / 60);
        }
    }

    // getmetatable(self) == Particles
    if (n->object.cls == &Particles_class) {
        tinsert(G->I.MOVEABLE, O(self));
    }
}

// particles.lua:63
void Particles_update(Particles *self, float dt) {
    Node *n = &self->moveable.node;
    if (SETTINGS_snap()->paused && !n->created_on_pause) {
        self->last_real_time = *TIMERS_get(self->timer_type);
        return;
    }
    int added_this_frame = 0;
    while (*TIMERS_get(self->timer_type) > self->last_real_time + self->timer &&
           (self->nparticles < self->max || self->pulsed < self->pulse_max) && added_this_frame < 20) {
        self->last_real_time = self->last_real_time + self->timer;
        // note: separate statements: the order of the math.random calls is x, then y
        Vec2 new_offset;
        new_offset.x = self->fill ? (float)((0.5 - math_random()) * n->T->w) : 0;
        new_offset.y = self->fill ? (float)((0.5 - math_random()) * n->T->h) : 0;
        if (self->fill && n->T->r < 0.1f && n->T->r > -0.1f) {
            Vec2 newer_offset = {
                .x = sinf(n->T->r) * new_offset.y + cosf(n->T->r) * new_offset.x,
                .y = sinf(n->T->r) * new_offset.x + cosf(n->T->r) * new_offset.y,
            };
            new_offset = newer_offset;
        }
        // table.insert(self.particles, {...})
        if (self->nparticles == self->room) {
            self->room = self->room ? self->room * 2 : 16;
            self->particles = realloc(self->particles, (size_t)self->room * sizeof(Particle));
            if (!self->particles) ERROR("particles.lua:79: out of memory");
        }
        Particle p = {0};
        // note: one statement per field: the math.random calls happen in the order of the Lua's constructor
        p.draw = false;
        p.dir = (float)(math_random() * 2 * M_PI);
        p.facing = (float)(math_random() * 2 * M_PI);
        p.size = (float)(math_random() * 0.5 + 0.1);
        p.age = 0;
        p.velocity = (float)(self->speed * (self->vel_variation * math_random() + (1 - self->vel_variation)) * 0.7);
        p.r_vel = (float)(0.2 * (0.5 - math_random()));
        p.e_prev = 0;
        p.e_curr = 0;
        p.e_vel_set = false; // e_vel = nil
        p.scale = 0;
        p.visible_scale = 0;
        p.time = *TIMERS_get(self->timer_type);
        p.colour = pseudorandom_element(self->colours, NIL, NULL).p;
        p.offset = new_offset;
        self->particles[self->nparticles++] = p;
        added_this_frame = added_this_frame + 1;
        if (self->pulsed <= self->pulse_max) self->pulsed = self->pulsed + 1;
    }
}

// particles.lua:100
void Particles_move(Particles *self, float dt) {
    if (SETTINGS_snap()->paused && !self->moveable.node.created_on_pause) return;

    Moveable_move(&self->moveable, dt);

    if (!streq(self->timer_type, "REAL")) dt = dt * (float)G->SPEEDFACTOR;

    for (int i = self->nparticles; i >= 1; i--) {
        Particle *p = &self->particles[i - 1];
        p->draw = true;
        if (!p->e_vel_set) { // e_vel or dt*self.scale
            p->e_vel = dt * self->scale;
            p->e_vel_set = true;
        }
        p->e_prev = p->e_curr;
        p->age = p->age + dt;

        p->e_curr = fminf(2 * fminf((p->age / self->lifespan) * self->scale, self->scale * ((self->lifespan - p->age) / self->lifespan)), self->scale);

        p->e_vel = (p->e_curr - p->e_prev) * self->scale * dt + (1 - self->scale * dt) * p->e_vel;

        p->scale = p->scale + p->e_vel;
        p->scale = fminf(2 * fminf((p->age / self->lifespan) * self->scale, self->scale * ((self->lifespan - p->age) / self->lifespan)), self->scale);

        if (p->scale < 0) {
            // table.remove(self.particles, i)
            memmove(p, p + 1, (size_t)(self->nparticles - i) * sizeof(Particle));
            self->nparticles--;
        } else {
            p->offset.x = p->offset.x + p->velocity * sinf(p->dir) * dt;
            p->offset.y = p->offset.y + p->velocity * cosf(p->dir) * dt;
            p->facing = p->facing + p->r_vel * dt;
            p->velocity = fmaxf(0, p->velocity - p->velocity * 0.07f * dt);
        }
    }
}

// particles.lua:131
void Particles_fade(Particles *self, num delay, float to) {
    // note: the ease points into self: the pointer dangles if self is freed while the event is still queued
    // (event.h, "Lifetime"); the Lua keeps the table alive.
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .timer = self->timer_type,
        .not_blockable = true,
        .not_blocking = true,
        .ref_float = &self->fade_alpha,
        .ease_to = to < 0 ? 1 : to,
        .delay = delay,
    }), NULL, false);
}

// particles.lua:144
void Particles_draw(Particles *self, float alpha) {
    Transform *T_ = self->moveable.node.T;
    if (alpha < 0) alpha = 1;
    prep_draw(self, 1, 0, NULL);
    love_graphics_translate(T_->w / 2, T_->h / 2);
    for (int k = 0; k < self->nparticles; k++) {
        Particle *v = &self->particles[k];
        if (v->draw) {
            love_graphics_push();
            love_graphics_setColor(v->colour[0], v->colour[1], v->colour[2], v->colour[3] * alpha * (1 - self->fade_alpha));
            love_graphics_translate(v->offset.x, v->offset.y);
            love_graphics_rotate(v->facing);

            love_graphics_rectangle("fill", -v->scale / 2, -v->scale / 2, v->scale, v->scale); // origin in the middle
            love_graphics_pop();
        }
    }
    love_graphics_pop();

    add_to_drawhash(self);
    Node_draw_boundingrect((Node *)self);
}

// particles.lua:165
void Particles_remove(Particles *self) {
    Moveable *major = self->moveable.role.major;
    if (major) {
        // note: pairs + table.remove with no break: after a removal the Lua's traversal goes on at k+1, as here.
        // Only the array part is walked: a number key past the border would be a table.remove beyond #t.
        Table *children = major->node.children;
        for (int k = 1; k <= tlen(children); k++) {
            Val v = tgeti(children, k);
            if (v.t == V_OBJ && v.obj == self) {
                tremove(children, k);
            }
        }
    }

    remove_all(self->moveable.node.children);

    // note: the particle array is C storage of self.particles (the Lua's collector frees it); released here and
    // left empty in case a method still runs on the removed object.
    free(self->particles);
    self->particles = NULL;
    self->nparticles = 0;
    self->room = 0;

    Moveable_remove(&self->moveable);
}
