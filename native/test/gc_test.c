// The tests of src/gc.c, alone (no game, no core.c):
//   gcc -std=gnu11 -O1 -g -Wall -Iinclude src/gc.c test/gc_test.c -o /tmp/gc_test && GC_HEAP=8 GC_STRESS=1 /tmp/gc_test
// test/gc_test.sh runs it in several settings. GC_STRESS must be set: a block freed by mistake is then filled
// with 0xDD, which the checks below see.
#include "gc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const int sizes[] = {1, 16, 17, 32, 48, 64, 80, 96, 100, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 1024, 1360, 2048, 2049, 4096, 5000, 70000};
#define NSIZES ((int)(sizeof sizes / sizeof *sizes))

// Blocks whose only reference is kept in memory the collector does not scan
static void **hidden; // libc's
static void *volatile *keep; // a root: registered below

static bool all(const unsigned char *p, int n, int v) {
    for (int i = 0; i < n; i++)
        if (p[i] != v) return false;
    return true;
}

// Called through a pointer so that nothing of its frame is left in the caller's registers
__attribute__((noinline)) static void make_classes(void) {
    for (int i = 0; i < NSIZES; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char *p = j & 1 ? gc_alloc_atomic(sizes[i]) : gc_alloc(sizes[i]);
            CHECK(all(p, sizes[i], 0)); // zeroed
            CHECK(((uintptr_t)p & 15) == 0);
            memset(p, 0x40 + i, sizes[i]);
            hidden[i * 4 + j] = p;
            // j 0, 1: kept by a pointer to the block's last byte (an interior pointer); 2, 3: by nothing
            keep[i * 4 + j] = j < 2 ? p + sizes[i] - 1 : NULL;
        }
}
__attribute__((noinline)) static void clobber_stack(void) {
    volatile char junk[4096];
    for (unsigned i = 0; i < sizeof junk; i++) junk[i] = 0; // (a memset of it would be dropped: nothing reads it)
}
static void (*volatile call)(void);

static void test_classes(void) {
    call = make_classes, call();
    call = clobber_stack, call();
    gc_collect();
    for (int i = 0; i < NSIZES; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char *p = hidden[i * 4 + j];
            if (j < 2) CHECK(all(p, sizes[i], 0x40 + i)); // alive, untouched
            // freed (stress fills). Not checked when collections come in the middle of make_classes: the block
            // was freed then and may already be another one
            else if (atoi(getenv("GC_STRESS")) >= 100000) CHECK(all(p, sizes[i], 0xDD));
            keep[i * 4 + j] = NULL;
        }
    // every byte of a block finds it: one block per size, each offset in turn the only reference
    for (int i = 0; i < NSIZES - 1; i++) {
        unsigned char *p = gc_alloc_atomic(sizes[i]);
        hidden[0] = p;
        for (int at = 0; at < sizes[i]; at += (sizes[i] > 5000 ? 997 : 1)) {
            memset(p, 0x77, sizes[i]);
            keep[0] = p + at, p = NULL;
            gc_collect();
            p = hidden[0];
            if (!all(p, sizes[i], 0x77)) { CHECK(!"interior pointer lost its block"); break; }
        }
        keep[0] = NULL;
    }
}

// Two pages of blocks of each small size, each block kept by a pointer to its last byte only: every block index of
// a page is found from an interior pointer
static void test_pages(void) {
    void **starts = calloc(512, sizeof *starts);          // libc's: not scanned
    void *volatile *lasts = calloc(512, sizeof *lasts);   // a root
    gc_add_root((void *)lasts, (void *)(lasts + 512));
    for (int i = 0; i < NSIZES && sizes[i] <= 2048; i++) {
        if (sizes[i] < 16) continue;
        int n = 2 * 4096 / sizes[i];
        for (int k = 0; k < n; k++) {
            unsigned char *p = gc_alloc_atomic(sizes[i]);
            memset(p, k & 0x7f, sizes[i]);
            starts[k] = p, lasts[k] = p + sizes[i] - 1;
        }
        gc_collect();
        for (int k = 0; k < n; k++)
            if (!all(starts[k], sizes[i], k & 0x7f)) { CHECK(!"a block kept by its last byte was freed"); break; }
        for (int k = 0; k < n; k++) lasts[k] = NULL;
    }
}

// The stack, a static and a scanned block as roots; an atomic block is not one
static void *static_root;
typedef struct Node { struct Node *next; long id; } Node;
static long chain = 100000; // CHAIN in the environment: fewer when every allocation collects

__attribute__((noinline)) static void make_chain(void) {
    Node *head = NULL;
    for (long i = 0; i < chain; i++) { // deep: marking must not recurse
        Node *n = gc_alloc(sizeof *n);
        n->next = head, n->id = i, head = n;
    }
    static_root = head;
    Node *a = gc_alloc(sizeof *a);
    void **atomic = gc_alloc_atomic(sizeof(void *));
    *atomic = a; // the only reference to a: in a block that is not scanned
    a->id = 12345;
    hidden[0] = a, keep[0] = atomic;
}

// (in a function of its own: the registers test_roots would walk the chain with are roots too)
__attribute__((noinline)) static void check_chain(void) {
    long n = 0;
    for (Node *p = static_root; p; p = p->next, n++) CHECK(p->id == chain - 1 - n);
    CHECK(n == chain);
    CHECK(all(hidden[0], sizeof(Node), 0xDD)); // behind an atomic block: collected
    static_root = NULL, keep[0] = NULL;
}

static void test_roots(void) {
    char *volatile on_stack = gc_strdup("on the stack");
    call = make_chain, call();
    call = clobber_stack, call();
    gc_collect();
    CHECK(!strcmp(on_stack, "on the stack"));
    size_t before = gc_stats()->used;
    call = check_chain, call();
    call = clobber_stack, call();
    gc_collect();
    if (before < gc_stats()->used + chain * sizeof(Node)) { // the chain went
        printf("used %lu before the chain was dropped, %lu after\n", (unsigned long)before, (unsigned long)gc_stats()->used);
        CHECK(!"the chain was not collected");
    }
}

// A traced kind and a weak table, as core.c's tables and its interned strings
typedef struct { int n; void *slot[4]; long not_a_pointer; } Traced;
static void trace(void *block) {
    Traced *t = block;
    for (int i = 0; i < t->n; i++) gc_mark(t->slot[i]);
}
static char **weak_set; // libc's: not scanned
static int weak_n;
static bool weak(long *work) {
    int k = 0;
    for (int i = 0; i < weak_n; i++)
        if (gc_marked(weak_set[i])) weak_set[k++] = weak_set[i];
    weak_n = k;
    return true;
}
__attribute__((noinline)) static void make_traced(void) {
    Traced *t = gc_alloc_traced(sizeof *t);
    weak_set = malloc(100 * sizeof *weak_set);
    for (int i = 0; i < 100; i++) {
        char text[20];
        snprintf(text, sizeof text, "string %d", i);
        keep[2 + i] = weak_set[weak_n] = gc_strdup(text); // (kept while the set is built: a collection may come at any allocation)
        weak_n++;
    }
    t->n = 2, t->slot[0] = weak_set[10] + 3, t->slot[1] = weak_set[20];
    t->slot[2] = weak_set[30]; // beyond n: the tracer does not report it
    t->not_a_pointer = (long)(uintptr_t)weak_set[40]; // a traced block is not scanned
    keep[0] = t;
    keep[1] = weak_set[50];
    for (int i = 0; i < 100; i++) keep[2 + i] = NULL;
}
static void test_traced_weak(void) {
    gc_set_hooks(NULL, trace, weak);
    call = make_traced, call();
    call = clobber_stack, call();
    gc_collect();
    CHECK(weak_n == 3);
    CHECK(weak_n == 3 && !strcmp(weak_set[0], "string 10") && !strcmp(weak_set[1], "string 20") && !strcmp(weak_set[2], "string 50"));
    keep[0] = keep[1] = NULL;
    gc_collect();
    CHECK(weak_n == 0);
    CHECK(gc_marked("not a block") && gc_marked(NULL));
    gc_set_hooks(NULL, NULL, NULL);
}

// Pinned blocks stay without a reference and are not looked into; the roots hook marks for them
static uintptr_t hide(void *p) { return ~(uintptr_t)p; }
static uintptr_t h_small, h_large, h_child, h_kept;
static void pin_roots(void) { gc_mark((void *)~h_kept); }
__attribute__((noinline)) static void make_pinned(void) {
    void **s = gc_alloc(2 * sizeof(void *)), **l = gc_alloc(10000);
    keep[0] = s, keep[1] = l;
    s[0] = gc_strdup("child of a pinned block"), s[1] = gc_strdup("marked by the roots hook");
    h_small = hide(s), h_large = hide((char *)l + 5000), h_child = hide(s[0]), h_kept = hide(s[1]);
    memset(l, 0x55, 10000);
    gc_pin(s), gc_pin((char *)l + 5000); // (an interior pointer pins the block)
    keep[0] = keep[1] = NULL;
}
static size_t pinned_before;
__attribute__((noinline)) static void check_pinned(void) { // (apart: its registers must not keep the blocks for the last check)
    void **s = (void **)~h_small;
    unsigned char *l = (unsigned char *)~h_large - 5000;
    CHECK(s[0] == (void *)~h_child && all(l, 10000, 0x55));           // the pinned blocks are whole
    CHECK(all((void *)~h_child, 8, 0xDD));                            // what only a pinned block refers to is not kept
    CHECK(!strcmp((char *)~h_kept, "marked by the roots hook"));      // unless the roots hook marks it
    CHECK(gc_marked(s));
    gc_free(l); // a pinned block can be freed by hand
    CHECK(gc_stats()->pinned < pinned_before);
}
static void test_pin(void) {
    gc_set_hooks(pin_roots, NULL, NULL);
    call = make_pinned, call();
    call = clobber_stack, call();
    pinned_before = gc_stats()->pinned;
    CHECK(pinned_before >= 10000 + 2 * sizeof(void *));
    gc_collect();
    call = check_pinned, call();
    gc_unpin_all();
    CHECK(gc_stats()->pinned == 0);
    call = clobber_stack, call();
    gc_collect();
    CHECK(all((void *)~h_small, 2 * sizeof(void *), 0xDD));
    gc_set_hooks(NULL, NULL, NULL);
}

static void test_realloc_free(void) {
    char *volatile p = NULL;
    size_t n = 0;
    for (int round = 0; round < 40; round++) { // grows through every class into large blocks
        size_t m = n + 1 + n / 2;
        p = gc_realloc_atomic(p, m);
        for (size_t i = 0; i < n; i++)
            if (p[i] != (char)(i * 7)) { CHECK(!"realloc lost the contents"); break; }
        for (size_t i = n; i < m; i++) p[i] = (char)(i * 7);
        n = m;
        if (n > 300000) break;
    }
    p = gc_realloc_atomic(p, 10); // shrinks
    for (size_t i = 0; i < 10; i++) CHECK(p[i] == (char)(i * 7));
    size_t used = gc_stats()->used;
    gc_free(p);
    CHECK(gc_stats()->used < used);
    void *big = gc_alloc(100000);
    used = gc_stats()->used;
    gc_free(big);
    CHECK(used - gc_stats()->used >= 100000);
    void *libc = malloc(100);
    libc = gc_realloc(libc, 200); // not the collector's: libc's realloc and free
    gc_free(libc);
    gc_free(NULL);
    // a scanned block keeps its kind through realloc: what it points to stays alive
    void **volatile arr = gc_alloc(2 * sizeof(void *));
    arr[0] = gc_strdup("kept through realloc");
    arr = gc_realloc(arr, 3000 * sizeof(void *));
    arr[2999] = gc_strdup("the last one");
    gc_collect();
    CHECK(!strcmp(arr[0], "kept through realloc") && !strcmp(arr[2999], "the last one"));
}

// A random graph changed at random against a model of it: every node reachable from the table of roots carries
// its number and a check of it, so a node freed too early (0xDD) or handed out twice shows.
typedef struct G { uint32_t id, check; int depth; struct G *kid[2]; char pad[]; } G;
#define SLOTS 256
static G *volatile *slots; // a registered root (libc's memory)
static uint32_t rnd_state = 12345, next_id = 1;
static uint32_t rnd(void) { return rnd_state = rnd_state * 1664525u + 1013904223u; }
static long visited;
static void visit(G *g, int depth) {
    if (!g || depth > 12) return;
    visited++;
    if (g->check != (g->id ^ 0xABCD1234u) || g->id == 0 || g->id >= next_id) {
        if (fails < 10) CHECK(!"stress: a reachable node is damaged");
        else fails++;
        return;
    }
    for (int i = 0; i < 2; i++) visit(g->kid[i], depth + 1);
}
static void test_stress(int rounds) {
    for (int r = 0; r < rounds; r++) {
        uint32_t x = rnd();
        size_t extra = (x >> 8) % 100 < 98 ? (x >> 16) % 200 : (x >> 16) % 9000; // mostly small, some large
        G *g = gc_alloc(sizeof *g + extra);
        g->id = next_id++, g->check = g->id ^ 0xABCD1234u;
        memset(g->pad, 0, extra);
        for (int i = 0; i < 2; i++) { // shares subgraphs; the depth is bounded, so what is alive is too
            G *kid = rnd() % 3 ? slots[rnd() % SLOTS] : NULL;
            if (kid && kid->depth >= 4) kid = NULL;
            if (kid && kid->depth >= g->depth) g->depth = kid->depth + 1;
            g->kid[i] = kid;
        }
        if (rnd() % 4 == 0) gc_alloc_atomic(rnd() % 3000); // garbage at once
        slots[rnd() % SLOTS] = g;
        if (rnd() % 16 == 0) slots[rnd() % SLOTS] = NULL;
        if (r % 5000 == 0) {
            gc_frame();
            for (int i = 0; i < SLOTS; i++) visit(slots[i], 0);
        }
    }
    for (int i = 0; i < SLOTS; i++) visit(slots[i], 0);
    for (int i = 0; i < SLOTS; i++) slots[i] = NULL;
}

__attribute__((noinline)) static int run(void) {
    if (!gc_stats()->heap) { printf("gc_test: the collector is off\n"); return 1; }
    if (!getenv("GC_STRESS")) { printf("gc_test: set GC_STRESS (freed blocks must be filled)\n"); return 1; }
    hidden = calloc(NSIZES * 4, sizeof *hidden);
    keep = calloc(NSIZES * 4, sizeof *keep);
    gc_add_root((void *)keep, (void *)(keep + NSIZES * 4));
    slots = calloc(SLOTS, sizeof *slots);
    gc_add_root((void *)slots, (void *)(slots + SLOTS));
    if (getenv("CHAIN")) chain = atol(getenv("CHAIN"));
    test_classes();
    test_pages();
    test_roots();
    test_traced_weak();
    if (atoi(getenv("GC_STRESS")) >= 100000) test_pin(); // (it checks what was freed: not with collections in its middle)
    test_realloc_free();
    int rounds = getenv("ROUNDS") ? atoi(getenv("ROUNDS")) : 200000;
    test_stress(rounds);
    gc_collect();
    const GcStats *s = gc_stats();
    printf("gc_test: %s  (%u collections, %lu KB used of %lu, %llu KB allocated, worst pause %.2f ms, %ld visits)\n", fails ? "FAILED" : "ok",
           s->collections, (unsigned long)(s->used >> 10), (unsigned long)(s->heap >> 10), s->allocated >> 10, s->worst_ms, visited);
    return fails != 0;
}

int main(void) {
    void *bottom;
    gc_init(&bottom);
    return run();
}
