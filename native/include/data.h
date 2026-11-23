// The game's static data: the tables its Lua builds from literals, extracted from the user's copy of the game at
// build time by tools/dump_data.sh into data/*.dat (never kept in this tree as source) and loaded here as Tables.
// Implemented in src/data.c.
//
// FILE FORMAT (.dat). Four bytes "BDT1", then one value (always a table). Integers are little endian. A value is
// one tag byte and its payload:
//   0  nil     (not written by the tool; accepted as a table value, where it means "no such key")
//   1  false
//   2  true
//   3  number: 8 bytes, IEEE 754 double, little endian (the exact double Lua had)
//   4  string: u32 length, then the bytes (no terminator; never a zero byte inside)
//   5  table:  u32 count, then count pairs (key value, value value). A key is a number or a string.
//   6  the same table (the same one, not a copy) as an earlier tag 5: u32 index, counting tag-5 tables from 0 in
//      the order their tag appears in the file (G.C.HAND_LEVELS[0] is G.C.RED itself, and the code relies on it)
// The pairs of a table are in the order the port iterates it (core.h titer): 1..n first, then every other key in
// the order the Lua's source defines it (the order of the fields in the table constructor, then of the later
// assignments). Lua itself would iterate the hash keys in an order of its own that nothing can reproduce; where
// the game's code built a table with pairs() (copy_table, bosses_used) the tool made pairs() follow the same
// definition order, so the result is what the translated C produces.
//
// game.dat: one table with these keys, each taken at a precise point and independent of the others (a table shared
// between two sections is two tables here):
//   globals     every field of G after Game:set_globals() (globals.lua), which is all `G = Game()` does: F_* flags,
//               SETTINGS, COLLABS, C (the colours, as {r, g, b, a}), STATES, STAGES, UIT, handlist,
//               keybind_mapping, the sizes... and the empty tables it creates (I, FUNCS, ARGS...). Fields set to nil
//               (F_RUMBLE, the whole of button_mapping) are absent. SEED (os.time()) is left out. The flags are the
//               ones of love.system.getOS() == 'ps4', the answer of the Lua port's boot.lua (DUMP_OS changes it).
//   render      animation_atli, asset_atli, asset_images: the fields Game:set_render_settings() adds (game.lua:998);
//               the paths are the ones of the default SETTINGS.GRAPHICS.texture_scaling, 2 ("resources/textures/2x/").
//               ANIMATION_ATLAS and ASSET_ATLAS, which it fills from these with the loaded images, are not here.
//   language    LANGUAGES, FONTS, LANG: the fields Game:set_language() adds (game.lua:927), after its loop that
//               replaces each language's font number by the FONTS entry itself (a tag 6); no FONT objects. LANG is
//               the entry of the default language, en-us.
//   prototypes  the fields Game:init_item_prototypes() (game.lua:216) has set when it reaches its call to
//               self:save_progress() at game.lua:729: P_SEALS, P_TAGS, tag_undiscovered, P_STAKES, P_BLINDS,
//               b_undiscovered, P_CARDS, j_locked ... booster_undiscovered, P_CENTERS, and P_CENTER_POOLS,
//               P_JOKER_RARITY_POOLS, P_LOCKED still empty. Nothing after that line has run: no `key` field, nothing
//               from the profile (unlocked, discovered, alerted are the literals), no pool filled or sorted: that
//               is code, for the translation of the rest of the function.
//               P_BLINDS.bl_ox.vars[1] is localize('ph_most_played') in the Lua: here it is that text as a string,
//               "localize('ph_most_played')", to be replaced after loading.
//   CHALLENGES  G.CHALLENGES (challenges.lua)
//   GAME        what Game:init_game_object() returns (game.lua:1862) right after the above: the template of G.GAME,
//               with hands, starting_params (get_starting_params()) and bosses_used (every boss blind = 0)
//   LITERALS    text that is in no table of the game: string literals of function bodies, read from the Lua's source
//               text (see lits() below)
// loc_<language>.dat: the table localization/<language>.lua returns, untouched (init_localization() has not run).
//
// Not in any file because the game does not keep them in tables: the shader and sound names (it lists the
// directories resources/shaders and resources/sounds). data/skipped.txt lists every value that could not be
// written (a function, or the localize() call above) as path, kind, Lua file:line.
#ifndef DATA_H
#define DATA_H
#include "core.h"

// The whole file in a malloc'd buffer (the caller frees it) and its size, or NULL if it cannot be read.
// Plain stdio by default; the platform points it at its own reader.
extern unsigned char *(*data_read_file)(const char *path, size_t *size);

// The table a .dat file holds, or NULL if the file cannot be read. A damaged file is an ERROR.
Table *data_load(const char *path);

// The table of data/game.dat, loaded on first use and kept (an ERROR if it cannot be read): the one copy every
// section is taken from (globals, prototypes, CHALLENGES).
Table *game_dat(void);

// game.dat's LITERALS: the text the game's Lua has as string literals inside function bodies (the credits, a few
// labels, the links), which tools/dump_data.lua reads from the source text of those functions. One entry per
// function, an array of its texts in the order of the source, except: credits = {tabs = {{label = , text = {...}} x 6},
// collabs = {{name = , artist = , artist_link = , ml_text = {...}, art = }...}} and url = {<G.FUNCS name> = url}.
// lit("cardarea", 3) is that function's third text; lits("credits.tabs") a table (a dotted path of names). An ERROR if absent: the data was made by an older
// tools/dump_data.sh or from another version of the game.
Table *lits(const char *path);
const char *lit(const char *path, int i);
const char *lit_at(Table *t, int i); // the same on a table lits() gave

// A textual dump for debugging and for the tests, one key per line, tables numbered in the order they are met and
// printed once ("@n" afterwards). sorted: keys as numbers ascending then strings by bytes; else in titer order.
// malloc'd: free it.
char *data_dump(Table *t, bool sorted);
#endif
