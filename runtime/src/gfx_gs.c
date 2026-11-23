// What gfx.c draws, on the PS2's GS through gsKit: 640x448 interlaced, two buffers, no Z, alpha
// blending. An atlas is cut into pages of up to 256x256 PSMT8 texels sharing its palette, so the texture manager
// keeps in VRAM only the pages in use (design D5); a rectangle of texels that spans pages is drawn in pieces.
// Main memory holds only the pages drawn lately: a row of pages is read from the atlas's file when one of them is
// first drawn and freed after KEEP frames unused. All of them at once were 7.65 MB of the 32 (docs/perf.md), 2.8
// of them the two logos of the splash.
// ponytail: so the first frame that shows an atlas waits for the disc. Read ahead of the screens that need it if
// that shows on the console.
// The game's one canvas is the frame buffer itself (design D6).
//
// Nothing here draws when the game does: a frame of a run is 50 ms and more of the EE, three vertical
// blanks or four. gfx.c's primitives are written down, a list per frame, and a thread above the game's presents
// at every vertical blank the mix of the last two finished lists for that moment: each primitive of the newer one
// at its place between where the older one had it and its own. So the screen moves 60 times a second and shows
// the game a frame of its own behind. A primitive with no pair in the older list (something that appeared)
// is drawn as it is.
// Only that thread calls gsKit, which is not made for two: the game's side leaves it what to free and what
// changed in two short lists. A texture made for a frame (and a kept picture given up) is freed when its frame's
// list is used again, three frames later, when no list on screen can name it.
#ifdef _EE
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dmaKit.h>
#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#include "gfx_gs.h"
#include "pack.h"

#define PAGE 256
#define KEEP 600 // frames a row of pages stays in main memory after it was last drawn
struct GsTex {
    int w, h, cols, rows;
    char *path;
    int *used; // per row of pages: the frame it was last drawn in
    GSTEXTURE pages[];
};
static GSGLOBAL *gs;
static struct GsTex *all[256];
static int count, frame;

void gs_init(void) {
    gs = gsKit_init_global();
    gs->Mode = GS_MODE_NTSC;
    gs->Interlace = GS_INTERLACED;
    gs->Field = GS_FIELD;
    gs->Width = 640;
    gs->Height = 448;
    gs->PSM = GS_PSM_CT24;
    gs->ZBuffering = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_TexManager_init(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0); // (source - destination) * alpha + destination
    gsKit_set_clamp(gs, GS_CMODE_CLAMP);
}

struct GsTex *gs_texture(int w, int h, const char *path, const unsigned char pal[256][4]) {
    int cols = (w + PAGE - 1) / PAGE, rows = (h + PAGE - 1) / PAGE;
    struct GsTex *t = calloc(1, sizeof *t + cols * rows * sizeof(GSTEXTURE));
    u32 *clut = memalign(128, 1024);
    if (!t || !clut || count == 256) return NULL;
    t->path = strdup(path), t->used = calloc(rows, sizeof *t->used);
    if (!t->path || !t->used) return NULL;
    all[count++] = t;
    for (int i = 0; i < 256; i++) { // the GS reads a 256-colour CLUT with the two middle bits of each 32 swapped
        int j = (i & 0x18) == 8 ? i + 8 : (i & 0x18) == 16 ? i - 8 : i;
        clut[j] = pal[i][0] | pal[i][1] << 8 | pal[i][2] << 16 | (u32)((pal[i][3] + 1) / 2) << 24; // alpha 0x80 is opaque
    }
    t->w = w, t->h = h, t->cols = cols, t->rows = rows;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            GSTEXTURE *p = &t->pages[r * cols + c];
            p->Width = w - c * PAGE < PAGE ? w - c * PAGE : PAGE;
            p->Height = h - r * PAGE < PAGE ? h - r * PAGE : PAGE;
            p->PSM = GS_PSM_T8;
            p->ClutPSM = GS_PSM_CT32;
            p->Clut = clut;
            p->Filter = GS_FILTER_NEAREST;
        }
    return t;
}

// The texels of row r of pages, from the file (after its 4 bytes of size and its palette). 0: no file or no memory.
static int load_row(struct GsTex *t, int r) {
    GSTEXTURE *row = &t->pages[r * t->cols];
    int height = row->Height, ok = 0;
    u8 *texels = malloc(t->w * height);
    if (texels && pack_read(t->path, 4 + 1024 + r * PAGE * t->w, texels, t->w * height)) {
        ok = 1;
        for (int c = 0; c < t->cols; c++)
            if (!(row[c].Mem = memalign(128, gsKit_texture_size_ee(row[c].Width, height, GS_PSM_T8)))) ok = 0;
        for (int c = 0; ok && c < t->cols; c++)
            for (int y = 0; y < height; y++) memcpy((u8 *)row[c].Mem + y * row[c].Width, texels + y * t->w + c * PAGE, row[c].Width);
        for (int c = 0; !ok && c < t->cols; c++) free(row[c].Mem), row[c].Mem = NULL;
    }
    free(texels);
    return ok;
}
// ---------------------------------------------------------------- the frames' lists
typedef struct {
    GSTEXTURE *tex;       // NULL: flat
    float xy[4][2];       // (a triangle's third corner twice)
    float u0, v0, u1, v1; // texels of the first corner and of the last
    unsigned char c[4];   // as the GS takes them: 0x80 is 1 for a texture's and for alpha
} Prim;
#define RETIRE 128
typedef struct {
    Prim *prims;
    short *pair; // per primitive, the one of the frame before it moves from, or -1
    int n, room, paired, cleared, nretire;
    u32 clear, at, span; // the vertical blank it was finished at, and how many it took
    GSTEXTURE *retire[RETIRE];
} Frame;
static Frame frames[3], *older, *newer, *building = &frames[0];
static volatile u32 ticks; // vertical blanks
static int lock, wake;     // the lists on screen are in use / a vertical blank

static Prim *prim(void) {
    Frame *f = building;
    if (f->n == f->room) {
        int room = f->room ? f->room * 2 : 2048;
        Prim *prims = realloc(f->prims, room * sizeof *prims);
        if (prims) f->prims = prims;
        short *pair = realloc(f->pair, room * sizeof *pair);
        if (pair) f->pair = pair;
        if (!prims || !pair || room > 32767) return NULL;
        f->room = room;
    }
    return &f->prims[f->n++];
}

// For the presenting thread: textures to forget (and memory to free), textures whose texels changed
static struct { GSTEXTURE *tex; void *mem; int own; } gone[512];
static GSTEXTURE *changed[128];
static volatile int ngone, nchanged;
static void later(GSTEXTURE *tex, void *mem, int own) { // (own: the GSTEXTURE itself is freed too)
    DI();
    if (ngone < 512) gone[ngone].tex = tex, gone[ngone].mem = mem, gone[ngone].own = own, ngone++; // ponytail: or it leaks
    EI();
}
static void touched(GSTEXTURE *tex) {
    DI();
    if (nchanged < 128) changed[nchanged++] = tex;
    EI();
}

// What a card shader makes of a sprite is kept from frame to frame: a debuffed or a negative card is the same
// picture every frame, and the shines (foil, holo, polychrome, voucher, booster) move slowly while a picture of
// one is milliseconds of the EE (docs/perf.md). So of the kept pictures whose stamp changed, one in `period` is
// made again every EVERY frames, each in its turn, and the rest are drawn as they were: 15 to 20 ms of every
// frame went there with one picture a frame.
// A second card with the same sprite and shader in a frame gets a picture of its own made every frame.
#define KEPT 48
#define EVERY 3
static struct Kept {
    GSTEXTURE *tex;
    GSTEXTURE *page;
    int x0, y0, x1, y1, kind, used; // used: the frame it was last drawn in
    unsigned stamp;
} kept[KEPT];
static int stale, period = 1; // kept pictures drawn this frame with another stamp than theirs
// A 32-bit texture made on the EE (what a recipe draws, gfx.c). NULL: no memory.
static GSTEXTURE *texture(int w, int h, int filter) {
    GSTEXTURE *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->Width = w, t->Height = h, t->PSM = GS_PSM_CT32, t->Filter = filter;
    if (!(t->Mem = memalign(128, gsKit_texture_size_ee(w, h, GS_PSM_CT32)))) return free(t), NULL;
    return t;
}
// t is freed when the list being written is used again
static GSTEXTURE *retire(GSTEXTURE *t) {
    if (!t) return NULL;
    if (building->nretire == RETIRE) return free(t->Mem), free(t), NULL;
    return building->retire[building->nretire++] = t;
}
static void drop(struct Kept *k) {
    retire(k->tex);
    k->tex = NULL;
}
// Rows of pages nobody drew for KEEP frames leave main memory (and VRAM).
static void forget(void) {
    for (int i = 0; i < count; i++)
        for (int r = 0; r < all[i]->rows; r++) {
            GSTEXTURE *row = &all[i]->pages[r * all[i]->cols];
            if (!row->Mem || frame - all[i]->used[r] < KEEP) continue;
            for (int c = 0; c < all[i]->cols; c++) {
                for (struct Kept *k = kept; k < kept + KEPT; k++)
                    if (k->page == &row[c]) drop(k); // made of these texels
                later(&row[c], row[c].Mem, 0);
                row[c].Mem = NULL;
            }
        }
    for (struct Kept *k = kept; k < kept + KEPT; k++)
        if (frame - k->used > 120) drop(k);
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
// A texture of the frame being drawn alone
static GSTEXTURE *temp(int w, int h, int filter) { return retire(texture(w, h, filter)); }
static void written(GSTEXTURE *t) { SyncDCache(t->Mem, (u8 *)t->Mem + t->Width * t->Height * 4); }
#define PACK(rgba) ((rgba)[0] | (rgba)[1] << 8 | (rgba)[2] << 16 | (u32)(((rgba)[3] + 1) / 2) << 24) // alpha 0x80 is opaque

void gs_raster_rgba(const float p0[2], const float e1[2], const float e2[2], const unsigned char *rgba, int w, int h, float u,
                    float v, float du, float dv, int linear) {
    GSTEXTURE *t = temp(w, h, linear ? GS_FILTER_LINEAR : GS_FILTER_NEAREST);
    Prim *q = t ? prim() : NULL;
    if (!q) return;
    for (int i = 0; i < w * h; i++) t->Mem[i] = PACK(rgba + i * 4);
    written(t);
    q->tex = t, q->u0 = u, q->v0 = v, q->u1 = u + du, q->v1 = v + dv;
    for (int i = 0; i < 4; i++)
        q->xy[i][0] = p0[0] + (i & 1 ? e1[0] : 0) + (i & 2 ? e2[0] : 0), q->xy[i][1] = p0[1] + (i & 1 ? e1[1] : 0) + (i & 2 ? e2[1] : 0);
    q->c[0] = q->c[1] = q->c[2] = q->c[3] = 0x80;
}

// The texel (i, j) of the sprite a recipe is working on, for the recipe
// ponytail: only from the page being drawn, its edge texels standing in for what lies beyond (hologram's glow
// reads a few texels around each one; a sprite across two pages would show a seam in it).
static GSTEXTURE *fetch_page;
static int fetch_x, fetch_y; // of the sprite's corner in the page
static void unpack(u32 texel, unsigned char *rgba) {
    rgba[0] = texel, rgba[1] = texel >> 8, rgba[2] = texel >> 16, rgba[3] = (texel >> 24) * 2 > 255 ? 255 : (texel >> 24) * 2;
}
static void fetch(int i, int j, unsigned char *rgba) {
    int x = fetch_x + i, y = fetch_y + j;
    x = x < 0 ? 0 : x >= fetch_page->Width ? fetch_page->Width - 1 : x, y = y < 0 ? 0 : y >= fetch_page->Height ? fetch_page->Height - 1 : y;
    int n = ((u8 *)fetch_page->Mem)[y * fetch_page->Width + x];
    unpack(fetch_page->Clut[(n & 0x18) == 8 ? n + 8 : (n & 0x18) == 16 ? n - 8 : n], rgba);
}

void gs_raster(const float p0[2], const float e1[2], const float e2[2], const float bend[2], int tri, struct GsTex *t, float u, float v, float du,
               float dv, const float c[4], Filter *filter, int kind, unsigned stamp) {
    if (!t) {
        Prim *q = prim();
        if (!q) return;
        q->tex = NULL;
        for (int i = 0; i < 4; i++) {
            int n = tri && i == 3 ? 2 : i;
            q->xy[i][0] = p0[0] + (n & 1 ? e1[0] : 0) + (n & 2 ? e2[0] : 0), q->xy[i][1] = p0[1] + (n & 1 ? e1[1] : 0) + (n & 2 ? e2[1] : 0);
        }
        q->c[0] = (int)(c[0] * 255), q->c[1] = (int)(c[1] * 255), q->c[2] = (int)(c[2] * 255), q->c[3] = (int)(c[3] * 128);
        return;
    }
    if (du <= 0 || dv <= 0) return;
    float u1 = clampf(u + du, 0, t->w), v1 = clampf(v + dv, 0, t->h), u0 = clampf(u, 0, t->w), v0 = clampf(v, 0, t->h);
    for (int r = (int)v0 / PAGE; r * PAGE < v1; r++) {
        if (!t->pages[r * t->cols].Mem && !load_row(t, r)) continue; // not drawn this frame
        t->used[r] = frame;
        for (int col = (int)u0 / PAGE; col * PAGE < u1; col++) {
            GSTEXTURE *p = &t->pages[r * t->cols + col];
            float ua = clampf(u0, col * PAGE, col * PAGE + p->Width), ub = clampf(u1, col * PAGE, col * PAGE + p->Width);
            float va = clampf(v0, r * PAGE, r * PAGE + p->Height), vb = clampf(v1, r * PAGE, r * PAGE + p->Height);
            float sa = (ua - u) / du, sb = (ub - u) / du, ra = (va - v) / dv, rb = (vb - v) / dv;
            if (sb <= sa || rb <= ra) continue;
            int x0 = col * PAGE, y0 = r * PAGE; // the texel of the atlas at the corner of the texture drawn
            GSTEXTURE *f = NULL;
            if (filter) { // the texels of this piece, each through the recipe; not this frame if there is no room
                int x1 = (int)ceilf(ub), y1 = (int)ceilf(vb), make = 1;
                x0 = (int)ua, y0 = (int)va;
                struct Kept *k = NULL, *oldest = kept;
                for (struct Kept *e = kept; stamp && !k && e < kept + KEPT; e++) {
                    if (e->tex && e->page == p && e->kind == kind && e->x0 == x0 && e->y0 == y0 && e->x1 == x1 && e->y1 == y1) k = e;
                    if (e->used < oldest->used) oldest = e;
                }
                int twice = k && k->used == frame && k->stamp != stamp; // two of the sprite in a frame, each its own picture
                if (twice) {
                    k = NULL; // this one through a texture of the frame
                } else if (k) { // again only when its turn comes
                    if (k->stamp != stamp) stale++;
                    make = k->stamp != stamp && frame % (period * EVERY) == (k - kept) % period * EVERY;
                } else if (stamp && oldest->used != frame) { // in the place of the one drawn longest ago
                    drop(k = oldest);
                    memset(k, 0, sizeof *k);
                    k->page = p, k->kind = kind, k->x0 = x0, k->y0 = y0, k->x1 = x1, k->y1 = y1;
                    if (!(k->tex = texture(x1 - x0, y1 - y0, GS_FILTER_NEAREST))) k = NULL;
                }
                if (k) f = k->tex, k->used = frame;
                else if (!(f = temp(x1 - x0, y1 - y0, GS_FILTER_NEAREST))) continue;
                if (k && make) k->stamp = stamp;
                u32 *out = f->Mem;
                fetch_page = p, fetch_x = (int)u - col * PAGE, fetch_y = (int)v - r * PAGE;
                for (int y = y0; make && y < y1; y++)
                    for (int x = x0; x < x1; x++) {
                        unsigned char rgba[4];
                        fetch(x - (int)u, y - (int)v, rgba);
                        filter(x - (int)u, y - (int)v, rgba, fetch);
                        *out++ = PACK(rgba);
                    }
                if (make) written(f), touched(f); // (the lists on screen that show it get the new picture early)
            }
            Prim *q = prim();
            if (!q) return;
            q->tex = f ? f : p, q->u0 = ua - x0, q->v0 = va - y0, q->u1 = ub - x0, q->v1 = vb - y0;
            for (int i = 0; i < 4; i++) {
                float s = i & 1 ? sb : sa, w = i & 2 ? rb : ra;
                q->xy[i][0] = p0[0] + e1[0] * s + e2[0] * w + bend[0] * s * w, q->xy[i][1] = p0[1] + e1[1] * s + e2[1] * w + bend[1] * s * w;
            }
            q->c[0] = (int)(c[0] * 128), q->c[1] = (int)(c[1] * 128), q->c[2] = (int)(c[2] * 128), q->c[3] = (int)(c[3] * 128);
        }
    }
}

void gs_smooth(struct GsTex *t) {
    for (int i = 0; i < t->cols * t->rows; i++) t->pages[i].Filter = GS_FILTER_LINEAR;
}

void gs_swirl(int id, const unsigned char *idx, int w, int h, int fresh, const unsigned char pal[256][4], float alpha) {
    static GSTEXTURE swirls[3];
    GSTEXTURE *t = &swirls[id];
    if (!t->Mem) {
        t->Width = w, t->Height = h, t->PSM = GS_PSM_T8, t->ClutPSM = GS_PSM_CT32, t->Filter = GS_FILTER_LINEAR;
        t->Clut = memalign(128, 1024);
        if (!t->Clut || !(t->Mem = memalign(128, gsKit_texture_size_ee(w, h, GS_PSM_T8)))) return;
        fresh = 1;
    }
    u32 clut[256];
    for (int i = 0; i < 256; i++) // (the order of a CLUT: gs_texture)
        clut[(i & 0x18) == 8 ? i + 8 : (i & 0x18) == 16 ? i - 8 : i] = PACK(pal[i]);
    if (fresh || memcmp(clut, t->Clut, sizeof clut)) {
        if (fresh) memcpy(t->Mem, idx, w * h);
        memcpy(t->Clut, clut, sizeof clut);
        SyncDCache(t->Mem, (u8 *)t->Mem + w * h);
        SyncDCache(t->Clut, (u8 *)t->Clut + sizeof clut);
        touched(t);
    }
    Prim *q = prim();
    if (!q) return;
    q->tex = t, q->u0 = 0.5f, q->v0 = 0.5f, q->u1 = w - 0.5f, q->v1 = h - 0.5f;
    for (int i = 0; i < 4; i++) q->xy[i][0] = i & 1 ? gs->Width : 0, q->xy[i][1] = i & 2 ? gs->Height : 0;
    q->c[0] = q->c[1] = q->c[2] = 0x80, q->c[3] = (int)(alpha * 128);
}

void gs_clear(const float c[4]) {
    building->n = 0; // (what was drawn before it is under it)
    building->cleared = 1, building->clear = GS_SETREG_RGBAQ((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), 0x80, 0);
}

// ---------------------------------------------------------------- presenting
// For each primitive of b, the one of a it is the next state of: the same texels (or both flat and near each
// other), in the same order. What a has in between is something that went away; what b has in between, new.
static int alike(const Prim *p, const Prim *q) {
    if (p->tex != q->tex) return 0;
    if (p->tex) return p->u0 == q->u0 && p->v0 == q->v0 && p->u1 == q->u1 && p->v1 == q->v1;
    for (int i = 0; i < 4; i++) // two flat ones have nothing else to tell them apart
        if (fabsf(p->xy[i][0] - q->xy[i][0]) > 48 || fabsf(p->xy[i][1] - q->xy[i][1]) > 48) return 0;
    return 1;
}
static void pairs(const Frame *a, Frame *b) {
    int j = 0;
    for (int i = 0; i < b->n; i++) {
        b->pair[i] = -1;
        for (int ahead = 0; a && ahead < 16 && j + ahead < a->n; ahead++)
            if (alike(&a->prims[j + ahead], &b->prims[i])) {
                b->pair[i] = j + ahead, j += ahead + 1;
                break;
            }
    }
}

// The newer list as one GIF packet (pairs of a register's value and its number), written once per frame of the
// game. A picture then costs the EE little: the corners and the colour of the primitives that move are written
// again, each texture's place in VRAM too, and the packet is sent as it is. (Through gsKit's own primitives a
// picture was 3.3 ms of the EE, a fifth of all of it at 60 a second.)
// The packet is in runs, each with a tag of its own: a texture's registers and the primitives that use it. If VRAM
// cannot hold every texture of the frame at once (the texture manager then gives one's place to another), the
// runs are sent one by one, each after its texture is back: slow, and right.
// ponytail: MOST primitives and WORDS of packet; a frame with more is drawn without its last ones.
static volatile u32 crowdings; // pictures sent run by run
#define MOST 3072
#define WORDS 65536
static u64 packet[WORDS] __attribute__((aligned(64)));
static int words, at[MOST], nruns, nmoving, nused; // at: where a primitive's colour is in the packet
static short moving[MOST];
static struct { int start, tex; } runs[MOST + 1]; // the run's tag in the packet, and its texture in used (-1: none)
static GSTEXTURE *used[256];                       // the frame's textures
static int log2up(int v) {
    int n = 0;
    while (1 << n < v) n++;
    return n;
}
static u64 corner(float x, float y) { return GS_SETREG_XYZ2(gsKit_float_to_int_x(gs, x), gsKit_float_to_int_y(gs, y), 0); }
static u64 tex0(const GSTEXTURE *x) {
    return GS_SETREG_TEX0(x->Vram / 256, x->TBW, x->PSM, log2up(x->Width), log2up(x->Height), 1, 0, x->VramClut / 256, x->ClutPSM, 0, 0, 1);
}
// Worth placing in between: another colour, or a corner half a pixel away (the whole room sways a little every
// frame, so nearly nothing is where it was to the last bit)
static int moved(const Prim *p, const Prim *q) {
    if (memcmp(p->c, q->c, 4)) return 1;
    for (int n = 0; n < 4; n++)
        if (fabsf(p->xy[n][0] - q->xy[n][0]) > 0.5f || fabsf(p->xy[n][1] - q->xy[n][1]) > 0.5f) return 1;
    return 0;
}
#define PUT(data, reg) (*w++ = (data), *w++ = (reg), items++)
#define TAG(tag, items) ((tag)[0] = (u64)(items) | 1ULL << 15 | 1ULL << 60, (tag)[1] = GIF_AD) // NLOOP, EOP, one register: address + data
static void build(const Frame *a, const Frame *b) {
    u64 *w = packet + 2, *tag = packet;
    int items = 0, bound = -1;
    nruns = 1, nmoving = nused = 0;
    runs[0].start = 0, runs[0].tex = -1;
    PUT(gs->PrimAlpha, GS_ALPHA_1);
    PUT(gs->PABE, GS_PABE);
    for (int i = 0; i < b->n && i < MOST && w + 32 <= packet + WORDS; i++) {
        const Prim *q = &b->prims[i];
        int textured = q->tex != NULL;
        if ((textured && (bound < 0 || q->tex != used[bound])) || items > 32000) { // (a tag counts to 32767)
            if (textured && (bound < 0 || q->tex != used[bound])) {
                for (bound = 0; bound < nused && used[bound] != q->tex; bound++) {}
                if (bound == 256) break;
                if (bound == nused) used[nused++] = q->tex;
            }
            TAG(tag, items);
            tag = w, w += 2, items = 0;
            runs[nruns].start = tag - packet, runs[nruns++].tex = bound;
            if (bound >= 0) {
                PUT(0, GS_TEX0_1);
                PUT(GS_SETREG_TEX1(1, 0, used[bound]->Filter, used[bound]->Filter, 0, 0, 0), GS_TEX1_1);
            }
        }
        PUT(GS_SETREG_PRIM(GS_PRIM_PRIM_TRISTRIP, 0, textured, 0, 1, 0, textured, 0, 0), GS_PRIM);
        at[i] = w - packet;
        PUT(GS_SETREG_RGBAQ(q->c[0], q->c[1], q->c[2], q->c[3], 0), GS_RGBAQ);
        for (int n = 0; n < 4; n++) {
            if (textured)
                PUT((u64)gsKit_float_to_int_u(q->tex, n & 1 ? q->u1 : q->u0) | (u64)gsKit_float_to_int_v(q->tex, n & 2 ? q->v1 : q->v0) << 16, GS_UV);
            PUT(corner(q->xy[n][0], q->xy[n][1]), GS_XYZ2);
        }
        if (a && b->pair[i] >= 0 && moved(&a->prims[b->pair[i]], q)) moving[nmoving++] = i;
    }
    TAG(tag, items);
    words = w - packet;
    runs[nruns].start = words;
}
// Whether two of the frame's textures have the same VRAM (or one has none)
static int crowded(void) {
    static u32 from[512], to[512];
    int n = 0;
    for (int i = 0; i < nused; i++) {
        const GSTEXTURE *x = used[i];
        if (x->Vram == GSKIT_ALLOC_ERROR) return 1;
        from[n] = x->Vram, to[n++] = x->Vram + gsKit_texture_size(x->Width, x->Height, x->PSM);
        if (x->Clut) {
            if (x->VramClut == GSKIT_ALLOC_ERROR) return 1;
            from[n] = x->VramClut, to[n++] = x->VramClut + gsKit_texture_size(16, 16, x->ClutPSM);
        }
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (from[i] < to[j] && from[j] < to[i]) return 1;
    return 0;
}
// The frame's textures into VRAM; whether they all fit there
static int fit(void) {
    for (int i = 0; i < nused; i++) gsKit_TexManager_bind(gs, used[i]);
    return !crowded();
}
// A picture: the packet to the GS, whole or run by run
static void send(int whole) {
    for (int i = 0; whole && i < nruns; i++)
        if (runs[i].tex >= 0) packet[runs[i].start + 2] = tex0(used[runs[i].tex]);
    gsKit_queue_exec(gs); // the textures that were not in VRAM, the buffer to draw in
    dmaKit_wait(DMA_CHANNEL_GIF, 0); // (the game's side may write the textures' memory again)
    SyncDCache(packet, packet + words);
    if (whole) return dmaKit_send(DMA_CHANNEL_GIF, packet, words / 2);
    for (int i = 0; i < nruns; i++) {
        if (runs[i].tex >= 0) {
            gsKit_TexManager_bind(gs, used[runs[i].tex]);
            gsKit_queue_exec(gs);
            dmaKit_wait(DMA_CHANNEL_GIF, 0);
            packet[runs[i].start + 2] = tex0(used[runs[i].tex]);
            SyncDCache(packet + runs[i].start, packet + runs[i].start + 4);
        }
        dmaKit_send(DMA_CHANNEL_GIF, packet + runs[i].start, (runs[i + 1].start - runs[i].start) / 2);
        dmaKit_wait(DMA_CHANNEL_GIF, 0);
    }
}
// The primitives that move, where they are a part t of the way from the older list to the newer
static void place(const Frame *a, const Frame *b, float t) {
    for (int m = 0; m < nmoving; m++) {
        const Prim *q = &b->prims[moving[m]], *p = &a->prims[b->pair[moving[m]]];
        u64 *w = packet + at[moving[m]];
        int c[4], step = q->tex ? 4 : 2;
        for (int n = 0; n < 4; n++) {
            c[n] = p->c[n] + (int)((q->c[n] - p->c[n]) * t);
            w[step + n * step] = corner(p->xy[n][0] + (q->xy[n][0] - p->xy[n][0]) * t, p->xy[n][1] + (q->xy[n][1] - p->xy[n][1]) * t);
        }
        w[0] = GS_SETREG_RGBAQ(c[0], c[1], c[2], c[3], 0);
    }
}

// The finished frame is shown by the next vertical blank's interrupt, which then wakes the presenter for the one
// after it.
static volatile int pending;
static u32 finished;
static volatile u32 pictures, busy; // presented, and the EE's count register (147.456 MHz) spent presenting them
static int vblank(int cause) {
    ticks++;
    if (pending) {
        GS_SET_DISPFB2(finished / 8192, gs->Width / 64, gs->PSM, 0, 0);
        pending = 0;
    }
    iSignalSema(wake);
    ExitHandler();
    return 0;
}
static void presenter(void *arg) {
    static struct { GSTEXTURE *tex; void *mem; int own; } went[512];
    static GSTEXTURE *dirty[128];
    for (;;) {
        WaitSema(wake);
        WaitSema(lock);
        DI();
        int nwent = ngone, ndirty = nchanged;
        memcpy(went, gone, nwent * sizeof *went);
        memcpy(dirty, changed, ndirty * sizeof *dirty);
        ngone = nchanged = 0;
        EI();
        for (int i = 0; i < ndirty; i++) gsKit_TexManager_invalidate(gs, dirty[i]); // (before any of them is freed)
        for (int i = 0; i < nwent; i++) {
            gsKit_TexManager_free(gs, went[i].tex);
            free(went[i].mem);
            if (went[i].own) free(went[i].tex);
        }
        Frame *a = older, *b = newer;
        u32 from = GetCop0(9);
        if (b) {
            static float placed;
            static int settled; // the newer list is on screen as it is: nothing to do until the next one
            dmaKit_wait(DMA_CHANNEL_GIF, 0); // the picture before this one
            if (!b->paired) pairs(a, b), build(a, b), b->paired = 1, placed = -1, settled = 0;
            if (settled) {
                SignalSema(lock);
                continue;
            }
            float t = a ? (float)(ticks - b->at) / b->span : 1; // the newer list is reached when the next one is due
            int whole = fit();
            // ponytail: a frame whose textures do not fit in VRAM together is shown once, as it is, not in steps:
            // every picture of it would load its textures again (run by run). Less VRAM in the frame buffers
            // (half their lines a field, or 16 bits) would make room.
            if (t > 1 || !whole) t = 1;
            if (nmoving && placed != t) place(a, b, t);
            placed = t, settled = t == 1, crowdings += !whole;
            if (b->cleared) gsKit_clear(gs, b->clear);
            send(whole);
            finished = gs->ScreenBuffer[gs->ActiveBuffer & 1];
            pending = 1;
            gs->ActiveBuffer ^= 1;
            gsKit_setactive(gs);
            gsKit_TexManager_nextFrame(gs);
            pictures++, busy += GetCop0(9) - from;
        }
        SignalSema(lock);
    }
}

extern void *_gp;
void gs_present(void) {
    // The handler and the thread go in with the first frame, not in gs_init: one ELF stopped taking interrupts
    // while loading the game (docs/perf.md, not explained), and its handler was the one new thing running then.
    static char stack[16 * 1024] __attribute__((aligned(16)));
    static int started;
    if (!started) {
        started = 1;
        ee_sema_t sema = {.init_count = 1, .max_count = 1};
        lock = CreateSema(&sema);
        sema.init_count = 0;
        wake = CreateSema(&sema);
        // above the game, below the sound (snd.c)
        ee_thread_t thread = {.func = presenter, .stack = stack, .stack_size = sizeof stack, .gp_reg = &_gp, .initial_priority = 50};
        StartThread(CreateThread(&thread), NULL);
        gsKit_add_vsync_handler(vblank);
    }
    while (newer && ticks == newer->at) {} // only when this frame took less than a vertical blank
    WaitSema(lock);
    Frame *f = building, *next = older ? older : newer ? &frames[2] : &frames[1];
    f->at = ticks, f->span = newer ? f->at - newer->at : 1, f->paired = 0;
    if (f->span > 12) f->span = 12; // (a frame that waited for the disc is not a movement that slow)
    older = newer, newer = f, building = next;
    SignalSema(lock);
    for (int i = 0; i < next->nretire; i++) later(next->retire[i], next->retire[i]->Mem, 1);
    next->nretire = next->n = next->cleared = 0;
    period = stale > 1 ? stale : 1, stale = 0;
    if (++frame % 64 == 0) forget();
    static u32 since;
    if (frame % 60 == 0) { // beside boot.lua's PERF line
        u32 n = pictures, spent = busy, blanks = ticks - since;
        pictures = busy = 0, since = ticks;
        printf("SHOWN %u pictures in %u blanks for 60 frames, %.2f ms each, %d primitives, %u not in steps\n", (unsigned)n, (unsigned)blanks,
               n ? spent / 147456.0 / n : 0.0, newer->n, (unsigned)crowdings);
        crowdings = 0;
    }
}
#endif
