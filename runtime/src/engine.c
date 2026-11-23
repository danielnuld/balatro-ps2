// The game's hot engine functions in C: Moveable's move family, Node:collides_with_point and the
// point helpers of engine/moveable.lua, engine/node.lua and functions/misc_functions.lua (Balatro 1.0.1o). Each one does the same operations in the
// same order as the Lua it replaces, on the same tables, so the game cannot tell; what is not worth translating
// (or a case a function does not cover) calls the game's own Lua. boot.lua installs them over the classes after
// the game loads and checks no subclass overrides them; runtime/test/native.sh compares the two.
//
// ponytail: this exists because PUC Lua on the EE runs a frame of a run in ~150 ms (docs/perf.md). It reads the
// tables through Lua's internals (vendored, so the layout is ours): no API stack traffic, no string hashing.
#define LUA_CORE
#include <math.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"

#include "gfx.h"

#define KEYS                                                                                                       \
    X(G) X(T) X(VT) X(x) X(y) X(w) X(h) X(r) X(scale) X(velocity) X(role) X(role_type) X(major) X(offset)         \
    X(xy_bond) X(r_bond) X(alignment) X(type) X(prev_type) X(prev_offset) X(type_list) X(a) X(m) X(c) X(b)        \
    X(t) X(l) X(i) X(Mid) X(FRAME) X(MOVE) X(MAJOR) X(OLD_MAJOR) X(created_on_pause) X(CALCING) X(STATIONARY)     \
    X(NEW_ALIGNMENT) X(config) X(refresh_movement) X(juice) X(handled_elsewhere) X(end_time) X(start_time)        \
    X(scale_amt) X(r_amt) X(lr_clamp) X(zoom) X(states) X(drag) X(hover) X(is) X(pinch) X(shadow_parrallax)       \
    X(Glued) X(Minor) X(Major) X(Weak) X(FRAMES) X(SETTINGS) X(paused) X(TIMERS) X(REAL) X(exp_times) X(xy)      \
    X(max_vel) X(ROOM) X(scale_bond) X(wh_bond) X(Strong) X(layered_parallax) X(temp_offs) X(ARGS) X(get_major)   \
    X(REFRESH_FRAME_MAJOR_CACHE) X(container) X(CT) X(collides_with_point_point)                                  \
    X(collides_with_point_translation) X(collides_with_point_rotation) X(COLLISION_BUFFER) X(cos) X(sin) X(TILESCALE) X(TILESIZE) X(parent) X(UIT) X(B) X(C)       \
    X(R) X(ROOT) X(visible) X(force_focus) X(force_collision) X(button_UIE) X(button) X(collide) X(can) X(colour)  \
    X(shadow) X(GRAPHICS) X(shadows) X(On) X(text_parallax) X(sx) X(sy) X(lang) X(font) X(FONTSCALE)               \
    X(TEXT_OFFSET) X(squish) X(vert) X(text_drawable) X(shadow_colour) X(emboss) X(button_delay) X(progress_bar)   \
    X(button_colours) X(last_clicked) X(UI) X(HOVER) X(outline) X(focus) X(chosen) X(DEBUG) X(under_overlay)       \
    X(focus_timer) X(pixellated_rect) X(vertices) X(parallax) X(sw) X(sh) X(progress) X(DRAW_HASH) X(line)        \
    X(line_emboss) X(fill) X(children) X(particle_effect) X(strings) X(focused_string) X(W_offset) X(H_offset)   \
    X(text_offset) X(spacing) X(colours) X(letters) X(min_cycle_time) X(pop_in) X(letter) X(dims) X(prefix)        \
    X(suffix) X(draw_shadow_norm) X(draw_layer) X(h_popup) X(alert) X(draw_self) X(draw_after) X(draw_children)  \
    X(FUNC_TRACKER) X(button_temp) X(button_clicked) X(func) X(FUNCS) X(text) X(ref_table) X(ref_value)            \
    X(prev_value) X(O) X(draw_major) X(sprite_pos) X(sprite_pos_copy) X(video) X(atlas) X(image) X(sprite)        \
    X(BRUTE_OVERLAY) X(WHITE) X(scale_mag) X(draw_from_offset) X(vortex) X(object) X(focus_with_object)                 \
    X(object_focus_timer) X(UIBox) X(CONTROLLER) X(is_cursor_down) X(button_dist) X(TEXT_INACTIVE)       X(dissolve) X(dissolve_colours) X(ID) \
    X(hover_tilt) X(tilt_var) X(mx) X(my) X(mouse_damping) X(CANV_SCALE) X(cursor_position) X(reduced_motion) \
    X(ability) X(name) X(set) X(consumeable) X(max_highlighted) X(sticker_run) X(flipping) X(sprite_facing) X(f2b) \
    X(b2f) X(back) X(front) X(Joker) X(Voucher) X(Edition) X(Booster) X(focused_ui) X(area) X(collection) X(center) \
    X(STAGE) X(STAGES) X(RUN) X(perma_debuff) X(debuff) X(jokers) X(consumeables) X(bypass_lock) \
    X(bypass_discovery_center) X(bypass_discovery_ui) X(sell_cost_label) X(facing) X(sell_cost) X(deck) \
    X(view_deck) X(deck_height) X(cards) X(shuffle_amt) X(rank) X(STATES) \
    X(send_to_shader) X(amt) X(shared_shadow) X(no_shadow) X(effect) X(greyed) X(discard) X(highlighted) \
    X(play) X(shadow_height) X(hand) X(particles) X(price) X(buy_button) X(buy_and_use_button) X(use_button) \
    X(edition) X(negative) X(holo) X(foil) X(polychrome) X(negative_shine) X(played) X(discovered) \
    X(unlocked) X(demo) X(seal) X(eternal) X(rental) X(perishable) X(sticker) X(run_stake_stickers) \
    X(soul_pos) X(back_overlay) X(draw) X(draw_shader) X(draw_boundingrect) X(dx) X(dy) X(hover_offset) \
    X(ambient_tilt) X(card) X(both) X(floating_sprite) X(soul_parts) X(Spectral) X(VIEWING_DECK) X(TAROT_INTERRUPT) \
    X(STATE) X(invisible_area_types) X(SHOP) X(TAROT_PACK) X(SPECTRAL_PACK) X(STANDARD_PACK) X(BUFFOON_PACK) X(PLANET_PACK) \
    X(ROUND_EVAL) X(BLIND_SELECT) X(shop) X(shop_vouchers) X(area_uibox) X(draw_layers) X(thin_draw) X(focused) \
    X(target) X(dragging) X(joker) X(title_2) X(title) X(voucher) X(HID) X(controller) \
    X(SELECTING_HAND) X(peek_deck) X(deck_preview) X(buttons)
#define X(name) K_##name,
enum { KEYS K_COUNT };
#undef X
#define X(name) #name,
static const char *const key_names[] = {KEYS};
#undef X
static TString *K[K_COUNT];
#define k(name) K[K_##name]

// o[key] as the VM reads it: the table, then its classes (__index tables). An __index function is not expected
// on these objects and aborts, as does indexing what is not a table.
static __attribute__((noinline)) const TValue *idx_slow(lua_State *L, const TValue *o, TString *key) {
    for (;;) {
        if (!ttistable(o)) luaL_error(L, "native: '%s' of a %s value", getstr(key), lua_typename(L, ttype(o)));
        Table *tab = hvalue(o);
        const TValue *v = luaH_getstr_inline(tab, key);
        if (!ttisnil(v) || !tab->metatable) return v;
        const TValue *tm = fasttm(L, tab->metatable, TM_INDEX);
        if (!tm) return v;
        if (!ttistable(tm)) luaL_error(L, "native: '%s' behind an __index function", getstr(key));
        o = tm;
    }
}
// The usual case inline at every call: the table itself holds the key, or has no class to ask.
static inline const TValue *idx(lua_State *L, const TValue *o, TString *key) {
    if (ttistable(o)) {
        const TValue *v = luaH_getstr_inline(hvalue(o), key);
        if (!ttisnil(v) || !hvalue(o)->metatable) return v;
    }
    return idx_slow(L, o, key);
}
// A sub-table by value: pointers into a table's slots do not survive an insert into it.
static TValue sub(lua_State *L, const TValue *o, TString *key) { return *idx(L, o, key); }
static lua_Number num(lua_State *L, const TValue *o, TString *key) {
    const TValue *v = idx(L, o, key);
    if (!ttisnumber(v)) luaL_error(L, "native: '%s' is a %s, not a number", getstr(key), lua_typename(L, ttype(v)));
    return nvalue(v);
}
static int truthy(lua_State *L, const TValue *o, TString *key) { return !l_isfalse(idx(L, o, key)); }
static int is(lua_State *L, const TValue *o, TString *key, TString *s) {
    const TValue *v = idx(L, o, key);
    return ttisstring(v) && rawtsvalue(v) == s; // every Lua 5.1 string is interned
}
static void setn(lua_State *L, const TValue *o, TString *key, lua_Number n) {
    setnvalue(luaH_setstr(L, hvalue(o), key), n);
}
static void setb(lua_State *L, const TValue *o, TString *key, int b) { setbvalue(luaH_setstr(L, hvalue(o), key), b); }
static void setv(lua_State *L, const TValue *o, TString *key, const TValue *v) { // o[key] = v, nil included
    if (ttisnil(v)) {
        TValue *slot = (TValue *)luaH_getstr(hvalue(o), key);
        if (slot != luaO_nilobject) setnilvalue(slot);
        return;
    }
    setobj2t(L, luaH_setstr(L, hvalue(o), key), v);
    luaC_barriert(L, hvalue(o), v);
}
static lua_Number mx(lua_Number a, lua_Number b) { return b > a ? b : a; } // math.max(a, b)
static lua_Number mn(lua_Number a, lua_Number b) { return b < a ? b : a; } // math.min(a, b)

static TValue game(lua_State *L) { return // the global G
    *luaH_getstr(hvalue(gt(L)), k(G)); }
static void push(lua_State *L, const TValue *v) {
    setobj2s(L, L->top, v);
    L->top++; // inside the LUA_MINSTACK slots a C function owns
}
// obj:name(dt) through the VM (a method of the game's, or one a subclass overrides)
// A method's name as Lua's string for it, found once: the key is the literal's address
static TString *interned(lua_State *L, const char *name) {
    static struct { const char *name; TString *s; } known[64];
    int i;
    for (i = 0; i < 64 && known[i].name; i++)
        if (known[i].name == name) return known[i].s;
    TString *str = luaS_new(L, name);
    if (i < 64) luaS_fix(str), known[i].name = name, known[i].s = str;
    return str;
}
static void method(lua_State *L, const TValue *obj, const char *name, lua_Number dt) {
    push(L, idx(L, obj, interned(L, name)));
    push(L, obj);
    lua_pushnumber(L, dt);
    lua_call(L, 2, 0);
}

// The sine and cosine of an angle; the one asked for last is kept. Node:collides_with_point turns its point by
// the container's rotation plus a quarter turn for every node under the cursor's test, every frame, and that
// rotation is nearly always 0: two of libm's functions in software doubles each time (docs/perf.md).
static void sin_cos(lua_Number angle, lua_Number *sine, lua_Number *cosine) {
    static lua_Number of, s, c;
    static int have;
    if (!have || memcmp(&of, &angle, sizeof of)) of = angle, s = sin(angle), c = cos(angle), have = 1;
    *sine = s, *cosine = c;
}
static int same(lua_Number a, lua_Number b) { return !memcmp(&a, &b, sizeof a); } // the same bits (a zero's sign too)
// self.juice, self.zoom: fields of the object alone, which no class of the game has. Read from the object's own
// table: asked the usual way, a moveable without one (nearly all of them) asks each of its four classes.
static TValue own(const TValue *self, TString *key) { return *luaH_getstr(hvalue(self), key); }

// Moveable:move_juice(dt), moveable.lua:267
static void move_juice(lua_State *L, const TValue *self) {
    TValue j = own(self, k(juice));
    if (l_isfalse(&j) || truthy(L, &j, k(handled_elsewhere))) return;
    TValue g = game(L), timers = sub(L, &g, k(TIMERS));
    lua_Number real = num(L, &timers, k(REAL)), end = num(L, &j, k(end_time)), start = num(L, &j, k(start_time));
    if (end < real) {
        setv(L, self, k(juice), luaO_nilobject);
        return;
    }
    setn(L, &j, k(scale), num(L, &j, k(scale_amt)) * sin(50.8 * (real - start)) * mx(0, pow((end - real) / (end - start), 3)));
    setn(L, &j, k(r), num(L, &j, k(r_amt)) * sin(40.8 * (real - start)) * mx(0, pow((end - real) / (end - start), 2)));
}

// Moveable:move_xy(dt), moveable.lua:405
static void move_xy(lua_State *L, const TValue *self, lua_Number dt) {
    TValue T = sub(L, self, k(T)), VT = sub(L, self, k(VT)), vel = sub(L, self, k(velocity));
    lua_Number Tx = num(L, &T, k(x)), Ty = num(L, &T, k(y)), VTx = num(L, &VT, k(x)), VTy = num(L, &VT, k(y));
    lua_Number vx = num(L, &vel, k(x)), vy = num(L, &vel, k(y));
    if (!((Tx != VTx || fabs(vx) > 0.01) || (Ty != VTy || fabs(vy) > 0.01))) return;
    TValue g = game(L), et = sub(L, &g, k(exp_times));
    lua_Number xy = num(L, &et, k(xy)), max_vel = num(L, &et, k(max_vel));
    vx = xy * vx + (1 - xy) * (Tx - VTx) * 35 * dt;
    vy = xy * vy + (1 - xy) * (Ty - VTy) * 35 * dt;
    if (vx * vx + vy * vy > max_vel * max_vel) {
        lua_Number actual_vel = sqrt(vx * vx + vy * vy);
        vx = max_vel * vx / actual_vel;
        vy = max_vel * vy / actual_vel;
    }
    setb(L, self, k(STATIONARY), 0);
    VTx = VTx + vx;
    VTy = VTy + vy;
    if (fabs(VTx - Tx) < 0.01 && fabs(vx) < 0.01) VTx = Tx, vx = 0;
    if (fabs(VTy - Ty) < 0.01 && fabs(vy) < 0.01) VTy = Ty, vy = 0;
    setn(L, &vel, k(x), vx);
    setn(L, &vel, k(y), vy);
    setn(L, &VT, k(x), VTx);
    setn(L, &VT, k(y), VTy);
}

// self.juice and self.juice.<field> or 0
static lua_Number juice_or_0(lua_State *L, const TValue *self, TString *field, lua_Number times) {
    TValue j = own(self, k(juice));
    if (l_isfalse(&j)) return 0;
    const TValue *v = idx(L, &j, field);
    if (!ttisnumber(v)) luaL_error(L, "native: juice.%s is not a number", getstr(field));
    return nvalue(v) * times;
}

// Moveable:move_scale(dt), moveable.lua:423
static void move_scale(lua_State *L, const TValue *self) {
    TValue T = sub(L, self, k(T)), VT = sub(L, self, k(VT)), vel = sub(L, self, k(velocity));
    lua_Number zoom = 0;
    TValue zooms = own(self, k(zoom));
    if (!l_isfalse(&zooms)) {
        TValue states = sub(L, self, k(states)), drag = sub(L, &states, k(drag)), hover = sub(L, &states, k(hover));
        zoom = (truthy(L, &drag, k(is)) ? 0.1 : 0) + (truthy(L, &hover, k(is)) ? 0.05 : 0);
    }
    lua_Number des_scale = num(L, &T, k(scale)) + zoom + juice_or_0(L, self, k(scale), 1);
    lua_Number VTscale = num(L, &VT, k(scale)), vscale = num(L, &vel, k(scale));
    if (des_scale != VTscale || fabs(vscale) > 0.001) {
        setb(L, self, k(STATIONARY), 0);
        TValue g = game(L), et = sub(L, &g, k(exp_times));
        lua_Number es = num(L, &et, k(scale));
        vscale = es * vscale + (1 - es) * (des_scale - VTscale);
        setn(L, &vel, k(scale), vscale);
        setn(L, &VT, k(scale), VTscale + vscale);
    }
}

// Moveable:move_wh(dt), moveable.lua:434
static void move_wh(lua_State *L, const TValue *self, lua_Number dt) {
    TValue T = sub(L, self, k(T)), VT = sub(L, self, k(VT)), pinch = sub(L, self, k(pinch));
    int px = truthy(L, &pinch, k(x)), py = truthy(L, &pinch, k(y));
    lua_Number Tw = num(L, &T, k(w)), Th = num(L, &T, k(h)), VTw = num(L, &VT, k(w)), VTh = num(L, &VT, k(h));
    if ((Tw != VTw && !px) || (Th != VTh && !py) || (VTw > 0 && px) || (VTh > 0 && py)) {
        setb(L, self, k(STATIONARY), 0);
        VTw = VTw + (8 * dt) * (px ? -1 : 1) * Tw;
        VTh = VTh + (8 * dt) * (py ? -1 : 1) * Th;
        setn(L, &VT, k(w), mx(mn(VTw, Tw), 0));
        setn(L, &VT, k(h), mx(mn(VTh, Th), 0));
    }
}

// Moveable:move_r(dt, vel), moveable.lua:447
static void move_r(lua_State *L, const TValue *self, lua_Number dt, const TValue *vel_arg) {
    TValue T = sub(L, self, k(T)), VT = sub(L, self, k(VT)), vel = sub(L, self, k(velocity));
    lua_Number Tr = num(L, &T, k(r));
    lua_Number vx = num(L, vel_arg, k(x));
    // (at rest, 0.015 * 0 / dt is a zero, which changes nothing of a T.r that is not one)
    lua_Number des_r = (same(vx, 0) && dt > 0 && Tr != 0 ? Tr : Tr + 0.015 * vx / dt) + juice_or_0(L, self, k(r), 2);
    lua_Number VTr = num(L, &VT, k(r)), vr = num(L, &vel, k(r));
    if (des_r != VTr || fabs(vr) > 0.001) {
        setb(L, self, k(STATIONARY), 0);
        TValue g = game(L), et = sub(L, &g, k(exp_times));
        lua_Number er = num(L, &et, k(r));
        vr = er * vr + (1 - er) * (des_r - VTr);
        VTr = VTr + vr;
        setn(L, &vel, k(r), vr);
        setn(L, &VT, k(r), VTr);
    }
    if (same(VTr, Tr) && same(vr, 0)) return; // (what the lines below would write is there)
    if (fabs(VTr - Tr) < 0.001 && fabs(vr) < 0.001) {
        setn(L, &VT, k(r), Tr);
        setn(L, &vel, k(r), 0);
    }
}

// Moveable:calculate_parrallax(), moveable.lua:459
static void calculate_parrallax(lua_State *L, const TValue *self) {
    TValue g = game(L), room = sub(L, &g, k(ROOM));
    if (l_isfalse(&room)) return;
    TValue roomT = sub(L, &room, k(T)), T = sub(L, self, k(T)), sp = sub(L, self, k(shadow_parrallax));
    lua_Number rw = num(L, &roomT, k(w));
    setn(L, &sp, k(x), (num(L, &T, k(x)) + num(L, &T, k(w)) * 0.5 - rw * 0.5) / (rw * 0.5) * 1.5); // (a half is exact either way)
}

// Moveable:lr_clamp(), moveable.lua:322
static void lr_clamp(lua_State *L, const TValue *self) {
    TValue T = sub(L, self, k(T)), VT = sub(L, self, k(VT)), g = game(L), room = sub(L, &g, k(ROOM));
    TValue roomT = sub(L, &room, k(T));
    if (num(L, &T, k(x)) < 0) setn(L, &T, k(x), 0);
    if (num(L, &VT, k(x)) < 0) setn(L, &VT, k(x), 0);
    lua_Number rw = num(L, &roomT, k(w));
    if (num(L, &T, k(x)) + num(L, &T, k(w)) > rw) setn(L, &T, k(x), rw - num(L, &T, k(w)));
    rw = num(L, &roomT, k(w));
    if (num(L, &VT, k(x)) + num(L, &VT, k(w)) > rw) setn(L, &VT, k(x), rw - num(L, &VT, k(w)));
}

// Moveable:align_to_major(), moveable.lua:114. Returns 0 when the alignment type changed: that case builds the
// type list with string.find, and the caller runs the game's Lua for it.
static int align_to_major(lua_State *L, const TValue *self) {
    TValue al = sub(L, self, k(alignment));
    if (!luaO_rawequalObj(idx(L, &al, k(type)), idx(L, &al, k(prev_type)))) return 0;
    TValue off = sub(L, &al, k(offset)), prev = sub(L, &al, k(prev_offset));
    if (luaO_rawequalObj(idx(L, &prev, k(x)), idx(L, &off, k(x))) &&
        luaO_rawequalObj(idx(L, &prev, k(y)), idx(L, &off, k(y))))
        return 1;
    setb(L, self, k(NEW_ALIGNMENT), 1);
    TValue list = sub(L, &al, k(type_list)), role = sub(L, self, k(role)), major = sub(L, &role, k(major));
    if (truthy(L, &list, k(a)) || l_isfalse(&major)) return 1;
    TValue mT = sub(L, &major, k(T)), T = sub(L, self, k(T)), ro = sub(L, &role, k(offset));
    int inner = truthy(L, &list, k(i));
    if (truthy(L, &list, k(m)) || truthy(L, &list, k(c))) {
        TValue mid = sub(L, self, k(Mid)), midT = sub(L, &mid, k(T));
        if (truthy(L, &list, k(m)))
            setn(L, &ro, k(x), 0.5 * num(L, &mT, k(w)) - num(L, &midT, k(w)) / 2 + num(L, &off, k(x)) - num(L, &midT, k(x)) + num(L, &T, k(x)));
        if (truthy(L, &list, k(c)))
            setn(L, &ro, k(y), 0.5 * num(L, &mT, k(h)) - num(L, &midT, k(h)) / 2 + num(L, &off, k(y)) - num(L, &midT, k(y)) + num(L, &T, k(y)));
    }
    if (truthy(L, &list, k(b)))
        setn(L, &ro, k(y), inner ? num(L, &off, k(y)) + num(L, &mT, k(h)) - num(L, &T, k(h)) : num(L, &off, k(y)) + num(L, &mT, k(h)));
    if (truthy(L, &list, k(r)))
        setn(L, &ro, k(x), inner ? num(L, &off, k(x)) + num(L, &mT, k(w)) - num(L, &T, k(w)) : num(L, &off, k(x)) + num(L, &mT, k(w)));
    if (truthy(L, &list, k(t)))
        setn(L, &ro, k(y), inner ? num(L, &off, k(y)) : num(L, &off, k(y)) - num(L, &T, k(h)));
    if (truthy(L, &list, k(l)))
        setn(L, &ro, k(x), inner ? num(L, &off, k(x)) : num(L, &off, k(x)) - num(L, &T, k(w)));
    if (!truthy(L, &ro, k(x))) setn(L, &ro, k(x), 0);
    if (!truthy(L, &ro, k(y))) setn(L, &ro, k(y), 0);
    setn(L, &T, k(x), num(L, &mT, k(x)) + num(L, &ro, k(x)));
    setn(L, &T, k(y), num(L, &mT, k(y)) + num(L, &ro, k(y)));
    setv(L, &prev, k(x), idx(L, &off, k(x)));
    setv(L, &prev, k(y), idx(L, &off, k(y)));
    return 1;
}

// o[key] = {} and the new table
static TValue newtab(lua_State *L, const TValue *o, TString *key) {
    lua_newtable(L);
    TValue t = *(L->top - 1);
    setv(L, o, key, &t);
    L->top--; // o holds it now
    return t;
}
// o[key] = o[key] or {}
static TValue sub_or_new(lua_State *L, const TValue *o, TString *key) {
    TValue v = sub(L, o, key);
    return l_isfalse(&v) ? newtab(L, o, key) : v;
}
// dst[key] = src[key]
static void copy(lua_State *L, const TValue *dst, const TValue *src, TString *key) {
    TValue v = sub(L, src, key);
    setv(L, dst, key, &v);
}
// EMPTY(t) of misc_functions.lua:129 for a table that exists: every value to nil
static void empty(const TValue *o) {
    Table *t = hvalue(o);
    for (int i = 0; i < t->sizearray; i++) setnilvalue(&t->array[i]);
    for (int i = 0; i < sizenode(t); i++) setnilvalue(gval(gnode(t, i)));
}
// self.juice and self.juice.<field> or 0
static lua_Number juice_field(lua_State *L, const TValue *self, TString *field) {
    TValue j = sub(L, self, k(juice));
    if (l_isfalse(&j)) return 0;
    const TValue *v = idx(L, &j, field);
    if (l_isfalse(v)) return 0;
    if (!ttisnumber(v)) luaL_error(L, "native: juice.%s is not a number", getstr(field));
    return nvalue(v);
}

// Moveable:get_major(), moveable.lua:482
static TValue get_major(lua_State *L, const TValue *self) {
    TValue role = sub(L, self, k(role));
    if (!is(L, &role, k(role_type), k(Major)) && !luaO_rawequalObj(idx(L, &role, k(major)), self) &&
        !is(L, &role, k(xy_bond), k(Weak)) && !is(L, &role, k(r_bond), k(Weak))) {
        TValue frame = sub(L, self, k(FRAME)), g = game(L);
        if (!truthy(L, &frame, k(MAJOR)) || truthy(L, &g, k(REFRESH_FRAME_MAJOR_CACHE))) {
            TValue fm = sub(L, &frame, k(MAJOR));
            if (l_isfalse(&fm)) {
                fm = sub(L, &frame, k(OLD_MAJOR));
                if (l_isfalse(&fm)) fm = newtab(L, &frame, k(MAJOR));
                else {
                    empty(&fm);
                    setv(L, &frame, k(MAJOR), &fm);
                }
            }
            TValue to = sub(L, self, k(temp_offs));
            if (l_isfalse(&to)) to = newtab(L, self, k(temp_offs));
            else empty(&to);
            TValue rm = sub(L, &role, k(major)), major = get_major(L, &rm);
            copy(L, &fm, &major, k(major));
            TValue off = sub(L, &fm, k(offset));
            if (l_isfalse(&off)) {
                off = to;
                setv(L, &fm, k(offset), &off);
            }
            TValue mo = sub(L, &major, k(offset)), ro = sub(L, &role, k(offset)), lp = sub(L, self, k(layered_parallax));
            lua_Number x = num(L, &mo, k(x)) + num(L, &ro, k(x)) + num(L, &lp, k(x));
            lua_Number y = num(L, &mo, k(y)) + num(L, &ro, k(y)) + num(L, &lp, k(y));
            setn(L, &off, k(x), x);
            setn(L, &off, k(y), y);
        }
        return sub(L, &frame, k(MAJOR));
    }
    TValue args = sub(L, self, k(ARGS)), gm = sub_or_new(L, &args, k(get_major));
    setv(L, &gm, k(major), self);
    TValue off = sub_or_new(L, &gm, k(offset));
    setn(L, &off, k(x), 0);
    setn(L, &off, k(y), 0);
    return gm;
}

// Moveable:glue_to_major(major_tab), moveable.lua:329
static void glue_to_major(lua_State *L, const TValue *self, const TValue *major) {
    TValue mT = sub(L, major, k(T)), mVT = sub(L, major, k(VT)), VT = sub(L, self, k(VT));
    setv(L, self, k(T), &mT);
    lua_Number mx_ = num(L, &mVT, k(x)), mvw = num(L, &mVT, k(w)), mw = num(L, &mT, k(w));
    // (a major at its full width adds a zero: w / w is 1 for any w that is a number and not 0)
    setn(L, &VT, k(x), mvw == mw && mw != 0 && fabs(mw) < 1e300 && mx_ != 0 ? mx_ : mx_ + (0.5 * (1 - mvw / mw) * mw));
    copy(L, &VT, &mVT, k(y));
    copy(L, &VT, &mVT, k(w));
    copy(L, &VT, &mVT, k(h));
    copy(L, &VT, &mVT, k(r));
    copy(L, &VT, &mVT, k(scale));
    copy(L, self, major, k(pinch));
    copy(L, self, major, k(shadow_parrallax));
}

// Moveable:move_with_major(dt), moveable.lua:350. The game keeps the intermediate values in the global MWM,
// which nothing else reads; they are locals here.
static void move_with_major(lua_State *L, const TValue *self, lua_Number dt) {
    TValue role = sub(L, self, k(role));
    if (!is(L, &role, k(role_type), k(Minor))) return;
    TValue rm = sub(L, &role, k(major)), tab = get_major(L, &rm);
    move_juice(L, self);
    TValue m = sub(L, &tab, k(major)), mT = sub(L, &m, k(T)), mVT = sub(L, &m, k(VT)), mo = sub(L, &tab, k(offset));
    TValue ro = sub(L, &role, k(offset)), T = sub(L, self, k(T)), VT = sub(L, self, k(VT));
    lua_Number rox, roy, mr = num(L, &mVT, k(r));
    if (is(L, &role, k(r_bond), k(Weak)) || (mr < 0.0001 && mr > -0.0001)) {
        rox = num(L, &ro, k(x)) + num(L, &mo, k(x));
        roy = num(L, &ro, k(y)) + num(L, &mo, k(y));
    } else {
        lua_Number c, s;
        sin_cos(mr, &s, &c);
        lua_Number w = -num(L, &T, k(w)) / 2 + num(L, &mT, k(w)) / 2, h = -num(L, &T, k(h)) / 2 + num(L, &mT, k(h)) / 2;
        lua_Number ox = num(L, &ro, k(x)) + num(L, &mo, k(x)) - w, oy = num(L, &ro, k(y)) + num(L, &mo, k(y)) - h;
        rox = ox * c - oy * s + w;
        roy = ox * s + oy * c + h;
    }
    setn(L, &T, k(x), num(L, &mT, k(x)) + rox);
    setn(L, &T, k(y), num(L, &mT, k(y)) + roy);

    if (is(L, &role, k(xy_bond), k(Strong))) {
        setn(L, &VT, k(x), num(L, &mVT, k(x)) + rox);
        setn(L, &VT, k(y), num(L, &mVT, k(y)) + roy);
    } else if (is(L, &role, k(xy_bond), k(Weak)))
        move_xy(L, self, dt);

    if (is(L, &role, k(r_bond), k(Strong)))
        setn(L, &VT, k(r), num(L, &T, k(r)) + num(L, &mVT, k(r)) + juice_field(L, self, k(r)));
    else if (is(L, &role, k(r_bond), k(Weak))) {
        TValue vel = sub(L, self, k(velocity));
        move_r(L, self, dt, &vel);
    }

    if (is(L, &role, k(scale_bond), k(Strong)))
        setn(L, &VT, k(scale), num(L, &T, k(scale)) * (num(L, &mVT, k(scale)) / num(L, &mT, k(scale))) + juice_field(L, self, k(scale)));
    else if (is(L, &role, k(scale_bond), k(Weak)))
        move_scale(L, self);

    if (is(L, &role, k(wh_bond), k(Strong))) {
        setn(L, &VT, k(x), num(L, &VT, k(x)) + (0.5 * (1 - num(L, &mVT, k(w)) / num(L, &mT, k(w))) * num(L, &T, k(w))));
        setn(L, &VT, k(w), num(L, &T, k(w)) * (num(L, &mVT, k(w)) / num(L, &mT, k(w))));
        setn(L, &VT, k(h), num(L, &T, k(h)) * (num(L, &mVT, k(h)) / num(L, &mT, k(h))));
    } else if (is(L, &role, k(wh_bond), k(Weak)))
        move_wh(L, self, dt);

    calculate_parrallax(L, self);
}

// Node:collides_with_point(point), node.lua:143. The scratch tables in self.ARGS end with the values the Lua
// leaves in them.
static int l_collides_with_point(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    TValue self = L->base[0], point = L->base[1], cont = sub(L, &self, k(container));
    if (l_isfalse(&cont)) return 0;
    TValue T = sub(L, &self, k(CT));
    if (l_isfalse(&T)) T = sub(L, &self, k(T));
    TValue args = sub(L, &self, k(ARGS));
    TValue p = sub_or_new(L, &args, k(collides_with_point_point));
    TValue t = sub_or_new(L, &args, k(collides_with_point_translation));
    TValue r = sub_or_new(L, &args, k(collides_with_point_rotation));
    TValue states = sub(L, &self, k(states)), hover = sub(L, &states, k(hover)), g = game(L);
    lua_Number b = truthy(L, &hover, k(is)) && truthy(L, &g, k(COLLISION_BUFFER)) ? num(L, &g, k(COLLISION_BUFFER)) : 0;
    lua_Number px = num(L, &point, k(x)), py = num(L, &point, k(y)), tx = 0, ty = 0;
    int t_set = 0;
    if (!luaO_rawequalObj(&cont, &self)) {
        TValue cT = sub(L, &cont, k(T));
        lua_Number cr = num(L, &cT, k(r)), cx = num(L, &cT, k(x)), cy = num(L, &cT, k(y));
        if (fabs(cr) < 0.1) {
            lua_Number cw = num(L, &cT, k(w)), ch = num(L, &cT, k(h));
            px = px + -cw / 2, py = py + -ch / 2;
            lua_Number c, s, ox = px, oy = py;
            sin_cos(cr + 3.14159265358979323846 / 2, &s, &c);
            px = -oy * c + ox * s;
            py = oy * s + ox * c;
            tx = cw / 2 - cx, ty = ch / 2 - cy;
        } else
            tx = -cx, ty = -cy;
        px = px + tx, py = py + ty;
        t_set = 1;
    }
    lua_Number Tx = num(L, &T, k(x)), Ty = num(L, &T, k(y)), Tw = num(L, &T, k(w)), Th = num(L, &T, k(h)), Tr = num(L, &T, k(r));
    if (!(fabs(Tr) < 0.1)) {
        lua_Number c, s;
        sin_cos(Tr + 3.14159265358979323846 / 2, &s, &c);
        setn(L, &r, k(cos), c);
        setn(L, &r, k(sin), s);
        px = px - (Tx + 0.5 * Tw), py = py - (Ty + 0.5 * Th);
        tx = py * c - px * s, ty = py * s + px * c;
        px = tx + (Tx + 0.5 * Tw), py = ty + (Ty + 0.5 * Th);
        t_set = 1;
    }
    setn(L, &p, k(x), px);
    setn(L, &p, k(y), py);
    if (t_set) {
        setn(L, &t, k(x), tx);
        setn(L, &t, k(y), ty);
    }
    if (px >= Tx - b && py >= Ty - b && px <= Tx + Tw + b && py <= Ty + Th + b) {
        lua_pushboolean(L, 1);
        return 1;
    }
    return 0;
}

static int orig_align_to_major; // registry ref of the game's Moveable.align_to_major

// Moveable:move(dt), moveable.lua:278
static int l_move(lua_State *L) {
    TValue self = *L->base;
    // dt may be nil: UIBox:drag passes an undefined global (ui.lua:327), harmless there because the root has
    // already moved this frame; it is asked to be a number only where the Lua first computes with it
    lua_Number dt = lua_tonumber(L, 2);
    TValue g = game(L), frame = sub(L, &self, k(FRAME)), frames = sub(L, &g, k(FRAMES));
    lua_Number now = num(L, &frames, k(MOVE));
    if (num(L, &frame, k(MOVE)) >= now) return 0;
    setv(L, &frame, k(OLD_MAJOR), idx(L, &frame, k(MAJOR)));
    setv(L, &frame, k(MAJOR), luaO_nilobject);
    setn(L, &frame, k(MOVE), now);
    TValue settings = sub(L, &g, k(SETTINGS)); // (the pause first: created_on_pause is asked of every class of self)
    if (truthy(L, &settings, k(paused)) && !truthy(L, &self, k(created_on_pause))) return 0;

    if (!align_to_major(L, &self)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, orig_align_to_major);
        push(L, &self);
        lua_call(L, 1, 0);
    }
    setv(L, &self, k(CALCING), luaO_nilobject);
    TValue role = sub(L, &self, k(role));
    if (is(L, &role, k(role_type), k(Glued))) {
        TValue major = sub(L, &role, k(major));
        if (!l_isfalse(&major)) glue_to_major(L, &self, &major);
    } else if (is(L, &role, k(role_type), k(Minor)) && truthy(L, &role, k(major))) {
        TValue major = sub(L, &role, k(major)), mframe = sub(L, &major, k(FRAME));
        luaL_checknumber(L, 2);
        frames = sub(L, &g, k(FRAMES));
        if (num(L, &mframe, k(MOVE)) < num(L, &frames, k(MOVE))) method(L, &major, "move", dt);
        role = sub(L, &self, k(role));
        major = sub(L, &role, k(major));
        setv(L, &self, k(STATIONARY), idx(L, &major, k(STATIONARY)));
        TValue config = sub(L, &self, k(config));
        if (!truthy(L, &self, k(STATIONARY)) || truthy(L, &self, k(NEW_ALIGNMENT)) ||
            truthy(L, &config, k(refresh_movement)) || !l_isfalse(luaH_getstr(hvalue(&self), k(juice))) ||
            is(L, &role, k(xy_bond), k(Weak)) || is(L, &role, k(r_bond), k(Weak))) {
            setb(L, &self, k(CALCING), 1);
            move_with_major(L, &self, dt);
        }
    } else if (is(L, &role, k(role_type), k(Major))) {
        setb(L, &self, k(STATIONARY), 1);
        luaL_checknumber(L, 2);
        move_juice(L, &self);
        move_xy(L, &self, dt);
        TValue vel = sub(L, &self, k(velocity));
        move_r(L, &self, dt, &vel);
        move_scale(L, &self);
        move_wh(L, &self, dt);
        calculate_parrallax(L, &self);
    }
    TValue al = sub(L, &self, k(alignment));
    if (!l_isfalse(&al) && truthy(L, &al, k(lr_clamp))) lr_clamp(L, &self);
    setb(L, &self, k(NEW_ALIGNMENT), 0);
    return 0;
}

// The same functions as the methods the game's Lua calls by name (move_with_major does).
static int l_move_juice(lua_State *L) { TValue self = *L->base; move_juice(L, &self); return 0; }
static int l_move_xy(lua_State *L) { TValue self = *L->base; move_xy(L, &self, luaL_checknumber(L, 2)); return 0; }
static int l_move_scale(lua_State *L) { TValue self = *L->base; move_scale(L, &self); return 0; }
static int l_move_wh(lua_State *L) { TValue self = *L->base; move_wh(L, &self, luaL_checknumber(L, 2)); return 0; }
static int l_move_r(lua_State *L) {
    luaL_checktype(L, 3, LUA_TTABLE);
    TValue self = *L->base, vel = L->base[2];
    move_r(L, &self, luaL_checknumber(L, 2), &vel);
    return 0;
}
static int l_calculate_parrallax(lua_State *L) { TValue self = *L->base; calculate_parrallax(L, &self); return 0; }
static int l_lr_clamp(lua_State *L) { TValue self = *L->base; lr_clamp(L, &self); return 0; }
static int l_align_to_major(lua_State *L) {
    TValue self = *L->base;
    if (align_to_major(L, &self)) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, orig_align_to_major);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 0);
    return 0;
}

static int l_get_major(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = *L->base, m = get_major(L, &self);
    push(L, &m);
    return 1;
}
static int l_glue_to_major(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    TValue self = *L->base, major = L->base[1];
    glue_to_major(L, &self, &major);
    return 0;
}
static int l_move_with_major(lua_State *L) {
    TValue self = *L->base;
    move_with_major(L, &self, luaL_checknumber(L, 2));
    return 0;
}

// point_translate(_T, delta), misc_functions.lua:824
static int l_point_translate(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue p = L->base[0], d = L->base[1];
    setn(L, &p, k(x), num(L, &p, k(x)) + num(L, &d, k(x)));
    setn(L, &p, k(y), num(L, &p, k(y)) + num(L, &d, k(y)));
    return 0;
}
// point_rotate(_T, angle), misc_functions.lua:829
static int l_point_rotate(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue p = L->base[0];
    lua_Number angle = luaL_checknumber(L, 2);
    lua_Number c, s;
    sin_cos(angle + 3.14159265358979323846 / 2, &s, &c);
    lua_Number ox = num(L, &p, k(x)), oy = num(L, &p, k(y));
    setn(L, &p, k(x), -oy * c + ox * s);
    setn(L, &p, k(y), oy * s + ox * c);
    return 0;
}

// (t and t.layered_parallax and t.layered_parallax[axis]) or nil
static const TValue *parallax(lua_State *L, const TValue *t, TString *axis) {
    if (l_isfalse(t)) return luaO_nilobject;
    TValue lp = sub(L, t, k(layered_parallax));
    return l_isfalse(&lp) ? luaO_nilobject : idx(L, &lp, axis);
}
// prep_draw(moveable, scale, rotate, offset), misc_functions.lua:792; rotate and offset may be nil
static void prep_draw(lua_State *L, const TValue *m, lua_Number scale, const TValue *rotate, const TValue *offset) {
    TValue g = game(L), VT = sub(L, m, k(VT)), parent = sub(L, m, k(parent));
    lua_Number xy[2];
    gfx_push(L);
    gfx_scale(num(L, &g, k(TILESCALE)) * num(L, &g, k(TILESIZE)), num(L, &g, k(TILESCALE)) * num(L, &g, k(TILESIZE)));
    for (int i = 0; i < 2; i++) {
        TString *axis = i ? k(y) : k(x);
        const TValue *lp = parallax(L, m, axis);
        if (l_isfalse(lp)) lp = parallax(L, &parent, axis);
        const TValue *o = l_isfalse(offset) ? luaO_nilobject : idx(L, offset, axis);
        if ((!l_isfalse(lp) && !ttisnumber(lp)) || (!l_isfalse(o) && !ttisnumber(o))) luaL_error(L, "native: prep_draw offsets");
        xy[i] = num(L, &VT, axis) + num(L, &VT, i ? k(h) : k(w)) / 2 + (l_isfalse(o) ? 0 : nvalue(o)) + (l_isfalse(lp) ? 0 : nvalue(lp));
    }
    gfx_translate(xy[0], xy[1]);
    if (!l_isfalse(rotate) && !ttisnumber(rotate)) luaL_error(L, "native: prep_draw rotate");
    if (num(L, &VT, k(r)) != 0 || truthy(L, m, k(juice)) || !l_isfalse(rotate))
        gfx_rotate(num(L, &VT, k(r)) + (l_isfalse(rotate) ? 0 : nvalue(rotate)));
    gfx_translate(-scale * num(L, &VT, k(w)) * num(L, &VT, k(scale)) / 2, -scale * num(L, &VT, k(h)) * num(L, &VT, k(scale)) / 2);
    gfx_scale(num(L, &VT, k(scale)) * scale, num(L, &VT, k(scale)) * scale);
}
static int l_prep_draw(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Number scale = luaL_checknumber(L, 2);
    lua_settop(L, 4);
    TValue m = L->base[0], rotate = L->base[2], offset = L->base[3];
    prep_draw(L, &m, scale, &rotate, &offset);
    return 0;
}

// love.graphics.setColor(c) of a colour table
static void set_colour(lua_State *L, const TValue *c) {
    lua_Number v[4];
    if (!ttistable(c)) luaL_error(L, "native: a colour that is not a table");
    for (int i = 0; i < 4; i++) {
        const TValue *n = luaH_getnum(hvalue(c), i + 1);
        if (!ttisnumber(n) && !(i == 3 && ttisnil(n))) luaL_error(L, "native: a colour component that is not a number");
        v[i] = ttisnumber(n) ? nvalue(n) : 1;
    }
    gfx_colour(v[0], v[1], v[2], v[3]);
}
// add_to_drawhash(obj), misc_functions.lua:669
static void add_to_drawhash(lua_State *L, const TValue *obj) {
    TValue g = game(L), hash = sub(L, &g, k(DRAW_HASH));
    if (!ttistable(&hash)) luaL_error(L, "native: no G.DRAW_HASH");
    Table *t = hvalue(&hash);
    setobj2t(L, luaH_setnum(L, t, luaH_getn(t) + 1), obj);
    luaC_barriert(L, t, obj);
}

static void call0(lua_State *L, const TValue *obj, const char *name);
static int orig_draw_pixellated_rect, orig_draw_self; // registry refs of the game's UIElement methods

// UIElement:draw_pixellated_rect(_type, _parallax, _emboss, _progress), ui.lua:853: the polygon it keeps, drawn;
// when what it was built from changed, the game's Lua builds it again (and draws it).
static void pixellated_rect(lua_State *L, const TValue *self, TString *type, const TValue *parallax, const TValue *emboss,
                            const TValue *progress) {
    TValue pr = sub(L, self, k(pixellated_rect)), vertices;
    int kept = 0;
    if (!l_isfalse(&pr)) {
        TValue VT = sub(L, self, k(VT)), sp = sub(L, self, k(shadow_parrallax)), of_type = sub(L, &pr, type);
        TValue one;
        setnvalue(&one, 1);
        vertices = sub(L, &of_type, k(vertices));
        if (!ttistable(&vertices)) luaL_error(L, "native: pixellated_rect without vertices");
        kept = luaH_getn(hvalue(&vertices)) >= 1 && luaO_rawequalObj(parallax, idx(L, &pr, k(parallax))) &&
               luaO_rawequalObj(idx(L, &pr, k(w)), idx(L, &VT, k(w))) && luaO_rawequalObj(idx(L, &pr, k(h)), idx(L, &VT, k(h))) &&
               luaO_rawequalObj(idx(L, &pr, k(sw)), idx(L, &sp, k(x))) && luaO_rawequalObj(idx(L, &pr, k(sh)), idx(L, &sp, k(y))) &&
               luaO_rawequalObj(idx(L, &pr, k(progress)), l_isfalse(progress) ? &one : progress);
    }
    if (!kept) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, orig_draw_pixellated_rect);
        push(L, self);
        setsvalue2s(L, L->top, type);
        L->top++;
        push(L, parallax);
        push(L, emboss);
        push(L, progress);
        lua_call(L, 5, 0);
        return;
    }
    float xy[512];
    Table *t = hvalue(&vertices);
    int n = luaH_getn(t);
    if (n < 6 || n > 512) luaL_error(L, "native: polygon of %d numbers", n);
    for (int i = 0; i < n; i++) {
        const TValue *v = luaH_getnum(t, i + 1);
        if (!ttisnumber(v)) luaL_error(L, "native: a vertex that is not a number");
        xy[i] = nvalue(v);
    }
    gfx_polygon(L, xy, n / 2, type == k(line) || type == k(line_emboss) ? "line" : "fill");
}
static int l_draw_pixellated_rect(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checkstring(L, 2);
    lua_settop(L, 5);
    TValue self = L->base[0], parallax = L->base[2], emboss = L->base[3], progress = L->base[4];
    pixellated_rect(L, &self, rawtsvalue(L->base + 1), &parallax, &emboss, &progress);
    return 0;
}

// UIElement:draw_self(), ui.lua:663, for text, boxes (embossed ones and buttons too), objects and elements with
// nothing to draw. An outline, the focus, the 'chosen' mark, a button on its delay, a progress bar, an object that
// takes the focus and everything under G.DEBUG run the game's Lua.
static int l_draw_self(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], g = game(L), config = sub(L, &self, k(config)), states = sub(L, &self, k(states));
    if (!truthy(L, &states, k(visible))) {
        if (truthy(L, &config, k(force_focus))) add_to_drawhash(L, &self);
        return 0;
    }
    TValue colour = sub(L, &config, k(colour)), uit = sub(L, &self, k(UIT)), uits = sub(L, &g, k(UIT));
    if (!ttistable(&colour) || !ttisnumber(luaH_getnum(hvalue(&colour), 4))) luaL_error(L, "native: config.colour");
    lua_Number alpha = nvalue(luaH_getnum(hvalue(&colour), 4));
    int is_text = luaO_rawequalObj(&uit, idx(L, &uits, k(T))) && truthy(L, &config, k(scale));
    int is_box = luaO_rawequalObj(&uit, idx(L, &uits, k(B))) || luaO_rawequalObj(&uit, idx(L, &uits, k(C))) ||
                 luaO_rawequalObj(&uit, idx(L, &uits, k(R))) || luaO_rawequalObj(&uit, idx(L, &uits, k(ROOT)));
    int is_object = luaO_rawequalObj(&uit, idx(L, &uits, k(O))) && truthy(L, &config, k(object)) &&
                    !truthy(L, &config, k(focus_with_object));
    TValue focus = sub(L, &states, k(focus)), button = sub(L, &config, k(button)), uie = sub(L, &config, k(button_UIE));
    if (truthy(L, &config, k(outline)) || truthy(L, &focus, k(is)) || truthy(L, &config, k(chosen)) || truthy(L, &g, k(DEBUG)) ||
        truthy(L, &config, k(button_delay)) || truthy(L, &config, k(progress_bar)) ||
        (alpha > 0.01 && !is_text && !is_box && !is_object)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, orig_draw_self);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 0);
        return 0;
    }

    int has_button = !l_isfalse(&button), has_uie = !l_isfalse(&uie);
    TValue collide = sub(L, &states, k(collide)), hover = sub(L, &states, k(hover)), timers = sub(L, &g, k(TIMERS));
    if (truthy(L, &config, k(force_focus)) || truthy(L, &config, k(force_collision)) || has_uie || has_button ||
        truthy(L, &collide, k(can)))
        add_to_drawhash(L, &self);
    TValue sp = sub(L, &self, k(shadow_parrallax));
    lua_Number ts = num(L, &g, k(TILESIZE)), parallax_dist = 1.5;
    int active = 1, pressed = 0, cshadow = truthy(L, &config, k(shadow));
    if (has_button || has_uie) {
        TValue lp = sub(L, &self, k(layered_parallax)), parent = sub(L, &self, k(parent));
        lua_Number px = 0, py = 0;
        if (!l_isfalse(&parent) && !luaO_rawequalObj(&parent, idx(L, &self, k(UIBox)))) {
            TValue plp = sub(L, &parent, k(layered_parallax));
            px = num(L, &plp, k(x)), py = num(L, &plp, k(y));
        }
        setn(L, &lp, k(x), px + (cshadow ? 0.4 * num(L, &sp, k(x)) : 0) / ts);
        setn(L, &lp, k(y), py + (cshadow ? 0.4 * num(L, &sp, k(y)) : 0) / ts);
        if (has_button) {
            TValue drag = sub(L, &states, k(drag)), controller = sub(L, &g, k(CONTROLLER));
            if ((truthy(L, &self, k(last_clicked)) && num(L, &self, k(last_clicked)) > num(L, &timers, k(REAL)) - 0.1) ||
                ((truthy(L, &hover, k(is)) || truthy(L, &drag, k(is))) && truthy(L, &controller, k(is_cursor_down)))) {
                lua_Number dist = truthy(L, &config, k(button_dist)) ? num(L, &config, k(button_dist)) : 1;
                setn(L, &lp, k(x), num(L, &lp, k(x)) - parallax_dist * num(L, &sp, k(x)) / ts * dist);
                setn(L, &lp, k(y), num(L, &lp, k(y)) - parallax_dist * num(L, &sp, k(y)) / ts * dist);
                parallax_dist = 0;
                pressed = 1;
            }
        }
        if (has_uie) {
            TValue uc = sub(L, &uie, k(config));
            if (!truthy(L, &uc, k(button))) active = 0;
        }
    }
    if (alpha > 0.01) {
        TValue settings = sub(L, &g, k(SETTINGS)), graphics = sub(L, &settings, k(GRAPHICS)), args = sub(L, &self, k(ARGS));
        TValue VT = sub(L, &self, k(VT));
        int shadows_on = is(L, &graphics, k(shadows), k(On));
        if (is_text) {
            TValue tp = sub_or_new(L, &args, k(text_parallax)), lang = sub(L, &config, k(lang)), font = sub(L, &lang, k(font));
            TValue off = sub(L, &font, k(TEXT_OFFSET)), drawable = sub(L, &config, k(text_drawable));
            lua_Number scale = num(L, &config, k(scale)), fs = num(L, &font, k(FONTSCALE)), squish = num(L, &font, k(squish));
            lua_Number sx = -num(L, &sp, k(x)) * 0.5 / (scale * fs), sy = -num(L, &sp, k(y)) * 0.5 / (scale * fs);
            int vert = truthy(L, &config, k(vert));
            setn(L, &tp, k(sx), sx);
            setn(L, &tp, k(sy), sy);
            push(L, &drawable);
            if ((has_uie && active) || (!has_uie && cshadow && shadows_on)) { // the shadow
                prep_draw(L, &self, 0.97, luaO_nilobject, luaO_nilobject);
                if (vert) {
                    gfx_translate(0, num(L, &VT, k(h)));
                    gfx_rotate(-3.14159265358979323846 / 2);
                }
                if ((cshadow || (has_uie && active)) && shadows_on) {
                    gfx_colour(0, 0, 0, 0.3 * alpha);
                    gfx_draw_text(L, (num(L, &off, k(x)) + (vert ? -sy : sx)) * scale * fs / ts,
                                  (num(L, &off, k(y)) + (vert ? sx : sy)) * scale * fs / ts, 0, scale * squish * fs / ts,
                                  scale * fs / ts, 0, 0);
                }
                gfx_pop(L);
            }
            prep_draw(L, &self, 1, luaO_nilobject, luaO_nilobject);
            if (vert) {
                gfx_translate(0, num(L, &VT, k(h)));
                gfx_rotate(-3.14159265358979323846 / 2);
            }
            if (active) set_colour(L, &colour);
            else {
                TValue colours = sub(L, &g, k(C)), ui = sub(L, &colours, k(UI)), inactive = sub(L, &ui, k(TEXT_INACTIVE));
                set_colour(L, &inactive);
            }
            gfx_draw_text(L, num(L, &off, k(x)) * scale * fs / ts, num(L, &off, k(y)) * scale * fs / ts, 0,
                          scale * squish * fs / ts, scale * fs / ts, 0, 0);
            gfx_pop(L);
            L->top--;
        } else if (is_box) {
            int rounded = truthy(L, &config, k(r)) && num(L, &VT, k(w)) > 0.01;
            TValue dist;
            setnvalue(&dist, parallax_dist);
            prep_draw(L, &self, 1, luaO_nilobject, luaO_nilobject);
            gfx_scale(1 / ts, 1 / ts);
            if (cshadow && shadows_on) {
                TValue sc = sub(L, &config, k(shadow_colour));
                gfx_scale(0.98, 0.98);
                if (!l_isfalse(&sc)) set_colour(L, &sc);
                else gfx_colour(0, 0, 0, 0.3 * alpha);
                if (rounded) pixellated_rect(L, &self, k(shadow), &dist, luaO_nilobject, luaO_nilobject);
                else
                    gfx_fill_rectangle(L, -num(L, &sp, k(x)) * parallax_dist, -num(L, &sp, k(y)) * parallax_dist,
                                       num(L, &VT, k(w)) * ts, num(L, &VT, k(h)) * ts);
                gfx_scale(1 / 0.98, 1 / 0.98);
            }
            gfx_scale(pressed ? 0.985 : 1, pressed ? 0.985 : 1);
            TValue emboss = sub(L, &config, k(emboss));
            if (!l_isfalse(&emboss)) { // darken() is the game's
                lua_getglobal(L, "darken");
                push(L, &colour);
                lua_pushnumber(L, truthy(L, &hover, k(is)) ? 0.5 : 0.3);
                lua_pushboolean(L, 1);
                lua_call(L, 3, 4); // its third argument: the four numbers, not a table
                gfx_colour(luaL_checknumber(L, -4), luaL_checknumber(L, -3), luaL_checknumber(L, -2), luaL_checknumber(L, -1));
                lua_pop(L, 4);
                pixellated_rect(L, &self, k(emboss), &dist, &emboss, luaO_nilobject);
            }
            TValue collided = has_uie ? uie : self, cconfig = sub(L, &collided, k(config)), cstates = sub(L, &collided, k(states));
            TValue chover = sub(L, &cstates, k(hover)), bc = sub_or_new(L, &args, k(button_colours)), second;
            setnilvalue(&second);
            if ((truthy(L, &cconfig, k(hover)) && truthy(L, &chover, k(is))) ||
                (truthy(L, &collided, k(last_clicked)) && num(L, &collided, k(last_clicked)) > num(L, &timers, k(REAL)) - 0.1)) {
                TValue colours = sub(L, &g, k(C)), ui = sub(L, &colours, k(UI));
                second = sub(L, &ui, k(HOVER));
                if (l_isfalse(&second)) setnilvalue(&second);
            }
            setobj2t(L, luaH_setnum(L, hvalue(&bc), 1), &colour);
            luaC_barriert(L, hvalue(&bc), &colour);
            setobj2t(L, luaH_setnum(L, hvalue(&bc), 2), &second);
            luaC_barriert(L, hvalue(&bc), &second);
            for (int i = 1;; i++) { // ipairs
                TValue v = *luaH_getnum(hvalue(&bc), i);
                if (ttisnil(&v)) break;
                set_colour(L, &v);
                if (rounded) pixellated_rect(L, &self, k(fill), &dist, luaO_nilobject, luaO_nilobject);
                else gfx_fill_rectangle(L, 0, 0, num(L, &VT, k(w)) * ts, num(L, &VT, k(h)) * ts);
            }
            gfx_pop(L);
        } else if (is_object) {
            TValue object = sub(L, &config, k(object));
            setv(L, &self, k(object_focus_timer), luaO_nilobject);
            call0(L, &object, "draw");
        }
    }
    setv(L, &self, k(focus_timer), luaO_nilobject);
    copy(L, &self, &g, k(under_overlay)); // Node:draw_boundingrect() without G.DEBUG
    return 0;
}

// Node:translate_container(), node.lua:307
static int l_translate_container(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], cont = sub(L, &self, k(container));
    if (l_isfalse(&cont) || luaO_rawequalObj(&cont, &self)) return 0;
    TValue g = game(L), T = sub(L, &cont, k(T));
    lua_Number ts = num(L, &g, k(TILESCALE)), tz = num(L, &g, k(TILESIZE)), w = num(L, &T, k(w)), h = num(L, &T, k(h));
    gfx_translate(w * ts * tz * 0.5, h * ts * tz * 0.5);
    gfx_rotate(num(L, &T, k(r)));
    gfx_translate(-w * ts * tz * 0.5 + num(L, &T, k(x)) * ts * tz, -h * ts * tz * 0.5 + num(L, &T, k(y)) * ts * tz);
    return 0;
}

// obj:name()
static void call0(lua_State *L, const TValue *obj, const char *name) {
    push(L, idx(L, obj, interned(L, name)));
    push(L, obj);
    lua_call(L, 1, 0);
}

// UIElement:draw_children(layer), ui.lua:535
static int l_draw_children(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], states = sub(L, &self, k(states));
    if (!truthy(L, &states, k(visible))) return 0;
    lua_settop(L, 1);
    push(L, idx(L, &self, k(children)));
    lua_pushnil(L);
    while (lua_next(L, 2)) { // the same order as pairs
        TValue key = L->base[2], v = L->base[3], config = sub(L, &v, k(config));
        if (!truthy(L, &config, k(draw_layer)) && !(ttisstring(&key) && (rawtsvalue(&key) == k(h_popup) || rawtsvalue(&key) == k(alert)))) {
            call0(L, &v, truthy(L, &v, k(draw_self)) && !truthy(L, &config, k(draw_after)) ? "draw_self" : "draw");
            if (truthy(L, &v, k(draw_children))) call0(L, &v, "draw_children");
            config = sub(L, &v, k(config));
            call0(L, &v, truthy(L, &v, k(draw_self)) && truthy(L, &config, k(draw_after)) ? "draw_self" : "draw");
        }
        lua_pop(L, 1);
    }
    return 0;
}

static int orig_dynatext_draw; // registry ref of the game's DynaText.draw

// DynaText:draw(), text.lua:257 (under G.DEBUG, the game's Lua)
static int l_dynatext_draw(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], g = game(L);
    if (truthy(L, &g, k(DEBUG))) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, orig_dynatext_draw);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 0);
        return 0;
    }
    TValue children = sub(L, &self, k(children)), effect = sub(L, &children, k(particle_effect));
    if (!l_isfalse(&effect)) call0(L, &effect, "draw");
    for (int pass = truthy(L, &self, k(shadow)) ? 0 : 1; pass < 2; pass++) { // the shadow, then the letters
        TValue config = sub(L, &self, k(config)), font = sub(L, &self, k(font)), strings = sub(L, &self, k(strings));
        if (!ttistable(&strings)) luaL_error(L, "native: DynaText without strings");
        TValue str = *luaH_get(hvalue(&strings), idx(L, &self, k(focused_string))), letters = sub(L, &str, k(letters));
        TValue toff = sub(L, &self, k(text_offset)), sp = sub(L, &self, k(shadow_parrallax)), colours = sub(L, &self, k(colours));
        if (!ttistable(&letters) || !ttistable(&colours)) luaL_error(L, "native: DynaText letters or colours");
        lua_Number fs = num(L, &font, k(FONTSCALE)), ts = num(L, &g, k(TILESIZE)), nx = 0, ny = 0;
        prep_draw(L, &self, 1, luaO_nilobject, luaO_nilobject);
        gfx_translate(num(L, &str, k(W_offset)) + num(L, &toff, k(x)) * fs / ts, num(L, &str, k(H_offset)) + num(L, &toff, k(y)) * fs / ts);
        if (truthy(L, &config, k(spacing))) gfx_translate(num(L, &config, k(spacing)) * fs / ts, 0);
        if (!pass) {
            TValue sc = sub(L, &config, k(shadow_colour));
            if (!l_isfalse(&sc)) set_colour(L, &sc);
            else {
                TValue first = *luaH_getnum(hvalue(&colours), 1);
                if (!ttistable(&first) || !ttisnumber(luaH_getnum(hvalue(&first), 4))) luaL_error(L, "native: DynaText colours[1]");
                gfx_colour(0, 0, 0, 0.3 * nvalue(luaH_getnum(hvalue(&first), 4)));
            }
        } else {
            TValue args = sub(L, &self, k(ARGS)), norm = sub_or_new(L, &args, k(draw_shadow_norm));
            lua_Number x = num(L, &sp, k(x)), y = num(L, &sp, k(y));
            nx = x / sqrt(y * y + x * x) * fs / ts;
            ny = y / sqrt(y * y + x * x) * fs / ts;
            setn(L, &norm, k(x), nx);
            setn(L, &norm, k(y), ny);
        }
        for (int i = 1;; i++) { // ipairs
            TValue letter = *luaH_getnum(hvalue(&letters), i);
            if (ttisnil(&letter)) break;
            TValue dims = sub(L, &letter, k(dims)), off = sub(L, &letter, k(offset)), text = sub(L, &letter, k(letter));
            const TValue *cycle = idx(L, &config, k(min_cycle_time)), *rot = idx(L, &letter, k(r));
            lua_Number pop = ttisnumber(cycle) && nvalue(cycle) == 0 ? 1 : num(L, &letter, k(pop_in));
            lua_Number scale = num(L, &self, k(scale)), dx = num(L, &dims, k(x)), dy = num(L, &dims, k(y));
            if (!l_isfalse(rot) && !ttisnumber(rot)) luaL_error(L, "native: letter.r");
            lua_Number r = l_isfalse(rot) ? 0 : nvalue(rot);
            push(L, &text);
            if (!pass)
                gfx_draw_text(L, 0.5 * (dx - num(L, &off, k(x))) * fs / ts - num(L, &sp, k(x)) * scale / ts,
                              0.5 * dy * fs / ts - num(L, &sp, k(y)) * scale / ts, r, pop * scale * fs / ts, pop * scale * fs / ts,
                              0.5 * dx / scale, 0.5 * dy / scale);
            else {
                TValue c = sub(L, &letter, k(prefix));
                if (l_isfalse(&c)) c = sub(L, &letter, k(suffix));
                if (l_isfalse(&c)) c = sub(L, &letter, k(colour));
                if (l_isfalse(&c)) c = *luaH_getnum(hvalue(&colours), i % luaH_getn(hvalue(&colours)) + 1);
                set_colour(L, &c);
                lua_Number ls = num(L, &letter, k(scale));
                gfx_draw_text(L, 0.5 * (dx - num(L, &off, k(x))) * fs / ts + nx, 0.5 * (dy - num(L, &off, k(y))) * fs / ts + ny, r,
                              pop * ls * scale * fs / ts, pop * ls * scale * fs / ts, 0.5 * dx / scale, 0.5 * dy / scale);
            }
            L->top--;
            gfx_translate(dx * fs / ts, 0);
        }
        gfx_pop(L);
    }
    add_to_drawhash(L, &self);
    copy(L, &self, &g, k(under_overlay)); // Node:draw_boundingrect() without G.DEBUG
    return 0;
}

static int orig_sprite_draw_self, orig_sprite_draw_from, orig_sprite_draw_shader; // registry refs of the game's Sprite methods

// the game's Lua for the same call: its n arguments are the stack
static int original(lua_State *L, int ref, int n) {
    lua_settop(L, n);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_insert(L, 1);
    lua_call(L, n, 0);
    return 0;
}
// love.graphics.setColor(overlay or G.BRUTE_OVERLAY or G.C.WHITE)
static void sprite_colour(lua_State *L, const TValue *g, const TValue *overlay) {
    TValue c = *overlay;
    if (l_isfalse(&c)) c = sub(L, g, k(BRUTE_OVERLAY));
    if (l_isfalse(&c)) {
        TValue colours = sub(L, g, k(C));
        c = sub(L, &colours, k(WHITE));
    }
    set_colour(L, &c);
}
// love.graphics.draw(self.atlas.image, self.sprite, x, 0, 0, sx, sy)
static void draw_sprite(lua_State *L, const TValue *self, lua_Number x, lua_Number sx, lua_Number sy) {
    TValue atlas = sub(L, self, k(atlas)), image = sub(L, &atlas, k(image)), quad = sub(L, self, k(sprite));
    push(L, &image);
    push(L, &quad);
    gfx_draw_quad(L, x, 0, 0, sx, sy);
    L->top -= 2;
}

// Sprite:draw_self(overlay), sprite.lua:126 (a video, or G.DEBUG: the game's Lua)
static int l_sprite_draw_self(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    TValue self = L->base[0], overlay = L->base[1], g = game(L), states = sub(L, &self, k(states));
    if (!truthy(L, &states, k(visible))) return 0;
    if (truthy(L, &self, k(video)) || truthy(L, &g, k(DEBUG))) return original(L, orig_sprite_draw_self, 2);
    TValue pos = sub(L, &self, k(sprite_pos)), was = sub(L, &self, k(sprite_pos_copy));
    if (!luaO_rawequalObj(idx(L, &pos, k(x)), idx(L, &was, k(x))) || !luaO_rawequalObj(idx(L, &pos, k(y)), idx(L, &was, k(y)))) {
        push(L, &self);
        lua_getfield(L, -1, "set_sprite_pos");
        lua_insert(L, -2);
        push(L, &pos);
        lua_call(L, 2, 0);
    }
    prep_draw(L, &self, 1, luaO_nilobject, luaO_nilobject);
    TValue VT = sub(L, &self, k(VT)), T = sub(L, &self, k(T)), scale = sub(L, &self, k(scale));
    gfx_scale(1 / (num(L, &scale, k(x)) / num(L, &VT, k(w))), 1 / (num(L, &scale, k(y)) / num(L, &VT, k(h))));
    sprite_colour(L, &g, &overlay);
    draw_sprite(L, &self, 0, num(L, &VT, k(w)) / num(L, &T, k(w)), num(L, &VT, k(h)) / num(L, &T, k(h)));
    gfx_pop(L);
    add_to_drawhash(L, &self);
    copy(L, &self, &g, k(under_overlay)); // Node:draw_boundingrect() without G.DEBUG
    return 0;
}

// Sprite:draw_from(other_obj, ms, mr, mx, my), sprite.lua:180 (under G.DEBUG, the game's Lua)
static int l_sprite_draw_from(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 6);
    TValue self = L->base[0], other = L->base[1], g = game(L);
    if (truthy(L, &g, k(DEBUG))) return original(L, orig_sprite_draw_from, 6);
    lua_Number v[4]; // ms, mr, mx, my, each "or 0"
    for (int i = 0; i < 4; i++) v[i] = lua_toboolean(L, 3 + i) ? luaL_checknumber(L, 3 + i) : 0;
    TValue args = sub(L, &self, k(ARGS)), off = sub_or_new(L, &args, k(draw_from_offset)), rotate;
    setn(L, &off, k(x), v[2]);
    setn(L, &off, k(y), v[3]);
    setnvalue(&rotate, v[1]);
    prep_draw(L, &other, 1 + v[0], &rotate, &off);
    TValue oVT = sub(L, &other, k(VT)), oT = sub(L, &other, k(T));
    const TValue *mag = idx(L, &other, k(scale_mag));
    if (!l_isfalse(mag) && !ttisnumber(mag)) luaL_error(L, "native: scale_mag");
    lua_Number s = 1 / (l_isfalse(mag) ? num(L, &oVT, k(scale)) : nvalue(mag));
    gfx_scale(s, s);
    sprite_colour(L, &g, luaO_nilobject);
    draw_sprite(L, &self, -(num(L, &oT, k(w)) / 2 - num(L, &oVT, k(w)) / 2) * 10, num(L, &oVT, k(w)) / num(L, &oT, k(w)),
                num(L, &oVT, k(h)) / num(L, &oT, k(h)));
    copy(L, &self, &g, k(under_overlay)); // Node:draw_boundingrect() without G.DEBUG
    gfx_pop(L);
    return 0;
}

static int orig_card_update, orig_align_cards;
static TString *question, *busy[13]; // '?', and the abilities Card:update does something for every frame

// Card:update(dt), card.lua:4126, for the cards it has nothing more to do for than its first lines: the ones with
// an ability of `busy`, a Joker without its sticker yet and a consumeable with max_highlighted run the game's Lua.
static int l_card_update(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], ability = sub(L, &self, k(ability)), cons = sub(L, &ability, k(consumeable));
    const TValue *name = idx(L, &ability, k(name)), *set = idx(L, &ability, k(set));
    TString *kind = ttisstring(set) ? rawtsvalue(set) : NULL;
    int lua = (kind == k(Joker) && !truthy(L, &self, k(sticker_run))) || (!l_isfalse(&cons) && truthy(L, &cons, k(max_highlighted)));
    for (int i = 0; !lua && ttisstring(name) && i < 13; i++) lua = rawtsvalue(name) == busy[i];
    if (lua) return original(L, orig_card_update, 2);
    const TValue *fl = idx(L, &self, k(flipping));
    if (ttisstring(fl) && (rawtsvalue(fl) == k(f2b) || rawtsvalue(fl) == k(b2f))) {
        TValue VT = sub(L, &self, k(VT));
        if (num(L, &VT, k(w)) <= 0) {
            TValue s, pinch = sub(L, &self, k(pinch));
            setsvalue(L, &s, rawtsvalue(fl) == k(f2b) ? k(back) : k(front));
            setv(L, &self, k(sprite_facing), &s);
            setb(L, &pinch, k(x), 0);
        }
    }
    TValue states = sub(L, &self, k(states)), focus = sub(L, &states, k(focus)), children = sub(L, &self, k(children));
    if (!truthy(L, &focus, k(is)) && truthy(L, &children, k(focused_ui))) {
        TValue ui = sub(L, &children, k(focused_ui));
        call0(L, &ui, "remove");
        children = sub(L, &self, k(children));
        setv(L, &children, k(focused_ui), luaO_nilobject);
    }
    if (kind == k(Joker) || kind == k(Voucher) || !l_isfalse(&cons) || kind == k(Edition) || kind == k(Booster)) { // update_alert()
        TValue area = sub(L, &self, k(area)), config = sub(L, &self, k(config));
        if (!l_isfalse(&area)) {
            TValue ac = sub(L, &area, k(config));
            if (truthy(L, &ac, k(collection)) && truthy(L, &config, k(center))) call0(L, &self, "update_alert");
        }
    }
    TValue g = game(L), stages = sub(L, &g, k(STAGES));
    if (luaO_rawequalObj(idx(L, &g, k(STAGE)), idx(L, &stages, k(RUN)))) {
        ability = sub(L, &self, k(ability));
        if (!l_isfalse(&ability) && truthy(L, &ability, k(perma_debuff))) setb(L, &self, k(debuff), 1);
        TValue area = sub(L, &self, k(area));
        if (!l_isfalse(&area) && (luaO_rawequalObj(&area, idx(L, &g, k(jokers))) || luaO_rawequalObj(&area, idx(L, &g, k(consumeables))))) {
            setb(L, &self, k(bypass_lock), 1);
            setb(L, &self, k(bypass_discovery_center), 1);
            setb(L, &self, k(bypass_discovery_ui), 1);
        }
        TValue label = sub(L, &self, k(sell_cost));
        if (is(L, &self, k(facing), k(back))) setsvalue(L, &label, question);
        setv(L, &self, k(sell_cost_label), &label);
    }
    return 0;
}

// CardArea:align_cards(), cardarea.lua:410, for a deck (its 40-odd cards every frame); the other areas, and any
// while the deck is being viewed, run the game's Lua.
static int l_align_cards(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], g = game(L), config = sub(L, &self, k(config));
    if (!is(L, &config, k(type), k(deck)) || truthy(L, &g, k(view_deck))) return original(L, orig_align_cards, 1);
    const TValue *dh = idx(L, &config, k(deck_height));
    lua_Number deck_height = (l_isfalse(dh) ? 0.15 : num(L, &config, k(deck_height))) / 52;
    lua_Number halves = luaO_rawequalObj(&self, idx(L, &g, k(deck))) ? 1 : 2;
    TValue cards = sub(L, &self, k(cards));
    int i;
    for (i = 1;; i++) {
        const TValue *slot = luaH_getnum(hvalue(&cards), i);
        if (ttisnil(slot)) break;
        TValue card = *slot;
        if (is(L, &card, k(facing), k(front))) call0(L, &card, "flip");
        TValue cs = sub(L, &card, k(states)), drag = sub(L, &cs, k(drag));
        if (truthy(L, &drag, k(is))) continue;
        TValue T = sub(L, &self, k(T)), cT = sub(L, &card, k(T)), sp = sub(L, &self, k(shadow_parrallax)), csp = sub(L, &card, k(shadow_parrallax));
        lua_Number n = luaH_getn(hvalue(&cards)), shuffle = num(L, &self, k(shuffle_amt)), at = i, side = i % 2 == 1 ? 1 : 0; // (1 or -0: the sign of a zero added to a sum)
        setn(L, &cT, k(x), num(L, &T, k(x)) + 0.5 * (num(L, &T, k(w)) - num(L, &cT, k(w))) + num(L, &sp, k(x)) * deck_height * (n / halves - at) + 0.9 * shuffle * (1 - at * 0.01) * side);
        setn(L, &cT, k(y), num(L, &T, k(y)) + 0.5 * (num(L, &T, k(h)) - num(L, &cT, k(h))) + num(L, &sp, k(y)) * deck_height * (n / halves - at));
        setn(L, &cT, k(r), 0 + 0.3 * shuffle * (1 + at * 0.05) * side);
        setn(L, &cT, k(x), num(L, &cT, k(x)) + num(L, &csp, k(x)) / 30);
    }
    for (int j = 1; j < i; j++) {
        TValue card = *luaH_getnum(hvalue(&cards), j);
        setn(L, &card, k(rank), j);
    }
    TValue children = sub(L, &self, k(children)), view = sub(L, &children, k(view_deck));
    if (!l_isfalse(&view)) { // view_deck:set_role{major = self.cards[1] or self}
        const TValue *first = luaH_getnum(hvalue(&cards), 1);
        push(L, &view);
        lua_getfield(L, -1, "set_role");
        lua_insert(L, -2);
        lua_createtable(L, 0, 1);
        push(L, l_isfalse(first) ? &self : first);
        lua_setfield(L, -2, "major");
        lua_call(L, 2, 0);
    }
    return 0;
}

static int orig_card_draw, orig_cardarea_draw;
static TString *s_glass, *s_stone, *s_none, *s_soul, *s_antimatter, *s_invisible;

// obj:name(a, b, c) with the first n of them
static void callv(lua_State *L, const TValue *obj, TString *name, int n, const TValue *a, const TValue *b, const TValue *c) {
    push(L, idx(L, obj, name));
    push(L, obj);
    if (n > 0) push(L, a);
    if (n > 1) push(L, b);
    if (n > 2) push(L, c);
    lua_call(L, n + 1, 0);
}
// sprite:draw_shader(shader) or sprite:draw_shader(shader, nil, send)
static void shade(lua_State *L, const TValue *sprite, TString *shader, const TValue *send) {
    TValue name;
    setsvalue(L, &name, shader);
    callv(L, sprite, k(draw_shader), send ? 3 : 1, &name, luaO_nilobject, send);
}
// if t[key] then t[key]:draw() end
static void draw_if(lua_State *L, const TValue *t, TString *key) {
    TValue v = sub(L, t, key);
    if (!l_isfalse(&v)) callv(L, &v, k(draw), 0, NULL, NULL, NULL);
}
static lua_Number mod(lua_Number a, lua_Number b) { return a - floor(a / b) * b; } // Lua's %

// Card:draw(layer), card.lua:4340, for a card with nothing on it but an edition, the debuff or the grey of a played
// one, face up or down. One with a seal, a sticker, a floating sprite, the vortex, a Voucher, a Booster, a Spectral
// card, one not discovered yet... runs the game's Lua.
static int l_card_draw(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    TValue self = L->base[0], g = game(L), ability = sub(L, &self, k(ability)), config = sub(L, &self, k(config));
    TValue center = sub(L, &config, k(center)), args = sub(L, &self, k(ARGS)), settings = sub(L, &g, k(SETTINGS));
    const TValue *nm = idx(L, &ability, k(name)), *st = idx(L, &ability, k(set)), *run = idx(L, &self, k(sticker_run));
    TString *name = ttisstring(nm) ? rawtsvalue(nm) : NULL, *set = ttisstring(st) ? rawtsvalue(st) : NULL;
    if (!ttistable(&center) || !truthy(L, &args, k(send_to_shader)) || truthy(L, &self, k(vortex)) || truthy(L, &self, k(seal)) ||
        truthy(L, &self, k(sticker)) || truthy(L, &ability, k(eternal)) || truthy(L, &ability, k(rental)) || truthy(L, &ability, k(perishable)) ||
        (!l_isfalse(run) && !(ttisstring(run) && rawtsvalue(run) == s_none) && truthy(L, &settings, k(run_stake_stickers))) ||
        set == k(Spectral) || set == k(Voucher) || set == k(Booster) || name == s_soul || name == s_antimatter || name == s_invisible ||
        truthy(L, &center, k(soul_pos)) || truthy(L, &center, k(demo)) ||
        (!truthy(L, &center, k(discovered)) && (truthy(L, &ability, k(consumeable)) || truthy(L, &center, k(unlocked))) && !truthy(L, &self, k(bypass_discovery_center))))
        return original(L, orig_card_draw, 2);

    const TValue *la = L->base + 1;
    TString *layer = ttisstring(la) ? rawtsvalue(la) : l_isfalse(la) ? k(both) : NULL;
    int shadow_layer = layer == k(shadow) || layer == k(both), card_layer = layer == k(card) || layer == k(both);
    setn(L, &self, k(hover_tilt), 1);
    TValue states = sub(L, &self, k(states));
    if (!truthy(L, &states, k(visible))) return 0;
    TValue timers = sub(L, &g, k(TIMERS)), VT = sub(L, &self, k(VT)), tv = sub(L, &self, k(tilt_var)), children = sub(L, &self, k(children));
    TValue send = sub(L, &args, k(send_to_shader));
    lua_Number real = num(L, &timers, k(REAL));
    if (shadow_layer) {
        lua_Number v = mn(num(L, &VT, k(r)) * 3, 1) + real / 28 + juice_or_0(L, &self, k(r), 20) + num(L, &tv, k(amt));
        setnvalue(luaH_setnum(L, hvalue(&send), 1), v);
        setnvalue(luaH_setnum(L, hvalue(&send), 2), real);
        TValue scale = sub(L, &VT, k(scale));
        Table *t = hvalue(&children); // for k, v in pairs(self.children) do v.VT.scale = self.VT.scale end
        for (int i = 0; i < t->sizearray + sizenode(t); i++) {
            const TValue *v = i < t->sizearray ? &t->array[i] : gval(gnode(t, i - t->sizearray));
            if (ttisnil(v)) continue;
            TValue child = *v, cVT = sub(L, &child, k(VT));
            setv(L, &cVT, k(scale), &scale);
        }
    }
    const TValue *facing = idx(L, &self, k(sprite_facing));
    TString *face = ttisstring(facing) ? rawtsvalue(facing) : NULL;
    TValue shared = sub(L, &children, face == k(front) ? k(center) : k(back));
    if (face == k(front) && l_isfalse(&shared)) shared = sub(L, &children, k(back));
    setv(L, &g, k(shared_shadow), &shared);

    TValue area = sub(L, &self, k(area)), drag = sub(L, &states, k(drag)), graphics = sub(L, &settings, k(GRAPHICS));
    int in_area = !l_isfalse(&area), highlighted = truthy(L, &self, k(highlighted));
    TString *area_type = NULL;
    if (in_area) {
        TValue ac = sub(L, &area, k(config));
        const TValue *t = idx(L, &ac, k(type));
        area_type = ttisstring(t) ? rawtsvalue(t) : NULL;
    }
    if (!truthy(L, &self, k(no_shadow)) && is(L, &graphics, k(shadows), k(On)) && shadow_layer && !is(L, &ability, k(effect), s_glass) &&
        !truthy(L, &self, k(greyed)) &&
        ((in_area && !luaO_rawequalObj(&area, idx(L, &g, k(discard))) && area_type != k(deck)) || !in_area || truthy(L, &drag, k(is)))) {
        TValue vel = sub(L, &self, k(velocity)), h, shader;
        lua_Number vx = num(L, &vel, k(x));
        // 0*(0.08 + 0.4*math.sqrt(self.velocity.x^2)): a zero unless the square overflows
        lua_Number height = (fabs(vx) < 1e150 ? 0 : 0 * (0.08 + 0.4 * sqrt(vx * vx))) +
                            ((highlighted && luaO_rawequalObj(&area, idx(L, &g, k(play)))) || truthy(L, &drag, k(is)) ? 0.35 : in_area && area_type == k(title_2) ? 0.04 : 0.1);
        setn(L, &self, k(shadow_height), height);
        setnvalue(&h, height);
        setsvalue(L, &shader, k(dissolve));
        shared = sub(L, &g, k(shared_shadow));
        callv(L, &shared, k(draw_shader), 2, &shader, &h, NULL);
    }
    int in_hand = luaO_rawequalObj(&area, idx(L, &g, k(hand)));
    if (card_layer && !in_hand) draw_if(L, &children, k(focused_ui));
    if (!card_layer) return 0;

    TValue focus = sub(L, &states, k(focus)), hover = sub(L, &states, k(hover));
    if (truthy(L, &focus, k(is)) || truthy(L, &hover, k(is))) {
        TValue controller = sub(L, &g, k(CONTROLLER)), cursor = sub(L, &controller, k(cursor_position)), ho = sub(L, &self, k(hover_offset));
        if (truthy(L, &focus, k(is))) {
            TValue T = sub(L, &self, k(T));
            lua_Number ts = num(L, &g, k(TILESCALE)), tz = num(L, &g, k(TILESIZE)), dx = num(L, &tv, k(dx)), dy = num(L, &tv, k(dy));
            lua_Number x = num(L, &cursor, k(x)) + dx * num(L, &T, k(w)) * ts * tz, y = num(L, &cursor, k(y)) + dy * num(L, &T, k(h)) * ts * tz;
            setn(L, &tv, k(mx), x);
            setn(L, &tv, k(my), y);
            setn(L, &tv, k(amt), fabs(num(L, &ho, k(y)) + num(L, &ho, k(x)) - 1 + num(L, &tv, k(dx)) + num(L, &tv, k(dy)) - 1) * 0.3);
        } else {
            lua_Number x = num(L, &cursor, k(x)), y = num(L, &cursor, k(y));
            setn(L, &tv, k(mx), x);
            setn(L, &tv, k(my), y);
            setn(L, &tv, k(amt), fabs(num(L, &ho, k(y)) + num(L, &ho, k(x)) - 1) * 0.3);
        }
    } else if (truthy(L, &self, k(ambient_tilt))) {
        TValue room = sub(L, &g, k(ROOM)), roomT = sub(L, &room, k(T));
        lua_Number amb = num(L, &self, k(ambient_tilt)), id = num(L, &self, k(ID)), tz = num(L, &g, k(TILESIZE)), ts = num(L, &g, k(TILESCALE));
        lua_Number angle = real * (1.56 + mod(id / 1.14212, 1)) + id / 1.35122, c = cos(angle), sn = sin(angle);
        setn(L, &tv, k(mx), ((0.5 + 0.5 * amb * c) * num(L, &VT, k(w)) + num(L, &VT, k(x)) + num(L, &roomT, k(x))) * tz * ts);
        setn(L, &tv, k(my), ((0.5 + 0.5 * amb * sn) * num(L, &VT, k(h)) + num(L, &VT, k(y)) + num(L, &roomT, k(y))) * tz * ts);
        setn(L, &tv, k(amt), amb * (0.5 + c) * 0.3);
    }
    draw_if(L, &children, k(particles));
    draw_if(L, &children, k(price));
    TValue buy = sub(L, &children, k(buy_button));
    if (!l_isfalse(&buy)) {
        TValue bs = sub(L, &buy, k(states));
        setb(L, &bs, k(visible), highlighted);
        if (highlighted) {
            callv(L, &buy, k(draw), 0, NULL, NULL, NULL);
            draw_if(L, &children, k(buy_and_use_button));
        }
    }
    if (highlighted) draw_if(L, &children, k(use_button));

    children = sub(L, &self, k(children));
    TValue centre = sub(L, &children, k(center)), front = sub(L, &children, k(front)), edition = sub(L, &self, k(edition));
    int with_front = !l_isfalse(&front) && !is(L, &ability, k(effect), s_stone), has_edition = !l_isfalse(&edition);
    if (face == k(front)) {
        int negative = has_edition && truthy(L, &edition, k(negative)), greyed = truthy(L, &self, k(greyed)), debuff = truthy(L, &self, k(debuff));
        if (negative || !greyed) {
            shade(L, &centre, negative ? k(negative) : k(dissolve), negative ? &send : NULL);
            if (with_front) shade(L, &front, negative ? k(negative) : k(dissolve), negative ? &send : NULL);
        }
        if (has_edition || debuff || greyed) {
            TString *const kinds[] = {k(holo), k(foil), k(polychrome)};
            for (int i = 0; i < 3; i++)
                if (has_edition && truthy(L, &edition, kinds[i])) {
                    shade(L, &centre, kinds[i], &send);
                    if (with_front) shade(L, &front, kinds[i], &send);
                }
            if (negative) shade(L, &centre, k(negative_shine), &send);
            if (debuff) {
                shade(L, &centre, k(debuff), &send);
                if (with_front) shade(L, &front, k(debuff), &send);
            }
            if (greyed) {
                shade(L, &centre, k(played), &send);
                if (with_front) shade(L, &front, k(played), &send);
            }
        }
    } else if (face == k(back)) {
        TValue colours = sub(L, &g, k(C)), overlay = sub(L, &colours, k(WHITE)), back = sub(L, &children, k(back));
        if (in_area && area_type == k(deck)) {
            lua_Number rank = num(L, &self, k(rank));
            if (rank > 3) {
                overlay = sub_or_new(L, &self, k(back_overlay));
                TValue ac = sub(L, &area, k(cards));
                lua_Number shade_of = 0.5 + mod(luaH_getn(hvalue(&ac)) - rank, 7) / 50;
                for (int i = 1; i <= 4; i++) setnvalue(luaH_setnum(L, hvalue(&overlay), i), i < 4 ? shade_of : 1);
            }
            callv(L, &back, k(draw), 1, &overlay, NULL, NULL);
        } else
            shade(L, &back, k(dissolve), NULL);
    }
    push(L, &children);
    lua_pushnil(L);
    while (lua_next(L, 3)) { // the same order as pairs
        TValue key = L->base[3], v = L->base[4];
        TString *n = ttisstring(&key) ? rawtsvalue(&key) : NULL;
        if (n != k(focused_ui) && n != k(front) && n != k(back) && n != k(soul_parts) && n != k(center) && n != k(floating_sprite) &&
            n != k(shadow) && n != k(use_button) && n != k(buy_button) && n != k(buy_and_use_button) && n != k(debuff) && n != k(price) &&
            n != k(particles) && n != k(h_popup))
            callv(L, &v, k(draw), 0, NULL, NULL, NULL);
        lua_pop(L, 1);
    }
    if (in_hand) draw_if(L, &children, k(focused_ui));
    add_to_drawhash(L, &self);
    callv(L, &self, k(draw_boundingrect), 0, NULL, NULL, NULL);
    return 0;
}

// self.cards[i]:draw(layer) unless the card is the one the controller has focused (but: drawn anyway) or drags
static void draw_card(lua_State *L, const TValue *cards, int i, const TValue *layer, const TValue *focused, const TValue *dragging, int but) {
    const TValue *card = luaH_getnum(hvalue(cards), i);
    if ((luaO_rawequalObj(card, focused) && !but) || luaO_rawequalObj(dragging, card)) return;
    TValue c = *card;
    callv(L, &c, k(draw), 1, layer, NULL, NULL);
}

// CardArea:draw(), cardarea.lua:270. When it has a box of its own to build or to remove (the first frame of an
// area, the deck's buttons coming and going), the game's Lua.
static int l_cardarea_draw(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    TValue self = L->base[0], g = game(L), states = sub(L, &self, k(states));
    if (!truthy(L, &states, k(visible))) return 0;
    int is_deck = luaO_rawequalObj(&self, idx(L, &g, k(deck))), is_hand = luaO_rawequalObj(&self, idx(L, &g, k(hand)));
    if (truthy(L, &g, k(VIEWING_DECK)) && (is_deck || is_hand || luaO_rawequalObj(&self, idx(L, &g, k(play))))) return 0;
    TValue args = sub(L, &self, k(ARGS)), config = sub(L, &self, k(config)), children = sub(L, &self, k(children)), all = sub(L, &g, k(STATES));
    TValue invisible = sub(L, &args, k(invisible_area_types)), layers = sub(L, &args, k(draw_layers)), box = sub(L, &children, k(area_uibox));
    TValue controller = sub(L, &g, k(CONTROLLER)), hid = sub(L, &controller, k(HID)), peek = sub(L, &children, k(peek_deck));
    const TValue *t = idx(L, &config, k(type));
    TString *type = ttisstring(t) ? rawtsvalue(t) : NULL;
    int pad = truthy(L, &hid, k(controller)), selecting = luaO_rawequalObj(idx(L, &g, k(STATE)), idx(L, &all, k(SELECTING_HAND)));
    if (!type || l_isfalse(&invisible) || l_isfalse(&layers) ||
        (is_deck && ((pad && selecting) == l_isfalse(&peek) || !truthy(L, &children, k(view_deck)))))
        return original(L, orig_cardarea_draw, 1);
    int hidden = truthy(L, &invisible, type) || (type == k(deck) && !is_deck) ||
                 (type == k(shop) && !luaO_rawequalObj(&self, idx(L, &g, k(shop_vouchers))));
    if (!hidden && type == k(hand)) {
        TValue state = sub(L, &g, k(TAROT_INTERRUPT));
        if (l_isfalse(&state)) state = sub(L, &g, k(STATE));
        TString *const away[] = {k(SHOP), k(TAROT_PACK), k(SPECTRAL_PACK), k(STANDARD_PACK), k(BUFFOON_PACK), k(PLANET_PACK), k(ROUND_EVAL), k(BLIND_SELECT)};
        for (int i = 0; i < 8; i++) hidden |= luaO_rawequalObj(&state, idx(L, &all, away[i]));
    }
    if (!hidden) {
        if (l_isfalse(&box)) return original(L, orig_cardarea_draw, 1);
        callv(L, &box, k(draw), 0, NULL, NULL, NULL);
    }
    callv(L, &self, k(draw_boundingrect), 0, NULL, NULL, NULL);
    add_to_drawhash(L, &self);
    for (int l = 1;; l++) {
        const TValue *slot = luaH_getnum(hvalue(&layers), l);
        if (ttisnil(slot)) break;
        TValue layer = *slot, cards = sub(L, &self, k(cards)), T = sub(L, &self, k(T));
        TValue fo = sub(L, &controller, k(focused)), dr = sub(L, &controller, k(dragging)), focused = sub(L, &fo, k(target)), dragging = sub(L, &dr, k(target));
        int n = luaH_getn(hvalue(&cards));
        if (type == k(deck)) {
            const TValue *thin = idx(L, &config, k(thin_draw));
            lua_Number every = l_isfalse(thin) ? 9 : num(L, &config, k(thin_draw));
            for (int i = n; i >= 1; i--) {
                TValue card = *luaH_getnum(hvalue(&cards), i);
                if (luaO_rawequalObj(&card, &focused)) continue;
                TValue cVT = sub(L, &card, k(VT));
                if (i == 1 || mod(i, every) == 0 || i == luaH_getn(hvalue(&cards)) || fabs(num(L, &cVT, k(x)) - num(L, &T, k(x))) > 1 ||
                    fabs(num(L, &cVT, k(y)) - num(L, &T, k(y))) > 1)
                    draw_card(L, &cards, i, &layer, &focused, &dragging, 0);
            }
        }
        if (type == k(joker) || type == k(consumeable) || type == k(shop) || type == k(title_2))
            for (int pass = 0; pass < 2; pass++) // the highlighted ones over the rest
                for (int i = 1; i <= n; i++) {
                    TValue card = *luaH_getnum(hvalue(&cards), i);
                    if (!luaO_rawequalObj(&card, &focused) && truthy(L, &card, k(highlighted)) == pass) draw_card(L, &cards, i, &layer, &focused, &dragging, 0);
                }
        if (type == k(discard))
            for (int i = 1; i <= n; i++) {
                TValue card = *luaH_getnum(hvalue(&cards), i);
                if (luaO_rawequalObj(&card, &focused)) continue;
                TValue cVT = sub(L, &card, k(VT));
                if (fabs(num(L, &cVT, k(x)) - num(L, &T, k(x))) > 1) draw_card(L, &cards, i, &layer, &focused, &dragging, 0);
            }
        if (type == k(hand) || type == k(play) || type == k(title) || type == k(voucher))
            for (int i = 1; i <= n; i++) draw_card(L, &cards, i, &layer, &focused, &dragging, is_hand);
    }
    if (is_deck) {
        TValue buttons = sub(L, &g, k(buttons)), collide = sub(L, &states, k(collide));
        int over = truthy(L, &g, k(deck_preview)) || truthy(L, &collide, k(is));
        if (!over && !l_isfalse(&buttons)) {
            TValue bs = sub(L, &buttons, k(states)), bc = sub(L, &bs, k(collide));
            over = truthy(L, &bc, k(is)) && pad;
        }
        if (over) draw_if(L, &children, k(view_deck));
        draw_if(L, &children, k(peek_deck));
    }
    return 0;
}

// Sprite:draw_shader(_shader, _shadow_height, _send, _no_tilt, other_obj, ms, mr, mx, my, custom_shader,
// tilt_shadow), sprite.lua:73, for the game's own shaders but 'vortex'; those and the custom ones run the game's Lua.
// The uniforms the Lua sends to the shader are not sent here (they were 8 % of the Lua of a frame): the recipe
// in gfx.c gets the ones it reads straight from the sprite.
static void numbers(const TValue *t, float out[4]) { // t[1] .. t[4] of a colour or of a shader's own pair; zeros if there is none
    for (int j = 0; j < 4; j++) {
        const TValue *n = ttistable(t) ? luaH_getnum(hvalue(t), j + 1) : luaO_nilobject;
        out[j] = ttisnumber(n) ? nvalue(n) : 0;
    }
}
static int l_sprite_draw_shader(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 12);
    if (lua_toboolean(L, 11) || (ttisstring(L->base + 1) && rawtsvalue(L->base + 1) == k(vortex)))
        return original(L, orig_sprite_draw_shader, 12);
    TValue self = L->base[0], role = sub(L, &self, k(role)), major = sub(L, &role, k(draw_major));
    if (l_isfalse(&major)) major = self;
    int shadow = lua_toboolean(L, 3);
    lua_Number height = shadow ? luaL_checknumber(L, 3) : 0;
    if (shadow) {
        TValue VT = sub(L, &self, k(VT)), sp = sub(L, &major, k(shadow_parrallax));
        setn(L, &VT, k(y), num(L, &VT, k(y)) - num(L, &sp, k(y)) * height);
        setn(L, &VT, k(x), num(L, &VT, k(x)) - num(L, &sp, k(x)) * height);
        setn(L, &VT, k(scale), num(L, &VT, k(scale)) * (1 - 0.2 * height));
    }
    {
        int plain = !ttisstring(L->base + 1) || rawtsvalue(L->base + 1) == k(dissolve); // _shader or 'dissolve'
        const TValue *d = idx(L, &major, k(dissolve));
        lua_Number amount = ttisnumber(d) ? fabs(nvalue(d)) : 0, time = 0;
        float burn1[4] = {0}, burn2[4] = {0}, own[4];
        if (amount >= 0.001 || !plain) { // (the plain dissolve of a whole sprite reads neither)
            time = 123.33412 * (num(L, &major, k(ID)) / 1.14212);
            time -= floor(time / 3000) * 3000; // Lua's %
        }
        if (amount >= 0.001) {
            TValue colours = sub(L, &major, k(dissolve_colours)); // each, or G.C.CLEAR
            if (ttistable(&colours)) numbers(luaH_getnum(hvalue(&colours), 1), burn1), numbers(luaH_getnum(hvalue(&colours), 2), burn2);
        }
        numbers(L->base + 3, own); // _send
        gfx_card_shader(plain ? "dissolve" : svalue(L->base + 1), shadow, amount, time, burn1, burn2, own[0], own[1]);
        // mouse_screen_pos, screen_scale and hovering (_no_tilt is argument 5, tilt_shadow 12)
        TValue g = game(L), settings = sub(L, &g, k(SETTINGS));
        const TValue *hover = idx(L, &major, k(hover_tilt)), *damping = idx(L, &major, k(mouse_damping));
        if (ttisnumber(hover) && nvalue(hover) > 0 && !(shadow && !lua_toboolean(L, 12)) && !lua_toboolean(L, 5) &&
            !truthy(L, &settings, k(reduced_motion))) {
            TValue at = sub(L, &major, k(tilt_var));
            int own_pos = !l_isfalse(&at);
            if (!own_pos) at = sub(L, &g, k(CONTROLLER)), at = sub(L, &at, k(cursor_position));
            lua_Number canv = num(L, &g, k(CANV_SCALE));
            gfx_tilt(nvalue(hover) * luaL_optnumber(L, 12, 1), num(L, &at, own_pos ? k(mx) : k(x)) * canv,
                     num(L, &at, own_pos ? k(my) : k(y)) * canv,
                     num(L, &g, k(TILESCALE)) * num(L, &g, k(TILESIZE)) * (ttisnumber(damping) ? nvalue(damping) : 1) * canv);
        }
    }
    if (lua_toboolean(L, 6)) {
        push(L, idx(L, L->base, interned(L, "draw_from")));
        lua_pushvalue(L, 1);
        for (int i = 6; i <= 10; i++) lua_pushvalue(L, i);
        lua_call(L, 6, 0);
    } else
        call0(L, L->base, "draw_self");
    if (shadow) {
        TValue VT = sub(L, &self, k(VT)), sp = sub(L, &major, k(shadow_parrallax));
        setn(L, &VT, k(y), num(L, &VT, k(y)) + num(L, &sp, k(y)) * height);
        setn(L, &VT, k(x), num(L, &VT, k(x)) + num(L, &sp, k(x)) * height);
        setn(L, &VT, k(scale), num(L, &VT, k(scale)) / (1 - 0.2 * height));
    }
    gfx_plain();
    return 0;
}

static int orig_uielement_update; // registry ref of the game's UIElement.update

// UIElement:update(dt), ui.lua:938. A button on its delay runs the game's Lua; update_text and update_object are
// the game's too, update_text only when it has something to do. Node.update, which it ends with, is empty.
static int l_uielement_update(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    TValue self = L->base[0], g = game(L), config = sub(L, &self, k(config));
    if (truthy(L, &config, k(button_delay))) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, orig_uielement_update);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 0);
        return 0;
    }
    TValue args = sub(L, &g, k(ARGS)), tracker = sub_or_new(L, &args, k(FUNC_TRACKER));
    TValue temp = sub(L, &config, k(button_temp));
    if (!l_isfalse(&temp)) setv(L, &config, k(button), &temp);
    if (truthy(L, &self, k(button_clicked))) setv(L, &self, k(button_clicked), luaO_nilobject);
    TValue func = sub(L, &config, k(func));
    if (!l_isfalse(&func)) {
        lua_settop(L, 2);
        push(L, &tracker); // 3
        push(L, &func);    // 4
        lua_pushvalue(L, 4);
        lua_gettable(L, 3);
        lua_pushnumber(L, (lua_isnil(L, -1) ? 0 : luaL_checknumber(L, -1)) + 1);
        lua_remove(L, -2);
        lua_pushvalue(L, 4);
        lua_insert(L, -2);
        lua_settable(L, 3); // G.ARGS.FUNC_TRACKER[func] = (... or 0) + 1
        TValue funcs = sub(L, &g, k(FUNCS));
        push(L, &funcs);
        lua_pushvalue(L, 4);
        lua_gettable(L, -2);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 0); // G.FUNCS[func](self)
        lua_settop(L, 2);
    }
    TValue uit = sub(L, &self, k(UIT)), uits = sub(L, &g, k(UIT));
    if (luaO_rawequalObj(&uit, idx(L, &uits, k(T)))) {
        config = sub(L, &self, k(config));
        int todo = !l_isfalse(&config) && truthy(L, &config, k(text)) && !truthy(L, &config, k(text_drawable));
        TValue table = sub(L, &config, k(ref_table));
        if (!todo && !l_isfalse(&table)) { // config.ref_table[config.ref_value] ~= config.prev_value, as the VM compares
            TValue key = sub(L, &config, k(ref_value)), prev = sub(L, &config, k(prev_value));
            push(L, &table);
            push(L, &key);
            lua_gettable(L, -2);
            push(L, &prev);
            todo = !lua_equal(L, -1, -2);
            lua_pop(L, 3);
        }
        if (todo) call0(L, &self, "update_text");
    }
    uit = sub(L, &self, k(UIT));
    if (luaO_rawequalObj(&uit, idx(L, &uits, k(O)))) call0(L, &self, "update_object");
    return 0;
}

// engine.install(): put the C functions over the game's. Called by boot.lua once the game's Lua is loaded.
static int l_install(lua_State *L) {
    static const luaL_Reg moveable[] = {
        {"move", l_move}, {"move_juice", l_move_juice}, {"move_xy", l_move_xy}, {"move_scale", l_move_scale},
        {"move_wh", l_move_wh}, {"move_r", l_move_r}, {"calculate_parrallax", l_calculate_parrallax},
        {"lr_clamp", l_lr_clamp}, {"align_to_major", l_align_to_major}, {"get_major", l_get_major},
        {"glue_to_major", l_glue_to_major}, {"move_with_major", l_move_with_major}, {NULL, NULL}};
    static const luaL_Reg node[] = {{"collides_with_point", l_collides_with_point}, {"translate_container", l_translate_container}, {NULL, NULL}};
    static const luaL_Reg uielement[] = {{"draw_pixellated_rect", l_draw_pixellated_rect}, {"draw_self", l_draw_self}, {"draw_children", l_draw_children}, {"update", l_uielement_update}, {NULL, NULL}};
    static const luaL_Reg dynatext[] = {{"draw", l_dynatext_draw}, {NULL, NULL}};
    static const luaL_Reg sprite[] = {{"draw_self", l_sprite_draw_self}, {"draw_from", l_sprite_draw_from}, {"draw_shader", l_sprite_draw_shader}, {NULL, NULL}};
    static const luaL_Reg globals[] = {{"point_translate", l_point_translate}, {"point_rotate", l_point_rotate}, {"prep_draw", l_prep_draw}, {NULL, NULL}};
    for (int i = 0; i < K_COUNT; i++) {
        K[i] = luaS_new(L, key_names[i]);
        luaS_fix(K[i]); // never collected
    }
    lua_getglobal(L, "Moveable");
    lua_getfield(L, -1, "align_to_major");
    orig_align_to_major = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_register(L, NULL, moveable);
    lua_getglobal(L, "Node");
    luaL_register(L, NULL, node);
    lua_getglobal(L, "UIElement");
    lua_getfield(L, -1, "draw_pixellated_rect");
    orig_draw_pixellated_rect = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "draw_self");
    orig_draw_self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "update");
    orig_uielement_update = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_register(L, NULL, uielement);
    lua_getglobal(L, "DynaText");
    lua_getfield(L, -1, "draw");
    orig_dynatext_draw = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_register(L, NULL, dynatext);
    lua_getglobal(L, "Sprite");
    lua_getfield(L, -1, "draw_self");
    orig_sprite_draw_self = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "draw_from");
    orig_sprite_draw_from = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "draw_shader");
    orig_sprite_draw_shader = luaL_ref(L, LUA_REGISTRYINDEX);
    luaL_register(L, NULL, sprite);
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, NULL, globals);
    static const char *const names[13] = {"Temperance", "Throwback", "Driver's License", "Steel Joker", "Cloud 9", "Stone Joker", "Joker Stencil",
                                          "The Wheel of Fortune", "Ectoplasm", "Hex", "Blueprint", "Brainstorm", "Swashbuckler"};
    for (int i = 0; i < 13; i++) luaS_fix(busy[i] = luaS_new(L, names[i]));
    luaS_fix(question = luaS_new(L, "?"));
    lua_getglobal(L, "Card");
    lua_getfield(L, -1, "update");
    orig_card_update = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, l_card_update);
    lua_setfield(L, -2, "update");
    luaS_fix(s_glass = luaS_new(L, "Glass Card"));
    luaS_fix(s_stone = luaS_new(L, "Stone Card"));
    luaS_fix(s_none = luaS_new(L, "NONE"));
    luaS_fix(s_soul = luaS_new(L, "The Soul"));
    luaS_fix(s_antimatter = luaS_new(L, "Antimatter"));
    luaS_fix(s_invisible = luaS_new(L, "Invisible Joker"));
    lua_getfield(L, -1, "draw");
    orig_card_draw = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, l_card_draw);
    lua_setfield(L, -2, "draw");
    lua_getglobal(L, "CardArea");
    lua_getfield(L, -1, "draw");
    orig_cardarea_draw = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, l_cardarea_draw);
    lua_setfield(L, -2, "draw");
    lua_getfield(L, -1, "align_cards");
    orig_align_cards = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, l_align_cards);
    lua_setfield(L, -2, "align_cards");
    return 0;
}

int luaopen_engine(lua_State *L) {
    static const luaL_Reg engine[] = {{"install", l_install}, {NULL, NULL}};
    luaL_register(L, "engine", engine);
    return 1;
}
