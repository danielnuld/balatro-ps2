// A small conservative, non-moving mark-and-sweep collector: what stands in for Lua's garbage collector in the
// native port. Implemented in src/gc.c (plain C, no dependencies: it is tested alone by test/gc_test.c).
//
// One arena taken once at gc_init, cut in 4 KB pages. A page holds blocks of one size class (16 to 2048 bytes), or
// is part of a span of pages holding one large block, so any pointer into a block (interior ones included) finds
// the block's start, size, kind and mark bit through the page table. Nothing ever moves.
//
// What is alive: whatever can be reached from the roots, which are the C stack (from the stack pointer to the
// bottom given to gc_init, the registers included), the program's data and bss, and the ranges of gc_add_root.
// A word that holds the address of any byte of a block keeps that block.
//
// Off (no arena: every function below is libc's calloc/realloc/free) until gc_init, and for good with -DNO_GC or,
// on the host, GC_OFF in the environment.
#ifndef GC_H
#define GC_H
#include <stdbool.h>
#include <stddef.h>

// A collection is done in slices, one per frame (gc_frame), each bounded by a count of work units; see gc.c.
//
// stack_bottom: an address in the frame of main (the stack is scanned up to it). Host environment: GC_HEAP (the
// arena in MB, default 64), GC_STEP_KB (what is allocated between two collections, default a quarter of the
// arena), GC_SLICE (the work units of a slice), GC_STRESS=n (a whole collection every n allocations),
// GC_STRESS_SLICE=n (a slice every n allocations, a collection always in progress), GC_POISON (freed blocks are
// filled with 0xDD; the two stress modes do it too), GC_STAT (a GCSTAT line per collection), GC_OFF.
// PS2: -DGC_HEAP_EE=<MB>, -DGC_SLICE=<units>, -DGC_STAT.
void gc_init(void *stack_bottom);
void *gc_alloc(size_t size);        // zeroed; scanned word by word: any word may be a pointer
void *gc_alloc_atomic(size_t size); // zeroed; never scanned: strings, numbers, pixels
void *gc_alloc_traced(size_t size); // zeroed; its pointers are reported by the tracer of gc_set_hooks
// A block of at least size bytes with p's contents and p's kind; p NULL: a new one (scanned / atomic). The new
// bytes are not promised to be zero. A p that is not the collector's is given to libc's realloc.
void *gc_realloc(void *p, size_t size);
void *gc_realloc_atomic(void *p, size_t size);
char *gc_strdup(const char *s);     // an atomic copy
// Frees a block now (optional: the collector would find it). A pointer that is not the collector's goes to
// libc's free, so code that frees what the platform gave it and what it allocated itself needs no telling apart.
void gc_free(void *p);
void gc_add_root(void *start, void *end); // [start, end) is scanned at every collection (memory of libc that holds pointers to blocks)
void gc_collect(void);              // now: the collection in progress is finished and a whole one is done
// Once a frame, where the stack is shallow: a slice of the collection in progress; one starts when enough was
// allocated since the last.
void gc_frame(void);

// roots(): called when a collection starts, to gc_mark what only the caller knows to be alive (and to pin).
// trace(block): called for each reachable block of gc_alloc_traced; it calls gc_mark on every pointer the block
// holds (gc_mark takes anything: NULL, pointers that are not the collector's, interior pointers).
// weak(work): called after everything reachable is marked and before the rest is freed: tables that must not keep
// their entries alive (they live outside the scanned world) drop the entries for which gc_marked is false. In
// pieces: it takes a unit from *work for each entry it looks at, returns false when *work is used up (it is called
// again in a later slice and goes on where it was) and true when it is done; work NULL: all of it now. The program
// runs between the pieces: while gc_reviving is set, whoever takes an entry out of such a table for a new use must
// gc_mark it (it may be one the collection did not reach and has not dropped yet).
// None may allocate.
// While gc_barrier is set (a collection is marking), the owner of traced blocks must gc_mark every pointer it
// stores into one, at the store: a block already traced is not traced again.
extern bool gc_barrier, gc_reviving;
void gc_set_hooks(void (*roots)(void), void (*trace)(void *block), bool (*weak)(long *work));
// census(block, size): with GC_STAT, called for every live scanned block once per collection (to count them by what they are)
void gc_set_census(void (*census)(void *block, size_t size));
void gc_mark(const void *p);
bool gc_marked(const void *p);      // inside weak(): true for a reachable or pinned block, and for what is not a block at all
// A pinned block is never freed by a collection and never looked into: marking stops at it, so whatever it refers
// to must be pinned too, or be marked by roots() at every collection. For the big tables of static data, which
// would otherwise be walked again at each collection (core.c: tfreeze). gc_free still frees one.
void gc_pin(const void *p);
void gc_unpin_all(void);
// A part of the program's data or bss that holds no pointer to a block (a big buffer of pixels or numbers): not scanned.
void gc_skip(const void *start, const void *end);

typedef struct GcStats {
    size_t heap;          // bytes of the arena (0: the collector is off)
    size_t used;          // bytes in blocks now
    size_t live;          // bytes in blocks after the last collection
    size_t pinned;        // bytes in pinned blocks (part of both)
    unsigned long long allocated; // bytes ever allocated
    unsigned collections;
    double last_ms, worst_ms; // the longest slice of the last collection, and of all (gc_frame's; not the whole collections of gc_collect)
    long slice, worst_units;  // the work units of a slice, and the most one took (the final step is not cut)
    double final_ms, worst_final_ms; // the final step of the last collection, and the longest
    long final_units;
} GcStats;
const GcStats *gc_stats(void);
#endif
