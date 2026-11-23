// challenges.lua: G.CHALLENGES, the list of challenge runs. The file is one table literal and no code: there is no
// challenges.c. The table comes from the data file (data.h: the key CHALLENGES of game.dat) and is the Game's
// CHALLENGES member (game.h), a Table *: an array, 1-based, in the file's order (the order the menu shows and
// unlocks them in; #G.CHALLENGES is tlen). The functions that read it are in other files (get_challenge_int_from_id
// and get_challenge_rule in misc_functions.h, the run's start in game.h, the menus in UI_definitions.h).
//
// The shape of an entry, with the Lua's keys (every list is an array and may be empty):
//   name          string: the internal name ('The Omelette'); the shown one is localize(id, 'challenge_names')
//   id            string: 'c_omelette_1'...: what G.GAME.challenge holds and the profile's records are keyed by
//   rules         {custom = {{id = string, value = number or string or nil}...},     the rules shown and applied
//                  modifiers = {{id = string, value = number}...}}                   overrides of starting_params
//   jokers        {{id = center key, eternal = true or nil, pinned = true or nil, edition = string or nil}...}
//   consumeables  {{id = center key}...}
//   vouchers      {{id = center key}...}
//   deck          {type = 'Challenge Deck', and optionally cards = {{s = suit letter, r = rank letter, e =
//                  enhancement key or nil, g = seal or nil}...}, yes_ranks / no_ranks = {rank
//                  letter -> true}, yes_suits / no_suits = {suit letter -> true}, enhancement, edition, gold_seal}
//   restrictions  {banned_cards = {{id = center key, ids = {center keys} or nil}...},
//                  banned_tags = {{id = tag key}...},
//                  banned_other = {{id = blind key, type = 'blind'}...}}
// Read them with tget / tstr / ttab as any table. Nothing here is written by the game.
#ifndef CHALLENGES_H
#define CHALLENGES_H
#include "types.h"
#include "node.h"
// (no declarations: challenges.lua defines no function, no class and no state of its own)
#endif
