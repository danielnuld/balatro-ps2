// Loads every data file of data/ (made by tools/dump_data.sh), checks a few values everyone knows, and compares
// the full dump of each loaded table with the dump Lua made of the same table when it wrote the file (data/ref):
// with sorted keys (nothing lost or changed) and in iteration order (the definition order survives the loading).
//   gcc -std=gnu99 -Wall -g -fsanitize=address,undefined -Iinclude src/core.c src/gc.c src/data.c test/data_test.c -lm -o /tmp/data_test
//   /tmp/data_test [data dir]      (run from native/; the default is "data")
#include <glob.h>
#include "core.h"
#include "data.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void compare(Table *t, bool sorted, const char *dir, const char *name) {
    const char *path = fmt("%s/ref/%s.%s.txt", dir, name, sorted ? "sorted" : "ordered");
    size_t size = 0;
    char *want = (char *)data_read_file(path, &size);
    char *got = data_dump(t, sorted);
    if (!want) {
        printf("FAIL %s: cannot read\n", path), failures++;
    } else if (strlen(got) != size || memcmp(got, want, size) != 0) {
        size_t at = 0, line = 1;
        while (at < size && got[at] == want[at]) line += got[at++] == '\n';
        printf("FAIL %s: differs at line %zu (C %zu bytes, Lua %zu):\n  C:   %.60s\n  Lua: %.*s\n", path, line,
               strlen(got), size, got + at, (int)(size - at < 60 ? size - at : 60), want + at);
        failures++;
    }
    free(want);
    free(got);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "data";
    CHECK(data_load(fmt("%s/no_such_file.dat", dir)) == NULL);

    Table *game = data_load(fmt("%s/game.dat", dir));
    if (!game) return printf("FAIL: no %s/game.dat (run tools/dump_data.sh)\n", dir), 1;
    Table *protos = ttab(game, "prototypes"), *C = ttab(ttab(game, "globals"), "C");
    Table *j_joker = ttab(ttab(protos, "P_CENTERS"), "j_joker");
    CHECK(tnum(ttab(j_joker, "config"), "mult") == 4);
    CHECK(streq(tstr(j_joker, "name"), "Joker") && tnum(j_joker, "cost") == 2 && tbool(j_joker, "unlocked"));
    CHECK(tget(j_joker, "key").t == V_NIL); // taken before the function gives the prototypes their key
    CHECK(tnum(ttab(ttab(protos, "P_BLINDS"), "bl_big"), "mult") == 1.5);
    CHECK(tnum(ttab(ttab(protos, "P_BLINDS"), "bl_wall"), "mult") == 4);
    CHECK(tcount(ttab(protos, "P_CARDS")) == 52 && tpath(protos, "P_CARDS.H_A.pos.x").n == 12);
    CHECK(tcount(ttab(ttab(protos, "P_CENTER_POOLS"), "Joker")) == 0);
    CHECK(tnum(ttab(ttab(game, "globals"), "STATES"), "SHOP") == 5);
    CHECK(tnumi(ttab(C, "MULT"), 1) == 254 / 255.0 && tlen(ttab(C, "MULT")) == 4);
    CHECK(tgeti(ttab(C, "HAND_LEVELS"), 0).tab == ttab(C, "RED")); // the same table, not a copy
    CHECK(ttab(C, "UI_MULT") != ttab(C, "RED"));
    CHECK(tlen(ttab(game, "CHALLENGES")) == 20 && streq(tstr(ttabi(ttab(game, "CHALLENGES"), 1), "id"), "c_omelette_1"));
    Table *flush_five = ttab(ttab(ttab(game, "GAME"), "hands"), "Flush Five");
    CHECK(tnum(flush_five, "chips") == 160 && tnum(flush_five, "l_mult") == 3 && !tbool(flush_five, "visible"));
    CHECK(streq(tstri(ttabi(ttab(flush_five, "example"), 1), 1), "S_A"));
    CHECK(tget(flush_five, "visible").t == V_BOOL); // false is kept, it is not nil
    // definition order: the first prototype of the Lua's constructor comes first
    TIter it = titer(ttab(protos, "P_CENTERS"));
    CHECK(tnext(&it) && streq(it.key.s, "c_base") && tnext(&it) && streq(it.key.s, "j_joker"));
    compare(game, true, dir, "game");
    compare(game, false, dir, "game");

    Table *en = data_load(fmt("%s/loc_en-us.dat", dir));
    CHECK(en && streq(tstr(ttab(ttab(ttab(en, "descriptions"), "Joker"), "j_joker"), "name"), "Joker"));
    CHECK(streq(tstr(ttab(ttab(en, "misc"), "poker_hands"), "Flush Five"), "Flush Five"));

    glob_t g;
    CHECK(glob(fmt("%s/loc_*.dat", dir), 0, NULL, &g) == 0 && g.gl_pathc == 15);
    for (size_t i = 0; i < g.gl_pathc; i++) {
        const char *base = strrchr(g.gl_pathv[i], '/') + 1;
        const char *name = str_sub(base, 1, -5); // without ".dat"
        Table *loc = data_load(g.gl_pathv[i]);
        CHECK(loc && ttab(loc, "descriptions") && ttab(loc, "misc"));
        compare(loc, true, dir, name);
        compare(loc, false, dir, name);
    }
    printf("%s: game.dat and %zu languages, %d failures\n", failures ? "FAILED" : "ok", g.gl_pathc, failures);
    globfree(&g);
    return failures != 0;
}
