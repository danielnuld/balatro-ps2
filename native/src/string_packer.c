// engine/string_packer.lua:1-72 (the packer is MIT, Copyright (c) 2017 Robert Herlihy; see the Lua for the licence).
#include "all.h"

// ---------------------------------------------------------------- writer
typedef struct { char *s; size_t n, cap; } PackBuf;

static void buf_put(PackBuf *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        while (b->n + len + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 256;
        b->s = realloc(b->s, b->cap);
        if (!b->s) abort();
    }
    memcpy(b->s + b->n, s, len);
    b->n += len;
    b->s[b->n] = '\0';
}
static void buf_puts(PackBuf *b, const char *s) { buf_put(b, s, strlen(s)); }

// string.format("%q", s), Lua 5.1 lstrlib.c addquoted (the form the spec gives). A NUL cannot be in a C string.
static void buf_quoted(PackBuf *b, const char *s) {
    buf_put(b, "\"", 1);
    for (;; s++) {
        size_t plain = strcspn(s, "\"\\\n\r"); // (a run at a time: a save is mostly its keys)
        buf_put(b, s, plain);
        s += plain;
        if (!*s) break;
        if (*s == '\r') buf_put(b, "\\r", 2);
        else {
            buf_put(b, "\\", 1);
            buf_put(b, s, 1);
        }
    }
    buf_put(b, "\"", 1);
}

// number_tostring(n) appended. The whole numbers, which a save is full of, are written without printf: the EE
// formats a double in software, and a run's save took 54 ms of a frame to pack, half of it there.
static void buf_num(PackBuf *b, num n) {
    int i = n > -1e9 && n < 1e9 ? (int)n : 0;
    if ((num)i != n || (i == 0 && signbit(n))) return buf_puts(b, number_tostring(n)); // (-0 is "-0")
    char text[12], *p = text + sizeof text;
    unsigned u = i < 0 ? -(unsigned)i : (unsigned)i;
    do *--p = '0' + u % 10; while (u /= 10);
    if (i < 0) *--p = '-';
    buf_put(b, p, text + sizeof text - p);
}

// string_packer.lua:22-47, appending to b instead of building ret_str
static Val pack_path[32]; // the keys down to the value being packed, for the error message only
static int pack_depth;
static const char *pack_where(void) {
    const char *w = "";
    for (int k = 0; k < pack_depth && k < 32; k++) w = fmt("%s.%s", w, tostring(pack_path[k]));
    return w;
}
static void pack(PackBuf *b, Table *data, bool recursive) {
    buf_puts(b, recursive ? "{" : "return {"); // :23
    // note: pairs order is the table's insertion order here (arbitrary in the Lua; nothing depends on it)
    for (TIter it = titer(data); tnext(&it);) { // :25
        Val i = it.key, v = it.val;
        // :27 (an object is a table for type())
        if (i.t == V_TAB || i.t == V_OBJ) ERROR("Data table cannot have an table as a key reference");
        if (i.t == V_STR) { // :28-29
            buf_put(b, "[", 1);
            buf_quoted(b, i.s);
            buf_put(b, "]", 1);
        } else { // :31: only a number concatenates
            if (i.t != V_NUM) ERROR("attempt to concatenate local 'i' (a %s value)", lua_typename_of(i));
            buf_put(b, "[", 1);
            buf_num(b, i.n);
            buf_put(b, "]", 1);
        }
        if (pack_depth < 32) pack_path[pack_depth] = i;
        pack_depth++;
        buf_put(b, "=", 1); // :43
        if (v.t == V_OBJ) { // :34-35 v.is and v:is(Object)
            buf_puts(b, "\"MANUAL_REPLACE\"");
        } else if (v.t == V_TAB) { // :37
            pack(b, v.tab, true);
        } else if (v.t == V_STR) { // :40
            buf_quoted(b, v.s);
        } else if (v.t == V_BOOL) { // :41
            buf_puts(b, v.b ? "true" : "false");
        } else if (v.t == V_NUM) { // :43 number converted by ".."
            buf_num(b, v.n);
        } else {
            // note: a V_FN or V_PTR value (a function or userdata in the Lua) fails at the concatenation of :43.
            // A colour kept as P(float *) in a table that gets saved lands here: the caller must not pack it.
            ERROR("attempt to concatenate local 'v' (a %s value) at %s", lua_typename_of(v), pack_where());
        }
        pack_depth--;
        buf_put(b, ",", 1); // :43
    }
    buf_put(b, "}", 1); // :46
}

// string_packer.lua:22 into a buffer the caller frees
char *table_pack(Table *data, size_t *len) {
    PackBuf b = {0};
    pack(&b, data, false);
    if (len) *len = b.n;
    return b.s;
}

// string_packer.lua:22
const char *STR_PACK(Table *data, bool recursive) {
    PackBuf b = {0};
    pack(&b, data, recursive);
    const char *ret_str = str(b.s);
    free(b.s);
    return ret_str;
}

// ---------------------------------------------------------------- reader
// note: string_packer.lua:50 is loadstring: any Lua. This reads what STR_PACK writes, with free whitespace:
// return {[key]=value,...}; key and value: "string" (Lua escapes), number, true, false, nil, a nested table.
// A bare name is a global that does not exist (the "inf" and "nan" a PC save can hold): nil, as in the Lua.
// Positional fields and name=value fields are not read (STR_PACK never writes them): they fail to parse.
typedef struct { const char *p, *end; } Reader;

static void rd_ws(Reader *r) {
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r')) r->p++;
}
static bool rd_char(Reader *r, char c) {
    rd_ws(r);
    if (r->p < r->end && *r->p == c) { r->p++; return true; }
    return false;
}
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static bool rd_value(Reader *r, Val *out);

static bool rd_string(Reader *r, Val *out) {
    char q = *r->p++;
    char *buf = malloc((size_t)(r->end - r->p) + 1), *w = buf;
    if (!buf) abort();
    bool ok = false;
    while (r->p < r->end) {
        char c = *r->p++;
        if (c == q) { ok = true; break; }
        if (c == '\n') break; // unfinished string
        if (c == '\\') {
            if (r->p >= r->end) break;
            c = *r->p++;
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'v': c = '\v'; break;
            default:
                if (is_digit(c)) { // \ddd, decimal, up to three digits
                    int d = c - '0';
                    for (int k = 0; k < 2 && r->p < r->end && is_digit(*r->p); k++) d = d * 10 + (*r->p++ - '0');
                    if (d > 255) { free(buf); return false; }
                    c = (char)d; // note: a \0 ends the C string there (io.h: no NUL inside)
                }
                // else the character itself: \" \\ \' and backslash-newline
            }
        }
        *w++ = c;
    }
    *w = '\0';
    if (ok) *out = S(buf);
    free(buf);
    return ok;
}

static bool rd_table(Reader *r, Val *out) {
    Table *t = tnew();
    r->p++; // '{'
    for (;;) {
        if (rd_char(r, '}')) break;
        Val k, v;
        if (!rd_char(r, '[')) return false;
        if (!rd_value(r, &k) || !rd_char(r, ']') || !rd_char(r, '=') || !rd_value(r, &v)) return false;
        if (k.t == V_NIL) return false; // table index is nil
        tsetv(t, k, v);                 // a nil value sets nothing
        if (rd_char(r, ',') || rd_char(r, ';')) continue;
        if (!rd_char(r, '}')) return false;
        break;
    }
    *out = T(t);
    return true;
}

static bool rd_value(Reader *r, Val *out) {
    rd_ws(r);
    if (r->p >= r->end) return false;
    char c = *r->p;
    if (c == '{') return rd_table(r, out);
    if (c == '"' || c == '\'') return rd_string(r, out);
    if (is_alpha(c)) {
        const char *s = r->p;
        while (r->p < r->end && (is_alpha(*r->p) || is_digit(*r->p))) r->p++;
        size_t n = (size_t)(r->p - s);
        if (n == 4 && memcmp(s, "true", 4) == 0) *out = B(true);
        else if (n == 5 && memcmp(s, "false", 5) == 0) *out = B(false);
        else *out = NIL; // nil, or a global that is not there (inf, nan)
        return true;
    }
    // a number; "-inf" and "-nan" are arithmetic on nil in the Lua: an error
    char tok[64];
    size_t n = 0;
    const char *s = r->p;
    if (*s == '-') tok[n++] = *s++;
    if (s >= r->end || !(is_digit(*s) || *s == '.')) return false;
    while (s < r->end && n < sizeof tok - 1 &&
           (is_digit(*s) || is_alpha(*s) || *s == '.' || ((*s == '+' || *s == '-') && (s[-1] == 'e' || s[-1] == 'E'))))
        tok[n++] = *s++;
    tok[n] = '\0';
    char *endptr;
    num d = strtod(tok, &endptr);
    if (endptr == tok || *endptr != '\0') return false; // malformed number
    r->p = s;
    *out = N(d);
    return true;
}

// string_packer.lua:49 over len bytes; NULL if they do not parse
// note: the tables built before a parse error are left behind (no collector yet).
Table *table_unpack(const char *bytes, size_t len) {
    Reader r = {bytes, bytes + len};
    Val v;
    rd_ws(&r);
    if (r.end - r.p < 6 || memcmp(r.p, "return", 6) != 0) return NULL;
    r.p += 6;
    rd_ws(&r);
    if (r.p >= r.end || *r.p != '{' || !rd_table(&r, &v)) return NULL;
    rd_char(&r, ';');
    rd_ws(&r);
    if (r.p != r.end) return NULL;
    return v.tab;
}

// string_packer.lua:49
Table *STR_UNPACK(const char *str) {
    Table *t = table_unpack(str, strlen(str));
    if (!t) ERROR("STR_UNPACK: the text is not a packed table"); // assert(loadstring(str))
    return t;
}

// ---------------------------------------------------------------- files
// string_packer.lua:53
// ponytail: the text returned is interned and lives forever (io.h's contract); a caller that loads big saves often
// should read the file itself and use table_unpack.
const char *get_compressed(const char *_file) {
    if (love_filesystem_getInfo(_file, NULL)) { // :54-55
        size_t size = 0;
        char *file_string = love_filesystem_read(_file, &size); // :56
        if (!file_string) ERROR("get_compressed: could not read '%s'", _file); // string.sub(nil) at :58
        if (size != 0) { // :57
            const char *ret;
            if (size < 6 || memcmp(file_string, "return", 6) != 0) { // :58
                size_t out_size = 0;
                char *inflated = love_data_decompress(file_string, size, &out_size); // :60
                free(file_string);
                if (!inflated) return NULL; // :61
                ret = fmt("%.*s", (int)out_size, inflated);
                free(inflated);
            } else {
                ret = str(file_string);
                free(file_string);
            }
            return ret; // :63
        }
        free(file_string);
    }
    return NULL;
}

// string_packer.lua:70-71
static void deflate_and_write(const char *_file, const char *save_string, size_t len) {
    size_t out_size = 0;
    void *deflated = love_data_compress(save_string, len, 1, &out_size); // :70
    if (!deflated) ERROR("compress_and_save: could not compress '%s'", _file);
    love_filesystem_write(_file, deflated, out_size); // :71 (result ignored, as in the Lua)
    free(deflated);
}

// string_packer.lua:68, _data a string
void compress_and_save(const char *_file, const char *_data) {
    deflate_and_write(_file, _data, strlen(_data));
}

// string_packer.lua:68, _data a table
void compress_and_save_table(const char *_file, Table *_data) {
    size_t len = 0;
#ifdef SAVE_TIME // -DSAVE_TIME: what a save costs the frame before the card (packing, then deflate and the write's queueing)
    double t0 = love_timer_getTime();
#endif
    char *save_string = table_pack(_data, &len); // :69
#ifdef SAVE_TIME
    double t1 = love_timer_getTime();
#endif
    // :70-71. note: on the EE the platform deflates in the card's thread and get_compressed reads the text back
    // undeflated until the next boot: it starts with "return", which :58 takes as it is.
    love_filesystem_write_deflated(_file, save_string, len);
#ifdef SAVE_TIME
    printf("SAVE packed %s %d bytes in %.2f ms, deflate and write %.2f ms\n", _file, (int)len, (t1 - t0) * 1000, (love_timer_getTime() - t1) * 1000);
#endif
    free(save_string);
}
