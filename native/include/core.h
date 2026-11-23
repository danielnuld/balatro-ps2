// The base of the native port: Lua's values, tables and strings as C, and the object model of the game's classes.
// See docs/conventions.md. Implemented in src/core.c.
#ifndef CORE_H
#define CORE_H
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h" // the collector every table, string, object and closure context is allocated from

// NAN is "nil" for optional float arguments (docs/conventions.md) and isnan() the test. The PS2's FPU has no NaN: it
// compares one as an ordinary number, so the test reads the bits there (and everywhere, to run the same code).
static inline bool nan_bits_f(float f) { uint32_t u; memcpy(&u, &f, 4); return (u & 0x7fffffff) > 0x7f800000; }
static inline bool nan_bits_d(double d) { uint64_t u; memcpy(&u, &d, 8); return (u & 0x7fffffffffffffffULL) > 0x7ff0000000000000ULL; }
#undef isnan
#define isnan(x) (sizeof(x) == sizeof(float) ? nan_bits_f((float)(x)) : nan_bits_d((double)(x)))

typedef double num; // what the rules compute exactly (chips, mult, dollars, the random numbers)

// ---------------------------------------------------------------- values
typedef struct Table Table;
typedef struct Object Object;
typedef struct Val Val;
typedef Val (*FnPtr)(void *ctx, int argc, Val *argv);
typedef struct Fn { FnPtr f; void *ctx; } Fn;

typedef enum { V_NIL, V_BOOL, V_NUM, V_STR, V_TAB, V_OBJ, V_FN, V_PTR } VType;
struct Val {
    VType t;
    union {
        bool b;
        num n;
        const char *s;
        Table *tab;
        void *obj; // an Object of some class (is_a tells which)
        Fn *fn;
        void *p;
    };
};
extern const Val NIL;
static inline Val N(num n) { Val v; v.t = V_NUM, v.n = n; return v; }
static inline Val B(bool b) { Val v; v.t = V_BOOL, v.b = b; return v; }
Val S(const char *s);     // NULL -> NIL; the string is copied (interned)
Val T(Table *t);          // NULL -> NIL
Val O(void *obj);         // NULL -> NIL
Val P(void *p);           // a raw pointer the rules do not look into
Val FN(FnPtr f, void *ctx);
static inline bool truthy(Val v) { return !(v.t == V_NIL || (v.t == V_BOOL && !v.b)); } // Lua: only nil and false are false
bool veq(Val a, Val b);   // Lua's ==: numbers by value, strings by content, booleans, the rest by identity
num vnum(Val v);          // the number, or 0 (a numeric string is converted, as Lua's arithmetic does)
const char *vstr(Val v);  // the string, a number formatted as Lua's tostring does, or NULL
Val fcall(Val fn, int argc, Val *argv); // calls a V_FN; NIL if it is not one
static inline Val fcall0(Val fn) { return fcall(fn, 0, NULL); }
const char *lua_typename_of(Val v);     // type(v): "nil", "boolean", "number", "string", "table", "function", "userdata"
const char *tostring(Val v);            // Lua's tostring (numbers as %.14g)

// ---------------------------------------------------------------- strings
// NULL-safe; NULL equals only NULL. Inline: most calls are decided by the pointers (interned strings) or by the
// first character (a test against a literal), without a call to strcmp.
static inline bool streq(const char *a, const char *b) { return a == b || (a && b && *a == *b && strcmp(a, b) == 0); }
const char *str(const char *s);                         // the interned copy (lives while something refers to it: gc.h)
const char *fmt(const char *format, ...) __attribute__((format(printf, 1, 2))); // interned, as every string here
const char *str_sub(const char *s, int i, int j);       // string.sub (1-based, negatives from the end)
int str_find(const char *s, const char *plain, int init); // string.find(s, plain, init, true): the index or 0
const char *str_upper(const char *s);
const char *str_lower(const char *s);
const char *str_rep(const char *s, int n);
int str_len(const char *s);

// ---------------------------------------------------------------- tables
Table *tnew(void);
void tfree(Table *t);                      // the table itself, not what it refers to (optional: the collector frees tables)
// *slot (now and whenever it is assigned another table) is static data: thousands of tables that stay and hardly
// change. The collector then pins it with all it leads to and stops walking it at every collection (core.c).
// Only a hint: such a table is used and written as any other.
void tfreeze(Table **slot);
Val tget(Table *t, const char *key);       // V_NIL if absent or t is NULL
void tset(Table *t, const char *key, Val v); // NIL removes the key
Val tgeti(Table *t, int i);                // 1-based; any integer key works (sparse ones go to the hash)
void tseti(Table *t, int i, Val v);
Val tgetv(Table *t, Val key);              // t[key] for a key of any type (a number, a string, an object, a table)
void tsetv(Table *t, Val key, Val v);
int tlen(Table *t);                        // #t: the border of the array part
void tinsert(Table *t, Val v);
void tinsert_at(Table *t, int pos, Val v);
Val tremove(Table *t, int pos);            // pos 0: the last one
void tclear(Table *t);                     // every key to nil
Table *tcopy(Table *t);                    // copy_table of the game: deep for tables, objects by reference
Table *tshallow(Table *t);
bool tempty(Table *t);                     // next(t) == nil
int tcount(Table *t);                      // number of keys of any kind
Val tpath(Table *t, const char *dotted);   // tpath(t, "a.b.c")
// typed shortcuts (absent: 0, NULL, false)
num tnum(Table *t, const char *key);
const char *tstr(Table *t, const char *key);
Table *ttab(Table *t, const char *key);
bool tbool(Table *t, const char *key);
void *tobj(Table *t, const char *key);
void tsetn(Table *t, const char *key, num n);
void tsets(Table *t, const char *key, const char *s);
void tsett(Table *t, const char *key, Table *v);
void tsetb(Table *t, const char *key, bool b);
void tseto(Table *t, const char *key, void *obj);
num tnumi(Table *t, int i);
const char *tstri(Table *t, int i);
Table *ttabi(Table *t, int i);
void *tobji(Table *t, int i);
// ---- a literal key is interned once per call site
// tget(t, "key") hashed "key" and looked it up among the interned strings at every call: a third of a frame on
// the PS2. Each of the functions above that take a key is therefore a macro: where the key is a constant (a
// string literal), its interned string is kept in a static of that call site (a root for the collector, so the
// string stays) and the table is searched by that pointer, with the hash stored in front of its characters.
// Any other key goes the old way (site NULL). The key's text is written twice (once for __builtin_constant_p,
// which does not evaluate it) and every other argument once: the PS2's compiler dies on macros that repeat
// nested arguments.
Val tget_k(Table *t, const char *k);         // k: an interned string (of str(), fmt(), a Val's .s) or NULL
void tset_k(Table *t, const char *k, Val v);
Val tget_at(Table *t, const char *key, const char **site); // site: where the call site keeps key interned; NULL: none
void tset_at(Table *t, const char *key, const char **site, Val v);
num tnum_at(Table *t, const char *key, const char **site);
const char *tstr_at(Table *t, const char *key, const char **site);
Table *ttab_at(Table *t, const char *key, const char **site);
bool tbool_at(Table *t, const char *key, const char **site);
void *tobj_at(Table *t, const char *key, const char **site);
#ifndef CORE_NO_KEY_MACROS
#define KEY_SITE_(f, t, key, ...) ({ static const char *k_; f(t, key, __builtin_constant_p(key) ? &k_ : (const char **)0 __VA_OPT__(,) __VA_ARGS__); })
#define tget(t, key) KEY_SITE_(tget_at, t, key)
#define tset(t, key, v) KEY_SITE_(tset_at, t, key, v)
#define tnum(t, key) KEY_SITE_(tnum_at, t, key)
#define tstr(t, key) KEY_SITE_(tstr_at, t, key)
#define ttab(t, key) KEY_SITE_(ttab_at, t, key)
#define tbool(t, key) KEY_SITE_(tbool_at, t, key)
#define tobj(t, key) KEY_SITE_(tobj_at, t, key)
#define tsetn(t, key, n) KEY_SITE_(tset_at, t, key, N(n))
#define tsets(t, key, s) KEY_SITE_(tset_at, t, key, S(s))
#define tsett(t, key, v) KEY_SITE_(tset_at, t, key, T(v))
#define tsetb(t, key, b) KEY_SITE_(tset_at, t, key, B(b))
#define tseto(t, key, obj) KEY_SITE_(tset_at, t, key, O(obj))
#endif

// ---- a table's stamp: what lets per-frame code keep what it read from a table
// A number that changes whenever a key of the table (of the array part or not) is added, removed or given another
// value, by whatever function and through whatever alias of the table; writing the value a key already has leaves
// it. No stamp is ever given twice, to any table, so (table pointer, stamp) names one state of one table for good:
// a snapshot of some keys taken with that pair is valid exactly while the pair still reads the same (the holder's
// pointer keeps the table from being collected and its address from being reused; do not tfree a table something
// may hold a snapshot of). It says nothing of the tables a value refers to (they have their own stamps) or of what
// a P() value points to (read that every time). 0 for NULL.
static inline uint64_t tstamp(const Table *t) { return t ? *(const uint64_t *)(const void *)t : 0; }
// A walk of a list (a table with only an array part: G.MOVEABLES, G.I.CARD...) as titer / tnext make it, entry by
// entry and seeing what the body adds or removes, without a call per entry: the array is asked for again only when
// the table's stamp changed.   for (TArr a = tarr(list); tarr_next(&a);) { ... a.val ... }
const Val *tarray(Table *t, int *n); // the array part and its length, valid until the table is written
typedef struct { Table *t; const Val *arr; int n, i; uint64_t stamp; Val val; } TArr;
static inline TArr tarr(Table *t) { TArr a; a.t = t, a.arr = 0, a.n = 0, a.i = 0, a.stamp = 0, a.val.t = V_NIL; return a; }
static inline bool tarr_next(TArr *a) {
    for (;;) {
        if (a->stamp != tstamp(a->t)) a->arr = tarray(a->t, &a->n), a->stamp = tstamp(a->t);
        if (a->i >= a->n) return false;
        a->val = a->arr[a->i++];
        if (a->val.t != V_NIL) return true;
    }
}

// TAB("key", N(1), "other", S("x")): key/value pairs. ARR(N(1), N(2)): an array.
Table *tab_of(int count, ...);
Table *arr_of(int count, ...);
Table *tab_from(const Val *kv, int n); // n values: key, value, key, value... (a key is a V_STR); n < 0: up to VEND
Table *arr_from(const Val *v, int n);
#define VA_COUNT(...) (sizeof((Val[]){__VA_ARGS__}) / sizeof(Val))
// The arguments go through an array, not through the call: the PS2's compiler dies on a call with dozens of
// 16-byte structs as variable arguments (the UI definitions have them).
#define VEND ((Val){.t = (VType)99}) // ends an argument list (the arguments are written once: a macro that used
// them twice doubled the text at every level of nesting)
#define ARR(...) arr_from((const Val[]){__VA_ARGS__, VEND}, -1)
// TAB("key", N(1), "other", S("x")): keys are C strings, values Vals, through variable arguments. The PS2's
// compiler dies on that with dozens of pairs, so its build (-DTAB_ARRAYS) compiles copies of the sources in which
// tools/tabfix.py has wrapped every key: TAB(S("key"), N(1), ...), an array of Vals.
#ifdef TAB_ARRAYS
#define TAB(...) tab_from((const Val[]){__VA_ARGS__, VEND}, -1)
#else
#define TAB(...) tab_of(0, __VA_ARGS__, (const char *)NULL)
#endif
// iteration in insertion order: the array part 1..n first, then the other keys as they were first set
typedef struct { Table *t; int at; Val key, val; } TIter;
TIter titer(Table *t);
bool tnext(TIter *it);
// table.sort with Lua 5.1's algorithm; less(a, b, ctx) is the comparator ("a goes before b"); NULL: by <
typedef bool (*Less)(Val a, Val b, void *ctx);
void tsort(Table *t, Less less, void *ctx);
Val tconcat(Table *t, const char *sep);    // table.concat

// ---------------------------------------------------------------- numbers
static inline num lmod(num a, num b) { return a - floor(a / b) * b; } // Lua's %
static inline float lmodf(float a, float b) { return a - floorf(a / b) * b; }
// An angle that grows with time (G.TIMERS.REAL * speed), for sinf / cosf: beyond 201 radians the PS2's libm reduces
// the argument the slow way (__kernel_rem_pio2f), which after two minutes of play is every call of every frame.
// The angle is brought to [-pi, pi] here, with 2*pi in two parts so that x - k * 6.28125 is exact (k below 2^14:
// the result then has the float's own accuracy). Below 200 the angle is returned as it is: the same bits as before.
static inline float wrap_angle(float x) {
    if (x < 200.0f && x > -200.0f) return x;
    float k = (float)(int)(x * 0.15915494f + (x < 0 ? -0.5f : 0.5f));
    return (x - k * 6.28125f) - k * 0.0019353072f;
}
static inline num lmin(num a, num b) { return b < a ? b : a; }   // math.min(a, b)
static inline num lmax(num a, num b) { return b > a ? b : a; }
num tonumber(Val v, bool *ok);             // Lua's tonumber of a string or a number
const char *number_tostring(num n);        // %.14g, as Lua prints numbers

// ---------------------------------------------------------------- objects
typedef struct Class Class;
struct Object {
    const Class *cls;
    Table *dyn; // the fields the struct does not have (docs/conventions.md); never NULL
};
// The virtual methods are declared in node.h (struct Class): this header only needs the name and the super.
bool is_a(const void *obj, const Class *cls); // obj:is(cls), through the supers; false for NULL
void *object_new(size_t size, const Class *cls); // zeroed, cls and dyn set
void object_free(void *obj);
#define NEW(Type) ((Type *)object_new(sizeof(Type), &Type##_class))
#define NEWCTX(Type) ((Type *)gc_alloc(sizeof(Type)))

// ---------------------------------------------------------------- errors
// A part of the game that is not translated yet: aborts with the message when reached.
void todo_port(const char *file, int line, const char *what) __attribute__((noreturn));
#define TODO_PORT(what) todo_port(__FILE__, __LINE__, what)
void lua_error_(const char *file, int line, const char *format, ...) __attribute__((noreturn, format(printf, 3, 4)));
#define ERROR(...) lua_error_(__FILE__, __LINE__, __VA_ARGS__) // Lua's error()/assert failing
#endif
