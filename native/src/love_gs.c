// What love_gfx.c draws, on the PS2's GS through gsKit: 640x448 interlaced, two buffers, no Z, alpha
// blending. An atlas is cut into pages of up to 256x256 PSMT8 texels sharing its palette, so the texture manager
// keeps in VRAM only the pages in use (design D5); a rectangle of texels that spans pages is drawn in pieces.
// Main memory holds only the pages drawn lately: a row of pages is read from the atlas's file when one of them is
// first drawn and freed after KEEP frames unused. All of them at once were 7.65 MB of the 32 (docs/perf.md), 2.8
// of them the two logos of the splash.
// ponytail: so the first frame that shows an atlas waits for the disc. Read ahead of the screens that need it if
// that shows on the console.
// The game's one canvas is the frame buffer itself (design D6).
//
// Nothing here draws when the game does: a frame of a run is one to three vertical blanks of the EE.
// love_gfx.c's primitives are written down, a list per frame, and a thread above the game's presents at every
// vertical blank the game as it was a little before: between two of the last three finished lists, each primitive
// of the later one at its place between where the earlier one had it and its own ("presenting", below). So the
// screen moves 60 times a second and shows the game a frame or so behind. A primitive with no pair in the earlier
// list (something that appeared) is drawn as it is.
// Only that thread calls gsKit, which is not made for two: the game's side leaves it what to free and what
// changed in two short lists. A texture made for a frame (and a kept picture given up) is freed when its frame's
// list is used again, four frames later, when no list on screen can name it.
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

#define LOVE_INTERNAL
#include "love.h"

#define PAGE 256
#define KEEP 600 // frames a row of pages stays in main memory after it was last drawn
struct GsTex {
    int w, h, cols, rows;
    char *path;
    int *used; // per row of pages: the frame it was last drawn in
    unsigned char rgba[256][4]; // its palette as a recipe is given a texel (fetch)
    GSTEXTURE pages[];
};
static GSGLOBAL *gs;
static struct GsTex *all[256];
static int count, frame;

// The picture's modes. 480i is what every cable and screen shows; the other two need component cables (or an
// adapter from them) and a screen that takes them, so they are only ever asked for (love_video.c).
// The two frame buffers of 640x448 at 32 bits leave 1856 KB of the GS's 4 MB for textures, and the frames of a
// round have needed up to 1412 KB (-DVRAMSTAT). 720p is 1280x720, 16:9, and the GS shows a buffer's pixel 1, 2 or
// 3 wide there, so at 16 bits (dithered):
//  - "720p": 640x720, each pixel 2 wide. 2176 KB left. The picture fills the 16:9 screen: the screen has to be
//    told 4:3, as it is told for 480i. What small text lacks most is lines, and this has 1.6 times as many.
//  - "720p 4:3": 960x720, one to one between black sides: the right shape whatever the screen does, and 1.5
//    times the pixels across too, but only 1216 KB left: where a frame's textures need more they are loaded
//    again for every picture (the presenter's run by run), and nothing is placed between frames then.
// (1280x720 twice is 3.5 MB even at 16 bits.) Which of the two is the better one is for the console to say.
// ponytail: NTSC consoles only; a PAL one would want 576i/576p here.
// "480i SOFT": each field shows every line half mixed with the one under it (the GS's two read circuits, one a
// line lower), so a stroke one line thick does not blink at 30 Hz on a screen that shows the fields as they come.
// Its own entry and not what 480i is: on a screen that weaves the fields it only halves the sharpness down the
// screen, and small text is already drawn over more than one line (the font's levels). Which of the two a given
// screen and cable want is seen by trying. The development log, text levels.
static const struct Mode { const char *name; int mode, interlace, field, w, h, psm, soft; } modes[] = {
    {"480i", GS_MODE_NTSC, GS_INTERLACED, GS_FIELD, 640, 448, GS_PSM_CT24, 0},
    {"480i SOFT", GS_MODE_NTSC, GS_INTERLACED, GS_FIELD, 640, 448, GS_PSM_CT24, 1},
    {"480p", GS_MODE_DTV_480P, GS_NONINTERLACED, GS_FRAME, 640, 448, GS_PSM_CT24, 0},
    {"720p", GS_MODE_DTV_720P, GS_NONINTERLACED, GS_FRAME, 640, 720, GS_PSM_CT16, 0},
    {"720p 4:3", GS_MODE_DTV_720P, GS_NONINTERLACED, GS_FRAME, 960, 720, GS_PSM_CT16, 0},
};
static int flicker;
// The buffer drawn in goes on screen at the next vertical blank, and the other is drawn in (before the presenter exists)
static void gs_flip(void) {
    int shown = gs->ScreenBuffer[gs->ActiveBuffer & 1];
    gsKit_sync_flip(gs);
    if (flicker) GS_SET_DISPFB1(shown / 8192, gs->Width / 64, gs->PSM, 0, 1); // (the other read circuit, a line lower)
}
const char *gs_mode_name(int n) { return n >= 0 && n < (int)(sizeof modes / sizeof *modes) ? modes[n].name : NULL; }
// The lines between two vertical blanks, as the GS counts them (0: no blanks within about a second).
static int lines(void) {
    int n = 0, blanks = 0;
    *GS_CSR = 8 | 4;
    for (long i = 0; i < 40000000 && blanks < 2; i++) {
        u64 csr = *GS_CSR;
        if (csr & 4) *GS_CSR = 4, n += blanks == 1;
        if (csr & 8) *GS_CSR = 8, blanks++;
    }
    return blanks < 2 ? 0 : n;
}
// Whether something outside the game decides the video mode: asked while 480i is set. A loader can replace the
// kernel's SetGsCrt to show every game in a mode of its choice (Neutrino's -gsm=fp2: 480i shown as 480p, which is
// how the console of the first tests was started); its replacement ignores the modes this file asks for, and
// what follows the ignored request (the GS reset, display registers for a mode that is not there) loses the
// picture. 480i has 262 or 263 lines a blank, and PCSX2 counts that; that console counted 524 and 525.
int gs_forced(void) {
    int n = lines();
    printf("VIDEO %d lines a blank in 480i%s\n", n, n > 400 ? ": the loader forces the video mode, none is offered" : "");
    return n > 400;
}
// (The VIDEO lines before and after each step: on the first console run a switch to 480p left the screen without
// a signal and the log without another line, so where it stops has to show. And a mode in which the GS gives no
// vertical blank within about a second is given up for 480i instead of waiting for one for ever.)
int gs_mode(int n, int *w, int *h) {
    const struct Mode *m = &modes[gs_mode_name(n) ? n : 0];
    printf("VIDEO setting %s\n", m->name);
    dmaKit_wait(DMA_CHANNEL_GIF, 0);
    printf("VIDEO drawing done\n");
    gs->Mode = m->mode, gs->Interlace = m->interlace, gs->Field = m->field;
    gs->Width = m->w, gs->Height = m->h, gs->PSM = m->psm;
    gs->Dithering = m->psm == GS_PSM_CT16 ? GS_SETTING_ON : GS_SETTING_OFF;
    static const signed char bayer[16] = {-4, 0, -3, 1, 2, -2, 3, -1, -3, 1, -4, 0, 3, -1, 2, -2};
    for (int i = 0; i < 16; i++) gs->DitherMatrix[i] = bayer[i];
    void gs_screen_init(GSGLOBAL *g); // gs_screen.c: gsKit_init_screen with its steps in the log and bounded waits
    gs_screen_init(gs); // (the frame buffers again from the start of VRAM: no texture is where it was)
    printf("VIDEO screen set\n");
    int blanks = 0;
    *GS_CSR = 8; // (the vertical blank's flag, cleared by writing it)
    for (long i = 0; i < 20000000 && blanks < 3; i++) // (about a second of polling at the least)
        if (*GS_CSR & 8) blanks++, *GS_CSR = 8;
    if (blanks < 3) {
        printf("VIDEO mode %s: no vertical blank, back to 480i\n", m->name);
        if (m != modes) gs_mode(0, w, h);
        return 0;
    }
    printf("VIDEO blanks seen\n");
    gsKit_TexManager_init(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0); // (source - destination) * alpha + destination
    gsKit_set_clamp(gs, GS_CMODE_CLAMP);
    flicker = m->soft;
    if (flicker) GS_SET_PMODE(1, 1, 1, 1, 0, 0x80); // both read circuits, half and half
    for (int i = 0; i < 2; i++) { // both buffers black
        gsKit_clear(gs, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
        gsKit_queue_exec(gs);
        gs_flip();
    }
    if (w) *w = m->w, *h = m->h;
    printf("VIDEO mode %s: %dx%d, %d bits, %d KB of VRAM left for textures\n", m->name, m->w, m->h, m->psm == GS_PSM_CT16 ? 16 : 32,
           (int)((4 * 1024 * 1024 - gs->CurrentPointer) / 1024));
    return 1;
}
void gs_init(void) {
    gs = gsKit_init_global();
    gs->ZBuffering = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gs_mode(0, NULL, NULL);
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
        int alpha = (pal[i][3] + 1) / 2 * 2; // (unpack() of that)
        t->rgba[i][0] = pal[i][0], t->rgba[i][1] = pal[i][1], t->rgba[i][2] = pal[i][2], t->rgba[i][3] = alpha > 255 ? 255 : alpha;
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
// A frame's list is the GIF packet the GS will take, written while the game draws: every primitive's registers as
// the GS wants them (corners in sixteenths of a pixel, the colour's bytes, 0x80 being 1 for a texture's and for
// alpha), 8 bytes a register (REGLIST: a tag names the registers of the primitives after it), a texture's own
// registers ahead of the primitives that use it.
// The packet is only ever written, and through the EE's uncached accelerated window: writing it loads no line of
// the 8 KB data cache, and there is nothing to flush before the DMA. Beside it, in ordinary memory, the frame's
// shadow: per primitive its colour, its texels and its corners, 32 bits each. That is what the next frame is paired
// with (pairs), a third of the packet's size.
// (The list was floats that the presenter turned into one packet of 16 bytes a register for every frame of the
// game, and a rounded box a triangle per corner: 300 KB and 3 to 4 us a primitive for 2700 to 4300 of them.)
#ifdef NO_UCAB // -DNO_UCAB: the packet through the cache, as any memory (dmaKit_send flushes it)
#define UCAB(p) ((u64 *)(p))
#else
#define UCAB(p) ((u64 *)((u32)(p) | 0x30000000))
#endif
typedef struct {
    GSTEXTURE *tex;  // NULL: flat
    int at, xy, sh;  // in the packet: its colour, and its first corner; in the shadow: its first word
    short n, step;   // corners (4, or a fan's), and the packet's words from one to the next
} Prim;
typedef struct { int start, tex; } Run; // a texture's registers in the packet, and its place in used (-1: the frame's first words)
#define RETIRE 128
typedef struct {
    u64 *pk;      // the packet (its address through the cache: UCAB() to write it)
    u32 *sh, *mv; // the shadow: per primitive its colour, the texels of its first and last corner if textured, its corners; what moves (pairs)
    Prim *prims;
    Run *runs;
    int words, room, n, most, nruns, nused, nsh, shroom, nmv, mvroom;
    int bound, open, tag, count; // being written: the texture of the last run (in used), the kind of the last tag, where it is and its primitives
    int placed, cleared, cut, nretire, serial; // placed: where its moving primitives are in the packet, of 256; cut: nothing moves into it
    u32 clear, at, tick; // when it was finished: the EE's count register, and the vertical blank
    u64 tex0[256];        // per texture, the place in VRAM its runs have in the packet
    GSTEXTURE *used[256]; // its textures
    GSTEXTURE *retire[RETIRE];
} Frame;
// The last three finished frames, oldest first, and the one being drawn
static Frame frames[4], *held[3], *building = &frames[0];
static int nheld;
static volatile u32 ticks; // vertical blanks
static int lock, wake;     // the frames on screen are in use / a vertical blank

// ponytail: a packet is one DMA transfer (65535 quadwords, 1 MB), a frame 32767 primitives and 256 textures; one
// with more is drawn without its last ones. Blind select, the most seen, is 114 KB and 1400.
#define WORDS 131008
#define GIFTAG(w, nloop, reglist, nreg, regs) ((w)[0] = (u64)(nloop) | 1ULL << 15 | (u64)(reglist) << 58 | (u64)(nreg) << 60, (w)[1] = (regs))
// Room in the frame being drawn for one more primitive of `words` words of packet (and no more of shadow); NULL: none
static Prim *prim(int words) {
    Frame *f = building;
    if (f->n == f->most) {
        int most = f->most ? f->most * 2 : 1024;
        if (most > 32767) most = 32767;
        if (f->n == most) return NULL;
        Prim *prims = realloc(f->prims, most * sizeof *prims);
        if (prims) f->prims = prims;
        Run *runs = realloc(f->runs, (most + 2) * sizeof *runs);
        if (runs) f->runs = runs;
        if (!prims || !runs) return NULL;
        f->most = most;
    }
    if (f->nsh + words > f->shroom) {
        int room = f->shroom ? f->shroom * 2 : 8192;
        u32 *sh = realloc(f->sh, room * sizeof *sh);
        if (!sh) return NULL;
        f->sh = sh, f->shroom = room;
    }
    if (f->words + words + 16 > f->room) { // (16: a texture's run, a tag and the frame's first words)
        int room = f->room ? f->room * 2 : 16384;
        if (room > WORDS) room = WORDS;
        if (f->words + words + 16 > room) return NULL;
        u64 *pk = memalign(64, room * sizeof *pk); // (whole lines of the cache, so no line of it holds anything else)
        if (!pk) return NULL;
        SyncDCache(pk, pk + room); // what the cache has of this memory from its life before must not land on what is written around it
#ifndef NO_UCAB
        __asm__ volatile("sync.l"); // (what was written last has left the EE's write buffer)
#endif
        memcpy(UCAB(pk), UCAB(f->pk), f->words * sizeof *pk);
        free(f->pk);
        f->pk = pk, f->room = room;
    }
    if (!f->words) {
        u64 *w = UCAB(f->pk);
        GIFTAG(w, 2, 0, 1, GIF_AD);
        w[2] = gs->PrimAlpha, w[3] = GS_ALPHA_1, w[4] = gs->PABE, w[5] = GS_PABE;
        f->words = 6, f->runs[0] = (Run){0, -1}, f->nruns = 1, f->nused = 0, f->bound = -1, f->open = 0, f->nsh = 0;
    }
    return &f->prims[f->n++];
}
static void empty(Frame *f) { f->n = f->words = f->nruns = f->nused = f->nsh = f->open = 0; }
// The open tag gets its count when no more quads will follow it (the packet is not read back)
static void seal(Frame *f) {
    if (f->open) UCAB(f->pk)[f->tag] = (u64)f->count | 1ULL << 15 | 1ULL << 58 | (u64)(f->open == 2 ? 10 : 6) << 60;
    f->open = 0;
}
// A quad in the frame being drawn, flat or of t's texels. In the packet (from its `at` - 1) it is PRIM (written
// here), RGBAQ, and for each corner UV (if textured) and XYZ2; in the shadow, RGBAQ, the first and the last UV (if
// textured) and the four XYZ2. NULL: no room.
static Prim *quad(GSTEXTURE *t) {
    Frame *f = building;
    int kind = t ? 2 : 1, bound = f->words ? f->bound : -1, size = t ? 10 : 6;
    if (t && (bound < 0 || f->used[bound] != t)) {
        for (bound = 0; bound < f->nused && f->words && f->used[bound] != t; bound++) {}
        if (bound == 256) return NULL;
    }
    Prim *q = prim(size);
    if (!q) return NULL;
    u64 *pk = UCAB(f->pk), *w = pk + f->words;
    if (bound != f->bound) { // another texture: a run
        seal(f);
        if (bound == f->nused) f->used[f->nused++] = t, f->tex0[bound] = 0;
        f->bound = bound;
        f->runs[f->nruns++] = (Run){f->words, bound};
        GIFTAG(w, 2, 0, 1, GIF_AD);
        w[2] = 0, w[3] = GS_TEX0_1; // (where it is in VRAM: send)
        w[4] = GS_SETREG_TEX1(1, 0, t->Filter, t->Filter, 0, 0, 0), w[5] = GS_TEX1_1;
        w += 6;
    }
    if (f->open != kind || f->count == 32767) { // (a tag counts to 32767)
        seal(f);
        f->open = kind, f->tag = w - pk, f->count = 0;
        w[1] = t ? 0x5353535310ULL : 0x555510; // PRIM, RGBAQ, four XYZ2, each after its UV if textured (the count: seal)
        w += 2;
    }
    f->count++;
    w[0] = GS_SETREG_PRIM(GS_PRIM_PRIM_TRISTRIP, 0, t != NULL, 0, 1, 0, t != NULL, 0, 0);
    q->tex = t, q->at = w + 1 - pk, q->xy = q->at + (t ? 2 : 1), q->n = 4, q->step = t ? 2 : 1, q->sh = f->nsh;
    f->nsh += t ? 7 : 5;
    f->words = w + size - pk;
    return q;
}
// A corner as the GS takes it (XYZ2 with no Z): x and y in sixteenths of a pixel, 16 bits each
static inline __attribute__((always_inline)) u32 corner(float x, float y) { return (u32)gsKit_float_to_int_x(gs, x) | (u32)gsKit_float_to_int_y(gs, y) << 16; }
// Whether four corners (pixels) are all off the same side of the screen: nothing of them would be drawn
static inline __attribute__((always_inline)) int unseen(const float p[4][2]) {
    float w = gs->Width, h = gs->Height;
    return (p[0][0] < 0 && p[1][0] < 0 && p[2][0] < 0 && p[3][0] < 0) || (p[0][1] < 0 && p[1][1] < 0 && p[2][1] < 0 && p[3][1] < 0) ||
           (p[0][0] > w && p[1][0] > w && p[2][0] > w && p[3][0] > w) || (p[0][1] > h && p[1][1] > h && p[2][1] > h && p[3][1] > h);
}
// The texels (u0, v0) to (u1, v1) of t between four corners. Nothing if it is clear or off the screen.
static void textured(GSTEXTURE *t, const float p[4][2], float u0, float v0, float u1, float v1, u32 rgba) {
    if (!(rgba >> 24) || unseen(p)) return;
    Prim *q = quad(t);
    if (!q) return;
    u64 *w = UCAB(building->pk) + q->at;
    u32 *s = building->sh + q->sh;
    u32 ua = gsKit_float_to_int_u(t, u0), ub = gsKit_float_to_int_u(t, u1), va = (u32)gsKit_float_to_int_v(t, v0) << 16, vb = (u32)gsKit_float_to_int_v(t, v1) << 16;
    u32 a = corner(p[0][0], p[0][1]), b = corner(p[1][0], p[1][1]), c = corner(p[2][0], p[2][1]), d = corner(p[3][0], p[3][1]);
    w[0] = rgba, w[1] = ua | va, w[2] = a, w[3] = ub | va, w[4] = b, w[5] = ua | vb, w[6] = c, w[7] = ub | vb, w[8] = d;
    s[0] = rgba, s[1] = ua | va, s[2] = ub | vb, s[3] = a, s[4] = b, s[5] = c, s[6] = d;
}
// A flat quad: the strip of four corners. Nothing if it is clear or off the screen.
static void flat(const float p[4][2], u32 rgba) {
    Prim *q;
    if (!(rgba >> 24) || unseen(p) || !(q = quad(NULL))) return;
    u64 *w = UCAB(building->pk) + q->at;
    u32 *s = building->sh + q->sh;
    u32 a = corner(p[0][0], p[0][1]), b = corner(p[1][0], p[1][1]), c = corner(p[2][0], p[2][1]), d = corner(p[3][0], p[3][1]);
    w[0] = rgba, w[1] = a, w[2] = b, w[3] = c, w[4] = d;
    s[0] = rgba, s[1] = a, s[2] = b, s[3] = c, s[4] = d;
}
// p0, p0 + e1, p0 + e2 and their sum + bend
#define CORNERS(p0, e1, e2, bend) \
    {{(p0)[0], (p0)[1]}, {(p0)[0] + (e1)[0], (p0)[1] + (e1)[1]}, {(p0)[0] + (e2)[0], (p0)[1] + (e2)[1]}, \
     {(p0)[0] + (e1)[0] + (e2)[0] + (bend)[0], (p0)[1] + (e1)[1] + (e2)[1] + (bend)[1]}}
static const float none[2];
#define FLAT(c) (((int)((c)[0] * 255) & 255) | ((int)((c)[1] * 255) & 255) << 8 | ((int)((c)[2] * 255) & 255) << 16 | (u32)((int)((c)[3] * 128) & 255) << 24)
#define TINT(c) (((int)((c)[0] * 128) & 255) | ((int)((c)[1] * 128) & 255) << 8 | ((int)((c)[2] * 128) & 255) << 16 | (u32)((int)((c)[3] * 128) & 255) << 24)

// A filled polygon of n corners (screen pixels), as the fan the GS draws from its first corner
void gs_fan(const float (*p)[2], int n, const float c[4]) {
    u32 rgba = FLAT(c);
    if (n < 3 || n > 256 || !(rgba >> 24)) return; // (clear: nothing to see)
    if (n <= 4) { // the strip of a quad: the fan's two triangles, or one with its last corner twice
        const float q[4][2] = {{p[1][0], p[1][1]}, {p[0][0], p[0][1]}, {p[2][0], p[2][1]}, {p[n - 1][0], p[n - 1][1]}};
        flat(q, rgba);
        return;
    }
    Frame *f = building;
    Prim *q = prim(6 + n + (n & 1));
    if (!q) return;
    seal(f);
    u64 *w = UCAB(f->pk) + f->words;
    u32 *s = f->sh + f->nsh;
    int ox = gs->OffsetX, oy = gs->OffsetY, x0 = 65535, y0 = 65535, x1 = 0, y1 = 0;
    for (int i = 0; i < n; i++) { // (corner(), and whether they are all off the same side of the screen)
        int x = (int)(p[i][0] * 16.0f) + ox, y = (int)(p[i][1] * 16.0f) + oy;
        x = x < 0 ? 0 : x > 65535 ? 65535 : x, y = y < 0 ? 0 : y > 65535 ? 65535 : y;
        x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1, y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
        w[6 + i] = s[1 + i] = (u32)x | (u32)y << 16;
    }
    if (x1 < ox || y1 < oy || x0 > ox + (gs->Width << 4) || y0 > oy + (gs->Height << 4)) { // off the screen
        f->n--;
        return;
    }
    GIFTAG(w, 1, 1, 2, 0x10); // PRIM, RGBAQ
    w[2] = GS_SETREG_PRIM(GS_PRIM_PRIM_TRIFAN, 0, 0, 0, 1, 0, 0, 0, 0), w[3] = s[0] = rgba;
    GIFTAG(w + 4, n, 1, 1, 0x5); // XYZ2 n times (and a word to make them even)
    w[6 + n] = 0;
    q->tex = NULL, q->at = f->words + 3, q->xy = f->words + 6, q->n = n, q->step = 1, q->sh = f->nsh;
    f->words += 6 + n + (n & 1), f->nsh += 1 + n;
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
// A 32-bit texture made on the EE (what a recipe draws, love_gfx.c). NULL: no memory.
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
    if (!t) return;
    for (int i = 0; i < w * h; i++) t->Mem[i] = PACK(rgba + i * 4);
    written(t);
    const float xy[4][2] = CORNERS(p0, e1, e2, none);
    textured(t, xy, u, v, u + du, v + dv, 0x80808080);
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
        float xy[4][2] = CORNERS(p0, e1, e2, none);
        if (tri) xy[3][0] = xy[2][0], xy[3][1] = xy[2][1]; // (a triangle's third corner twice)
        flat(xy, FLAT(c));
        return;
    }
    if (du <= 0 || dv <= 0) return;
    if (!filter && u >= 0 && v >= 0 && (int)u / PAGE < t->cols && (int)v / PAGE < t->rows) {
        // Every glyph and nearly every sprite: texels of one page, as they are. What the pieces below come to then
        // (to the bit: their parts along e1 and e2 are 0 and 1), without their divisions.
        int col = (int)u / PAGE, r = (int)v / PAGE;
        GSTEXTURE *p = &t->pages[r * t->cols + col];
        float ub = u + du, vb = v + dv;
        if (ub <= col * PAGE + p->Width && vb <= r * PAGE + p->Height && ub - u == du && vb - v == dv) {
            if (!t->pages[r * t->cols].Mem && !load_row(t, r)) return; // not drawn this frame
            t->used[r] = frame;
            const float xy[4][2] = CORNERS(p0, e1, e2, bend);
            textured(p, xy, u - col * PAGE, v - r * PAGE, ub - col * PAGE, vb - r * PAGE, TINT(c));
            return;
        }
    }
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
                for (int y = y0; make && y < y1; y++) {
                    const u8 *row = (u8 *)p->Mem + (y - r * PAGE) * p->Width - col * PAGE; // (fetch() of this piece's own texels: all in the page)
                    for (int x = x0; x < x1; x++) {
                        unsigned char rgba[4];
                        memcpy(rgba, t->rgba[row[x]], 4);
                        filter(x - (int)u, y - (int)v, rgba, fetch);
                        *out++ = PACK(rgba);
                    }
                }
                if (make) written(f), touched(f); // (the lists on screen that show it get the new picture early)
            }
            float xy[4][2];
            for (int i = 0; i < 4; i++) {
                float s = i & 1 ? sb : sa, w = i & 2 ? rb : ra;
                xy[i][0] = p0[0] + e1[0] * s + e2[0] * w + bend[0] * s * w, xy[i][1] = p0[1] + e1[1] * s + e2[1] * w + bend[1] * s * w;
            }
            textured(f ? f : p, xy, ua - x0, va - y0, ub - x0, vb - y0, TINT(c));
        }
    }
}

void gs_smooth(struct GsTex *t) {
    for (int i = 0; i < t->cols * t->rows; i++) t->pages[i].Filter = GS_FILTER_LINEAR;
}

// The swirls' pictures and their one palette, which is a texture of its own (16 x 16) for the texture manager:
// the palette changes with the uniforms, every frame in the menu, and as the CLUT of the pictures it took their
// 72 KB of texels to the GS again each time, for both pictures on screen.
static GSTEXTURE swirls[3], swirl_clut;
void gs_swirl(int id, const unsigned char *idx, int w, int h, int fresh, const unsigned char pal[256][4], float alpha) {
    static u32 has[256]; // the palette as the texture has it
    GSTEXTURE *t = &swirls[id], *c = &swirl_clut;
    int first = !c->Mem;
    if (first) {
        c->Width = c->Height = 16, c->PSM = GS_PSM_CT32, c->Filter = GS_FILTER_NEAREST;
        if (!(c->Mem = memalign(128, sizeof has))) return;
    }
    if (!t->Mem) { // (no Clut: where the palette is in VRAM is written before each picture, fit)
        t->Width = w, t->Height = h, t->PSM = GS_PSM_T8, t->ClutPSM = GS_PSM_CT32, t->Filter = GS_FILTER_LINEAR;
        if (!(t->Mem = memalign(128, gsKit_texture_size_ee(w, h, GS_PSM_T8)))) return;
        fresh = 1;
    }
    u32 clut[256];
    for (int i = 0; i < 256; i++) // (the order of a CLUT: gs_texture)
        clut[(i & 0x18) == 8 ? i + 8 : (i & 0x18) == 16 ? i - 8 : i] = PACK(pal[i]);
    if (first || memcmp(clut, has, sizeof clut)) {
        memcpy(has, clut, sizeof clut);
        memcpy(c->Mem, clut, sizeof clut);
        SyncDCache(c->Mem, (u8 *)c->Mem + sizeof clut);
        touched(c);
    }
    if (fresh) {
        memcpy(t->Mem, idx, w * h);
        SyncDCache(t->Mem, (u8 *)t->Mem + w * h);
        touched(t);
    }
    const float xy[4][2] = {{0, 0}, {gs->Width, 0}, {0, gs->Height}, {gs->Width, gs->Height}};
    Frame *f = building;
    int n = f->n;
    textured(t, xy, 0.5f, 0.5f, w - 0.5f, h - 0.5f, 0x808080 | (u32)((int)(alpha * 128) & 255) << 24);
    if (f->n == n) return; // (clear: not drawn)
    for (n = 0; n < f->nused && f->used[n] != c; n++) {}
    if (n == f->nused && n < 256) f->used[f->nused++] = c, f->tex0[n] = 0; // the palette is one of the frame's textures too
}

void gs_clear(const float c[4]) {
    empty(building); // (what was drawn before it is under it)
    building->cleared = 1, building->clear = GS_SETREG_RGBAQ((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), 0x80, 0);
}

// ---------------------------------------------------------------- presenting
// For each primitive of b, the one of a it is the next state of: the same texels (or both flat, of as many
// corners and near each other), in the same order. What a has in between is something that went away; what b has
// in between, new. All of it on the frames' shadows: a corner is x and y in 16 bits each.
// A pair is worth placing in between if its colour changed or a corner is half a pixel away (the whole room sways
// a little every frame, so nearly nothing is where it was to the last bit). Those go to b's list of what moves,
// each with all a picture needs: where its colour and corners are in the packet, and both ends of each (so a
// picture reads that list alone, and writes the packet).
// 0: q is not p's next state; 1: it is, and where p was; 2: it is, and moved
static int alike(const Prim *p, const u32 *was, const Prim *q, const u32 *is) {
    if (p->tex != q->tex || p->n != q->n) return 0;
    int moved = was[0] != is[0], far = 48 * 16; // two flat ones have nothing else to tell them apart
    if (p->tex) {
        if (was[1] != is[1] || was[2] != is[2]) return 0; // the first corner's texel and the last's
        was += 2, is += 2, far = 1 << 20;
    }
    for (int i = p->n; i; i--) {
        u32 a = *++was, b = *++is;
        if (a == b) continue;
        int dx = (int)(a & 0xffff) - (int)(b & 0xffff), dy = (int)(a >> 16) - (int)(b >> 16);
        dx = dx < 0 ? -dx : dx, dy = dy < 0 ? -dy : dy;
        if (dy > dx) dx = dy;
        if (dx > far) return 0;
        moved |= dx > 8;
    }
    return 1 + moved;
}
// (by the game's thread, when b is finished: a is the frame before it, which nobody writes any more)
static void pairs(const Frame *a, Frame *b) {
    int j = 0, room = 5 * b->n + 2 * b->nsh; // (if every one moves)
    b->nmv = 0;
    if (!a || b->cut) return;
    if (b->mvroom < room) {
        u32 *mv = realloc(b->mv, room * sizeof *mv);
        if (!mv) return; // (then nothing moves in between)
        b->mv = mv, b->mvroom = room;
    }
    u32 *m = b->mv;
    for (int i = 0; i < b->n; i++) {
        const Prim *q = &b->prims[i];
        const u32 *is = b->sh + q->sh;
        for (int ahead = 0; ahead < 16 && j + ahead < a->n; ahead++) {
            const Prim *p = &a->prims[j + ahead];
            const u32 *was = a->sh + p->sh;
            int same = alike(p, was, q, is);
            if (!same) continue;
            j += ahead + 1;
            if (same == 2) {
                int skip = q->tex ? 3 : 1;
                m[0] = q->at, m[1] = q->xy, m[2] = q->n | q->step << 16, m[3] = was[0], m[4] = is[0], m += 5;
                for (int n = 0; n < q->n; n++) *m++ = was[skip + n], *m++ = is[skip + n];
            }
            break;
        }
    }
    b->nmv = m - b->mv;
}
// The primitives of f that move, where they are k/256 of the way from the frame before to their own
static void place(Frame *f, int k) {
    u64 *pk = UCAB(f->pk);
    for (const u32 *m = f->mv, *end = m + f->nmv; m < end;) {
        u64 *w = pk + m[1];
        int n = m[2] & 0xffff, step = m[2] >> 16;
        u32 c = 0;
        for (int bit = 0; bit < 32; bit += 8) {
            int v = m[3] >> bit & 255;
            c |= (u32)(v + ((int)(m[4] >> bit & 255) - v) * k / 256) << bit;
        }
        pk[m[0]] = c;
        for (m += 5; n; n--, m += 2, w += step) {
            int x = m[0] & 0xffff, y = m[0] >> 16;
            x += ((int)(m[1] & 0xffff) - x) * k / 256, y += ((int)(m[1] >> 16) - y) * k / 256;
            *w = (u32)x | (u32)y << 16;
        }
    }
    f->placed = k;
}

// A picture costs the EE little: the corners and the colour of the primitives that move are written again, a
// texture's place in VRAM too if it is another, and the frame's packet is sent as it is. (Through gsKit's own
// primitives a picture was 3.3 ms of the EE, a fifth of all of it at 60 a second.)
// The packet is in runs: a texture's registers and the primitives up to the next texture. If VRAM cannot hold
// every texture of the frame at once (the texture manager then gives one's place to another), the runs are sent
// one by one, each after its texture is back: slow, and right.
static volatile u32 crowdings; // pictures sent run by run
static int log2up(int v) {
    int n = 0;
    while (1 << n < v) n++;
    return n;
}
static u64 tex0(const GSTEXTURE *x) {
    return GS_SETREG_TEX0(x->Vram / 256, x->TBW, x->PSM, log2up(x->Width), log2up(x->Height), 1, 0, x->VramClut / 256, x->ClutPSM, 0, 0, 1);
}
// Whether two of the frame's textures have the same VRAM (or one has none)
static int crowded(const Frame *f) {
    static u32 from[512], to[512];
    int n = 0;
    for (int i = 0; i < f->nused; i++) {
        const GSTEXTURE *x = f->used[i];
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
static int fit(const Frame *f) {
    for (int i = 0; i < f->nused; i++) gsKit_TexManager_bind(gs, f->used[i]);
    for (int i = 0; i < 3; i++) swirls[i].VramClut = swirl_clut.Vram; // (gs_swirl)
#ifdef VRAMSTAT // -DVRAMSTAT: the most VRAM a frame's textures (and their palettes) have needed so far, when it grows
    static int most;
    int sum = 0;
    for (int i = 0; i < f->nused; i++)
        sum += gsKit_texture_size(f->used[i]->Width, f->used[i]->Height, f->used[i]->PSM) + (f->used[i]->Clut ? gsKit_texture_size(16, 16, f->used[i]->ClutPSM) : 0);
    if (sum > most + 32768) most = sum, printf("VRAM a frame's %d textures: %d KB\n", f->nused, sum / 1024);
#endif
    return !crowded(f);
}
// A picture: the packet to the GS, whole or run by run
static void send(Frame *f, int whole) {
    static unsigned char moved[256]; // the textures that are elsewhere in VRAM than the packet says
    u64 *pk = UCAB(f->pk);
    int any = 0;
    for (int i = 0; whole && i < f->nused; i++) {
        u64 now = tex0(f->used[i]);
        any |= moved[i] = now != f->tex0[i];
        f->tex0[i] = now;
    }
    for (int i = 0; any && i < f->nruns; i++)
        if (f->runs[i].tex >= 0 && moved[f->runs[i].tex]) pk[f->runs[i].start + 2] = f->tex0[f->runs[i].tex];
    gsKit_queue_exec(gs); // the textures that were not in VRAM, the buffer to draw in
    dmaKit_wait(DMA_CHANNEL_GIF, 0); // (the game's side may write the textures' memory again)
    if (!f->words) return;
#ifndef NO_UCAB
    __asm__ volatile("sync.l"); // (what was written last has left the EE's write buffer)
#endif
    if (whole) return dmaKit_send(DMA_CHANNEL_GIF, f->pk, f->words / 2);
    for (int i = 0; i < f->nruns; i++) {
        int start = f->runs[i].start, end = i + 1 < f->nruns ? f->runs[i + 1].start : f->words, t = f->runs[i].tex;
        if (t >= 0) {
            gsKit_TexManager_bind(gs, f->used[t]);
            gsKit_queue_exec(gs);
            dmaKit_wait(DMA_CHANNEL_GIF, 0);
            pk[start + 2] = tex0(f->used[t]), f->tex0[t] = 0; // (0: a whole picture of it writes them all again)
#ifndef NO_UCAB
            __asm__ volatile("sync.l");
#endif
        }
        dmaKit_send(DMA_CHANNEL_GIF, f->pk + start, (end - start) / 2);
        dmaKit_wait(DMA_CHANNEL_GIF, 0);
    }
}

// What is on screen is the game a little while ago: at every vertical blank, the game as it was `delay` before,
// which lies between two of the frames held and is made of the later one's packet, its moving primitives placed
// that far from the earlier frame's. The delay is the longest of the game's last frames and a quarter of a blank,
// so there is nearly always a later frame to go towards, and the time shown moves on a blank per blank whatever
// the game's frames take each: one blank, then two, then one. (It was "the newer frame is reached when the next is
// due, taken to last as long as the one before, in whole blanks": at a blank and a half a frame, a picture on three
// blanks of four, and unevenly.) A frame's time is the EE's count register when the game finished it.
// While the game keeps up with the screen (frame after frame within a blank) nothing is placed: the newest frame
// is shown as it is, once. Going from one way to the other, the time shown runs at half or one and a half times
// the clock until it is where it should be, never back and never in a jump.
// The count register between two vertical blanks (16.683 ms): measured, the shortest of the first two seconds'
// (the register counts the EE's clock, 294.912 MHz, in PCSX2; half of it by other accounts)
static volatile u32 blank = 4920120;
#define BLANK blank
static u32 delay;
static int direct = 1;
static volatile int pending;
static u32 finished;
static volatile u32 pictures, busy; // presented, and the EE's count register spent presenting them
#ifdef PACE // -DPACE: with every SHOWN line, how far the time shown moved at each of the last blanks, in 1/16 of a blank (16: even)
static volatile unsigned char pace[32];
static volatile u32 paces;
#endif
static int vblank(int cause) {
    static u32 at, least = ~0u;
    u32 now = iGetCop0(9);
    if (ticks > 1 && ticks < 120 && now - at > 2000000 && now - at < least) least = now - at;
    if (ticks == 120 && least != ~0u) blank = least;
    at = now;
    ticks++;
    if (pending) {
        GS_SET_DISPFB2(finished / 8192, gs->Width / 64, gs->PSM, 0, 0);
        if (flicker) GS_SET_DISPFB1(finished / 8192, gs->Width / 64, gs->PSM, 0, 1); // (the other read circuit, a line lower)
        pending = 0;
    }
    iSignalSema(wake);
    ExitHandler();
    return 0;
}
static void presenter(void *arg) {
    static struct { GSTEXTURE *tex; void *mem; int own; } went[512];
    static GSTEXTURE *dirty[128];
    static u32 shown; // the time on screen
    static int running, last, last_k; // the frame on screen (its serial) and where its moving primitives were
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
        u32 from = GetCop0(9);
        if (nheld) {
            Frame *newest = held[nheld - 1], *x = held[0];
            u32 time = direct ? newest->at : from - delay, was = shown;
            if (running && from - shown < 30 * BLANK) { // towards it, between half and one and a half times the clock's speed
                if ((int)(time - (shown + BLANK / 2)) < 0) time = shown + BLANK / 2;
                else if ((int)(time - (shown + BLANK + BLANK / 2)) > 0) time = shown + BLANK + BLANK / 2;
            }
            if ((int)(time - newest->at) > 0) time = newest->at; // no later frame to go towards: the newest stays
            if ((int)(time - x->at) < 0) time = x->at;           // (and the oldest held is the furthest back)
            int k = 256;
            for (int i = nheld - 1; i > 0; i--)
                if ((int)(time - held[i - 1]->at) > 0) { // between held[i - 1] and held[i]
                    u32 span = (held[i]->at - held[i - 1]->at) >> 8;
                    x = held[i], k = (int)(((time - held[i - 1]->at) >> 8) * 256 / (span ? span : 1));
                    if (k > 256) k = 256;
                    if (held[i]->cut || !k) x = held[i - 1], k = 256; // (a frame nothing moves into waits for its own time)
                    break;
                }
            shown = time, running = 1;
#ifdef PACE
            pace[paces++ & 31] = (time - was) / (BLANK / 16);
#endif
            int whole = 1;
            if (x->serial != last || k != last_k) { // (else it is on screen as it is: nothing to do)
                dmaKit_wait(DMA_CHANNEL_GIF, 0); // the picture before this one
                whole = fit(x);
                // ponytail: a frame whose textures do not fit in VRAM together is shown once, as it is, not in steps:
                // every picture of it would load its textures again (run by run). Less VRAM in the frame buffers
                // (half their lines a field, or 16 bits) would make room.
                if (!whole) k = 256;
            }
            if (x->serial != last || k != last_k) {
                if (x->placed != k) place(x, k);
                last = x->serial, last_k = k, crowdings += !whole;
                if (x->cleared) gsKit_clear(gs, x->clear);
                send(x, whole);
                finished = gs->ScreenBuffer[gs->ActiveBuffer & 1];
                pending = 1;
                gs->ActiveBuffer ^= 1;
                gsKit_setactive(gs);
                gsKit_TexManager_nextFrame(gs);
                pictures++, busy += GetCop0(9) - from;
            }
        }
        SignalSema(lock);
    }
}
// Whether the game's frames have been fitting in a vertical blank (for what can be made at half the pace otherwise)
int gs_keeping_up(void) { return direct; }
// The mode's screen (love_video.c), before the game: what was drawn, on screen at the next vertical blank, with
// no thread and no handler of its own (they go in with the game's first frame: gs_present).
void gs_show(void) {
    Frame *f = building;
    seal(f);
    int whole = fit(f);
    if (f->cleared) gsKit_clear(gs, f->clear);
    send(f, whole);
    dmaKit_wait(DMA_CHANNEL_GIF, 0);
    gs_flip();
    gsKit_TexManager_nextFrame(gs);
    f->cleared = 0, empty(f);
}

extern void *_gp;
void gs_present(void) {
    // The handler and the thread go in with the first frame, not in gs_init: one ELF stopped taking interrupts
    // while loading the game (docs/perf.md, not explained), and its handler was the one new thing running then.
    static char stack[16 * 1024] __attribute__((aligned(16)));
    static int started, serial, streak = 30;
    static u32 took[4], ntook;
    if (!started) {
        started = 1;
        took[0] = took[1] = took[2] = took[3] = BLANK;
        ee_sema_t sema = {.init_count = 1, .max_count = 1};
        lock = CreateSema(&sema);
        sema.init_count = 0;
        wake = CreateSema(&sema);
        // above the game, below the sound (love_snd.c)
        ee_thread_t thread = {.func = presenter, .stack = stack, .stack_size = sizeof stack, .gp_reg = &_gp, .initial_priority = 50};
        StartThread(CreateThread(&thread), NULL);
        gsKit_add_vsync_handler(vblank);
    }
    Frame *f = building, *newest = nheld ? held[nheld - 1] : NULL;
    seal(f);
    while (newest && ticks == newest->tick) {} // only when this frame took less than a vertical blank
    f->at = GetCop0(9), f->tick = ticks, f->serial = ++serial, f->placed = 256, f->nmv = 0;
    f->cut = !newest || f->at - newest->at > 12 * BLANK; // (a frame that waited for the disc is not a movement that slow)
    if (!f->cut) {
        took[ntook++ & 3] = f->at - newest->at;
        streak = f->at - newest->at < BLANK + BLANK / 8 ? streak + 1 : 0;
    }
    u32 longest = took[0];
    for (int i = 1; i < 4; i++) longest = took[i] > longest ? took[i] : longest;
    if (streak < 30) pairs(newest, f); // (half a second of frames within a blank: the game is keeping up)
    WaitSema(lock);
    delay = longest + BLANK / 4, direct = streak >= 30;
    Frame *next;
    if (nheld == 3) next = held[0], held[0] = held[1], held[1] = held[2], held[2] = f;
    else held[nheld++] = f, next = &frames[nheld];
    building = next;
    SignalSema(lock);
    for (int i = 0; i < next->nretire; i++) later(next->retire[i], next->retire[i]->Mem, 1);
    next->nretire = next->cleared = 0, empty(next);
    period = stale > 1 ? stale : 1, stale = 0;
    if (++frame % 64 == 0) forget();
    static u32 since;
    if (frame % 60 == 0) { // beside the PERF line
        u32 n = pictures, spent = busy, blanks = ticks - since;
        pictures = busy = 0, since = ticks;
        printf("SHOWN %u pictures in %u blanks for 60 frames, %.2f ms each, %d primitives, %u not in steps, %d KB, %d moving, %s, blank %u\n", (unsigned)n,
               (unsigned)blanks, n ? spent * 16.683 / BLANK / n : 0.0, f->n, (unsigned)crowdings, f->words / 128, f->nmv,
               direct ? "keeping up" : "paced", (unsigned)BLANK);
        crowdings = 0;
#ifdef PACE
        printf("PACE");
        for (int i = 0; i < 32; i++) printf(" %d", pace[(paces + i) & 31]);
        printf("\n");
#endif
    }
}
#endif
