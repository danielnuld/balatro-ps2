// Lua's values, tables and strings as C, and the object model: the implementation of core.h.
#define CORE_NO_KEY_MACROS // this file defines the functions the macros stand in front of
#include "core.h"
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>

const Val NIL = {.t = V_NIL};

// ---------------------------------------------------------------- interned strings
// Every string the port keeps is interned: equal contents share one pointer, so table keys compare by pointer.
// The hash is stored in front of the characters so a table lookup does not hash twice.
typedef struct IStr {
    uint32_t hash;
    uint32_t len;
    char s[];
} IStr;

// The set is weak: it is libc's memory, which the collector does not scan, so a string lives only while a table,
// an object, the stack or a static refers to it (a pointer to its characters is enough), and intern_weak drops the
// others from the set before they are freed.
static IStr **iset; // open addressing, linear probing, NULL = empty
static uint32_t icap, icount;
static uint32_t weak_at; // intern_weak: the slots before it are done

#define FNV_INIT 2166136261u
#define FNV_STEP(h, c) (((h) ^ (uint8_t)(c)) * 16777619u)

static uint32_t hash_bytes(const char *s, size_t len) {
    uint32_t h = FNV_INIT;
    for (size_t i = 0; i < len; i++) h = FNV_STEP(h, s[i]);
    return h;
}

static IStr *intern_find(const char *s, size_t len, uint32_t h) {
    if (!icap) return NULL;
    for (uint32_t i = h & (icap - 1);; i = (i + 1) & (icap - 1)) {
        IStr *e = iset[i];
        if (!e) return NULL;
        if (e->hash == h && e->len == len && memcmp(e->s, s, len) == 0) return e;
    }
}

static void intern_grow(void) {
    uint32_t ncap = icap ? icap * 2 : 1024;
    IStr **nset = calloc(ncap, sizeof *nset);
    if (!nset) abort();
    for (uint32_t k = 0; k < icap; k++) {
        IStr *e = iset[k];
        if (!e) continue;
        uint32_t i = e->hash & (ncap - 1);
        while (nset[i]) i = (i + 1) & (ncap - 1);
        nset[i] = e;
    }
    free(iset);
    iset = nset, icap = ncap;
    weak_at = 0; // (every entry is somewhere else: intern_weak, if it is on its way, starts over)
}

// Before a collection frees what it did not reach: the strings nothing refers to leave the set. An entry is
// taken out by moving back the ones of its run that it was in the way of (no tombstones). In pieces (gc.h): the
// slots before weak_at are done; what intern() adds or moves meanwhile is marked, so it may land anywhere.
static bool intern_weak(long *work) {
    uint32_t mask = icap - 1;
    for (; weak_at < icap; weak_at++) {
        if (work) { // (10: a slot is a jump to a string's mark, in gc.c's units; at least a slot per call)
            if (*work <= 0) return false;
            *work -= 10;
        }
        uint32_t i = weak_at;
        while (iset[i] && !gc_marked(iset[i])) {
            uint32_t hole = i;
            for (uint32_t j = (hole + 1) & mask; iset[j]; j = (j + 1) & mask) {
                uint32_t home = iset[j]->hash & mask;
                // it stays where it is if its own slot is after the hole, up to where it is (around the end)
                if (hole <= j ? (hole < home && home <= j) : (hole < home || home <= j)) continue;
                iset[hole] = iset[j], hole = j;
            }
            iset[hole] = NULL;
            icount--;
        }
    }
    weak_at = 0;
    return true;
}

static void table_roots(void);
static void table_trace(void *block);
static bool gc_hooked;
static inline void gc_hook(void) {
    if (!gc_hooked) gc_set_hooks(table_roots, table_trace, intern_weak), gc_hooked = true;
}

static IStr *intern(const char *s, size_t len) {
    uint32_t h = hash_bytes(s, len);
    gc_hook();
    IStr *e = intern_find(s, len, h);
    if (e) {
        if (gc_reviving) gc_mark(e); // (found before intern_weak came to drop it: it is in use again)
        return e;
    }
    e = gc_alloc_atomic(sizeof *e + len + 1); // (may collect: the set is looked at again below, not before)
    e->hash = h, e->len = (uint32_t)len;
    memcpy(e->s, s, len);
    e->s[len] = 0;
    if ((icount + 1) * 4 > icap * 3) intern_grow();
    uint32_t i = h & (icap - 1);
    while (iset[i]) i = (i + 1) & (icap - 1);
    iset[i] = e;
    icount++;
    return e;
}

// The interned entry of a C string without creating it: NULL means no table can have it as a key.
static IStr *intern_lookup(const char *s) {
    uint32_t h = FNV_INIT;
    size_t len = 0;
    for (; s[len]; len++) h = FNV_STEP(h, s[len]);
    return intern_find(s, len, h);
}

const char *str(const char *s) { return s ? intern(s, strlen(s))->s : NULL; }


const char *fmt(const char *format, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    if (n < 0) ERROR("fmt: bad format '%s'", format);
    if (n < (int)sizeof buf) return intern(buf, (size_t)n)->s;
    char *big = malloc((size_t)n + 1);
    if (!big) abort();
    va_start(ap, format);
    vsnprintf(big, (size_t)n + 1, format, ap);
    va_end(ap);
    const char *r = intern(big, (size_t)n)->s;
    free(big);
    return r;
}

static const char *need_str(const char *s, const char *fname) {
    if (!s) ERROR("bad argument #1 to '%s' (string expected, got nil)", fname);
    return s;
}

int str_len(const char *s) { return (int)strlen(need_str(s, "len")); }

// lstrlib.c posrelat
static int posrelat(int pos, int len) {
    if (pos < 0) pos += len + 1;
    return pos >= 0 ? pos : 0;
}

const char *str_sub(const char *s, int i, int j) {
    int l = (int)strlen(need_str(s, "sub"));
    int start = posrelat(i, l), end = posrelat(j, l);
    if (start < 1) start = 1;
    if (end > l) end = l;
    return start <= end ? intern(s + start - 1, (size_t)(end - start + 1))->s : intern("", 0)->s;
}

int str_find(const char *s, const char *plain, int init) {
    int l1 = (int)strlen(need_str(s, "find"));
    if (!plain) ERROR("bad argument #2 to 'find' (string expected, got nil)");
    init = posrelat(init, l1) - 1;
    if (init < 0) init = 0;
    else if (init > l1) init = l1;
    const char *hit = strstr(s + init, plain);
    return hit ? (int)(hit - s) + 1 : 0;
}

static const char *str_map(const char *s, int (*f)(int), const char *fname) {
    size_t l = strlen(need_str(s, fname));
    char *tmp = malloc(l + 1);
    if (!tmp) abort();
    for (size_t i = 0; i < l; i++) tmp[i] = (char)f((unsigned char)s[i]);
    const char *r = intern(tmp, l)->s;
    free(tmp);
    return r;
}
const char *str_upper(const char *s) { return str_map(s, toupper, "upper"); }
const char *str_lower(const char *s) { return str_map(s, tolower, "lower"); }

const char *str_rep(const char *s, int n) {
    size_t l = strlen(need_str(s, "rep"));
    if (n <= 0 || l == 0) return intern("", 0)->s;
    char *tmp = malloc(l * (size_t)n);
    if (!tmp) abort();
    for (int i = 0; i < n; i++) memcpy(tmp + l * (size_t)i, s, l);
    const char *r = intern(tmp, l * (size_t)n)->s;
    free(tmp);
    return r;
}

// ---------------------------------------------------------------- values
Val S(const char *s) {
    if (!s) return NIL;
    Val v;
    v.t = V_STR, v.s = intern(s, strlen(s))->s;
    return v;
}
Val T(Table *t) {
    if (!t) return NIL;
    Val v;
    v.t = V_TAB, v.tab = t;
    return v;
}
Val O(void *obj) {
    if (!obj) return NIL;
    Val v;
    v.t = V_OBJ, v.obj = obj;
    return v;
}
Val P(void *p) {
    if (!p) return NIL;
    Val v;
    v.t = V_PTR, v.p = p;
    return v;
}
Val FN(FnPtr f, void *ctx) {
    if (!f) return NIL;
    Fn *fn = gc_alloc(sizeof *fn);
    fn->f = f, fn->ctx = ctx;
    Val v;
    v.t = V_FN, v.fn = fn;
    return v;
}

bool veq(Val a, Val b) {
    if (a.t != b.t) return false;
    switch (a.t) {
    case V_NIL: return true;
    case V_BOOL: return a.b == b.b;
    case V_NUM: return a.n == b.n;
    case V_STR: return a.s == b.s || strcmp(a.s, b.s) == 0; // a Val built by hand may not be interned
    default: return a.p == b.p;
    }
}

Val fcall(Val fn, int argc, Val *argv) { return fn.t == V_FN ? fn.fn->f(fn.fn->ctx, argc, argv) : NIL; }

const char *lua_typename_of(Val v) {
    switch (v.t) {
    case V_NIL: return "nil";
    case V_BOOL: return "boolean";
    case V_NUM: return "number";
    case V_STR: return "string";
    case V_TAB: return "table";
    case V_OBJ: return "table"; // note: the game's objects are Lua tables, and its type() checks rely on that
    case V_FN: return "function";
    default: return "userdata";
    }
}

const char *number_tostring(num n) { return fmt("%.14g", n); }

// lobject.c luaO_str2d
static bool str2d(const char *s, num *result) {
    char *endptr;
    *result = strtod(s, &endptr);
    if (endptr == s) return false;
    if (*endptr == 'x' || *endptr == 'X') *result = (num)strtoul(s, &endptr, 16);
    if (*endptr == '\0') return true;
    while (isspace((unsigned char)*endptr)) endptr++;
    return *endptr == '\0';
}

num tonumber(Val v, bool *ok) {
    num n = 0;
    bool good = v.t == V_NUM || (v.t == V_STR && str2d(v.s, &n));
    if (v.t == V_NUM) n = v.n;
    if (ok) *ok = good;
    return good ? n : 0;
}

num vnum(Val v) { return tonumber(v, NULL); }

const char *vstr(Val v) { return v.t == V_STR ? v.s : v.t == V_NUM ? number_tostring(v.n) : NULL; }

const char *tostring(Val v) {
    switch (v.t) {
    case V_NIL: return "nil";
    case V_BOOL: return v.b ? "true" : "false";
    case V_NUM: return number_tostring(v.n);
    case V_STR: return v.s;
    case V_TAB:
    case V_OBJ: return fmt("table: %p", v.p);
    case V_FN: return fmt("function: %p", v.p);
    default: return fmt("userdata: %p", v.p);
    }
}

// ---------------------------------------------------------------- tables
// arr[0..n-1] is the array part (keys 1..n; a hole left by t[i] = nil in the middle stays as NIL, arr[n-1] never is).
// Every other key is an entry of ents[], appended in insertion order so iteration is deterministic; idx[] is the
// open-addressing index over it. A removed entry stays as a tombstone (key NIL) until the next rebuild, which is what
// makes t[k] = nil safe inside an iteration.
typedef struct {
    Val key, val;
} Entry;

struct Table {
    uint64_t stamp; // FIRST (core.h's tstamp reads it there): see tstamp
    Val *arr;
    int n, acap;
    Entry *ents;
    int32_t *idx; // 2 * ecap slots: an entry index, or -1
    int nents, ecap; // entries used (tombstones included), capacity
    int live;        // entries that are not tombstones
    int nnum;        // live entries with a number key: zero lets integer lookups skip the hash
    uint16_t old;    // == freeze_gen: frozen (see tfreeze)
    uint8_t dirty;   // frozen and in the dirty list
};

// A table is a traced block (table_trace reports what its values point to) and its three arrays are atomic
// blocks nobody else refers to: the collector does not read numbers and booleans as if they were pointers.
static uint64_t stamp_now; // the last stamp given: none is given twice (2^64 writes)
#define STAMP(t) ((t)->stamp = ++stamp_now)
// writing v over o changes something (not when it is the same value, bit for bit)
static inline bool val_differs(Val o, Val v) {
    return o.t != v.t || (v.t == V_NUM ? memcmp(&o.n, &v.n, sizeof v.n) != 0 : v.t == V_BOOL ? o.b != v.b : v.t != V_NIL && o.p != v.p);
}
Table *tnew(void) {
    gc_hook();
    Table *t = gc_alloc_traced(sizeof(Table));
    STAMP(t);
    return t;
}

// Only these kinds of value carry a pointer (a string's is to its characters, inside its block)
static inline void mark_val(Val v) {
    if (v.t >= V_STR) gc_mark(v.p);
}
static void table_trace(void *block) {
    Table *t = block;
    gc_mark(t->arr), gc_mark(t->ents), gc_mark(t->idx);
    for (int i = 0; i < t->n; i++) mark_val(t->arr[i]);
    for (int e = 0; e < t->nents; e++) mark_val(t->ents[e].key), mark_val(t->ents[e].val);
}

// ---- frozen tables
// The static data (game.dat, the localization) is thousands of tables that never die and hardly change: walking
// them at every collection was most of its pause. A table whose slot is given to tfreeze is frozen at the next
// collection, with every table and string it leads to: their blocks are pinned (gc.h), the collector neither
// frees them nor looks into them. A frozen table that is written a value that may be a pointer, or whose arrays
// are replaced, or that refers to something that is neither a table nor a string, goes to the dirty list, which
// is walked at every collection as the roots are: what it refers to stays alive. When a slot holds another
// table than the one frozen (the language was changed), or the dirty list has grown long, everything is unpinned
// and frozen again from the slots, and what only the old table led to is collected.
#define FROZEN(t) ((t)->old == freeze_gen)
static uint16_t freeze_gen = 1;
static struct { Table **slot; Table *was; } frozen[4];
static int nfrozen;
static Table **dirty; // libc's memory: its tables are pinned
static int ndirty, capdirty, dirty_limit = 512;

static void dirty_add(Table *t) {
    if (ndirty == capdirty && !(dirty = realloc(dirty, (size_t)(capdirty = capdirty * 2 + 256) * sizeof *dirty))) abort();
    dirty[ndirty++] = t, t->dirty = 1;
}
// The write barrier: when a table takes a value that may be a pointer, and when its arrays are replaced.
// Always AFTER the last allocation of the operation: an allocation may collect, and that collection may be the
// one that freezes this very table; a barrier passed before it would have seen a table that was not frozen yet,
// and the new array or value put in afterwards would belong to a frozen table nobody walks.
// And while a collection is marking, what is stored is marked at once: the table may already have been traced.
static inline void wb(Table *t, Val v) {
    if (v.t < V_STR) return;
    if (gc_barrier) gc_mark(v.p);
    if (FROZEN(t) && !t->dirty) dirty_add(t);
}
static inline void wb_arrays(Table *t) {
    if (FROZEN(t) && !t->dirty) dirty_add(t);
}

// ponytail: recursive, as deep as the tables are nested (a dozen levels in the game's data)
static void freeze(Table *t);
static inline bool freeze_val(Val v) { // true: a pointer the freeze does not follow
    if (v.t == V_STR) gc_pin(v.s);
    else if (v.t == V_TAB) freeze(v.tab);
    else return v.t > V_TAB;
    return false;
}
static void freeze(Table *t) {
    if (FROZEN(t)) return;
    t->old = freeze_gen, t->dirty = 0;
    gc_pin(t), gc_pin(t->arr), gc_pin(t->ents), gc_pin(t->idx);
    bool other = false;
    for (int i = 0; i < t->n; i++) other |= freeze_val(t->arr[i]);
    for (int e = 0; e < t->nents; e++) other |= freeze_val(t->ents[e].key) | freeze_val(t->ents[e].val);
    if (other) dirty_add(t);
}

void tfreeze(Table **slot) {
    for (int i = 0; i < nfrozen; i++)
        if (frozen[i].slot == slot) return;
    if (nfrozen < 4) frozen[nfrozen++].slot = slot; // (frozen at the next collection: frozen[].was is NULL)
}

// When a collection starts
static void table_roots(void) {
    bool again = ndirty > dirty_limit;
    for (int i = 0; i < nfrozen; i++) again |= *frozen[i].slot != frozen[i].was;
    if (again) {
        if (!++freeze_gen) freeze_gen = 1;
        ndirty = 0;
        gc_unpin_all();
        for (int i = 0; i < nfrozen; i++)
            if ((frozen[i].was = *frozen[i].slot)) freeze(frozen[i].was);
        dirty_limit = ndirty * 2 + 512; // (the tables that hold objects are dirty for good)
    }
    for (int i = 0; i < ndirty; i++) table_trace(dirty[i]);
}

void tfree(Table *t) {
    if (!t || FROZEN(t)) return;
    gc_free(t->arr);
    gc_free(t->ents);
    gc_free(t->idx);
    gc_free(t);
}

static inline IStr *istr_of(const char *interned) { return (IStr *)(interned - offsetof(IStr, s)); }

// k is normalized: a string key is the interned pointer
static uint32_t key_hash(Val k) {
    switch (k.t) {
    case V_STR: return istr_of(k.s)->hash;
    case V_BOOL: return k.b;
    case V_NUM: {
        num d = k.n + 0.0; // -0 and 0 are the same key
        uint64_t u;
        memcpy(&u, &d, sizeof u);
        return (uint32_t)(u ^ (u >> 32)) * 2654435761u;
    }
    default: {
        uintptr_t p = (uintptr_t)k.p;
        return (uint32_t)((p >> 3) ^ ((uint64_t)p >> 32)) * 2654435761u;
    }
    }
}

static inline bool key_same(Val a, Val b) {
    if (a.t != b.t) return false;
    if (a.t == V_NUM) return a.n == b.n;
    if (a.t == V_BOOL) return a.b == b.b;
    return a.p == b.p; // strings are interned: by pointer too
}

static int hfind(Table *t, Val k, uint32_t h) {
    if (!t->live) return -1;
    uint32_t mask = (uint32_t)t->ecap * 2 - 1;
    for (uint32_t i = h & mask;; i = (i + 1) & mask) {
        int e = t->idx[i];
        if (e < 0) return -1;
        if (key_same(t->ents[e].key, k)) return e;
    }
}

static void hindex(Table *t, int e, uint32_t h) {
    uint32_t mask = (uint32_t)t->ecap * 2 - 1;
    uint32_t i = h & mask;
    while (t->idx[i] >= 0) i = (i + 1) & mask;
    t->idx[i] = e;
}

// Drops the tombstones and grows if the live entries fill more than half
static void hrebuild(Table *t) {
    int ncap = !t->ecap ? 4 : t->live * 2 > t->ecap ? t->ecap * 2 : t->ecap;
    Entry *nents = gc_alloc_atomic((size_t)ncap * sizeof *nents);
    int32_t *nidx = gc_realloc_atomic(t->idx, (size_t)ncap * 2 * sizeof *nidx);
    wb_arrays(t);
    t->idx = nidx; // (now: the old block is gone)
    memset(nidx, 0xff, (size_t)ncap * 2 * sizeof *nidx);
    int k = 0;
    for (int e = 0; e < t->nents; e++)
        if (t->ents[e].key.t != V_NIL) nents[k++] = t->ents[e];
    gc_free(t->ents);
    t->ents = nents, t->ecap = ncap, t->nents = k;
    for (int e = 0; e < k; e++) hindex(t, e, key_hash(nents[e].key));
}

static void hremove(Table *t, int e) {
    STAMP(t);
    if (t->ents[e].key.t == V_NUM) t->nnum--;
    t->ents[e].key = NIL, t->ents[e].val = NIL;
    t->live--;
}

static void hset(Table *t, Val k, Val v) {
    uint32_t h = key_hash(k);
    int e = hfind(t, k, h);
    if (e >= 0) {
        wb(t, v);
        if (v.t == V_NIL) hremove(t, e);
        else {
            // (the same value written again, which per-frame code does a lot, leaves the stamp alone)
            if (val_differs(t->ents[e].val, v)) STAMP(t);
            t->ents[e].val = v;
        }
        return;
    }
    if (v.t == V_NIL) return;
    if (t->nents == t->ecap) hrebuild(t);
    STAMP(t);
    wb(t, k), wb(t, v);
    e = t->nents++;
    t->ents[e].key = k, t->ents[e].val = v;
    hindex(t, e, h);
    t->live++;
    if (k.t == V_NUM) t->nnum++;
}

static void arr_push(Table *t, Val v) {
    if (t->n == t->acap) {
        int ncap = t->acap ? t->acap * 2 : 4;
        t->arr = gc_realloc_atomic(t->arr, (size_t)ncap * sizeof *t->arr);
        t->acap = ncap;
        wb_arrays(t);
    }
    wb(t, v);
    STAMP(t);
    t->arr[t->n++] = v;
}

// After the array grew: integer keys waiting in the hash that now continue it move over, as Lua's rehash does
static void arr_migrate(Table *t) {
    while (t->nnum) {
        Val k = N(t->n + 1);
        int e = hfind(t, k, key_hash(k));
        if (e < 0) break;
        Val v = t->ents[e].val;
        hremove(t, e);
        arr_push(t, v);
    }
}

static inline void arr_trim(Table *t) {
    while (t->n > 0 && t->arr[t->n - 1].t == V_NIL) t->n--;
}

Val tgeti(Table *t, int i) {
    if (!t) return NIL;
    if ((unsigned)(i - 1) < (unsigned)t->n) return t->arr[i - 1];
    if (!t->nnum) return NIL;
    Val k = N(i);
    int e = hfind(t, k, key_hash(k));
    return e >= 0 ? t->ents[e].val : NIL;
}

void tseti(Table *t, int i, Val v) {
    if (!t) ERROR("attempt to index a nil value (key %d)", i);
    if ((unsigned)(i - 1) < (unsigned)t->n) {
        wb(t, v);
        if (val_differs(t->arr[i - 1], v)) STAMP(t);
        t->arr[i - 1] = v;
        if (v.t == V_NIL && i == t->n) arr_trim(t);
    } else if (i == t->n + 1 && v.t != V_NIL) {
        arr_push(t, v); // key n+1 is never in the hash (arr_migrate keeps that true)
        arr_migrate(t);
    } else {
        hset(t, N(i), v);
    }
}

Val tget(Table *t, const char *key) {
    if (!t || !key || !t->live) return NIL;
    IStr *k = intern_lookup(key);
    if (!k) return NIL;
    Val kv;
    kv.t = V_STR, kv.s = k->s;
    int e = hfind(t, kv, k->hash);
    return e >= 0 ? t->ents[e].val : NIL;
}

Val tget_k(Table *t, const char *k) {
    if (!t || !k || !t->live) return NIL;
    Val kv;
    kv.t = V_STR, kv.s = k;
    int e = hfind(t, kv, istr_of(k)->hash);
    return e >= 0 ? t->ents[e].val : NIL;
}

void tset_k(Table *t, const char *k, Val v) {
    if (!t) ERROR("attempt to index a nil value (field '%s')", k ? k : "?");
    if (!k) ERROR("table index is nil");
    if (v.t == V_NIL && !t->live) return;
    Val kv;
    kv.t = V_STR, kv.s = k;
    hset(t, kv, v);
}

// The key-taking functions as the macros of core.h call them: a call site with a literal key interns it at its
// first call and keeps it in *site (a static: the collector sees it there and keeps the string)
// The lookup itself is written out here (not tget_k -> hfind): one call per read instead of three or four, and the
// typed readers below take the value in place instead of a 16-byte struct returned through each level.
static const Val *find_k(Table *t, const char *k) {
    uint32_t mask = (uint32_t)t->ecap * 2 - 1;
    const int32_t *idx = t->idx;
    const Entry *ents = t->ents;
    for (uint32_t i = istr_of(k)->hash & mask;; i = (i + 1) & mask) {
        int e = idx[i];
        if (e < 0) return NULL;
        if (ents[e].key.p == k && ents[e].key.t == V_STR) return &ents[e].val;
    }
}
// NULL: absent (or the key is not a literal: *other is then what the old way found)
static inline const Val *find_at(Table *t, const char *key, const char **site, Val *other) {
    if (!site) {
        *other = tget(t, key);
        return other->t == V_NIL ? NULL : other;
    }
    const char *k = *site;
    if (!k && !(k = *site = str(key))) return NULL;
    if (!t || !t->live) return NULL;
    return find_k(t, k);
}
Val tget_at(Table *t, const char *key, const char **site) {
    Val o;
    const Val *v = find_at(t, key, site, &o);
    return v ? *v : NIL;
}
void tset_at(Table *t, const char *key, const char **site, Val v) {
    if (!site) { tset(t, key, v); return; }
    if (!*site) *site = str(key);
    tset_k(t, *site, v);
}
num tnum_at(Table *t, const char *key, const char **site) { Val o; const Val *v = find_at(t, key, site, &o); return v && v->t == V_NUM ? v->n : 0; }
const char *tstr_at(Table *t, const char *key, const char **site) { Val o; const Val *v = find_at(t, key, site, &o); return v && v->t == V_STR ? v->s : NULL; }
Table *ttab_at(Table *t, const char *key, const char **site) { Val o; const Val *v = find_at(t, key, site, &o); return v && v->t == V_TAB ? v->tab : NULL; }
bool tbool_at(Table *t, const char *key, const char **site) { Val o; const Val *v = find_at(t, key, site, &o); return v && truthy(*v); }
void *tobj_at(Table *t, const char *key, const char **site) { Val o; const Val *v = find_at(t, key, site, &o); return v && (v->t == V_OBJ || v->t == V_PTR) ? v->p : NULL; }

void tset(Table *t, const char *key, Val v) {
    if (!t) ERROR("attempt to index a nil value (field '%s')", key ? key : "?");
    if (!key) ERROR("table index is nil");
    if (v.t == V_NIL && !t->live) return;
    hset(t, S(key), v);
}

static inline bool int_key(num n, int *i) {
    if (!(n >= INT_MIN && n <= INT_MAX)) return false;
    *i = (int)n;
    return (num)*i == n;
}

Val tgetv(Table *t, Val key) {
    int i;
    if (!t) return NIL;
    switch (key.t) {
    case V_NIL: return NIL;
    case V_STR: return tget_k(t, key.s);
    case V_NUM:
        if (int_key(key.n, &i)) return tgeti(t, i);
        if (key.n != key.n) return NIL;
        // fallthrough
    default: {
        int e = hfind(t, key, key_hash(key));
        return e >= 0 ? t->ents[e].val : NIL;
    }
    }
}

void tsetv(Table *t, Val key, Val v) {
    int i;
    if (!t) ERROR("attempt to index a nil value");
    switch (key.t) {
    case V_NIL: ERROR("table index is nil");
    case V_STR: tset_k(t, key.s, v); return;
    case V_NUM:
        if (int_key(key.n, &i)) { tseti(t, i, v); return; }
        if (key.n != key.n) ERROR("table index is NaN");
        // fallthrough
    default: hset(t, key, v);
    }
}

// The array part never ends in nil and key n+1 is never in the hash, so n is a border
int tlen(Table *t) { return t ? t->n : 0; }

// core.h tarr_next
const Val *tarray(Table *t, int *n) {
    *n = t ? t->n : 0;
    return t ? t->arr : NULL;
}

void tinsert(Table *t, Val v) { tseti(t, tlen(t) + 1, v); }

// ltablib.c tinsert
void tinsert_at(Table *t, int pos, Val v) {
    if (!t) ERROR("bad argument #1 to 'insert' (table expected, got nil)");
    int e = t->n + 1;
    if (pos >= 1 && pos < e && v.t != V_NIL) { // the common case, inside the array part: one memmove
        arr_push(t, NIL);
        memmove(t->arr + pos, t->arr + pos - 1, (size_t)(t->n - pos) * sizeof *t->arr);
        wb(t, v);
        STAMP(t);
        t->arr[pos - 1] = v;
        arr_migrate(t);
        return;
    }
    if (pos > e) e = pos;
    for (int i = e; i > pos; i--) tseti(t, i, tgeti(t, i - 1));
    tseti(t, pos, v);
}

// ltablib.c tremove
Val tremove(Table *t, int pos) {
    if (!t) ERROR("bad argument #1 to 'remove' (table expected, got nil)");
    int e = t->n;
    if (pos == 0) pos = e;
    if (!(1 <= pos && pos <= e)) return NIL;
    Val r = t->arr[pos - 1];
    STAMP(t);
    memmove(t->arr + pos - 1, t->arr + pos, (size_t)(e - pos) * sizeof *t->arr);
    t->n--;
    arr_trim(t);
    return r;
}

void tclear(Table *t) {
    if (!t) return;
    STAMP(t);
    t->n = 0;
    t->nents = t->live = t->nnum = 0;
    if (t->idx) memset(t->idx, 0xff, (size_t)t->ecap * 2 * sizeof *t->idx);
}

bool tempty(Table *t) { return !t || (t->n == 0 && t->live == 0); }

int tcount(Table *t) {
    if (!t) return 0;
    int c = t->live;
    for (int i = 0; i < t->n; i++) c += t->arr[i].t != V_NIL;
    return c;
}

TIter titer(Table *t) { return (TIter){t, 0, NIL, NIL}; }

// at >= 0: the next array slot to look at; at < 0: the next entry is -at - 1
bool tnext(TIter *it) {
    // 1..256 as numbers: the key of an array slot without an int to double conversion (a library call on the PS2,
    // once per element of every table walked in a frame)
    static num small_int[256];
    static bool filled;
    if (!filled) {
        for (int i = 0; i < 256; i++) small_int[i] = i + 1;
        filled = true;
    }
    Table *t = it->t;
    if (!t) return false;
    if (it->at >= 0) {
        while (it->at < t->n) {
            int i = it->at++;
            if (t->arr[i].t != V_NIL) {
                it->key.t = V_NUM, it->key.n = i < 256 ? small_int[i] : (num)(i + 1), it->val = t->arr[i];
                return true;
            }
        }
        it->at = -1;
    }
    for (int e = -it->at - 1; e < t->nents; e++) {
        if (t->ents[e].key.t != V_NIL) {
            it->key = t->ents[e].key, it->val = t->ents[e].val;
            it->at = -(e + 1) - 1;
            return true;
        }
    }
    it->at = -t->nents - 1;
    return false;
}

Table *tshallow(Table *t) {
    if (!t) return NULL;
    Table *c = tnew();
    for (TIter it = titer(t); tnext(&it);) tsetv(c, it.key, it.val);
    return c;
}

// misc_functions.lua:979 copy_table: tables (keys too) are copied in depth, anything else is the same value.
// note: an object is a V_OBJ here, not a table, so it is shared instead of cloned field by field; there are no
// metatables on plain tables, so nothing to copy there.
static Val copy_val(Val v) { return v.t == V_TAB ? T(tcopy(v.tab)) : v; }

Table *tcopy(Table *t) {
    if (!t) return NULL;
    Table *c = tnew();
    for (TIter it = titer(t); tnext(&it);) tsetv(c, copy_val(it.key), copy_val(it.val));
    return c;
}

Val tpath(Table *t, const char *dotted) {
    Val v = T(t);
    for (const char *p = dotted; v.t == V_TAB;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        IStr *k = intern_find(p, len, hash_bytes(p, len));
        if (!k) return NIL;
        Val kv;
        kv.t = V_STR, kv.s = k->s;
        int e = hfind(v.tab, kv, k->hash);
        v = e >= 0 ? v.tab->ents[e].val : NIL;
        if (!dot) return v;
        p = dot + 1;
    }
    return NIL; // a step of the path is not a table
}

num tnum(Table *t, const char *key) { Val v = tget(t, key); return v.t == V_NUM ? v.n : 0; }
const char *tstr(Table *t, const char *key) { Val v = tget(t, key); return v.t == V_STR ? v.s : NULL; }
Table *ttab(Table *t, const char *key) { Val v = tget(t, key); return v.t == V_TAB ? v.tab : NULL; }
bool tbool(Table *t, const char *key) { return truthy(tget(t, key)); }
void *tobj(Table *t, const char *key) { Val v = tget(t, key); return v.t == V_OBJ || v.t == V_PTR ? v.p : NULL; }
void tsetn(Table *t, const char *key, num n) { tset(t, key, N(n)); }
void tsets(Table *t, const char *key, const char *s) { tset(t, key, S(s)); }
void tsett(Table *t, const char *key, Table *v) { tset(t, key, T(v)); }
void tsetb(Table *t, const char *key, bool b) { tset(t, key, B(b)); }
void tseto(Table *t, const char *key, void *obj) { tset(t, key, O(obj)); }
num tnumi(Table *t, int i) { Val v = tgeti(t, i); return v.t == V_NUM ? v.n : 0; }
const char *tstri(Table *t, int i) { Val v = tgeti(t, i); return v.t == V_STR ? v.s : NULL; }
Table *ttabi(Table *t, int i) { Val v = tgeti(t, i); return v.t == V_TAB ? v.tab : NULL; }
void *tobji(Table *t, int i) { Val v = tgeti(t, i); return v.t == V_OBJ || v.t == V_PTR ? v.p : NULL; }

// Pairs until a NULL key (what TAB passes); a positive count reads exactly that many pairs instead
Table *tab_of(int count, ...) {
    Table *t = tnew();
    va_list ap;
    va_start(ap, count);
    for (int i = 0; count <= 0 || i < count; i++) {
        const char *key = va_arg(ap, const char *);
        if (!key) break;
        Val v = va_arg(ap, Val);
        tset(t, key, v);
    }
    va_end(ap);
    return t;
}

// A table constructor: {1, nil, 3} has length 3 in Lua, so the values go straight to the array part
Table *tab_from(const Val *kv, int n) {
    Table *t = tnew();
    if (n < 0) for (n = 0; (int)kv[n].t != 99; n++) {}
    for (int i = 0; i + 1 < n; i += 2) {
        if (kv[i].t != V_STR) ERROR("TAB: a key that is not a string");
        tset_k(t, kv[i].s, kv[i + 1]); // (every V_STR is interned: S() and this file make them all)
    }
    return t;
}
Table *arr_from(const Val *v, int n) {
    Table *t = tnew();
    if (n < 0) for (n = 0; (int)v[n].t != 99; n++) {}
    for (int i = 0; i < n; i++) arr_push(t, v[i]);
    arr_trim(t);
    return t;
}
Table *arr_of(int count, ...) {
    Table *t = tnew();
    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++) arr_push(t, va_arg(ap, Val));
    va_end(ap);
    arr_trim(t);
    return t;
}

// ---------------------------------------------------------------- table.sort (ltablib.c, Lua 5.1.5)
// lvm.c luaV_lessthan without metamethods
static bool sort_comp(Val a, Val b, Less less, void *ctx) {
    if (less) return less(a, b, ctx);
    if (a.t == V_NUM && b.t == V_NUM) return a.n < b.n;
    if (a.t == V_STR && b.t == V_STR) return strcmp(a.s, b.s) < 0;
    if (a.t == b.t) ERROR("attempt to compare two %s values", lua_typename_of(a));
    ERROR("attempt to compare %s with %s", lua_typename_of(a), lua_typename_of(b));
}

// A line by line port of auxsort: the same pivot and the same comparator calls in the same order, so a comparator
// that is not a strict order leaves the array as Lua would.
static void auxsort(Table *t, int l, int u, Less less, void *ctx) {
    while (l < u) { // for tail recursion
        int i, j;
        // sort elements a[l], a[(l+u)/2] and a[u]
        Val al = tgeti(t, l), au = tgeti(t, u);
        if (sort_comp(au, al, less, ctx)) tseti(t, l, au), tseti(t, u, al);
        if (u - l == 1) break; // only 2 elements
        i = (l + u) / 2;
        Val ai = tgeti(t, i);
        al = tgeti(t, l);
        if (sort_comp(ai, al, less, ctx)) {
            tseti(t, i, al), tseti(t, l, ai);
        } else {
            au = tgeti(t, u);
            if (sort_comp(au, ai, less, ctx)) tseti(t, i, au), tseti(t, u, ai);
        }
        if (u - l == 2) break; // only 3 elements
        Val P = tgeti(t, i);   // the pivot
        tseti(t, i, tgeti(t, u - 1)), tseti(t, u - 1, P);
        // a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2
        i = l, j = u - 1;
        for (;;) { // invariant: a[l..i] <= P <= a[j..u]
            Val vi, vj;
            // repeat ++i until a[i] >= P
            while (vi = tgeti(t, ++i), sort_comp(vi, P, less, ctx))
                if (i > u) ERROR("invalid order function for sorting");
            // repeat --j until a[j] <= P
            while (vj = tgeti(t, --j), sort_comp(P, vj, less, ctx))
                if (j < l) ERROR("invalid order function for sorting");
            if (j < i) break;
            tseti(t, i, vj), tseti(t, j, vi);
        }
        Val a = tgeti(t, u - 1), b = tgeti(t, i);
        tseti(t, u - 1, b), tseti(t, i, a); // swap pivot (a[u-1]) with a[i]
        // a[l..i-1] <= a[i] == P <= a[i+1..u]: recurse into the smaller half, loop on the larger one
        if (i - l < u - i) {
            j = l, i = i - 1, l = i + 2;
        } else {
            j = i + 1, i = u, u = j - 2;
        }
        auxsort(t, j, i, less, ctx);
    }
}

void tsort(Table *t, Less less, void *ctx) {
    if (!t) ERROR("bad argument #1 to 'sort' (table expected, got nil)");
    auxsort(t, 1, tlen(t), less, ctx);
    STAMP(t);
}

Val tconcat(Table *t, const char *sep) {
    if (!t) ERROR("bad argument #1 to 'concat' (table expected, got nil)");
    if (!sep) sep = "";
    int n = tlen(t);
    size_t seplen = strlen(sep), total = 0;
    for (int i = 1; i <= n; i++) {
        const char *s = vstr(tgeti(t, i));
        if (!s) ERROR("invalid value (%s) at index %d in table for 'concat'", lua_typename_of(tgeti(t, i)), i);
        total += strlen(s) + (i < n ? seplen : 0);
    }
    char *buf = malloc(total + 1), *p = buf;
    if (!buf) abort();
    for (int i = 1; i <= n; i++) {
        const char *s = vstr(tgeti(t, i)); // interned: the same pointer as in the first pass
        size_t l = strlen(s);
        memcpy(p, s, l), p += l;
        if (i < n) memcpy(p, sep, seplen), p += seplen;
    }
    Val r;
    r.t = V_STR, r.s = intern(buf, total)->s;
    free(buf);
    return r;
}

// ---------------------------------------------------------------- objects
// The start of struct Class (node.h): fixed as the name and the super, which is all this file reads.
struct ClassHead {
    const char *name;
    const Class *super;
};

bool is_a(const void *obj, const Class *cls) {
    if (!obj) return false;
    for (const Class *c = ((const Object *)obj)->cls; c; c = ((const struct ClassHead *)c)->super)
        if (c == cls) return true;
    return false;
}

// GC_STAT: the live scanned blocks by class (the ones that are not objects: by size), printed with each GCSTAT line
static struct { const Class *cls; long n, bytes; } census[48];
static int ncensus;
static void census_block(void *block, size_t size) {
    if (!block) { // the collection is done: print and start over
        printf("GCLIVE");
        for (int i = 0; i < ncensus; i++) {
            if (census[i].n) printf(" %s %ld (%ld KB)", census[i].cls ? ((const struct ClassHead *)census[i].cls)->name : "other", census[i].n, census[i].bytes >> 10);
            census[i].n = census[i].bytes = 0;
        }
        printf("\n");
        return;
    }
    const Class *cls = *(const Class **)block;
    int i = 1;
    while (i < ncensus && census[i].cls != cls) i++;
    if (i >= ncensus) i = 0; // [0]: not an object (its first word is no class object_new has seen)
    census[i].n++, census[i].bytes += (long)size;
}

void *object_new(size_t size, const Class *cls) {
    Object *o = gc_alloc(size < sizeof *o ? sizeof *o : size); // scanned word by word: the structs keep pointers anywhere
    if (!ncensus) ncensus = 1, gc_set_census(census_block);
    int i = 1;
    while (i < ncensus && census[i].cls != cls) i++;
    if (i == ncensus && ncensus < 48) census[ncensus++].cls = cls;
    o->cls = cls;
    o->dyn = tnew();
    return o;
}

void object_free(void *obj) {
    // Nothing to do: the Lua's remove() only unlinks an object (events queued earlier and other objects still hold
    // it afterwards and ask it `REMOVED`), and the collector frees it when nothing refers to it any more.
    (void)obj;
}

// ---------------------------------------------------------------- errors
// On the PS2 the line goes to stdout as "ERROR ..." (the console's network log and tools/pcsx2.sh look for it;
// stderr is not sent) and the game stops there: abort() only hung without a word.
#ifdef _EE
#define ERR_OUT stdout
#define ERR_TAG "ERROR "
static void die(void) { for (;;) {} }
#else
#define ERR_OUT stderr
#define ERR_TAG ""
static void die(void) { abort(); }
#endif
void todo_port(const char *file, int line, const char *what) {
    fprintf(ERR_OUT, ERR_TAG "%s:%d: TODO_PORT: %s\n", file, line, what);
    die();
    for (;;) {}
}

void lua_error_(const char *file, int line, const char *format, ...) {
    va_list ap;
    fprintf(ERR_OUT, ERR_TAG "%s:%d: ", file, line);
    va_start(ap, format);
    vfprintf(ERR_OUT, format, ap);
    va_end(ap);
    fputc('\n', ERR_OUT);
    die();
    for (;;) {}
}
