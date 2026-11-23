# Conventions of the native port

Every file of the native port follows these rules. They are what lets each part of the game's Lua be translated on
its own and the parts link. Where a rule does not cover a case, the code takes the option closest to the Lua and
says so in a `// note:` comment; there is never a second way of doing something a rule covers.

The reference is the game's own Lua (Balatro 1.0.1o), extracted from the user's copy (`BALATRO_GAME`). The C does
what the Lua does, in the same order: same conditions, same formulas, same events queued in the same order, same
random calls with the same seed keys. The game is not improved, no branch is skipped because it looks unused,
nothing is reordered.

## Layout

- `include/*.h`: shared declarations. `include/types.h` declares every class's struct name (enough when only
  pointers are needed). `include/core.h` (values, tables, strings, objects) and `include/love.h` (the platform:
  graphics, audio, input, files, time) are the base. One header per Lua file: `node.h`, `card.h`, `game.h`,
  `common_events.h`... `include/all.h` includes every one of them: a `.c` file includes only `all.h`.
- `src/<lua file stem>.c`, one per Lua file; a big file is split by line ranges into `card_1.c`, `card_2.c`... with
  the same header (`tools/chunks.json` has the ranges). Functions private to a `.c` are `static`.
- `include/needs/<file>.h`: declarations a source file needed that no shared header had (a function another file
  exports, a struct field), each with a comment naming the Lua line that needs it. A `.c` file starts with
  `#include "all.h"`, then its `needs/` header if it has one.
- C11 with GNU extensions (`-std=gnu11`). No C++, no VLAs in structs, no compiler-specific intrinsics.
- A file must pass
  `gcc -std=gnu11 -fsyntax-only -Wall -Wno-unused -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Iinclude src/<file>.c`;
  `build.sh` uses the same flags.
- `docs/header-notes.md` holds the decisions that are not obvious from a header; `docs/source-notes.md` the notes
  per source file.

## Names

- A Lua method `Card:set_ability(center, initial)` is `void Card_set_ability(Card *self, ...)`.
- A Lua global function `evaluate_poker_hand(hand)` keeps its name: `evaluate_poker_hand(...)`.
- `G.FUNCS.name` is `void FUNCS_name(UIElement *e)`, registered by name (`funcs_register`) because the UI refers to
  callbacks by string. `G.UIDEF.name(...)` is `UIDEF_name(...)`; `create_UIBox_x` keeps its name.
- The global `G` is `Game *G`. Constants tables keep their path: `G.C.RED` is `G->C.RED`, `G.STATES.SHOP` is the
  enum `STATES_SHOP`, `G.UIT.R` is `UIT_R`.
- Local variables keep the Lua's names where possible. Every function carries a comment with its Lua `file:line`.

## Numbers

- `num` is `double`: for what the rules compute and compare exactly: chips, mult, dollars, hand levels, costs,
  counters kept in tables, everything in the random number functions; also the game's timers and every delay of
  an event (a delay is written as the Lua writes it, `delay(0.2, NULL)`, never `0.2f`: a float delay loses the
  Lua's ties by a frame).
- `float` for geometry and drawing: T, VT, velocity, scale, colours, UI sizes. Mixed expressions in drawing code
  are float; per-frame drawing code takes `(float)` of a timer once.
- Lua's `%` is `lmod(a, b)` (`a - floor(a/b)*b`), `^` is `pow`, `//` does not exist in 5.1. `#t` is `tlen(t)`.
  `math.log(x, base)` takes the base (as LuaJIT, which the PC game runs on).
- Indices stay 1-based wherever they are visible to the rules (card ranks, positions in an area: `cards[1]` is
  `tgeti(cards, 1)` or `area->cards[0]` with the struct's own accessor; the header says which).

## Two kinds of data

1. **Objects with a struct** (`Node`, `Moveable`, `Sprite`, `Card`, `CardArea`, `UIBox`, `UIElement`, `DynaText`,
   `Blind`, `Tag`, `Event`, `Controller`, `Game`...): a field is a struct member **if and only if the header
   declares it**. Any other field the Lua reads or writes on that object lives in the object's dynamic table
   `obj->dyn` (a `Table *`, never NULL): `self.sticker_run` is `tget(self->dyn, "sticker_run")` when `Card` has no
   `sticker_run` member.
2. **Tables** (`Table *`): everything the Lua keeps in plain tables and the rules read by key: `G.GAME`,
   `card.ability`, `card.config`, `card.base`, a center of `G.P_CENTERS`, a UI node's `config`, contexts, effects,
   arguments passed as `{...}`. Same keys as the Lua.

### Values and tables (`core.h`)

```c
Val  tget(Table *t, const char *key);      // t.key (V_NIL if absent or t is NULL)
void tset(Table *t, const char *key, Val v);
Val  tgeti(Table *t, int i);               // t[i], 1-based
void tseti(Table *t, int i, Val v);
int  tlen(Table *t);                       // #t
void tinsert(Table *t, Val v);             // table.insert(t, v)
void tinsert_at(Table *t, int pos, Val v); // table.insert(t, pos, v)
Val  tremove(Table *t, int pos);           // table.remove(t, pos); pos 0 = last
// typed shortcuts: absent -> 0 / NULL / false
num         tnum(Table *t, const char *key);
const char *tstr(Table *t, const char *key);
Table      *ttab(Table *t, const char *key);
bool        tbool(Table *t, const char *key);   // Lua truthiness: nil and false are false, 0 is TRUE
void       *tobj(Table *t, const char *key);    // an object pointer stored in a table
void tsetn(Table *t, const char *key, num n);   // and tsets, tsett, tsetb, tseto
// constructors of values
Val N(num n); Val S(const char *s); Val B(bool b); Val T(Table *t); Val O(void *obj); extern const Val NIL;
bool truthy(Val v); bool veq(Val a, Val b);     // Lua's == (numbers by value, strings by content, the rest by identity)
// a new table, optionally filled: tnew(), or TAB("key", N(1), "other", S("x")) (key/value pairs), ARR(N(1), N(2))
// iteration, in insertion order (arrays first, 1..n):
for (TIter it = titer(t); tnext(&it);) { it.key (Val), it.val (Val) }
```

- A Lua expression `a.b.c` on tables is `tget(ttab(ttab(a, "b"), "c"))`, or the typed shortcut at the end.
  `tpath(t, "b.c")` does the walk.
- `x = t.k or 5` is `Val v = tget(t, "k"); num x = truthy(v) ? v.n : 5;`. `nil` and `false` are different values
  and stay different wherever the Lua tells them apart.
- Strings are C strings; contents are compared with `streq(a, b)` (NULL-safe), never with `==`. Strings are built
  with `fmt("%s_%d", a, n)` (it returns an interned string owned by the collector: never freed by hand). Lua's
  `..` is `fmt`.
- `pairs(t)` is `titer` (insertion order: deterministic). Where the Lua's result depends on hash order, a
  `// note:` says so; the one place where a rule depends on it (`G.GAME.hands`) reproduces Lua 5.1's order
  (`lua51_pairs_order`). `ipairs` is a `for` from 1 to the first nil. `table.sort(t, f)` is `tsort(t, cmp, ctx)`,
  which is Lua 5.1's own algorithm (unstable in the same way). `copy_table` is `tcopy` (deep).
- A colour kept in a table is `P(float *)` (a pointer to 4 floats).
- Memory: there is a garbage collector (`include/gc.h`, a conservative mark and sweep), as in the Lua: tables,
  strings, objects, `NEWCTX` structs and `Fn`s are never freed by hand, and are reclaimed when nothing refers to
  them any more. What keeps something alive: a pointer to it (to any byte of it) in a C local, in a static, in a
  table, or in a block of the collector's memory. What does NOT: a pointer kept only in libc's memory (the
  platform's `malloc`), or disguised (packed into a number, XORed, unaligned in a packed struct). In the game's
  sources `malloc`, `calloc`, `realloc`, `strdup` and `free` ARE the collector's (macros of `all.h`), so plain C
  arrays and structs may hold tables and strings; `free` is optional. A string from `fmt`/`str`/`S` lives as long
  as a pointer to it does.
- Per-frame code may keep what it read from a table together with the table's stamp (`tstamp`, core.h) and read
  again only when the stamp changed. Such cached members are not the Lua's and the header says so.

### Objects and classes

```c
struct Card { Moveable moveable; ... own fields ... };   // the base class is the first member
Card *c = NEW(Card);               // zeroed, class set, dyn table made; then call Card_init(c, ...)
obj->cls                           // const Class *: its name, its super, the virtual methods
is_a(obj, &Card_class)             // obj:is(Card)
CALL(obj, update, dt)              // a virtual method through the class (the engine's loops use these)
```

- Methods a subclass overrides and the engine calls without knowing the type (`update`, `move`, `draw`, `remove`,
  `hover`, `stop_hover`, `click`, `release`, `drag`, `stop_drag`, `animate`, `collides_with_point`, `set_role`...)
  are in `Class` (see `node.h`). `Card:draw(layer)` is `Card_draw(Card *self, const char *layer)`, and the class
  entry calls it with the default. The super method is called by name: `Moveable_move((Moveable *)self, dt)`.
- A pointer to a subclass is cast to its base with a C cast; the reverse only after `is_a`.

## Functions as values

- A callback kept in a table or passed as an argument is a `Fn`:
  `typedef Val (*FnPtr)(void *ctx, int argc, Val *argv);` made with `FN(function, ctx)` and called with
  `fcall(fn, argc, argv)` / `fcall0(fn)`.
- A closure is a `static` function plus a context struct with what it captured, allocated with `NEWCTX(struct)`;
  the captured variables are copied (a Lua upvalue that is assigned after capture is held by pointer, with a note).
- Events: `G.E_MANAGER:add_event(Event({trigger = 'after', delay = 0.2, func = function() ... return true end}))` is

  ```c
  static bool ev_name(void *ctx) { struct ev_name_ctx *c = ctx; ...; return true; }
  E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_name, .ctx = c}), NULL, false);
  ```

  Same fields as the Lua's Event config, same defaults; the queue name and `front` are the last two arguments.

## Optional and missing arguments

- A pointer argument that may be nil is NULL. A table of options (`args`) stays a `Table *`.
- A number that may be nil is passed as `Val` when the Lua distinguishes nil from 0, else as a number with the
  Lua's default applied by the callee (the header's comment says the default).
- A boolean that may be nil is `int` with -1 for nil only where the Lua tells nil from false; else `bool`.
- Several return values: the first is returned, the others through out-pointers.

## The platform (`love.h`)

LÖVE's functions keep their path as a name: `love.graphics.setColor(r, g, b, a)` is
`love_graphics_setColor(r, g, b, a)`, `love.graphics.draw(image, quad, x, y, r, sx, sy)` is
`love_graphics_draw_quad(...)`. Only what the game uses exists. The game's code calls nothing of gsKit, the PS2
SDK or libc's file functions: only `love.h`.

## No game content in the C

The repository carries no text or data of the game. Static data (centers, blinds, tags, the localization) and the
wording the Lua has as string literals inside functions come from `data/game.dat` and the localization dumps,
which `tools/dump_data.sh` generates from the user's own copy. What stays in the C are identifiers the code
compares (names of jokers, hands, states), symbols, placeholders and file names. See section 13 of
`docs/development-log.md`.

## What the code does not do

- No new abstractions, no rewrites "for speed" that change what is computed, no cleverness: a line of Lua is a
  line or a few of C. An optimisation must leave the comparison suites' output identical.
- No stubs that silently do nothing. What is not ported is `TODO_PORT("lua file:line: what is missing");` (it
  aborts with that message when reached) and is listed in `docs/source-notes.md` ("Not ported").
- Debug-only code (`G.DEBUG`, `test_functions.lua`, the HTTP manager, Steam) is not translated: a one-line comment
  stands where it was. (`G.FORCE_TAG` and `G.FORCE_BOSS` are translated: the test hooks use them.)
