// love.graphics, the part that draws: a transform stack, the colour, rectangles, polygons, textures
// with quads and canvases, as the `gfx` module boot.lua builds love.graphics from. Everything ends in raster():
// a parallelogram or a triangle in target pixels, flat or textured.
//
// On the EE that is the GS (gfx_gs.c). On the host it is a software rasteriser, on only when screenshots are
// asked for (SHOT=), so the picture can be checked without the emulator; otherwise nothing is drawn there.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

#include "gfx.h"
#include "gfx_gs.h"
#include "pack.h"

static int soft; // the software rasteriser is on (gfx.init)

typedef struct {
    int w, h;
    unsigned char *idx, (*pal)[4]; // an image: palette indices
    unsigned char *rgba;           // a canvas (and the screen, texture 0)
    struct GsTex *gs;              // an image, on the EE
    int smooth;                    // an image sampled between its texels (the font)
} Tex;
static Tex tex[256];
static int ntex = 1;
static Tex *target = tex;

typedef struct { float a, b, c, d, x, y; } Mat; // x' = a x + c y + x, y' = b x + d y + y
static Mat stack[64];
static int sp;
#define M stack[sp]
static float colour[4] = {1, 1, 1, 1}, line_width = 1;

static int primitives; // raster() calls since gfx.count()
static int frames;     // gfx.present() calls
// A shader's recipe (below) changes each texel of the sprite being drawn: texel (i, j) from the sprite's corner.
static Filter *filter;
static int kind;       // which recipe filter is
static unsigned stamp; // of what filter makes of a sprite: the same sprite, kind and stamp, the same texels; 0: none
static int linear; // the texture (an rgba one) is sampled between its texels
static float bend[2];  // of a tilted sprite (blit): its last corner is p0 + e1 + e2 + bend
// p0, p0 + e1, p0 + e2 (and p0 + e1 + e2 unless tri), in target pixels. Textured: (u, v) at p0, du along e1, dv
// along e2, in texels, nearest unless linear.
static const Tex *fetch_tex; // the atlas of the sprite the rasteriser is drawing, and the sprite's corner in it
static int fetch_u, fetch_v;
static void fetch(int i, int j, unsigned char *rgba) { // the software rasteriser's Fetch (gfx_gs.h): beyond the atlas, its edge
    int x = fetch_u + i, y = fetch_v + j;
    x = x < 0 ? 0 : x >= fetch_tex->w ? fetch_tex->w - 1 : x, y = y < 0 ? 0 : y >= fetch_tex->h ? fetch_tex->h - 1 : y;
    memcpy(rgba, fetch_tex->pal[fetch_tex->idx[y * fetch_tex->w + x]], 4);
}
static void raster(const float p0[2], const float e1[2], const float e2[2], int tri, const Tex *t, float u, float v, float du, float dv) {
    primitives++;
#ifdef NO_RASTER // -DNO_RASTER: nothing reaches the GS, to time the rest of the draw
    return;
#endif
#ifdef _EE
    if (t && t->rgba) gs_raster_rgba(p0, e1, e2, t->rgba, t->w, t->h, u, v, du, dv, linear);
    else gs_raster(p0, e1, e2, bend, tri, t ? t->gs : NULL, u, v, du, dv, colour, filter, kind, stamp);
    return;
#endif
    float det = e1[0] * e2[1] - e1[1] * e2[0];
    if (!soft || fabsf(det) < 1e-9f) return;
    // a bent quad is two triangles, as the GS draws it: the second from the last corner, p3, along f1 and f2
    int bent = !tri && (bend[0] != 0 || bend[1] != 0);
    float p3[2] = {p0[0] + e1[0] + e2[0] + bend[0], p0[1] + e1[1] + e2[1] + bend[1]};
    float f1[2] = {-e1[0] - bend[0], -e1[1] - bend[1]}, f2[2] = {-e2[0] - bend[0], -e2[1] - bend[1]}, det2 = f1[0] * f2[1] - f1[1] * f2[0];
    if (bent && fabsf(det2) < 1e-9f) return;
    fetch_tex = t, fetch_u = (int)u, fetch_v = (int)v;
    float x0 = p0[0], x1 = x0, y0 = p0[1], y1 = y0;
    for (int i = 1; i < (tri ? 3 : 4); i++) {
        float x = i == 3 ? p3[0] : p0[0] + (i & 1 ? e1[0] : 0) + (i & 2 ? e2[0] : 0), y = i == 3 ? p3[1] : p0[1] + (i & 1 ? e1[1] : 0) + (i & 2 ? e2[1] : 0);
        x0 = fminf(x0, x), x1 = fmaxf(x1, x), y0 = fminf(y0, y), y1 = fmaxf(y1, y);
    }
    int ix0 = x0 < 0 ? 0 : (int)x0, iy0 = y0 < 0 ? 0 : (int)y0;
    int ix1 = x1 >= target->w ? target->w - 1 : (int)x1, iy1 = y1 >= target->h ? target->h - 1 : (int)y1;
    for (int y = iy0; y <= iy1; y++)
        for (int x = ix0; x <= ix1; x++) {
            float px = x + 0.5f - p0[0], py = y + 0.5f - p0[1];
            float s = (px * e2[1] - py * e2[0]) / det, r = (e1[0] * py - e1[1] * px) / det;
            if (bent && s + r > 1) {
                float qx = x + 0.5f - p3[0], qy = y + 0.5f - p3[1];
                float a = (qx * f2[1] - qy * f2[0]) / det2, b = (f1[0] * qy - f1[1] * qx) / det2;
                if (a < 0 || b < 0) continue;
                s = 1 - a, r = 1 - b;
            } else if (s < 0 || r < 0 || (tri || bent ? s + r > 1 : s >= 1 || r >= 1)) continue;
            float c[4] = {colour[0], colour[1], colour[2], colour[3]};
            if (t && (linear || t->smooth)) { // as the GS does it: texel centres at .5, the edges clamped
                float fu = u + s * du - 0.5f, fv = v + r * dv - 0.5f, ax = fu - floorf(fu), ay = fv - floorf(fv);
                int xa = (int)floorf(fu), ya = (int)floorf(fv), xb = xa + 1, yb = ya + 1;
                xa = xa < 0 ? 0 : xa, ya = ya < 0 ? 0 : ya, xb = xb >= t->w ? t->w - 1 : xb, yb = yb >= t->h ? t->h - 1 : yb;
#define AT(x, y) (t->rgba ? t->rgba + ((y) * t->w + (x)) * 4 : t->pal[t->idx[(y) * t->w + (x)]])
                const unsigned char *aa = AT(xa, ya), *ba = AT(xb, ya), *ab = AT(xa, yb), *bb = AT(xb, yb);
#undef AT
                for (int i = 0; i < 4; i++)
                    c[i] *= ((aa[i] * (1 - ax) + ba[i] * ax) * (1 - ay) + (ab[i] * (1 - ax) + bb[i] * ax) * ay) / 255.0f;
            } else if (t) {
                int tx = (int)floorf(u + s * du), ty = (int)floorf(v + r * dv);
                if (tx < 0 || ty < 0 || tx >= t->w || ty >= t->h) continue;
                unsigned char texel[4];
                memcpy(texel, t->rgba ? t->rgba + (ty * t->w + tx) * 4 : t->pal[t->idx[ty * t->w + tx]], 4);
                if (filter) filter(tx - (int)u, ty - (int)v, texel, fetch);
                for (int i = 0; i < 4; i++) c[i] *= texel[i] / 255.0f;
            }
            unsigned char *d = target->rgba + (y * target->w + x) * 4;
            for (int i = 0; i < 3; i++) d[i] = (unsigned char)(c[i] * c[3] * 255 + d[i] * (1 - c[3]) + 0.5f);
            d[3] = (unsigned char)(c[3] * 255 + d[3] * (1 - c[3]) + 0.5f);
        }
}

static void transform(float x, float y, float out[2]) {
    out[0] = M.a * x + M.c * y + M.x;
    out[1] = M.b * x + M.d * y + M.y;
}
void gfx_translate(float x, float y) {
    float p[2];
    transform(x, y, p);
    M.x = p[0], M.y = p[1];
}
// The rectangle (x, y, w, h) of the current coordinates, flat or showing (u, v, uw, vh) of t.
static void rect(float x, float y, float w, float h, const Tex *t, float u, float v, float uw, float vh) {
    float p0[2], p1[2], p2[2];
    transform(x, y, p0);
    transform(x + w, y, p1);
    transform(x, y + h, p2);
    float e1[2] = {p1[0] - p0[0], p1[1] - p0[1]}, e2[2] = {p2[0] - p0[0], p2[1] - p0[1]};
    raster(p0, e1, e2, 0, t, u, v, uw, vh);
}
static void line(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    if (len == 0) return;
    float nx = -dy / len * line_width / 2, ny = dx / len * line_width / 2, p0[2], p1[2], p2[2];
    transform(ax - nx, ay - ny, p0);
    transform(bx - nx, by - ny, p1);
    transform(ax + nx, ay + ny, p2);
    float e1[2] = {p1[0] - p0[0], p1[1] - p0[1]}, e2[2] = {p2[0] - p0[0], p2[1] - p0[1]};
    raster(p0, e1, e2, 0, NULL, 0, 0, 0, 0);
}
// n points (x, y, x, y, ...): a convex polygon filled as a fan, or its outline
void gfx_polygon(lua_State *L, const float *xy, int n, const char *mode) {
    if (!strcmp(mode, "line")) {
        for (int i = 0; i < n; i++) line(xy[i * 2], xy[i * 2 + 1], xy[(i + 1) % n * 2], xy[(i + 1) % n * 2 + 1]);
        return;
    }
    if (strcmp(mode, "fill")) luaL_error(L, "gfx: draw mode '%s'", mode);
    float p0[2], a[2], b[2];
    transform(xy[0], xy[1], p0);
    transform(xy[2], xy[3], a);
    for (int i = 2; i < n; i++, a[0] = b[0], a[1] = b[1]) {
        transform(xy[i * 2], xy[i * 2 + 1], b);
        float e1[2] = {a[0] - p0[0], a[1] - p0[1]}, e2[2] = {b[0] - p0[0], b[1] - p0[1]};
        raster(p0, e1, e2, 1, NULL, 0, 0, 0, 0);
    }
}

int gfx_push(lua_State *L) {
    if (sp == 63) luaL_error(L, "gfx: transform stack overflow");
    stack[sp + 1] = M;
    sp++;
    return 0;
}
int gfx_pop(lua_State *L) {
    if (sp == 0) luaL_error(L, "gfx: pop without push");
    sp--;
    return 0;
}
static int l_origin(lua_State *L) { M = (Mat){1, 0, 0, 1, 0, 0}; return 0; }
static int l_translate(lua_State *L) {
    gfx_translate(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
    return 0;
}
void gfx_scale(float sx, float sy) { M.a *= sx, M.b *= sx, M.c *= sy, M.d *= sy; }
static int l_scale(lua_State *L) {
    float sx = luaL_checknumber(L, 1);
    gfx_scale(sx, luaL_optnumber(L, 2, sx));
    return 0;
}
void gfx_rotate(float r) {
    float c = cosf(r), s = sinf(r);
    Mat m = M;
    M.a = m.a * c + m.c * s, M.b = m.b * c + m.d * s, M.c = m.c * c - m.a * s, M.d = m.d * c - m.b * s;
}
static int l_rotate(lua_State *L) { gfx_rotate(luaL_checknumber(L, 1)); return 0; }

// (r, g, b [, a]) or {r, g, b [, a]} from argument i
static void get_colour(lua_State *L, int i, float out[4]) {
    out[3] = 1;
    for (int j = 0; j < 4; j++) {
        if (lua_istable(L, i)) lua_rawgeti(L, i, j + 1);
        else lua_pushvalue(L, i + j);
        if (j < 3 || !lua_isnoneornil(L, -1)) out[j] = luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
}
void gfx_colour(float r, float g, float b, float a) { colour[0] = r, colour[1] = g, colour[2] = b, colour[3] = a; }
static int l_setColor(lua_State *L) { get_colour(L, 1, colour); return 0; }
static int l_setLineWidth(lua_State *L) { line_width = luaL_checknumber(L, 1); return 0; }
static int l_clear(lua_State *L) {
    float c[4] = {0, 0, 0, 0};
    if (!lua_isnoneornil(L, 1)) get_colour(L, 1, c);
#ifdef _EE
    gs_clear(c);
#endif
    if (soft)
        for (int i = 0; i < target->w * target->h * 4; i++) target->rgba[i] = (unsigned char)(c[i & 3] * 255 + 0.5f);
    return 0;
}
// rectangle(mode, x, y, w, h): the corner radius LÖVE takes after these is ignored (the loading bar's)
static int l_rectangle(lua_State *L) {
    float x = luaL_checknumber(L, 2), y = luaL_checknumber(L, 3), w = luaL_checknumber(L, 4), h = luaL_checknumber(L, 5);
    float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    gfx_polygon(L, xy, 4, luaL_checkstring(L, 1));
    return 0;
}
void gfx_fill_rectangle(lua_State *L, float x, float y, float w, float h) {
    float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    gfx_polygon(L, xy, 4, "fill");
}
// polygon(mode, {x, y, ...}) or polygon(mode, x, y, ...)
static int l_polygon(lua_State *L) {
    float xy[512];
    int table = lua_istable(L, 2), n = table ? (int)lua_objlen(L, 2) : lua_gettop(L) - 1;
    if (n < 6 || n > 512) luaL_error(L, "gfx: polygon of %d numbers", n);
    for (int i = 0; i < n; i++) {
        if (table) lua_rawgeti(L, 2, i + 1);
        else lua_pushvalue(L, i + 2);
        xy[i] = luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    gfx_polygon(L, xy, n / 2, luaL_checkstring(L, 1));
    return 0;
}

static Tex *new_tex(lua_State *L, int w, int h) {
    if (ntex == 256) luaL_error(L, "gfx: more than 256 textures");
    Tex *t = &tex[ntex++];
    t->w = w, t->h = h;
    return t;
}
// gfx.texture(file) -> id, width, height: a .tex of tools/textures.py
static int l_texture(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    unsigned char head[4];
    if (!pack_read(path, 0, head, 4)) luaL_error(L, "gfx: no texture %s (tools/textures.py writes them)", path);
    Tex *t = new_tex(L, head[0] | head[1] << 8, head[2] | head[3] << 8);
#ifdef _EE
    unsigned char pal[256][4]; // the texels stay in the file until the GS needs them (gfx_gs.c)
    if (!pack_read(path, 4, pal, 1024)) luaL_error(L, "gfx: short texture %s", path);
    t->gs = gs_texture(t->w, t->h, path, pal);
    if (!t->gs) luaL_error(L, "gfx: no memory for texture %s", path);
#else
    if (soft) {
        t->pal = malloc(1024);
        t->idx = malloc(t->w * t->h);
        if (!t->pal || !t->idx || !pack_read(path, 4, t->pal, 1024) || !pack_read(path, 4 + 1024, t->idx, t->w * t->h))
            luaL_error(L, "gfx: short texture %s", path);
    }
#endif
    lua_pushinteger(L, t - tex);
    lua_pushinteger(L, t->w);
    lua_pushinteger(L, t->h);
    return 3;
}
// gfx.canvas(width, height) -> id
static int l_canvas(lua_State *L) {
    Tex *t = new_tex(L, luaL_checkint(L, 1), luaL_checkint(L, 2));
    if (soft) t->rgba = calloc(t->w * t->h, 4);
    lua_pushinteger(L, t - tex);
    return 1;
}
// gfx.target([id]): draw to that canvas, or to the screen
static int l_target(lua_State *L) {
    int id = luaL_optint(L, 1, 0);
    luaL_argcheck(L, id >= 0 && id < ntex && (id == 0 || tex[id].rgba || !soft), 1, "not a canvas");
    target = &tex[id];
    return 0;
}
// ---------------------------------------------------------------- shaders
// The GS has no shaders, so each of the game's has a recipe here, chosen by the shader's file name, that draws
// what its GLSL does (design D7). A shader without a recipe passes through (texture x colour): CRT (the whole
// frame again: the canvas is the frame buffer here).
//  - dissolve (every card and its shadow): the fragment shader, run per texel of the sprite instead of per pixel
//    of the screen, which is what it computes anyway (it floors to the sprite's texels). Not dissolving, it is
//    only a colour: white, or black at 30 % for a shadow.
//  - debuff, played, voucher, booster, negative, negative_shine, foil, holo, polychrome, hologram (what a card wears): the
//    same, per texel, each with the dissolve's mask. On the EE the texels a recipe makes are kept and, for the
//    ones that move, made again a few frames apart (gfx_gs.c).
//  - flame (the fire under the chips and the mult of a hand that wins): the fragment shader on a grid over its
//    sprite, coarser than the game's and each flame every other frame.
//  - background, splash (the swirls behind a run and behind the menu) and flash (the intro's white): the fragment
//    shader on a grid, which the GS stretches over the screen between texels ("The swirls" below).
static float sine[1024]; // a turn in 1024 steps, the nearest one taken: the recipes take thousands of sines a frame, too many for libm's on the EE
static inline __attribute__((always_inline)) float fsin(float x) { return sine[(int)(x * (1024 / 6.2831853f)) & 1023]; }
static inline __attribute__((always_inline)) float fcos(float x) { return fsin(x + 1.5707963f); }
static float ffloor(float v) { // libm's floorf is a loop of its own on the EE
    int i = (int)v;
    return v < i ? i - 1 : i;
}

// The shaders of a card, each a file's name, then the ones of the whole screen
static enum { PLAIN, DISSOLVE, DEBUFF, PLAYED, VOUCHER, BOOSTER, NEGATIVE_SHINE, NEGATIVE, FOIL, HOLO, POLYCHROME, HOLOGRAM, FLAME, BACKGROUND, SPLASH, FLASH } shader;
static const char *const card_shaders[] = {"dissolve", "debuff", "played", "voucher", "booster", "negative_shine", "negative", "foil", "holo", "polychrome", "hologram"};
// A card shader's uniforms: dissolve, time, burn_colour_1 and 2 and shadow, which all of them have, and the two
// numbers each takes under its own name (x, y)
static struct { int shadow; float amount, time, burn1[4], burn2[4], x, y; } dis;
void gfx_plain(void) { shader = PLAIN; }
// The vertex half of the card shaders (the same in each file): `hovering`, `mouse_screen_pos` and `screen_scale`.
// It adds to a corner's w, which moves the corner away from the middle of the screen: a card leans under the
// cursor, and the Jokers sway. ponytail: the texels of the leaning card go straight along each of its two
// triangles, not in perspective (a few percent of a card).
static struct { float hovering, x, y, scale; } tilt;
void gfx_tilt(float hovering, float x, float y, float scale) { tilt.hovering = hovering, tilt.x = x, tilt.y = y, tilt.scale = scale; }
static void lean(float p[2]) {
    float cx = target->w / 2.0f, cy = target->h / 2.0f, dx = p[0] - cx, dy = p[1] - cy;
    float mid = sqrtf(dx * dx + dy * dy) / sqrtf((float)target->w * target->w + (float)target->h * target->h);
    float ox = (p[0] - tilt.x) / tilt.scale, oy = (p[1] - tilt.y) / tilt.scale;
    float w = 1 + 0.2f * (-0.03f - 0.3f * (mid < 0.3f ? 0.3f - mid : 0)) * tilt.hovering * (ox * ox + oy * oy) / (2 - mid);
    if (w > 0.1f) p[0] = cx + dx / w, p[1] = cy + dy / w;
}
void gfx_card_shader(const char *name, int shadow, float amount, float time, const float burn1[4], const float burn2[4], float x, float y) {
    shader = PLAIN, tilt.hovering = 0;
    for (int i = 0; i < 11; i++)
        if (!strcmp(name, card_shaders[i])) shader = DISSOLVE + i;
    dis.shadow = shadow, dis.amount = amount, dis.time = time, dis.x = x, dis.y = y;
    memcpy(dis.burn1, burn1, sizeof dis.burn1);
    memcpy(dis.burn2, burn2, sizeof dis.burn2);
}

static float least(float a, float b) { return a < b ? a : b; }
static float most(float a, float b) { return a > b ? a : b; }
// The noise field dissolve.fs, holo.fs and polychrome.fs share: where it is read from at time t, and its value
static void field_from(float t, float o[6]) {
    o[0] = 50 * fsin(-t / 143.6340f), o[1] = 50 * fcos(-t / 99.4324f), o[2] = 50 * fcos(t / 53.1532f), o[3] = 50 * fcos(t / 61.4532f);
    o[4] = 50 * fsin(-t / 87.53218f), o[5] = 50 * fsin(-t / 49.0f);
}
static float field_at(float x, float y, const float o[6]) {
    float la = sqrtf((x + o[0]) * (x + o[0]) + (y + o[1]) * (y + o[1])), lb = sqrtf((x + o[2]) * (x + o[2]) + (y + o[3]) * (y + o[3]));
    float lc = sqrtf((x + o[4]) * (x + o[4]) + (y + o[5]) * (y + o[5]));
    return (1 + fcos(la / 19.483f) + fsin(lb / 33.155f) * fcos((y + o[3]) / 15.73f) + fcos(lc / 27.193f) * fsin((x + o[4]) / 21.92f)) / 2;
}
// The field of the dissolve does not move: `time` is a number per card. Per texel of a w x h sprite, the cosine
// and sine (x 32767) of (field - .5) * 3.14, kept while a card with that `time` is dissolving.
typedef struct { float time; int w, h, used; short *cs; } Field;
static Field fields[12];
static const Field *field_for(float time, int w, int h) {
    Field *f = fields;
    for (Field *g = fields; g < fields + 12; g++) {
        if (g->cs && g->time == time && g->w == w && g->h == h) {
            g->used = frames;
            return g;
        }
        if (g->used < f->used) f = g;
    }
    free(f->cs);
    if (!(f->cs = malloc(w * h * 2 * sizeof *f->cs))) return NULL;
    f->time = time, f->w = w, f->h = h, f->used = frames;
    float m = w > h ? w : h, o[6];
    field_from(time * 10 + 2003, o);
    short *cs = f->cs;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++, cs += 2) {
            float phase = (field_at((i / m - 0.5f) * 2.3f * m, (j / m - 0.5f) * 2.3f * m, o) - 0.5f) * 3.14f;
            cs[0] = (short)(fcos(phase) * 32767), cs[1] = (short)(fsin(phase) * 32767);
        }
    return f;
}

// The shaders' HSL(), hue() and RGB()
static float fract(float v) { return v - ffloor(v); }
static void to_hsl(const float c[4], float hsl[4]) {
    float low = least(c[0], least(c[1], c[2])), high = most(c[0], most(c[1], c[2])), delta = high - low, sum = high + low;
    hsl[0] = hsl[1] = 0, hsl[2] = 0.5f * sum, hsl[3] = c[3];
    if (delta == 0) return;
    hsl[1] = hsl[2] < 0.5f ? delta / sum : delta / (2 - sum);
    if (high == c[0]) hsl[0] = (c[1] - c[2]) / delta;
    else if (high == c[1]) hsl[0] = (c[2] - c[0]) / delta + 2;
    else hsl[0] = (c[0] - c[1]) / delta + 4;
    hsl[0] = fract(hsl[0] / 6);
}
static float hue(float s, float t, float h) {
    float hs = fract(h) * 6;
    return hs < 1 ? (t - s) * hs + s : hs < 3 ? t : hs < 4 ? (t - s) * (4 - hs) + s : s;
}
static void to_rgb(const float c[4], float out[4]) {
    float h = c[0], sat = c[1], l = c[2]; // (out may be c)
    out[3] = c[3];
    if (sat < 0.0001f) {
        out[0] = out[1] = out[2] = l;
        return;
    }
    float t = l < 0.5f ? sat * l + l : -sat * l + (sat + l), s = 2 * l - t;
    out[0] = hue(s, t, h + 1 / 3.0f), out[1] = hue(s, t, h), out[2] = hue(s, t, h - 1 / 3.0f);
}

static const Field *field;              // of the sprite being drawn, if it is dissolving
static int cell_w, cell_h;              // the sprite's texels
static float dis_adj, dis_cos, dis_sin; // adjusted_dissolve, and the cosine and sine of adjusted_dissolve / 82.612
static float dis_o[6];                  // holo.fs and polychrome.fs: where their field is read from this frame
// voucher.fs, booster.fs and negative_shine.fs are one shine with different numbers
static const float shines[3][13] = {
    {13, 5.32f, 10, 2.32f, 12, 6.32f, 4, 13.2f, 8, 0.6f, 0.07f, 0.17f, 0.8f},
    {13, 5.32f, 10, 2.32f, 12, 6.32f, 4, 13.2f, 8, 0.6f, 0.07f, 0.17f, 0.8f},
    {11, 4.32f, 8, 2.32f, 10, 5.32f, 3, 11.2f, 14.4f, 0.7f, 0.27f, 0.27f, 0.5f},
};
// hologram.fs lights each texel by the alpha of the texels around it (9 x 9 of them, 0.0015 of the atlas apart),
// which is the same every frame: kept here per texel of the one sprite that wears it, x 170.
static struct { int id, u, v; unsigned char glow[128 * 128]; } halo;
static int atlas, atlas_w, atlas_h, atlas_u, atlas_v; // the texture being drawn, its size and the sprite's corner in it
static void halo_make(Fetch *fetch) {
    halo.id = atlas, halo.u = atlas_u, halo.v = atlas_v;
    float dx = 0.0015f * atlas_w, dy = 0.0015f * atlas_h;
    for (int j = 0; j < cell_h; j++)
        for (int i = 0; i < cell_w; i++) {
            int n = 0, sum = 0;
            for (int b = -4; b <= 4; b++)
                for (int a = -4; a <= 4; a++) {
                    unsigned char near[4];
                    fetch((int)ffloor(i + 0.5f + a * dx), (int)ffloor(j + 0.5f + b * dy), near);
                    if (near[3] < 230) n++, sum += near[3]; // _a < 0.9
                }
            halo.glow[j * 128 + i] = n ? (unsigned char)(sum / 255.0f / (0.7f * n) * 170) : 0; // (the GLSL divides by zero)
        }
}
// dissolve.fs alone for a texel of a sprite that is dissolving, which is every texel of the menu's logo for its
// first seconds: card_texel's DISSOLVE and its mask without a float per colour. The tint towards the burn colour
// comes from a table per channel and the burn colours are ready as bytes (blit).
static unsigned char dis_tint[3][256], dis_burn[2][4];
static void dissolve_texel(int i, int j, unsigned char *rgba, Fetch *fetch) {
    if (i < 0 || j < 0 || i >= cell_w || j >= cell_h || !rgba[3]) return;
    float d = dis.amount, m = cell_w > cell_h ? cell_w : cell_h, fx = i / m, fy = j / m;
    const short *cs = field->cs + (j * cell_w + i) * 2;
    float res = 0.5f + 0.5f * (cs[0] * dis_cos - cs[1] * dis_sin) / 32767;
    if (fx > 0.8f || fy > 0.8f || fx < 0.2f || fy < 0.2f)
        res -= ((fx > 0.8f ? fx - 0.8f : 0) + (fy > 0.8f ? fy - 0.8f : 0) + (fx < 0.2f ? 0.2f - fx : 0) + (fy < 0.2f ? 0.2f - fy : 0)) * (5 + 5 * d) * d;
    if (res <= dis_adj) {
        rgba[3] = 0;
    } else if (dis.shadow) {
        rgba[0] = rgba[1] = rgba[2] = 0, rgba[3] = (unsigned char)(rgba[3] * 0.3f + 0.5f);
    } else {
        float band = 0.5f - fabsf(dis_adj - 0.5f);
        int burnt = rgba[3] > 2 && dis.burn1[3] > 0.01f && res < dis_adj + 0.8f * band;
        if (burnt && res < dis_adj + 0.5f * band) memcpy(rgba, dis_burn[0], 4);
        else if (burnt && dis.burn2[3] > 0.01f) memcpy(rgba, dis_burn[1], 4);
        else rgba[0] = dis_tint[0][rgba[0]], rgba[1] = dis_tint[1][rgba[1]], rgba[2] = dis_tint[2][rgba[2]];
    }
}

// effect() of the card shader in use for texel (i, j) of the sprite: the texel's colour as the shader leaves it,
// before the vertex colour, then dissolve_mask()
static void card_texel(int i, int j, unsigned char *rgba, Fetch *fetch) {
    if (i < 0 || j < 0 || i >= cell_w || j >= cell_h) return;
    if (!rgba[3] && shader != HOLOGRAM) return; // the others leave a clear texel clear
    float d = dis.amount, tex[4], hsl[4], x = (i + 0.5f) / cell_w, y = (j + 0.5f) / cell_h, r = dis.x, g = dis.y;
    for (int n = 0; n < 4; n++) tex[n] = rgba[n] * (1 / 255.0f);
    float low = least(tex[0], least(tex[1], tex[2])), high = most(tex[0], most(tex[1], tex[2]));
    switch (shader) {
    case DEBUFF: {
        float in[4] = {tex[0] * 0.8f + 0.2f, tex[1] * 0.8f, tex[2] * 0.8f, tex[3]}, width = g != 0 ? 0.1f : 0;
        to_hsl(in, hsl);
        int cross = (x + y > 1 - width && x + y < 1 + width) || (1 - x + y > 1 - width && 1 - x + y < 1 + width);
        if (cross) hsl[0] = 1, hsl[1] = 0.7f, hsl[2] *= 0.8f;
        else hsl[1] = 0.25f, hsl[2] *= 0.7f;
        to_rgb(hsl, tex);
        if (!cross) tex[3] *= 0.3f;
        break;
    }
    case PLAYED:
        to_hsl(tex, hsl);
        hsl[1] = hsl[1] * 0.5f + 0.000001f * r, hsl[2] *= 0.8f;
        to_rgb(hsl, tex);
        tex[3] *= 0.5f;
        break;
    case VOUCHER:
    case BOOSTER:
    case NEGATIVE_SHINE: {
        const float *k = shines[shader - VOUCHER];
        float delta = shader == VOUCHER ? high - low : shader == BOOSTER ? most(high - low, low * 0.7f) : high - low - 0.1f;
        float wave = fcos(r * 5.3f + y * 4.2f - x * 4);
        float fac = 0.8f + 0.9f * fsin(k[0] * x + k[1] * y + r * 12 + wave);
        float fac2 = 0.5f + 0.5f * fsin(k[2] * x + k[3] * y + r * 5 - fcos(r * 2.3f + x * 8.2f));
        float fac3 = 0.5f + 0.5f * fsin(k[4] * x + k[5] * y + r * 6.111f + fsin(r * 5.3f + y * 3.2f));
        float fac4 = 0.5f + 0.5f * fsin(k[6] * x + 2.32f * y + r * 8.111f + fsin(r * 1.3f + y * k[7]));
        float fac5 = fsin(k[8] * x + 5.32f * y + r * 12 + wave);
        float maxfac = k[9] * most(most(fac, most(fac2, most(fac3, 0))) + (fac + fac2 + fac3 * fac4), 0);
        tex[0] = tex[0] * 0.5f + 0.4f - delta + delta * maxfac * (0.7f + fac5 * k[10]) - 0.1f;
        tex[1] = tex[1] * 0.5f + 0.4f - delta + delta * maxfac * (0.7f - fac5 * k[11]) - 0.1f;
        tex[2] = tex[2] * 0.5f + 0.8f - delta + delta * maxfac * 0.7f - 0.1f;
        tex[3] *= k[12] * most(least(1, most(0, 0.3f * most(low * 0.2f, delta) + least(most(maxfac * 0.1f, 0), 0.4f))), 0) + 0.15f * maxfac * (0.1f + delta);
        break;
    }
    case NEGATIVE:
        to_hsl(tex, hsl);
        if (g != 0) hsl[2] = 1 - hsl[2];
        hsl[0] = -hsl[0] + 0.2f;
        to_rgb(hsl, tex);
        tex[0] += 0.8f * 79 / 255, tex[1] += 0.8f * 99 / 255, tex[2] += 0.8f * 103 / 255;
        if (tex[3] < 0.7f) tex[3] /= 3;
        break;
    case FOIL: {
        float ax = (x - 0.5f) * cell_w / cell_h, ay = y - 0.5f, len = sqrtf(ax * ax + ay * ay), delta = least(high, most(0.5f, 1 - low));
        float fac = most(least(2 * fsin(90 * len + r * 2 + 3 * (1 + 0.8f * fcos(113.1121f * len - r * 3.121f))) - 1 - most(5 - 90 * len, 0), 1), 0);
        float rx = fcos(r * 0.1221f), ry = fsin(r * 0.3512f), turn = sqrtf(rx * rx + ry * ry) * len;
        float angle = turn > 0 ? (rx * ax + ry * ay) / turn : 0; // the GLSL divides by zero at the very middle
        float fac2 = most(least(5 * fcos(g * 0.3f + angle * 3.14f * (2.2f + 0.9f * fsin(r * 1.65f + 0.2f * g))) - 4 - most(2 - 20 * len, 0), 1), 0);
        float fac3 = 0.3f * most(least(2 * fsin(r * 5 + x * 3 + 3 * (1 + 0.5f * fcos(r * 7))) - 1, 1), -1);
        float fac4 = 0.3f * most(least(2 * fsin(r * 6.66f + y * 3.8f + 3 * (1 + 0.5f * fcos(r * 3.414f))) - 1, 1), -1);
        float maxfac = most(most(fac, most(fac2, most(fac3, most(fac4, 0)))) + 2.2f * (fac + fac2 + fac3 + fac4), 0);
        tex[0] += -delta + delta * maxfac * 0.3f, tex[1] += -delta + delta * maxfac * 0.3f, tex[2] += delta * maxfac * 1.9f;
        tex[3] = least(tex[3], 0.3f * tex[3] + 0.9f * least(0.5f, maxfac * 0.1f));
        break;
    }
    case HOLO: {
        float in[4] = {0.5f * tex[0], 0.5f * tex[1], 0.5f * tex[2] + 0.5f, tex[3]}, out[4];
        to_hsl(in, hsl);
        float res = 0.5f + 0.5f * fcos(r * 2.612f + (field_at(((float)i / cell_w - 0.5f) * 250, ((float)j / cell_h - 0.5f) * 250, dis_o) - 0.5f) * 3.14f);
        float delta = 0.2f + 0.3f * (high - low) + 0.1f * high, grid = 0.79f;
        float fac = 0.5f * most(most(most(0, 7 * fabsf(fcos(x * grid * 20)) - 6), most(0, 7 * fcos(y * grid * 45 + x * grid * 20) - 6)),
                               most(0, 7 * fcos(y * grid * 45 - x * grid * 20) - 6));
        hsl[0] += res + fac, hsl[1] *= 1.3f, hsl[2] = hsl[2] * 0.6f + 0.4f;
        to_rgb(hsl, out);
        static const float tint[3] = {0.9f, 0.8f, 1.2f};
        for (int n = 0; n < 3; n++) tex[n] = (1 - delta) * tex[n] + delta * out[n] * tint[n];
        tex[3] = (1 - delta) * tex[3] + delta * out[3] * tex[3];
        if (tex[3] < 0.7f) tex[3] /= 3;
        break;
    }
    case POLYCHROME: {
        float fade = 1 - most(0, 0.05f * (1.1f - (high - low))), in[4] = {tex[0] * fade, tex[1] * fade, tex[2], tex[3]};
        to_hsl(in, hsl);
        float res = 0.5f + 0.5f * fcos(r * 2.612f + (field_at(((float)i / cell_w - 0.5f) * 50, ((float)j / cell_h - 0.5f) * 50, dis_o) - 0.5f) * 3.14f);
        hsl[0] += res + g * 0.04f, hsl[1] = least(0.6f, hsl[1] + 0.5f);
        to_rgb(hsl, tex);
        if (tex[3] < 0.7f) tex[3] /= 3;
        break;
    }
    case HOLOGRAM: {
        if (cell_w > 128 || cell_h > 128) break;
        if (halo.id != atlas || halo.u != atlas_u || halo.v != atlas_v) halo_make(fetch);
        float ty = (atlas_v + j + 0.5f) / atlas_h, left = -0.5f, right = -0.5f; // the glitch: rows pushed sideways
        static const float wl[4][2] = {{0.512f, 14}, {-0.8233f, 11.532f}, {0.333f, 13.3f}, {-0.1112331f, 4.044343f}};
        static const float wr[4][2] = {{0.6924f, 19}, {-0.9661f, 21.532f}, {0.4423f, 30.3f}, {-0.13321312f, 3.011f}};
        for (int n = 0; n < 4; n++) left += fsin(g * wl[n][0] + ty * wl[n][1]), right += fsin(g * wr[n][0] + ty * wr[n][1]);
        left *= -10, right *= -10;
        if (left >= 1.5f || left <= 0) left = 0;
        if (right >= 1.5f || right <= 0) right = 0;
        float at = i + 0.5f + 0.002f * (right - left) * atlas_w;
        unsigned char moved[4];
        fetch((int)ffloor(at), j, moved);
        for (int n = 0; n < 4; n++) tex[n] = moved[n] / 255.0f;
        if (tex[3] > 0.999f) tex[0] = tex[1] = tex[2] = tex[3] = 0;
        int clear = tex[3] < 0.001f;
        if (clear) tex[0] = 0, tex[1] = tex[2] = 1;
        x = at / cell_w;
        if (x > 0.95f || x < 0.05f || y > 0.95f || y < 0.05f) {
            tex[0] = tex[1] = tex[2] = tex[3] = 0;
            break;
        }
        float lit = 0.4f * (0.3f * fsin(2 * g) + 0.6f + 0.3f * fsin(r * 3) + 0.9f) * (1 + left + right) * halo.glow[j * 128 + i] / 170;
        static const float glows[2][4] = {{0, 0.3f, 0.2f, 0.3f}, {0, 1, 0.5f, 0.6f}};
        for (int n = 0; n < 4; n++) tex[n] += glows[clear][n] * lit;
        break;
    }
    default:
        break;
    }
    if (field) { // dissolve_mask() of a sprite that is dissolving
        float m = cell_w > cell_h ? cell_w : cell_h, fx = i / m, fy = j / m;
        const short *cs = field->cs + (j * cell_w + i) * 2;
        float res = 0.5f + 0.5f * (cs[0] * dis_cos - cs[1] * dis_sin) / 32767
                    - ((fx > 0.8f ? fx - 0.8f : 0) + (fy > 0.8f ? fy - 0.8f : 0) + (fx < 0.2f ? 0.2f - fx : 0) + (fy < 0.2f ? 0.2f - fy : 0)) * (5 + 5 * d) * d;
        float band = 0.5f - fabsf(dis_adj - 0.5f);
        if (tex[3] > 0.01f && dis.burn1[3] > 0.01f && !dis.shadow && res < dis_adj + 0.8f * band && res > dis_adj) {
            if (res < dis_adj + 0.5f * band) memcpy(tex, dis.burn1, sizeof tex);
            else if (dis.burn2[3] > 0.01f) memcpy(tex, dis.burn2, sizeof tex);
        }
        if (res <= dis_adj) tex[3] = 0;
    }
    if (dis.shadow) tex[0] = tex[1] = tex[2] = 0, tex[3] *= 0.3f;
    for (int n = 0; n < 4; n++) rgba[n] = (unsigned char)(least(1, most(0, tex[n])) * 255 + 0.5f);
}

// The swirls (background.fs, splash.fs) are in blocks of a pixel on this screen and their paint has detail that
// fine: on a coarse grid they are blotches. So they are computed on a SW_W x SW_H grid, a texel the EE's 3 us, but
// SW_ROWS rows a frame: a whole picture takes SW_H / SW_ROWS frames, and the screen shows the last two finished
// ones, fading from one to the next while the third is made. ponytail: the swirl moves as that fade, a second
// or more behind its uniforms (more rows a frame when the EE has the time).
// A picture holds no colours: per texel, how far the paint took it (the one thing the shaders' colours are made
// from), and the colours are a palette made every frame from the uniforms as they are now.
#define SW_W 320
#define SW_H 224
#define SW_ROWS 3
#define SW_SHIFT 47   // background.fs's centre is 0.12 of the diagonal right of the screen's: this many texels
#define SW_FAR_BG 48.0f // the paint's distance where background.fs's colour stops changing at any contrast (2 / (0.035 * 1.2))
#define SW_FAR_SPLASH 19.0f // and splash.fs's
#define BG_W 80 // flash.fs's grid: it is smooth
#define BG_H 56
// the uniforms of background.fs, splash.fs and flash.fs, and the ones the picture being made started with
static struct Screen { float time, spin_time, contrast, spin, vort_speed, mid_flash, vort_offset, c1[4], c2[4], c3[4]; } bg, snap;
// Of each texel's centre from the swirl's (SW_SHIFT texels to the left for background.fs): the angle in 1/65536
// of a turn and the distance in 1/65535 of the diagonal
static unsigned short sw_angle[(SW_W + SW_SHIFT) * SW_H], sw_len[(SW_W + SW_SHIFT) * SW_H];
static unsigned char sw[3][SW_W * SW_H], sw_pal[256][4];
static int sw_a, sw_b, sw_next, sw_row, sw_shader, sw_fresh; // shown, fading in, being made up to sw_row; bits of the ones the GS has not got
// The paint of background.fs and splash.fs: where the texture coordinate (x, y) ends up
static void paint(float *x, float *y, float speed) {
    float ux = *x, uy = *y, vx = ux + uy, vy = ux + uy;
    for (int i = 0; i < 5; i++) {
        float s = fsin(ux > uy ? ux : uy);
        vx += s + ux, vy += s + uy;
        ux += 0.5f * fcos(5.1123314f + 0.353f * vy + speed * 0.131121f), uy += 0.5f * fsin(vx - 0.113f * speed);
        s = fcos(ux + uy) - fsin(ux * 0.711f - uy);
        ux -= s, uy -= s;
    }
    *x = ux, *y = uy;
}
// Rows y0 to y1 of a picture of the swirl in use, with the uniforms of snap
static void swirl_rows(unsigned char *out, int y0, int y1) {
    int of = sw_shader == BACKGROUND ? 0 : SW_SHIFT;
    float speed = snap.time * snap.vort_speed, quick = least(6, speed);
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < SW_W; x++) {
            int at = y * (SW_W + SW_SHIFT) + x + of, i;
            float len = sw_len[at] * (1 / 65535.0f), angle = sw_angle[at] * (6.2831853f / 65536), ux, uy;
            if (sw_shader == BACKGROUND) {
                angle += snap.spin_time * 0.5f * 0.2f + 302.2f - 0.5f * 20 * (snap.spin * len + (1 - snap.spin));
                ux = len * fcos(angle) * 30, uy = len * fsin(angle) * 30;
                paint(&ux, &uy, snap.time * 2);
                i = (int)(sqrtf(ux * ux + uy * uy) * (255 / SW_FAR_BG) + 0.5f);
            } else {
                angle += (2.2f + 0.4f * quick) * len - 1 - speed * 0.05f - quick * speed * 0.02f + snap.vort_offset;
                ux = len * fcos(angle) * 30, uy = len * fsin(angle) * 30;
                paint(&ux, &uy, snap.time * 6 * snap.vort_speed + snap.vort_offset + 1033);
                i = (int)(sqrtf(ux * ux + uy * uy) * (255 / SW_FAR_SPLASH) + 0.5f);
            }
            out[y * SW_W + x] = i > 255 ? 255 : i;
        }
}
// The colour of each distance the paint can take a texel, as the shader in use makes it of its uniforms
static void swirl_pal(void) {
    for (int i = 0; i < 256; i++) {
        float c[4];
        if (shader == BACKGROUND) {
            float contrast = 0.25f * bg.contrast + 0.5f * bg.spin + 1.2f, amount = least(2, i * (SW_FAR_BG / 255) * 0.035f * contrast);
            float p1 = most(0, 1 - contrast * fabsf(1 - amount)), p2 = most(0, 1 - contrast * amount), p3 = 1 - least(1, p1 + p2);
            float base = 0.3f / bg.contrast;
            for (int n = 0; n < 4; n++)
                c[n] = base * bg.c1[n] + (1 - base) * (bg.c1[n] * p1 + bg.c2[n] * p2 + p3 * (n < 3 ? bg.c3[n] : bg.c1[3]));
        } else {
            float smoke = least(2, most(-2, 1.5f + i * (SW_FAR_SPLASH / 255) * 0.12f - 0.17f * least(10, bg.time * 1.2f - 4)));
            if (smoke < 0.2f) smoke = (smoke - 0.2f) * 0.6f + 0.2f;
            float p1 = most(0, 1 - 2 * fabsf(1 - smoke)), p2 = most(0, 1 - 2 * smoke), black = 1 - least(1, p1 + p2);
            float flash = most(bg.mid_flash * 0.8f, most(p1, p2) * 5 - 4.4f) + bg.mid_flash * most(p1, p2);
            static const float dark[3] = {0.6f * 79 / 255, 0.6f * 99 / 255, 0.6f * 103 / 255};
            for (int n = 0; n < 4; n++)
                c[n] = (bg.c1[n] * p1 + bg.c2[n] * p2 + black * (n < 3 ? dark[n] : bg.c1[3])) * (1 - flash) + flash;
        }
        for (int n = 0; n < 4; n++) sw_pal[i][n] = (unsigned char)(least(1, most(0, c[n])) * 255 + 0.5f);
    }
}
static void swirl_draw(int id, float alpha) {
#ifdef _EE
    gs_swirl(id, sw[id], SW_W, SW_H, sw_fresh >> id & 1, sw_pal, alpha);
    sw_fresh &= ~(1 << id);
#else
    Tex picture = {SW_W, SW_H, sw[id], sw_pal, NULL, NULL, 1};
    float p0[2] = {0, 0}, e1[2] = {target->w, 0}, e2[2] = {0, target->h};
    gfx_colour(1, 1, 1, alpha);
    raster(p0, e1, e2, 0, &picture, 0.5f, 0.5f, SW_W - 1, SW_H - 1);
#endif
}
// background.fs or splash.fs over the whole target
static void swirl(void) {
    float tw = target->w, sh = target->h, was[4];
    if (!sw_len[0]) {
        float diagonal = sqrtf(tw * tw + sh * sh);
        for (int y = 0; y < SW_H; y++)
            for (int x = 0; x < SW_W + SW_SHIFT; x++) {
                float ux = ((x - SW_SHIFT + 0.5f) * tw / SW_W - 0.5f * tw) / diagonal, uy = ((y + 0.5f) * sh / SW_H - 0.5f * sh) / diagonal;
                sw_angle[y * (SW_W + SW_SHIFT) + x] = (unsigned short)(int)(atan2f(uy, ux) * (65536 / 6.2831853f) + 65536);
                sw_len[y * (SW_W + SW_SHIFT) + x] = (unsigned short)(sqrtf(ux * ux + uy * uy) * 65535 + 0.5f);
            }
    }
    if (sw_shader != shader) { // another swirl: its first picture whole, in this frame
        sw_shader = shader, snap = bg, sw_a = sw_b = 0, sw_next = 1, sw_row = 0, sw_fresh |= 1;
        swirl_rows(sw[0], 0, SW_H);
    } else {
        int to = sw_row + SW_ROWS < SW_H ? sw_row + SW_ROWS : SW_H;
        swirl_rows(sw[sw_next], sw_row, to);
        sw_row = to;
    }
    if (sw_row == SW_H) sw_a = sw_b, sw_b = sw_next, sw_next = 3 - sw_a - sw_b, sw_fresh |= 1 << sw_b, sw_row = 0, snap = bg;
    swirl_pal();
    memcpy(was, colour, sizeof was);
    swirl_draw(sw_a, 1);
    if (sw_b != sw_a) swirl_draw(sw_b, (float)sw_row / SW_H);
    memcpy(colour, was, sizeof was);
}
// flash.fs (white, as opaque as its distance from the middle says) or a swirl over the whole target, whatever
// sprite carries it
static void screen_shader(void) {
    if (shader != FLASH) return swirl();
    static unsigned char rgba[BG_W * BG_H * 4];
    static float lens[BG_W * BG_H];
    float tw = target->w, sh = target->h, t = bg.time, flash_root = t > 2.5f ? sqrtf(t - 2.5f) : 0;
    if (!lens[0]) {
        float diagonal = sqrtf(tw * tw + sh * sh);
        for (int y = 0; y < BG_H; y++)
            for (int x = 0; x < BG_W; x++) {
                float ux = ((x + 0.5f) * tw / BG_W - 0.5f * tw) / diagonal, uy = ((y + 0.5f) * sh / BG_H - 0.5f * sh) / diagonal;
                lens[y * BG_W + x] = sqrtf(ux * ux + uy * uy);
            }
    }
    for (int i = 0; i < BG_W * BG_H; i++) {
        float len = lens[i];
        float a = bg.mid_flash * least(1, (t > 2.5f ? most(0, flash_root - 60 * len) : 0) + (t > 11 ? most(0, (t - 11) * (t - 11) - 5 * len) : 0));
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255, rgba[i * 4 + 3] = (unsigned char)(least(1, most(0, a)) * 255 + 0.5f);
    }
    Tex picture = {BG_W, BG_H, NULL, NULL, rgba};
    float p0[2] = {0, 0}, e1[2] = {tw, 0}, e2[2] = {0, sh}, was[4];
    memcpy(was, colour, sizeof was);
    gfx_colour(1, 1, 1, 1);
    linear = 1;
    raster(p0, e1, e2, 0, &picture, 0.5f, 0.5f, BG_W - 1, BG_H - 1);
    linear = 0;
    memcpy(colour, was, sizeof was);
}

#define FLAME_SIZE 32 // flame.fs is in blocks of 1/60 of its sprite; these are twice that, a quarter of the work
// flame.fs's uniforms, and the two flames (chips and mult), each made again every other frame
static struct { float time, amount, id, c1[4], c2[4]; } fl;
static struct { float id; int made; unsigned char rgba[FLAME_SIZE * FLAME_SIZE * 4]; } flames[2];
static float fmodp(float a, float b) { return a - b * floorf(a / b); } // GLSL's mod
// flame.fs on a FLAME_SIZE x FLAME_SIZE grid over the sprite; NULL for a flame that is out
static const unsigned char *flame(void) {
    float intensity = least(10, fl.amount), t = fl.time, id = fl.id;
    if (intensity < 0.1f) return NULL;
    int n = flames[0].id == id ? 0 : flames[1].id == id ? 1 : flames[0].made <= flames[1].made ? 0 : 1;
    if (flames[n].id == id && flames[n].made && frames % 2 != n) return flames[n].rgba; // not its turn
    flames[n].id = id, flames[n].made = frames ? frames : 1;
    float up = fmodp(4 * t, 10000) - 5000 + fmodp(1.781f * id, 1000), scale = 7.5f + 3 / (2 + 2 * intensity);
    float speed = fmodp(20.781f * id, 100) + fsin(t + id) * fcos(t * 0.151f + id);
    unsigned char *out = flames[n].rgba;
    for (int j = 0; j < FLAME_SIZE; j++)
        for (int i = 0; i < FLAME_SIZE; i++, out += 4) {
            float ux = (i + 0.5f) / FLAME_SIZE - 0.5f, uy = (j + 0.5f) / FLAME_SIZE - 0.5f, fx = ffloor(ux * 60) / 60, fy = ffloor(uy * 60) / 60;
            float wobble = 1 + 0.01f * fsin(-1.123f * fx + 0.2f * t) * fcos(5.3332f * fy + t * 0.931f), cx = fx * wobble, cy = fy * wobble;
            float sx = cx * scale, sy = cy * scale + up, vx = 0, vy = 0;
            for (int k = 0; k < 5; k++) {
                float len = sqrtf(sx * sx + sy * sy), add = 0.3f * (fcos(len * 0.411f) + 0.3344f * fsin(len) - 0.23f * fcos(len));
                float wx = vx + sx + 0.05f * vy + add, wy = vy + sy + 0.05f * vx + add; // (the shader's sign is always +)
                vx = wx, vy = wy;
                sx += 0.5f * fcos(fcos(vy) + speed * 0.0812f) * fsin(3.22f + vx - speed * 0.1531f);
                sy += 0.5f * fsin(-vx * 1.21222f + 0.113785f * speed) * fcos(vy * 0.91213f - 0.13582f * speed);
            }
            float dy = (sy - up) / scale * 5, dx = sx / scale * 5;
            float smoke = most(0, (sqrtf(dx * dx + dy * dy) + 0.1f * (sqrtf(cx * cx + cy * cy) - 0.5f)) * (2 / (2 + intensity * 0.2f)));
            smoke += most(0, 2 - 0.3f * intensity) * most(0, 2 * (cy - 0.5f) * (cy - 0.5f));
            if (fabsf(ux) > 0.4f) smoke += 10 * (fabsf(ux) - 0.4f);
            float core = sqrtf(ux * 0.19f * ux * 0.19f + (uy - 0.1f) * (uy - 0.1f));
            if (core < least(0.1f, intensity * 0.5f) && smoke > 1) smoke += least(8.5f, intensity * 10) * (core - 0.1f);
            float c[4] = {fl.c1[0], fl.c1[1], fl.c1[2], 1};
            if (smoke > 1) c[3] = 0;
            else if (uy < 0.12f)
                for (int m = 0; m < 3; m++) {
                    c[m] = c[m] * (1 - 0.5f * (0.12f - uy)) + 2.5f * (0.12f - uy) * fl.c2[m];
                    c[m] += c[m] * (-2 + 0.5f * intensity * smoke) * (0.12f - uy);
                }
            for (int m = 0; m < 4; m++) out[m] = (unsigned char)(least(1, most(0, c[m])) * 255 + 0.5f);
        }
    return flames[n].rgba;
}

static float uniform(lua_State *L, const char *name) { // of the table at 2, a number or a boolean
    lua_getfield(L, 2, name);
    float v = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}
static void uniform4(lua_State *L, const char *name, float out[4]) {
    lua_getfield(L, 2, name);
    for (int i = 0; i < 4; i++) {
        if (lua_istable(L, -1)) lua_rawgeti(L, -1, i + 1);
        else lua_pushnil(L);
        out[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}
// gfx.shader([name, uniforms]): love.graphics.setShader of the shader of that file, with what was sent to it
static int l_shader(lua_State *L) {
    const char *name = luaL_optstring(L, 1, "");
    shader = PLAIN;
    if (!strcmp(name, "background") || !strcmp(name, "splash") || !strcmp(name, "flash")) {
        shader = name[0] == 'b' ? BACKGROUND : name[0] == 's' ? SPLASH : FLASH;
        bg.time = uniform(L, "time"), bg.spin_time = uniform(L, "spin_time"), bg.contrast = uniform(L, "contrast");
        bg.spin = uniform(L, "spin_amount"), bg.vort_speed = uniform(L, "vort_speed"), bg.mid_flash = uniform(L, "mid_flash");
        bg.vort_offset = uniform(L, "vort_offset");
        uniform4(L, "colour_1", bg.c1);
        uniform4(L, "colour_2", bg.c2);
        uniform4(L, "colour_3", bg.c3);
    } else if (!strcmp(name, "flame")) {
        shader = FLAME;
        fl.time = uniform(L, "time"), fl.amount = uniform(L, "amount"), fl.id = uniform(L, "id");
        uniform4(L, "colour_1", fl.c1);
        uniform4(L, "colour_2", fl.c2);
    } else if (*name) {
        float burn1[4], burn2[4], own[4];
        uniform4(L, "burn_colour_1", burn1);
        uniform4(L, "burn_colour_2", burn2);
        uniform4(L, name, own);
        gfx_card_shader(name, uniform(L, "shadow"), uniform(L, "dissolve"), uniform(L, "time"), burn1, burn2, own[0], own[1]);
        uniform4(L, "mouse_screen_pos", own);
        gfx_tilt(uniform(L, "hovering"), own[0], own[1], uniform(L, "screen_scale"));
    }
    return 0;
}

// gfx.blit(id, u, v, uw, vh, w, h, x, y, r, sx, sy, ox, oy): love.graphics.draw of the texels (u, v, uw, vh) of a
// texture as a w x h rectangle
static void blit(int id, float u, float v, float uw, float vh, float w, float h, float x, float y, float r, float sx, float sy,
                 float ox, float oy) {
#ifdef _EE
    if (!tex[id].gs) return; // the canvas: on the GS it is the frame buffer, already on screen
#endif
    if (shader >= BACKGROUND) return screen_shader();
    float was[4];
    memcpy(was, colour, sizeof was);
    Tex picture = {FLAME_SIZE, FLAME_SIZE, NULL, NULL, NULL}; // what a flame draws in the place of its sprite
    if (shader == FLAME) {
        if (!(picture.rgba = (unsigned char *)flame())) return;
        gfx_colour(1, 1, 1, 1);
        u = v = 0, uw = vh = FLAME_SIZE;
    } else if (shader != PLAIN) {
        if (shader == DISSOLVE || shader == FOIL) gfx_colour(1, 1, 1, 1); // these two do not look at the colour
        cell_w = (int)(uw + 0.5f), cell_h = (int)(vh + 0.5f), field = NULL, atlas = id, atlas_w = tex[id].w, atlas_h = tex[id].h, atlas_u = (int)u, atlas_v = (int)v;
        if (dis.amount >= 0.001f && (field = field_for(dis.time, cell_w, cell_h))) {
            // ponytail: a dissolve goes in 64 steps, so the picture of a step is made once (gfx_gs.c keeps it)
            // and not every frame: the menu's logo is 72000 texels, 50 ms of the EE each time
            dis.amount = most(1, ffloor(dis.amount * 64 + 0.5f)) / 64;
            dis_adj = dis.amount * dis.amount * (3 - 2 * dis.amount) * 1.02f - 0.01f;
            dis_cos = fcos(dis_adj / 82.612f), dis_sin = fsin(dis_adj / 82.612f);
        }
        if (shader == HOLO || shader == POLYCHROME) field_from(dis.y * (shader == HOLO ? 7.221f : 2.221f) + dis.time, dis_o);
        if (shader != DISSOLVE) filter = card_texel;
        else if (field) {
            float d = dis.amount;
            const float *burn = d <= 0.01f ? NULL : dis.burn2[3] > 0.01f ? dis.burn2 : dis.burn1[3] > 0.01f ? dis.burn1 : NULL;
            for (int n = 0; n < 3; n++)
                for (int value = 0; value < 256; value++)
                    dis_tint[n][value] = burn ? (unsigned char)(least(1, most(0, value / 255.0f * (1 - 0.6f * d) + 0.6f * burn[n] * d)) * 255 + 0.5f) : value;
            for (int n = 0; n < 4; n++) {
                dis_burn[0][n] = (unsigned char)(least(1, most(0, dis.burn1[n])) * 255 + 0.5f);
                dis_burn[1][n] = (unsigned char)(least(1, most(0, dis.burn2[n])) * 255 + 0.5f);
            }
            filter = dissolve_texel;
        }
        else if (dis.shadow) gfx_colour(0, 0, 0, 0.3f); // all dissolve.fs does to a sprite that is whole
        if (filter) { // what the picture depends on: nothing that moves for debuff, played and negative
            float of[12] = {shader, 0, 0, 0, field ? dis.amount : 0, field ? dis.time : 0};
            if (shader == DEBUFF || shader == NEGATIVE) of[1] = dis.y != 0;
            else if (shader != PLAYED && shader != DISSOLVE) of[1] = dis.x, of[2] = dis.y, of[3] = shader >= HOLO ? dis.time : 0;
            for (int n = 0; field && n < 3; n++) of[6 + n] = dis.burn1[n] * dis.burn1[3], of[9 + n] = dis.burn2[n] * dis.burn2[3];
            for (int n = 0; n < 12; n++) {
                unsigned bits;
                memcpy(&bits, &of[n], 4);
                stamp = stamp * 31 + bits;
            }
            stamp |= 1, kind = shader + (dis.shadow ? 64 : 0); // (a card and its shadow are two pictures)
        }
    }
    Mat saved = M;
    gfx_translate(x, y);
    gfx_rotate(r);
    gfx_scale(sx, sy);
    float p[4][2];
    for (int i = 0; i < 4; i++) {
        transform(i & 1 ? w - ox : -ox, i & 2 ? h - oy : -oy, p[i]);
        if (shader >= DISSOLVE && shader <= HOLOGRAM && tilt.hovering > 0 && tilt.scale > 0) lean(p[i]);
    }
    float e1[2] = {p[1][0] - p[0][0], p[1][1] - p[0][1]}, e2[2] = {p[2][0] - p[0][0], p[2][1] - p[0][1]};
    bend[0] = p[3][0] - p[0][0] - e1[0] - e2[0], bend[1] = p[3][1] - p[0][1] - e1[1] - e2[1];
    raster(p[0], e1, e2, 0, picture.rgba ? &picture : &tex[id], u, v, uw, vh);
    bend[0] = bend[1] = 0;
    M = saved;
    filter = NULL, stamp = 0;
    memcpy(colour, was, sizeof was);
}
static int l_blit(lua_State *L) {
    int id = luaL_checkint(L, 1);
    luaL_argcheck(L, id > 0 && id < ntex, 1, "not a texture");
    float sx = luaL_optnumber(L, 11, 1);
    blit(id, luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5),
         luaL_checknumber(L, 6), luaL_checknumber(L, 7), luaL_optnumber(L, 8, 0), luaL_optnumber(L, 9, 0),
         luaL_optnumber(L, 10, 0), sx, luaL_optnumber(L, 12, sx), luaL_optnumber(L, 13, 0), luaL_optnumber(L, 14, 0));
    return 0;
}
// The one bitmap font (tools/font.py): boxes in its texture and offsets from the pen in texels, advances in the
// units of the size the metrics were taken at; font_unit of those units is one texel.
static struct { short have, x, y, w, h, ox, oy; float advance; } glyphs[1024];
static int font_tex;
static float font_unit;
static int l_font(lua_State *L) {
    font_tex = luaL_checkint(L, 1);
    font_unit = luaL_checknumber(L, 2);
    // Most text is 0.6 to 1.2 pixels a texel at 640x448: to the nearest texel, rows and columns of a letter drop
    // out or double. (The clear texels of the font are white too, so nothing dark bleeds into a letter's edge.)
    tex[font_tex].smooth = 1;
#ifdef _EE
    gs_smooth(tex[font_tex].gs);
#endif
    return 0;
}
// gfx.glyph(code point, x, y, w, h, x offset, y offset, advance)
static int l_glyph(lua_State *L) {
    int cp = luaL_checkint(L, 1);
    luaL_argcheck(L, cp >= 0 && cp < 1024, 1, "code point out of the table");
    glyphs[cp].have = 1;
    glyphs[cp].x = luaL_checkint(L, 2), glyphs[cp].y = luaL_checkint(L, 3);
    glyphs[cp].w = luaL_checkint(L, 4), glyphs[cp].h = luaL_checkint(L, 5);
    glyphs[cp].ox = luaL_checkint(L, 6), glyphs[cp].oy = luaL_checkint(L, 7);
    glyphs[cp].advance = luaL_checknumber(L, 8);
    return 0;
}
// The next code point of a UTF-8 string, or -1 at its end
static int next_code(const unsigned char **s, const unsigned char *end) {
    if (*s >= end) return -1;
    int c = *(*s)++, more = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    if (more) c &= 0x3F >> more;
    for (; more && *s < end; more--) c = c << 6 | (*(*s)++ & 0x3F);
    return c;
}
// gfx.width(text) -> the sum of its advances, and whether the font has every glyph ('?' stands in for the rest)
static int l_width(lua_State *L) {
    size_t len;
    const unsigned char *s = (const unsigned char *)luaL_checklstring(L, 1, &len), *end = s + len;
    float w = 0;
    int all = 1;
    for (int cp; (cp = next_code(&s, end)) >= 0;) {
        if (cp >= 1024 || !glyphs[cp].have) cp = '?', all = 0;
        w += glyphs[cp].advance;
    }
    lua_pushnumber(L, w);
    lua_pushboolean(L, all);
    return 2;
}
// A Text at k times the metrics' size, placed as love.graphics.draw places it
static void text(const unsigned char *s, size_t len, float k, float x, float y, float r, float sx, float sy, float ox, float oy) {
    const unsigned char *end = s + len;
    float unit = font_unit * k, pen = -ox;
    Mat saved = M;
    gfx_translate(x, y);
    gfx_rotate(r);
    gfx_scale(sx, sy);
    for (int cp; (cp = next_code(&s, end)) >= 0;) {
        if (cp >= 1024 || !glyphs[cp].have) cp = '?';
        if (glyphs[cp].w && glyphs[cp].h)
            rect(pen + glyphs[cp].ox * unit, glyphs[cp].oy * unit - oy, glyphs[cp].w * unit, glyphs[cp].h * unit, &tex[font_tex],
                 glyphs[cp].x, glyphs[cp].y, glyphs[cp].w, glyphs[cp].h);
        pen += glyphs[cp].advance * k;
    }
    M = saved;
}
// gfx.text(text, k, x, y, r, sx, sy, ox, oy): love.graphics.draw of a Text
static int l_text(lua_State *L) {
    size_t len;
    const unsigned char *s = (const unsigned char *)luaL_checklstring(L, 1, &len);
    float sx = luaL_optnumber(L, 6, 1);
    text(s, len, luaL_checknumber(L, 2), luaL_optnumber(L, 3, 0), luaL_optnumber(L, 4, 0), luaL_optnumber(L, 5, 0), sx,
         luaL_optnumber(L, 7, sx), luaL_optnumber(L, 8, 0), luaL_optnumber(L, 9, 0));
    return 0;
}
// gfx.info(table): boot.lua's table of what each LÖVE object is; a Text there is {text = string, k = size factor}
static int info_ref;
static int l_info(lua_State *L) {
    lua_settop(L, 1);
    info_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
// love.graphics.draw(text, x, y, r, sx, sy, ox, oy) of the Text object on top of the stack (it stays there)
// t[name] of one of info's tables (t: a negative index). The name's Lua string is made once, the literal's
// address its key: lua_getfield hashes the name at every call, and a frame has hundreds.
static void info_field(lua_State *L, int t, const char *name) {
    static struct { const char *name; int ref; } known[8];
    int i;
    for (i = 0; i < 8 && known[i].name && known[i].name != name; i++) {}
    if (i == 8) return lua_getfield(L, t, name);
    if (!known[i].name) {
        lua_pushstring(L, name);
        known[i].ref = luaL_ref(L, LUA_REGISTRYINDEX), known[i].name = name;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, known[i].ref);
    lua_rawget(L, t - 1);
}
void gfx_draw_text(lua_State *L, float x, float y, float r, float sx, float sy, float ox, float oy) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, info_ref);
    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    info_field(L, -1, "text");
    info_field(L, -2, "k");
    size_t len;
    const unsigned char *s = (const unsigned char *)luaL_checklstring(L, -2, &len);
    text(s, len, luaL_checknumber(L, -1), x, y, r, sx, sy, ox, oy);
    lua_pop(L, 4);
}
// love.graphics.draw(image, quad, x, y, r, sx, sy) of the Image and the Quad on top of the stack (they stay
// there), as boot.lua's draw does it
void gfx_draw_quad(lua_State *L, float x, float y, float r, float sx, float sy) {
    float t[2], q[4];
    lua_rawgeti(L, LUA_REGISTRYINDEX, info_ref);
    lua_pushvalue(L, -3);
    lua_rawget(L, -2); // the image: {id, w, h, k}
    info_field(L, -1, "id");
    info_field(L, -2, "k");
    int id = luaL_checkint(L, -2);
    t[0] = luaL_checknumber(L, -1);
    lua_pop(L, 3);
    lua_pushvalue(L, -2);
    lua_rawget(L, -2); // the quad: {x, y, w, h}
    for (int i = 0; i < 4; i++) {
        lua_rawgeti(L, -1, i + 1);
        q[i] = luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    if (id <= 0 || id >= ntex) luaL_error(L, "gfx: not a texture");
    blit(id, q[0] * t[0], q[1] * t[0], q[2] * t[0], q[3] * t[0], q[2], q[3], x, y, r, sx, sy, 0, 0);
}

// gfx.shot(file): the screen as a PPM
static int l_shot(lua_State *L) {
    FILE *f = fopen(luaL_checkstring(L, 1), "wb");
    if (!f || !soft) luaL_error(L, "gfx: cannot write the screenshot");
    fprintf(f, "P6 %d %d 255\n", tex->w, tex->h);
    for (int i = 0; i < tex->w * tex->h; i++) fwrite(tex->rgba + i * 4, 1, 3, f);
    fclose(f);
    return 0;
}

// gfx.init(width, height, soft) of the screen
static int l_init(lua_State *L) {
    tex->w = luaL_checkint(L, 1), tex->h = luaL_checkint(L, 2);
    soft = lua_toboolean(L, 3);
    if (soft) tex->rgba = calloc(tex->w * tex->h, 4);
    for (int i = 0; i < 1024; i++) sine[i] = sinf(i * 6.2831853f / 1024);
#ifdef _EE
    gs_init();
#endif
    return l_origin(L);
}
// gfx.count() -> primitives drawn since the last call
static int l_count(lua_State *L) {
    lua_pushinteger(L, primitives);
    primitives = 0;
    return 1;
}
// gfx.present(): the frame is done
static int l_present(lua_State *L) {
#ifdef _EE
    gs_present();
#endif
    if (++frames % 64 == 0) // the fields of cards that dissolved long ago
        for (Field *f = fields; f < fields + 12; f++)
            if (f->cs && frames - f->used > 600) free(f->cs), f->cs = NULL;
    return 0;
}

int luaopen_gfx(lua_State *L) {
    static const luaL_Reg gfx[] = {
        {"init", l_init}, {"push", gfx_push}, {"pop", gfx_pop}, {"origin", l_origin}, {"translate", l_translate},
        {"scale", l_scale}, {"rotate", l_rotate}, {"setColor", l_setColor}, {"setLineWidth", l_setLineWidth},
        {"clear", l_clear}, {"rectangle", l_rectangle}, {"polygon", l_polygon}, {"texture", l_texture},
        {"canvas", l_canvas}, {"target", l_target}, {"blit", l_blit}, {"shot", l_shot}, {"info", l_info}, {"present", l_present}, {"count", l_count}, {"font", l_font}, {"glyph", l_glyph},
        {"width", l_width}, {"text", l_text}, {"shader", l_shader}, {NULL, NULL}};
    luaL_register(L, "gfx", gfx);
    return 1;
}
