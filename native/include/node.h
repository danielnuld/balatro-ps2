// engine/object.lua, engine/node.lua, engine/moveable.lua, engine/sprite.lua, engine/animatedsprite.lua,
// engine/particles.lua: the classes everything on screen derives from.
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn. The scratch tables self.ARGS and self.RETS do not exist: use locals.
#ifndef NODE_H
#define NODE_H
#include "core.h"
#include "love.h"

typedef struct Node Node;
typedef struct Moveable Moveable;
typedef struct Sprite Sprite;
typedef struct AnimatedSprite AnimatedSprite;
typedef struct Particles Particles;

typedef struct { float x, y; } Vec2;
// A transform, in game units (tiles): self.T, self.VT, self.CT
typedef struct { float x, y, w, h, r, scale; } Transform;
typedef struct { bool can, is; } State;
// self.states
typedef struct { bool visible; State collide, focus, hover, click, drag, release_on; } States;
// A colour is the Lua's {r, g, b, a}: 4 floats, shared by pointer and changed in place (G.C.* animate), so a
// field that holds "a colour" is a float * to someone's 4 floats.
typedef float Colour[4];

// ---------------------------------------------------------------- classes
// The methods the engine calls without knowing the object's class. A class leaves NULL what it inherits;
// class_init() fills those from its super (call classes_init() once at start). CALL(obj, method, args...).
struct Class {
    const char *name;
    Class *super;
    bool ready; // class_init has filled the inherited methods
    void (*update)(void *self, float dt);
    void (*move)(void *self, float dt);
    void (*draw)(void *self);                      // obj:draw() with no arguments (the class's own draw takes its defaults)
    void (*remove)(void *self);
    void (*hover)(void *self);
    void (*stop_hover)(void *self);
    void (*click)(void *self);
    void (*release)(void *self, void *dragged);    // dragged: a Node
    void (*drag)(void *self, const Vec2 *offset);  // offset NULL: none given
    void (*stop_drag)(void *self);
    void *(*can_drag)(void *self);                 // the Node to drag, or NULL
    void (*animate)(void *self, float dt);
    bool (*collides_with_point)(void *self, Vec2 point);
    void (*set_container)(void *self, Node *container);
    void (*hard_set_T)(void *self, float X, float Y, float W, float H);
    void (*hard_set_VT)(void *self);
    void (*juice_up)(void *self, float amount, Val rot_amt); // amount < 0: the default 0.4; rot_amt NIL: random sign
    void (*put_focused_cursor)(void *self, float *x, float *y);
    void (*draw_self)(void *self, const float *overlay);     // Sprite / AnimatedSprite; overlay NULL: none
    void (*set_sprite_pos)(void *self, Table *sprite_pos);   // Sprite / AnimatedSprite; {x=, y=} or {v=, y=} or NULL
    void (*reset)(void *self);                               // Sprite / AnimatedSprite
    void (*get_pos_pixel)(void *self, float out[4]);         // Sprite / AnimatedSprite
};
void class_init(Class *cls);
#define CALL(obj, method, ...) (((Object *)(obj))->cls->method((obj), ##__VA_ARGS__))
extern Class Object_class, Node_class, Moveable_class, Sprite_class, AnimatedSprite_class, Particles_class;

// ---------------------------------------------------------------- Node (engine/node.lua)
struct Node {
    Object object;
    Transform T_own;     // the storage of T unless T points elsewhere
    Transform *T;        // self.T: &T_own, or the major's T once glued (Moveable_glue_to_major shares the table)
    Transform *CT;       // self.CT, the transform collisions use: T, or &VT for sprites; NULL as the Lua's nil
    Table *config;       // self.config: never NULL
    Table *children;     // self.children: name or index -> O(node); never NULL
    States states;
    Vec2 click_offset, hover_offset;
    bool created_on_pause;
    int ID;
    struct { int DRAW, MOVE; } FRAME; // (FRAME.MAJOR is Moveable.frame_major)
    Node *container;     // self.container (G.ROOM by default; NULL as nil)
    Node *parent;        // self.parent, where a class sets one (UI elements, particles, popups); NULL
    bool REMOVED, under_overlay, CALCING;
};
// args: {T = {x=, y=, w=, h=, r=, scale=} or {x, y, w, h, r, scale}, container = node}; NULL as {}
void Node_init(Node *self, Table *args);
void Node_draw_boundingrect(Node *self);
void Node_draw(Node *self);
bool Node_collides_with_point(Node *self, Vec2 point);
void Node_set_offset(Node *self, Vec2 point, const char *type); // 'Click' or 'Hover'
void Node_drag(Node *self);
Node *Node_can_drag(Node *self);
void Node_stop_drag(Node *self);
void Node_hover(Node *self);
void Node_stop_hover(Node *self);
void Node_put_focused_cursor(Node *self, float *x, float *y);
void Node_set_container(Node *self, Node *container);
void Node_translate_container(Node *self);
void Node_remove(Node *self);
float Node_fast_mid_dist(Node *self, Node *other_node);
void Node_release(Node *self, void *dragged);
void Node_click(Node *self);
void Node_animate(Node *self, float dt);
void Node_update(Node *self, float dt);

// ---------------------------------------------------------------- Moveable (engine/moveable.lua)
typedef enum { ROLE_MAJOR, ROLE_MINOR, ROLE_GLUED } RoleType; // 'Major', 'Minor', 'Glued'
typedef enum { BOND_STRONG, BOND_WEAK } Bond;                  // 'Strong', 'Weak'
RoleType role_type_of(const char *name);                       // from the Lua's string
Bond bond_of(const char *name);
// What Moveable:get_major() returns: the major at the top and this moveable's offset from it
typedef struct { Moveable *major; Vec2 offset; } MajorTab;
struct Moveable {
    Node node;
    Transform VT;        // the visible transform, which eases towards T
    struct { float x, y, r, scale, mag; } velocity;
    struct {
        RoleType role_type;
        Vec2 offset;
        Moveable *major;      // NULL for a Major
        Moveable *draw_major; // self by default
        Bond xy_bond, wh_bond, r_bond, scale_bond;
    } role;
    struct {
        const char *type;      // 'a', 'cm', 'tli'...: letters as the Lua
        const char *prev_type;
        Vec2 offset, prev_offset;
        bool lr_clamp;
        struct { bool a, m, c, b, t, l, r, i; } type_list; // which letters type has (set when type changes)
    } alignment;
    struct { bool x, y; } pinch;
    float last_moved, last_aligned;
    bool static_rotation;
    Vec2 offset;
    Moveable *Mid;       // self by default
    Vec2 shadow_parrallax, layered_parallax;
    float shadow_height;
    bool zoom;           // self.zoom (sprites set it)
    bool STATIONARY, NEW_ALIGNMENT;
    // self.juice: nil when !juice.on
    struct { bool on, handled_elsewhere; float scale, scale_amt, r, r_amt, start_time, end_time; } juice;
    // self.FRAME.MAJOR, this frame's get_major() result; valid for FRAME.MOVE's frame only
    struct { bool have; MajorTab tab; } frame_major;
    float mouse_damping; // self.mouse_damping, 0 as nil
    // not the Lua's: config.refresh_movement as Moveable_move last read it, valid while node.config is this table and
    // its stamp (core.h, tstamp) this one. is_true: the value is exactly `true`. Read with Moveable_refresh_movement.
    struct { Table *config; uint64_t stamp; bool refresh_movement, is_true; } cfg_snap;
    // not the Lua's: the shader's `time` of this node as a draw_major (Sprite_draw_shader: a function of node.ID
    // alone), kept with the ID it was computed for (0: none yet)
    int shader_time_id; float shader_time;
};
// Moveable(X, Y, W, H) or Moveable(args table as Node's)
void Moveable_init(Moveable *self, float X, float Y, float W, float H);
void Moveable_init_args(Moveable *self, Table *args);
void Moveable_draw(Moveable *self);
// args: {major=, type=, offset={x=,y=}, bond=, xy_bond=, wh_bond=, r_bond=, scale_bond=, lr_clamp=}
void Moveable_set_alignment(Moveable *self, Table *args);
void Moveable_align_to_major(Moveable *self);
void Moveable_hard_set_T(Moveable *self, float X, float Y, float W, float H);
void Moveable_hard_set_VT(Moveable *self);
void Moveable_drag(Moveable *self, const Vec2 *offset);
void Moveable_juice_up(Moveable *self, float amount, Val rot_amt);
void Moveable_move_juice(Moveable *self, float dt);
void Moveable_move(Moveable *self, float dt);
void Moveable_lr_clamp(Moveable *self);
void Moveable_glue_to_major(Moveable *self, Moveable *major_tab);
void Moveable_move_with_major(Moveable *self, float dt);
void Moveable_move_xy(Moveable *self, float dt);
void Moveable_move_scale(Moveable *self, float dt);
void Moveable_move_wh(Moveable *self, float dt);
void Moveable_move_r(Moveable *self, float dt, float vel_x); // vel: the velocity whose x tilts it
void Moveable_calculate_parrallax(Moveable *self);
// args: {role_type=, offset=, major=, xy_bond=, wh_bond=, r_bond=, scale_bond=, draw_major=}
void Moveable_set_role(Moveable *self, Table *args);
MajorTab Moveable_get_major(Moveable *self);
void Moveable_remove(Moveable *self);

// ---------------------------------------------------------------- Sprite (engine/sprite.lua)
// An entry of G.ASSET_ATLAS / G.ANIMATION_ATLAS / G.ASSET_ATLAS of fonts (game.lua, set_render_settings)
typedef struct Atlas {
    const char *name;
    Image *image;
    float px, py;      // the size of a sprite of it, in texels
    int frames;        // animation atlases
    const char *type;  // as the Lua sets it, or NULL
} Atlas;
struct Sprite {
    Moveable moveable;
    Atlas *atlas;
    Vec2 scale;          // {x = atlas.px, y = atlas.py}
    float scale_mag;
    Vec2 sprite_pos, sprite_pos_copy;
    Quad *sprite;        // the quad of the atlas it draws
    float image_dims[2];
    Table *draw_steps;   // define_draw_steps' list, or NULL
    float *video_dims;   // NULL (videos are not played)
    // not the Lua's: what Sprite_draw_self reads of dyn every frame (sprite_pos and its x, y, video, shader_tab), kept
    // while the stamps (core.h, tstamp) of dyn and of the sprite_pos table are these. Filled by Sprite_dyn_snap.
    struct { uint64_t stamp, pos_stamp; Table *sprite_pos; float x, y; bool video, shader_tab; } dyn_snap;
};
void Sprite_init(Sprite *self, float X, float Y, float W, float H, Atlas *new_sprite_atlas, Table *sprite_pos);
void Sprite_reset(Sprite *self);
void Sprite_set_sprite_pos(Sprite *self, Table *sprite_pos);
void Sprite_get_pos_pixel(Sprite *self, float out[4]);
const float *Sprite_get_image_dims(Sprite *self);
void Sprite_define_draw_steps(Sprite *self, Table *draw_step_definitions);
// _shader NULL: 'dissolve'. _shadow_height < 0: nil. _send: NULL, or the {a, b} pair / the custom shader's list.
// other_obj NULL: none; ms, mr, mx, my: NIL as nil. tilt_shadow < 0: nil.
void Sprite_draw_shader(Sprite *self, const char *_shader, float _shadow_height, Table *_send, bool _no_tilt, Sprite *other_obj,
                        Val ms, Val mr, Val mx, Val my, bool custom_shader, float tilt_shadow);
void Sprite_draw_self(Sprite *self, const float *overlay);
void Sprite_draw(Sprite *self, const float *overlay);
void Sprite_draw_from(Sprite *self, Sprite *other_obj, Val ms, Val mr, Val mx, Val my);
void Sprite_remove(Sprite *self);

// ---------------------------------------------------------------- AnimatedSprite (engine/animatedsprite.lua)
struct AnimatedSprite {
    Sprite sprite;
    struct { float x, y, w, h; int frames, current; } animation;
    float frame_offset;
    struct { int current, frames; float w, h; } current_animation;
    // not the Lua's: dyn.offset_seconds and dyn.float (truthy) as AnimatedSprite_animate last read them, valid while
    // dyn has this stamp (core.h, tstamp)
    struct { uint64_t stamp; num offset_seconds; bool float_; } anim_snap;
};
void AnimatedSprite_init(AnimatedSprite *self, float X, float Y, float W, float H, Atlas *new_sprite_atlas, Table *sprite_pos);
void AnimatedSprite_rescale(AnimatedSprite *self);
void AnimatedSprite_reset(AnimatedSprite *self);
void AnimatedSprite_set_sprite_pos(AnimatedSprite *self, Table *sprite_pos);
void AnimatedSprite_get_pos_pixel(AnimatedSprite *self, float out[4]);
void AnimatedSprite_draw_self(AnimatedSprite *self, const float *overlay);
void AnimatedSprite_animate(AnimatedSprite *self, float dt);
void AnimatedSprite_remove(AnimatedSprite *self);

// ---------------------------------------------------------------- Particles (engine/particles.lua)
typedef struct {
    bool draw;
    float dir, facing, size, age, velocity, r_vel, e_prev, e_curr, e_vel, scale, visible_scale, time;
    bool e_vel_set;      // e_vel is nil until the first move
    const float *colour;
    Vec2 offset;
} Particle;
struct Particles {
    Moveable moveable;
    bool fill;
    float padding, timer, last_real_time, last_drawn, lifespan, fade_alpha, speed, vel_variation, scale;
    const char *timer_type; // 'REAL' or 'TOTAL': a key of G.TIMERS
    num max;
    int pulse_max, pulsed;
    Particle *particles;  // self.particles, in order
    int nparticles, room;
    Table *colours;       // array of P(float *) colours
};
// config: {fill=, padding=, attach=node, timer=, timer_type=, lifespan=, speed=, max=, pulse_max=, vel_variation=,
// scale=, colours={...}, initialize=}
void Particles_init(Particles *self, float X, float Y, float W, float H, Table *config);
void Particles_update(Particles *self, float dt);
void Particles_move(Particles *self, float dt);
void Particles_fade(Particles *self, num delay, float to);
// self.config.refresh_movement (truthy), through cfg_snap
void Moveable_cfg_snap_fill(Moveable *self);
static inline bool Moveable_refresh_movement(Moveable *self) {
    if (self->cfg_snap.config != self->node.config || self->cfg_snap.stamp != tstamp(self->node.config)) Moveable_cfg_snap_fill(self);
    return self->cfg_snap.refresh_movement;
} // to < 0: the default 1
void Particles_draw(Particles *self, float alpha);            // alpha < 0: the default 1
void Particles_remove(Particles *self);
#endif
