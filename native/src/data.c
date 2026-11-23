// Loads the data files made by tools/dump_data.sh. The format and the contents are described in data.h.
// note: includes core.h and data.h instead of all.h: this file is below the game's headers and is tested alone.
#include "core.h"
#include "data.h"

static unsigned char *read_file_stdio(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = n >= 0 ? malloc(n ? n : 1) : NULL;
    if (buf && fread(buf, 1, n, f) != (size_t)n) free(buf), buf = NULL;
    fclose(f);
    if (buf) *size = n;
    return buf;
}
unsigned char *(*data_read_file)(const char *path, size_t *size) = read_file_stdio;

// ---------------------------------------------------------------- loading
typedef struct {
    const unsigned char *p, *end;
    const char *path;
    Table **tabs; // the tables read so far, for tag 6
    uint32_t ntabs, captabs;
    char *str; // a string of the file with its terminator, for S()
    size_t capstr;
} Reader;

static void need(Reader *r, size_t n) {
    if ((size_t)(r->end - r->p) < n) ERROR("%s: truncated data file", r->path);
}

static uint32_t read_u32(Reader *r) {
    need(r, 4);
    uint32_t v = r->p[0] | (uint32_t)r->p[1] << 8 | (uint32_t)r->p[2] << 16 | (uint32_t)r->p[3] << 24;
    r->p += 4;
    return v;
}

static Val read_value(Reader *r) {
    need(r, 1);
    int tag = *r->p++;
    switch (tag) {
    case 0: return NIL;
    case 1: return B(false);
    case 2: return B(true);
    case 3: {
        need(r, 8);
        uint64_t bits = 0;
        for (int i = 7; i >= 0; i--) bits = bits << 8 | r->p[i];
        r->p += 8;
        num n;
        memcpy(&n, &bits, sizeof n);
        return N(n);
    }
    case 4: {
        uint32_t len = read_u32(r);
        need(r, len);
        if (len + 1 > r->capstr) r->str = realloc(r->str, r->capstr = (size_t)len * 2 + 64);
        memcpy(r->str, r->p, len);
        r->str[len] = 0;
        r->p += len;
        return S(r->str);
    }
    case 5: {
        uint32_t count = read_u32(r);
        Table *t = tnew();
        if (r->ntabs == r->captabs) r->tabs = realloc(r->tabs, (r->captabs = r->captabs * 2 + 256) * sizeof *r->tabs);
        r->tabs[r->ntabs++] = t;
        for (uint32_t i = 0; i < count; i++) {
            Val key = read_value(r);
            if (key.t != V_NUM && key.t != V_STR) ERROR("%s: a table key that is not a number or a string", r->path);
            tsetv(t, key, read_value(r));
        }
        return T(t);
    }
    case 6: {
        uint32_t i = read_u32(r);
        if (i >= r->ntabs) ERROR("%s: reference to table %u before it", r->path, (unsigned)i);
        return T(r->tabs[i]);
    }
    }
    ERROR("%s: unknown tag %d", r->path, tag);
}

Table *data_load(const char *path) {
    size_t size = 0;
    unsigned char *buf = data_read_file(path, &size);
    if (!buf) return NULL;
    if (size < 4 || memcmp(buf, "BDT1", 4) != 0) ERROR("%s: not a data file", path);
    Reader r = {.p = buf + 4, .end = buf + size, .path = path};
    Val root = read_value(&r);
    if (root.t != V_TAB || r.p != r.end) ERROR("%s: damaged data file", path);
    free(r.tabs);
    free(r.str);
    free(buf);
    return root.tab;
}

Table *game_dat(void) {
    static Table *data;
    if (!data) data = data_load("data/game.dat"), tfreeze(&data);
    if (!data) ERROR("data/game.dat cannot be read");
    return data;
}

Table *lits(const char *path) {
    Val v = tpath(ttab(game_dat(), "LITERALS"), path);
    if (v.t != V_TAB) ERROR("data/game.dat has no LITERALS.%s: regenerate it with tools/dump_data.sh", path);
    return v.tab;
}

const char *lit_at(Table *t, int i) {
    Val v = tgeti(t, i);
    if (v.t != V_STR) ERROR("data/game.dat: a table of LITERALS has no text %d: regenerate it with tools/dump_data.sh", i);
    return v.s;
}

const char *lit(const char *path, int i) { return lit_at(lits(path), i); }

// ---------------------------------------------------------------- dumping
typedef struct {
    char *s;
    size_t n, cap;
    Table *seen; // table -> its number
    int nseen;
    bool sorted;
} Dump;

static void put(Dump *d, const char *s, size_t len) {
    if (d->n + len + 1 > d->cap) d->s = realloc(d->s, d->cap = (d->n + len) * 2 + 256);
    memcpy(d->s + d->n, s, len);
    d->s[d->n += len] = 0;
}

static void put_scalar(Dump *d, Val v) {
    char buf[40];
    switch (v.t) {
    case V_NIL: put(d, "nil", 3); break;
    case V_BOOL: put(d, v.b ? "true" : "false", v.b ? 4 : 5); break;
    case V_NUM: put(d, buf, snprintf(buf, sizeof buf, "%.17g", v.n)); break;
    case V_STR: put(d, "\"", 1), put(d, v.s, strlen(v.s)), put(d, "\"", 1); break;
    default: put(d, buf, snprintf(buf, sizeof buf, "<%s>", lua_typename_of(v))); break;
    }
}

static int key_cmp(const void *pa, const void *pb) { // numbers ascending, then strings by bytes
    const Val *a = pa, *b = pb;
    if (a->t != b->t) return a->t == V_NUM ? -1 : 1;
    if (a->t == V_NUM) return (a->n > b->n) - (a->n < b->n);
    return strcmp(a->s, b->s);
}

static void put_value(Dump *d, Val v, int depth) {
    char buf[40];
    if (v.t != V_TAB) { put_scalar(d, v); return; }
    Val id = tgetv(d->seen, v);
    if (id.t == V_NUM) { put(d, buf, snprintf(buf, sizeof buf, "@%d", (int)id.n)); return; }
    tsetv(d->seen, v, N(++d->nseen));
    put(d, buf, snprintf(buf, sizeof buf, "{#%d\n", d->nseen));

    int count = tcount(v.tab), i = 0;
    Val *keys = malloc((count ? count : 1) * sizeof *keys);
    for (TIter it = titer(v.tab); tnext(&it);) {
        if (it.key.t != V_NUM && it.key.t != V_STR) ERROR("data_dump: a key of type %s", lua_typename_of(it.key));
        keys[i++] = it.key;
    }
    if (d->sorted) qsort(keys, count, sizeof *keys, key_cmp);
    for (i = 0; i < count; i++) {
        for (int s = 0; s <= depth; s++) put(d, " ", 1);
        put_scalar(d, keys[i]);
        put(d, " = ", 3);
        put_value(d, tgetv(v.tab, keys[i]), depth + 1);
        put(d, "\n", 1);
    }
    free(keys);
    for (int s = 0; s < depth; s++) put(d, " ", 1);
    put(d, "}", 1);
}

char *data_dump(Table *t, bool sorted) {
    Dump d = {.seen = tnew(), .sorted = sorted};
    put_value(&d, T(t), 0);
    put(&d, "\n", 1);
    tfree(d.seen);
    return d.s;
}
