// The collector of gc.h: a conservative, non-moving, incremental mark-and-sweep over one arena of 4 KB pages.
//
// A collection (a "cycle") is done in slices, one per frame (gc_frame), each bounded by a count of work units (a
// word looked at, a table value followed, a set slot, a page swept), not by the clock: the same slices everywhere.
//   ROOTS  the data and bss are scanned, a piece per slice (the dirty frozen tables and the stack first)
//   MARK   the blocks waiting on the mark stack are scanned or traced
//   final  one step that is not cut: the stack, the data and bss and every marked scanned block are looked at
//          again, and what that finds is marked to the end. Then nothing reachable is unmarked.
//   WEAK   the weak hook drops what was not reached (the interned strings), a piece per slice
//   SWEEP  the pages give back their unmarked blocks, some per slice
// What makes cutting it sound:
//  - a traced block (a table) is only written through its owner's code, which marks every pointer it stores while
//    gc_barrier is set (core.c: wb): a table already traced never comes to hold an unmarked block;
//  - scanned blocks (objects, closures' contexts), the stack and the statics are written by plain C, with no
//    barrier: so they are all scanned again in the final step, where the program does not run in between;
//  - a block allocated during a cycle is born marked, until the sweep has passed its page.
// ponytail: the final step is whole (all the roots and all the live scanned blocks, whatever changed): it is the
// longest pause. Cutting it needs a write barrier on object fields, which plain C assignments cannot give.
// ponytail: conservative: a number or a stale stack slot that looks like an address keeps a block alive until it
// changes, and a block reached only by a pointer one past its end is not kept (none is known in the port).
// ponytail: one table or one scanned block is done in one go, however big (the game's are small; the big tables
// are frozen and not walked at all).
#include "gc.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef GC_HEAP_EE
#define GC_HEAP_EE 12 // MB of the PS2's 32
#endif
#ifndef GC_SLICE
#define GC_SLICE 60000 // work units of a slice
#endif
#define PAGE_SHIFT 12
#define PAGE ((size_t)1 << PAGE_SHIFT)
#define SMALL_MAX 2048
// A unit is a word read in a row (about 2.5 ns of the host, 30 ns of the PS2 by the first console run's pauses).
// What jumps about in memory is counted as what it costs there, where a cache miss is dear:
#define MARK_UNITS 9   // a pointer a trace hook reports (the page table, then the block's mark)
#define BLOCK_UNITS 12 // a block taken from the mark stack, besides its words or its pointers
#define SWEEP_UNITS 48 // a page swept
// (and core.c: 10 for a slot of the interned strings' set)

// The roots in the program's own image: initialised data and bss.
#ifdef _EE
extern char _fdata[], _end[]; // $PS2SDK/ee/startup/linkfile
// .rodata and the sections of constants after it lie between .data and .bss: Makefile.ps2 gives the linker these
// two (gc_ee.ld) and they are not scanned. Without them (another link): scanned, which only takes time.
extern char gc_rodata[] __attribute__((weak)), gc_rodata_end[] __attribute__((weak));
#define DATA_START _fdata
#define DATA_END _end
#else
extern char __data_start[], _end[]; // glibc's crt and ld's default script: .data to the end of .bss
#define DATA_START __data_start
#define DATA_END _end
#endif

#ifdef __SANITIZE_ADDRESS__
// The address sanitizer would move the locals whose address is taken to "fake stack" frames in its heap (to catch
// their use after a return), where the stack scan does not see them: the pointers in them would be lost.
const char *__asan_default_options(void);
const char *__asan_default_options(void) { return "detect_stack_use_after_return=0"; }
#endif

enum { P_FREE, P_SMALL, P_LARGE, P_CONT };
enum { K_FREE, K_ATOMIC, K_SCAN, K_TRACED };
enum { IDLE, ROOTS, MARK, WEAK, SWEEP };

// A block's kind is two bits (k1 k0), its mark one and its pin (gc_pin) one: four bit planes per page, kept side
// by side for each 32 blocks (a lookup touches one cache line of them). The sweep is a few word operations a
// page and touches no block.
typedef struct {
    uint8_t state, cls;      // P_SMALL: the size class
    uint8_t kind, mark, pin; // P_LARGE: of the one block
    uint16_t nfree;          // P_SMALL: free blocks
    uint32_t span;           // P_LARGE: pages of the block; P_CONT: pages back to its first
    struct { uint32_t k0, k1, mark, pin; } w[8];
} PageInfo;

#define NCLS 21
static const uint16_t cls_size[NCLS] = {16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 1024, 1360, 2048};
static uint16_t cls_n[NCLS];      // blocks in a page
static uint32_t cls_recip[NCLS];  // (offset * recip) >> 24 == offset / size for an offset in a page (gc_test.c checks every one)
static uint8_t size_cls[SMALL_MAX / 16 + 1]; // by (size + 15) / 16

bool gc_barrier, gc_reviving;
static bool on, busy, poison, stat;
static int phase;
static char *lo;             // the arena
static uintptr_t arena;      // its size
static uint32_t npages, free_pages;
static PageInfo *pi;
static uint32_t cls_cur[NCLS]; // no page of the class before it has a free block
static uint32_t free_cur;      // no page before it is free
static uint32_t sweep_at;      // SWEEP: the pages before it are swept
static const char *root_at;    // ROOTS: the data before it is scanned
static char *stack_bottom;
static struct { char *start, *end; } roots[16];
static int nroots;
static uintptr_t *ms; // the mark stack: blocks marked and not scanned yet, their kind in the low bits
static size_t ms_n, ms_cap;
static void (*roots_hook)(void), (*trace_hook)(void *), (*census_hook)(void *, size_t);
static bool (*weak_hook)(long *);
static struct { const char *start, *end; } skips[16];
static int nskips;
static size_t step, since, free_after;
static long slice_units = GC_SLICE, work; // work: units left in the slice being run
static unsigned stress, stress_n, stress_slice, stress_slice_n;
static GcStats stats;
// What the cycle being run did, for the GCSTAT line: units and clock ticks by part
static struct {
    long roots_u, mark_u, final_roots_u, final_scan_u, final_drain_u, weak_u, sweep_u;
    clock_t hook_t, roots_t, mark_t, final_t, weak_t, sweep_t;
    long tables, scanned, final_blocks, slices, worst_u;
    clock_t worst_t;
    size_t freed, n[4], bytes[4]; // live blocks and bytes by kind
} cy;

__attribute__((noreturn)) static void die(const char *what, size_t size) {
    fprintf(stderr, "gc: %s (asked %lu bytes; arena %lu KB, used %lu KB, free pages %u of %u)\n", what, (unsigned long)size,
            (unsigned long)(arena >> 10), (unsigned long)(stats.used >> 10), (unsigned)free_pages, (unsigned)npages);
    abort();
}

void gc_init(void *bottom) {
#ifdef NO_GC
    return;
#else
    if (on) return;
    size_t mb = GC_HEAP_EE;
    size_t step_kb = 0;
#ifdef GC_STAT
    stat = true;
#endif
#ifndef _EE
    if (getenv("GC_OFF")) return;
    mb = getenv("GC_HEAP") ? (size_t)atoi(getenv("GC_HEAP")) : 64;
    if (getenv("GC_STEP_KB")) step_kb = (size_t)atoi(getenv("GC_STEP_KB"));
    if (getenv("GC_STRESS")) stress = (unsigned)atoi(getenv("GC_STRESS"));
    if (getenv("GC_STRESS_SLICE")) stress_slice = (unsigned)atoi(getenv("GC_STRESS_SLICE"));
    if (getenv("GC_SLICE")) slice_units = atol(getenv("GC_SLICE"));
    if (getenv("GC_STAT")) stat = true;
    poison = stress || stress_slice || getenv("GC_POISON");
#endif
    if (mb < 1) mb = 1;
    if (slice_units < 1) slice_units = 1;
    npages = free_pages = (uint32_t)(mb << (20 - PAGE_SHIFT));
    arena = (uintptr_t)npages << PAGE_SHIFT;
    char *raw = malloc(arena + PAGE);
    pi = calloc(npages, sizeof *pi);
    if (!raw || !pi) die("no memory for the arena", arena);
    lo = (char *)(((uintptr_t)raw + PAGE - 1) & ~(uintptr_t)(PAGE - 1));
    for (int c = 0; c < NCLS; c++) {
        cls_n[c] = (uint16_t)(PAGE / cls_size[c]);
        cls_recip[c] = (uint32_t)(((1u << 24) + cls_size[c] - 1) / cls_size[c]);
    }
    for (int i = 0, c = 0; i <= SMALL_MAX / 16; i++) {
        if (i * 16 > cls_size[c]) c++;
        size_cls[i] = (uint8_t)c;
    }
    stack_bottom = bottom;
#ifdef _EE
    if (gc_rodata && gc_rodata_end) gc_skip(gc_rodata, gc_rodata_end);
#endif
    step = step_kb ? step_kb << 10 : arena / 4;
    free_after = arena;
    stats.heap = arena;
    stats.slice = slice_units;
    on = true;
#endif
}

void gc_set_hooks(void (*roots)(void), void (*trace)(void *block), bool (*weak)(long *work)) { roots_hook = roots, trace_hook = trace, weak_hook = weak; }
void gc_set_census(void (*census)(void *block, size_t size)) { census_hook = census; }

void gc_skip(const void *start, const void *end) {
    if (nskips < 16) skips[nskips].start = start, skips[nskips].end = end, nskips++;
}

void gc_add_root(void *start, void *end) {
    if (nroots == 16) die("more than 16 root ranges", 0);
    roots[nroots].start = start, roots[nroots].end = end, nroots++;
}

const GcStats *gc_stats(void) { return &stats; }

// ---------------------------------------------------------------- allocation
// A block allocated while a cycle is on is born marked, unless the sweep has already passed its page
static inline bool born_marked(uint32_t page) { return phase != IDLE && (phase != SWEEP || page >= sweep_at); }

static void *small(size_t size, int kind) {
    int c = size_cls[(size + 15) >> 4];
    uint32_t p = cls_cur[c];
    while (p < npages && !(pi[p].state == P_SMALL && pi[p].cls == c && pi[p].nfree)) p++;
    if (p == npages) { // no page of the class has room: a free page becomes one of them
        for (p = free_cur; p < npages && pi[p].state != P_FREE; p++) {}
        free_cur = p;
        if (p == npages) {
            cls_cur[c] = npages;
            return NULL;
        }
        pi[p].state = P_SMALL, pi[p].cls = (uint8_t)c, pi[p].nfree = cls_n[c];
        free_cur = p + 1, free_pages--;
    }
    cls_cur[c] = p;
    PageInfo *g = &pi[p];
    for (unsigned w = 0;; w++) {
        uint32_t taken = g->w[w].k0 | g->w[w].k1;
        if (taken == 0xffffffffu) continue; // (nfree > 0: a free bit below cls_n exists, the loop ends)
        unsigned b = (unsigned)__builtin_ctz(~taken);
        g->w[w].k0 |= (uint32_t)(kind & 1) << b, g->w[w].k1 |= (uint32_t)(kind >> 1) << b;
        if (born_marked(p)) g->w[w].mark |= 1u << b; // (the mark of a free block is clear)
        g->nfree--;
        char *block = lo + ((size_t)p << PAGE_SHIFT) + (size_t)(w * 32 + b) * cls_size[c];
        memset(block, 0, cls_size[c]);
        stats.used += cls_size[c], stats.allocated += cls_size[c], since += cls_size[c];
        return block;
    }
}

// Large blocks come from the top of the arena and small pages from its bottom, so the two do not cut each
// other's room into pieces.
static void *large(size_t size, int kind) {
    size_t n = (size + PAGE - 1) >> PAGE_SHIFT;
    uint32_t run = 0, p;
    if (n > free_pages || size + PAGE < size) return NULL;
    for (p = npages; p-- > 0;) {
        if (pi[p].state != P_FREE) run = 0;
        else if (++run == n) break;
    }
    if (run != n) return NULL;
    pi[p].state = P_LARGE, pi[p].kind = (uint8_t)kind, pi[p].mark = born_marked(p), pi[p].pin = 0, pi[p].span = (uint32_t)n;
    for (uint32_t i = 1; i < n; i++) pi[p + i].state = P_CONT, pi[p + i].span = i;
    free_pages -= (uint32_t)n;
    char *block = lo + ((size_t)p << PAGE_SHIFT);
    memset(block, 0, n << PAGE_SHIFT);
    stats.used += n << PAGE_SHIFT, stats.allocated += n << PAGE_SHIFT, since += n << PAGE_SHIFT;
    return block;
}

static void enter(long budget, bool whole);

static void *alloc(size_t size, int kind) {
    if (!on) {
        void *p = calloc(1, size ? size : 1);
        if (!p) die("out of memory (libc)", size);
        return p;
    }
    if (stress && ++stress_n >= stress) stress_n = 0, gc_collect();
    if (stress_slice && ++stress_slice_n >= stress_slice) stress_slice_n = 0, enter(slice_units, false);
    void *p = size <= SMALL_MAX ? small(size, kind) : large(size, kind);
    if (!p) { // the arena is full: the cycle is finished and a whole one run, here, however deep the stack is
        gc_collect();
        p = size <= SMALL_MAX ? small(size, kind) : large(size, kind);
        if (!p) die("out of memory", size);
    }
    return p;
}

void *gc_alloc(size_t size) { return alloc(size, K_SCAN); }
void *gc_alloc_atomic(size_t size) { return alloc(size, K_ATOMIC); }
void *gc_alloc_traced(size_t size) { return alloc(size, K_TRACED); }

char *gc_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    return memcpy(alloc(n, K_ATOMIC), s, n);
}

static void bad_free(void *p) {
    if (poison) die("free of a pointer that is not a live block's start", (size_t)(uintptr_t)p);
}

// (A block freed here may still be on the mark stack: drain() looks at what it takes from it.)
void gc_free(void *ptr) {
    if (!ptr) return;
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)lo;
    if (!on || off >= arena) {
        free(ptr);
        return;
    }
    if (busy) return;
    uint32_t p = (uint32_t)(off >> PAGE_SHIFT);
    PageInfo *g = &pi[p];
    if (g->state == P_LARGE && !(off & (PAGE - 1))) {
        uint32_t n = g->span;
        if (poison) memset(ptr, 0xDD, (size_t)n << PAGE_SHIFT);
        if (g->pin) stats.pinned -= (size_t)n << PAGE_SHIFT;
        for (uint32_t i = 0; i < n; i++) pi[p + i].state = P_FREE;
        g->mark = g->pin = 0;
        free_pages += n, stats.used -= (size_t)n << PAGE_SHIFT;
        if (p < free_cur) free_cur = p;
    } else if (g->state == P_SMALL) {
        unsigned c = g->cls, i = (unsigned)(((off & (PAGE - 1)) * cls_recip[c]) >> 24), w = i >> 5;
        uint32_t m = 1u << (i & 31);
        if ((off & (PAGE - 1)) != (size_t)i * cls_size[c] || !((g->w[w].k0 | g->w[w].k1) & m)) return bad_free(ptr);
        if (g->w[w].pin & m) stats.pinned -= cls_size[c];
        g->w[w].k0 &= ~m, g->w[w].k1 &= ~m, g->w[w].mark &= ~m, g->w[w].pin &= ~m;
        g->nfree++, stats.used -= cls_size[c];
        if (p < cls_cur[c]) cls_cur[c] = p;
        if (poison) memset(ptr, 0xDD, cls_size[c]);
    } else bad_free(ptr);
}

static void *re(void *ptr, size_t size, int kind) {
    if (!ptr) return alloc(size, kind);
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)lo;
    if (!on || off >= arena) {
        void *q = realloc(ptr, size ? size : 1);
        if (!q) die("out of memory (libc)", size);
        return q;
    }
    PageInfo *g = &pi[off >> PAGE_SHIFT];
    size_t room;
    if (g->state == P_LARGE) room = (size_t)g->span << PAGE_SHIFT, kind = g->kind;
    else if (g->state == P_SMALL) {
        unsigned i = (unsigned)(((off & (PAGE - 1)) * cls_recip[g->cls]) >> 24);
        room = cls_size[g->cls], kind = (g->w[i >> 5].k0 >> (i & 31) & 1) | (g->w[i >> 5].k1 >> (i & 31) & 1) << 1;
    } else die("realloc of a pointer that is not a block's start", size);
    if (size <= room && (room <= 64 || size > room / 4)) return ptr;
    void *q = alloc(size, kind ? kind : K_SCAN);
    memcpy(q, ptr, size < room ? size : room);
    gc_free(ptr);
    return q;
}
void *gc_realloc(void *p, size_t size) { return re(p, size, K_SCAN); }
void *gc_realloc_atomic(void *p, size_t size) { return re(p, size, K_ATOMIC); }

// ---------------------------------------------------------------- marking
static void push(uintptr_t block) {
    if (ms_n == ms_cap) {
        // ponytail: the mark stack is libc's and grows as needed (a word per block waiting, a few thousand in
        // the game); a fixed one with a rescan of the heap on overflow if libc's heap is ever too tight for it
        ms_cap = ms_cap ? ms_cap * 2 : 4096;
        if (!(ms = realloc(ms, ms_cap * sizeof *ms))) die("no memory for the mark stack", ms_cap * sizeof *ms);
    }
    ms[ms_n++] = block;
}

static inline __attribute__((always_inline)) void mark_word(uintptr_t a) {
    uintptr_t off = a - (uintptr_t)lo;
    if (off >= arena) return;
    uint32_t p = (uint32_t)(off >> PAGE_SHIFT);
    PageInfo *g = &pi[p];
    if (g->state == P_SMALL) {
        unsigned c = g->cls, i = (unsigned)(((uint32_t)(off & (PAGE - 1)) * cls_recip[c]) >> 24), w = i >> 5;
        uint32_t m = 1u << (i & 31);
        if ((g->w[w].mark | g->w[w].pin) & m) return;
        unsigned kind = (g->w[w].k0 & m ? 1 : 0) | (g->w[w].k1 & m ? 2 : 0);
        if (!kind) return; // a free block, or the unused end of the page (its bits are never set)
        g->w[w].mark |= m;
        if (kind != K_ATOMIC) push(((uintptr_t)lo + ((uintptr_t)p << PAGE_SHIFT) + (uintptr_t)i * cls_size[c]) | kind);
    } else if (g->state != P_FREE) {
        if (g->state == P_CONT) p -= g->span, g = &pi[p];
        if (g->mark | g->pin) return;
        g->mark = 1;
        if (g->kind != K_ATOMIC) push(((uintptr_t)lo + ((uintptr_t)p << PAGE_SHIFT)) | g->kind);
    }
}

void gc_mark(const void *p) {
    work -= MARK_UNITS;
    mark_word((uintptr_t)p);
}

bool gc_marked(const void *ptr) {
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)lo;
    if (!on || off >= arena) return true;
    uint32_t p = (uint32_t)(off >> PAGE_SHIFT);
    PageInfo *g = &pi[p];
    if (g->state == P_SMALL) {
        unsigned i = (unsigned)(((uint32_t)(off & (PAGE - 1)) * cls_recip[g->cls]) >> 24);
        return (g->w[i >> 5].mark | g->w[i >> 5].pin) >> (i & 31) & 1;
    }
    if (g->state == P_CONT) g -= g->span;
    return g->state == P_LARGE && (g->mark | g->pin);
}

void gc_pin(const void *ptr) {
    uintptr_t off = (uintptr_t)ptr - (uintptr_t)lo;
    if (!on || off >= arena) return;
    PageInfo *g = &pi[off >> PAGE_SHIFT];
    if (g->state == P_SMALL) {
        unsigned i = (unsigned)(((uint32_t)(off & (PAGE - 1)) * cls_recip[g->cls]) >> 24), w = i >> 5;
        uint32_t m = 1u << (i & 31);
        if (!((g->w[w].k0 | g->w[w].k1) & m) || g->w[w].pin & m) return;
        g->w[w].pin |= m, stats.pinned += cls_size[g->cls];
    } else if (g->state != P_FREE) {
        if (g->state == P_CONT) g -= g->span;
        if (!g->pin) g->pin = 1, stats.pinned += (size_t)g->span << PAGE_SHIFT;
    }
}

void gc_unpin_all(void) {
    for (uint32_t p = 0; p < npages; p++) {
        pi[p].pin = 0;
        for (int w = 0; w < 8; w++) pi[p].w[w].pin = 0;
    }
    stats.pinned = 0;
}

// Every aligned word of [start, end) as a possible pointer. Not for the sanitizer: the stack and the globals are
// read across its red zones.
__attribute__((no_sanitize_address)) static void scan(const char *start, const char *end) {
    const uintptr_t *p = (const uintptr_t *)(((uintptr_t)start + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
    const uintptr_t *e = (const uintptr_t *)((uintptr_t)end & ~(sizeof(uintptr_t) - 1));
    if (p < e) work -= e - p;
    for (; p < e; p++) mark_word(*p);
}

// The data and bss without the ranges of gc_skip
static void scan_data(const char *start, const char *end, int from) {
    for (int i = from; i < nskips; i++)
        if (skips[i].start < end && skips[i].end > start) {
            scan_data(start, skips[i].start, i + 1);
            scan_data(skips[i].end, end, i + 1);
            return;
        }
    scan(start, end);
}

// The blocks waiting, while the slice has work left (whole: all of them). A block may have been freed, and its
// place given to another, since it was put on the stack: it is taken only if a block of that kind starts there.
static void drain(bool whole) {
    while (ms_n && (whole || work > 0)) {
        uintptr_t b = ms[--ms_n];
        char *block = (char *)(b & ~(uintptr_t)3);
        uintptr_t off = (uintptr_t)(block - lo);
        PageInfo *g = &pi[off >> PAGE_SHIFT];
        size_t size;
        if (g->state == P_SMALL) {
            unsigned i = (unsigned)(((uint32_t)(off & (PAGE - 1)) * cls_recip[g->cls]) >> 24);
            size = cls_size[g->cls];
            if ((off & (PAGE - 1)) != (size_t)i * size || ((g->w[i >> 5].k0 >> (i & 31) & 1) | (g->w[i >> 5].k1 >> (i & 31) & 1) << 1) != (b & 3)) continue;
        } else if (g->state == P_LARGE && !(off & (PAGE - 1)) && g->kind == (b & 3)) size = (size_t)g->span << PAGE_SHIFT;
        else continue;
        work -= BLOCK_UNITS;
        if ((b & 3) == K_TRACED && trace_hook) cy.tables++, trace_hook(block);
        else cy.scanned++, scan(block, block + size);
    }
}

// The final step's second look at every marked scanned block: nothing told the collector what the program wrote
// in them since they were scanned.
static void rescan(void) {
    for (uint32_t p = 0; p < npages; p++) {
        PageInfo *g = &pi[p];
        if (g->state == P_SMALL) {
            unsigned c = g->cls, words = (cls_n[c] + 31u) >> 5;
            for (unsigned w = 0; w < words; w++)
                for (uint32_t bits = g->w[w].k1 & ~g->w[w].k0 & g->w[w].mark; bits; bits &= bits - 1) {
                    char *block = lo + ((size_t)p << PAGE_SHIFT) + (size_t)(w * 32 + (unsigned)__builtin_ctz(bits)) * cls_size[c];
                    cy.final_blocks++;
                    if (census_hook && stat) census_hook(block, cls_size[c]);
                    scan(block, block + cls_size[c]);
                }
        } else if (g->state == P_LARGE) {
            if (g->kind == K_SCAN && g->mark) {
                char *block = lo + ((size_t)p << PAGE_SHIFT);
                cy.final_blocks++;
                if (census_hook && stat) census_hook(block, (size_t)g->span << PAGE_SHIFT);
                scan(block, block + ((size_t)g->span << PAGE_SHIFT));
            }
            p += g->span - 1;
        }
    }
}

// ---------------------------------------------------------------- sweeping
// One page (a large block: all of its pages): what is neither marked nor pinned is free, and the marks are cleared
static void sweep_page(void) {
    uint32_t p = sweep_at++;
    PageInfo *g = &pi[p];
    if (g->state == P_SMALL) {
        unsigned c = g->cls, words = (cls_n[c] + 31u) >> 5;
        uint32_t any = 0;
        for (unsigned w = 0; w < words; w++) {
            uint32_t keep = g->w[w].mark | g->w[w].pin, dead = (g->w[w].k0 | g->w[w].k1) & ~keep;
            any |= keep;
            g->w[w].mark = 0;
            if (stat)
                for (int k = 1; k < 4; k++) {
                    unsigned n = (unsigned)__builtin_popcount((k & 1 ? g->w[w].k0 : ~g->w[w].k0) & (k & 2 ? g->w[w].k1 : ~g->w[w].k1) & keep);
                    cy.n[k] += n, cy.bytes[k] += (size_t)n * cls_size[c];
                }
            if (!dead) continue;
            g->w[w].k0 &= ~dead, g->w[w].k1 &= ~dead;
            unsigned n = (unsigned)__builtin_popcount(dead);
            g->nfree += n, stats.used -= (size_t)n * cls_size[c], cy.freed += (size_t)n * cls_size[c];
            if (poison)
                for (unsigned b = 0; b < 32; b++)
                    if (dead >> b & 1) memset(lo + ((size_t)p << PAGE_SHIFT) + (size_t)(w * 32 + b) * cls_size[c], 0xDD, cls_size[c]);
        }
        if (!any) { // an empty page serves any class, or a large block
            g->state = P_FREE, free_pages++;
            if (p < free_cur) free_cur = p;
        } else if (g->nfree && p < cls_cur[c]) cls_cur[c] = p;
    } else if (g->state == P_LARGE) {
        uint32_t n = g->span;
        if (g->mark | g->pin) {
            g->mark = 0;
            if (stat) cy.n[g->kind]++, cy.bytes[g->kind] += (size_t)n << PAGE_SHIFT;
        } else {
            if (poison) memset(lo + ((size_t)p << PAGE_SHIFT), 0xDD, (size_t)n << PAGE_SHIFT);
            for (uint32_t i = 0; i < n; i++) pi[p + i].state = P_FREE;
            free_pages += n, stats.used -= (size_t)n << PAGE_SHIFT, cy.freed += (size_t)n << PAGE_SHIFT;
            if (p < free_cur) free_cur = p;
        }
        sweep_at = p + n;
    }
}

// ---------------------------------------------------------------- a cycle, in slices
static double ms_of(clock_t t) { return (double)t * 1000.0 / CLOCKS_PER_SEC; }

static void gcstat(void) {
    printf("GCSTAT cycle %u: %ld slices of %ld units, worst %ld units %.3f ms | roots %ld u %.3f ms (hook %.3f) | mark %ld u %.3f ms: %ld tables, %ld scanned blocks"
           " | final %.3f ms: roots %ld u, %ld scanned blocks again %ld u, marked after %ld u | weak %ld u %.3f ms | sweep %ld u %.3f ms"
           " | live: atomic %lu (%lu KB) scanned %lu (%lu KB) traced %lu (%lu KB), pinned %lu KB, freed %lu KB\n",
           stats.collections, cy.slices, slice_units, cy.worst_u, ms_of(cy.worst_t), cy.roots_u, ms_of(cy.roots_t), ms_of(cy.hook_t), cy.mark_u, ms_of(cy.mark_t),
           cy.tables, cy.scanned, ms_of(cy.final_t), cy.final_roots_u, cy.final_blocks, cy.final_scan_u, cy.final_drain_u, cy.weak_u, ms_of(cy.weak_t), cy.sweep_u,
           ms_of(cy.sweep_t), (unsigned long)cy.n[1], (unsigned long)(cy.bytes[1] >> 10), (unsigned long)cy.n[2], (unsigned long)(cy.bytes[2] >> 10),
           (unsigned long)cy.n[3], (unsigned long)(cy.bytes[3] >> 10), (unsigned long)(stats.pinned >> 10), (unsigned long)(cy.freed >> 10));
}

// One slice: the cycle goes on (or starts) until `budget` units are spent; whole: to its end, whatever it takes.
__attribute__((noinline, no_sanitize_address)) static void slice(const char *stack_top, long budget, bool whole) {
    clock_t t0 = clock(), t = t0;
#define LAP(part) (cy.part += clock() - t, t = clock())
#define SPENT(part, expr) do { long w0_ = work; expr; cy.part += w0_ - work; } while (0)
    work = budget;
    long final_u = 0;
    clock_t final_t = 0;
    if (phase == IDLE) { // a cycle starts: every mark is clear
        memset(&cy, 0, sizeof cy);
        since = 0;
        if (roots_hook) roots_hook(); // first: it may pin and unpin
        LAP(hook_t);
        scan(stack_top, stack_bottom);
        phase = ROOTS, root_at = DATA_START, gc_barrier = true;
    }
    if (phase == ROOTS) {
        while ((whole || work > 0) && root_at < DATA_END) {
            const char *end = DATA_END - root_at > 8192 ? root_at + 8192 : DATA_END;
            SPENT(roots_u, scan_data(root_at, end, 0));
            root_at = end;
        }
        if (root_at >= DATA_END) {
            for (int i = 0; i < nroots; i++) SPENT(roots_u, scan(roots[i].start, roots[i].end));
            phase = MARK;
        }
        LAP(roots_t);
    }
    if (phase == MARK) {
        SPENT(mark_u, drain(whole));
        LAP(mark_t);
        // The final step, when nothing waits and this slice did little else (so that the slice is the final step
        // and not a slice's worth more): nothing of it is cut, the program must not run in its middle
        if (!ms_n && (whole || budget - work <= budget / 4)) {
            long w0 = work;
            SPENT(final_roots_u, (scan(stack_top, stack_bottom), scan_data(DATA_START, DATA_END, 0)));
            for (int i = 0; i < nroots; i++) SPENT(final_roots_u, scan(roots[i].start, roots[i].end));
            SPENT(final_scan_u, rescan());
            SPENT(final_drain_u, drain(true));
            final_u = w0 - work, final_t = clock() - t;
            LAP(final_t);
            phase = WEAK, gc_barrier = false, gc_reviving = true;
        }
    }
    if (phase == WEAK) {
        bool done = true;
        if (weak_hook) SPENT(weak_u, done = weak_hook(whole ? NULL : &work));
        if (done) phase = SWEEP, sweep_at = 0, gc_reviving = false;
        LAP(weak_t);
    }
    if (phase == SWEEP) {
        while ((whole || work > 0) && sweep_at < npages) sweep_page(), work -= SWEEP_UNITS, cy.sweep_u += SWEEP_UNITS;
        LAP(sweep_t);
    }
    // the slice's own account, then the cycle's if it ended here
    long used = budget - work;
    clock_t took = clock() - t0;
    cy.slices++;
    if (used > cy.worst_u) cy.worst_u = used;
    if (took > cy.worst_t) cy.worst_t = took;
    if (!whole) {
        if (ms_of(took) > stats.worst_ms) stats.worst_ms = ms_of(took);
        if (used > stats.worst_units) stats.worst_units = used;
    }
    if (final_u) {
        stats.final_ms = ms_of(final_t), stats.final_units = final_u;
        if (stats.final_ms > stats.worst_final_ms) stats.worst_final_ms = stats.final_ms;
    }
    if (phase == SWEEP && sweep_at >= npages) {
        phase = IDLE;
        free_after = arena - stats.used;
        stats.live = stats.used, stats.collections++;
        stats.last_ms = ms_of(cy.worst_t);
        if (stat) {
            gcstat();
            if (census_hook) census_hook(NULL, 0);
        }
    }
#undef LAP
#undef SPENT
}

// A frame with nothing of its own between enter()'s and the collector's working frames: the stack is scanned
// from here up, so what the marking leaves in its own frames (the last blocks it looked at) is never taken for
// a root by a later slice.
__attribute__((noinline)) static void run(long budget, bool whole) {
    char *volatile here = (char *)&here;
    busy = true;
    if (budget < 0) { // only the cycle in progress, to its end
        if (phase != IDLE) slice(here, 0, true);
    } else {
        if (whole && phase != IDLE) slice(here, 0, true); // the cycle in progress first, then a whole one
        slice(here, budget, whole);
    }
    busy = false;
    here = NULL; // (no tail call)
}

static void *volatile sink;
static void enter(long budget, bool whole) {
    if (!on || busy) return;
    // The registers a caller may keep a pointer in go to this frame, which run() scans: __builtin_unwind_init
    // saves every callee-saved one (setjmp alone would not do: glibc scrambles some of what it stores).
    jmp_buf regs;
    memset(regs, 0, sizeof regs); // (setjmp does not write all of it: what the stack had there would be taken for pointers)
    (void)setjmp(regs);
    __builtin_unwind_init();
    run(budget, whole);
    sink = (void *)((uintptr_t)regs ^ 1); // (the frame lives until run returns: no tail call)
}

void gc_collect(void) { enter(0, true); }

void gc_frame(void) {
    if (!on) return;
    // A cycle starts before the free room the last one left is half used, and then takes a slice of every frame:
    // here, where the stack is shallow and little that is dead is still referred to.
    size_t limit = free_after / 2 < step ? free_after / 2 : step;
    if (limit < arena / 64) limit = arena / 64;
    // A cycle that is still on when as much again was allocated is too slow for the program (slices too small for
    // what it allocates): it is finished at once, so that what is in use stays bounded.
    if (phase != IDLE && since >= limit) enter(-1, false);
    else if (phase != IDLE || since >= limit) enter(slice_units, false);
}
