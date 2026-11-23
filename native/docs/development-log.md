# Development log of the native port

What was found, fixed and measured while the C port was made to run and to match the game, by topic. The numbers
are from the logs of the runs named beside them. Where something was not verified, it says so.

Words used here:
- **The oracle**: the reference build. It is the game's own Lua running on this repository's Lua runtime
  (`runtime/`), built for the host with double precision numbers. The port is correct when it does what the oracle
  does.
- **The host**: Linux (WSL in practice), where the port is built with gcc and drawn by a software rasteriser.
- **The EE**: the PS2's main processor; "the console" is a real PS2, PCSX2 the emulator.
- **A blank**: a vertical blank of the video output (60 a second in NTSC).

Other documents: `docs/conventions.md` (how the Lua maps to C), `docs/header-notes.md` (decisions behind each
header), `docs/source-notes.md` (notes per source file).

## 1. Running and comparing against the reference

### The tools

- Build: `build.sh link` gives the optimised binary (`$BALATRO_BUILD/balatro`); `build.sh link asan` gives
  `balatro-asan` (`-O0 -fsanitize=address,undefined`, objects in `obj-asan`). The build is incremental (a file is
  compiled when it or any header is newer than its object, so a header change rebuilds everything), 4 jobs.
- Run: `tools/run.sh [frames] [shot frames] [asan|opt]` (default asan). It runs in `/root/native-run` (or
  `RUN_DIR`), copies `data/*.dat` there (the program reads `data/game.dat` relative to the current directory),
  writes `out.txt` and copies `shot-<n>.ppm` to `out/<PREFIX>shot-<n>.ppm`. `PAD="1300:a 1500:a"` scripts the pad.
  Do not edit `tools/run.sh` while a run is going (bash reads it as it goes).
- Deterministic runs: `TIME=1700000000` fixes `os_time()` (the seed of `math.random`); the oracle does the same
  when `TRACE=<n>` is set. `os_time()` replaces the two `time(NULL)` of the translation (globals.lua:125,
  game.lua:1404); without `TIME` nothing changes.
- `TRACE=<n>`: both sides print every n frames a `TRACE` line (state, number of moveables, a weighted sum of every
  moveable's T/VT/velocity, chips). `FRAMES=<n>` ends the run after n frames. Host test hooks only.
- `tools/both.sh`: the oracle and the port on one pad script, their TRACE lines side by side, `PARTS:` lines where
  state, moveables or chips first differ.
- Pictures: `tools/cmp.py <native prefix> <oracle prefix>` (Python with Pillow) prints the difference of every pair
  of `out/*.ppm` and writes PNGs; `tools/ppm2png.py`; `tools/sheet.py` makes contact sheets.
- Pad scripts used for the comparisons: `tools/pad_round1.txt` (fresh profile, seed TUTORIAL: the tutorial's first
  blind, won with a straight on the third hand, by frame ~8700), `tools/pad_shop1.txt` (the first presses in the
  shop, to frame 12900), `tools/pad_collection.txt` (every page of the collection). Use them with `DT=0.0166`.
- Hooks: to reach a state without minutes of pad presses, both sides run the same commands every frame. The oracle
  takes `HOOK=<file.lua>` (a chunk called with the frame number), the port `HOOK=<name>` (calls `hook_<name>(frame)`
  of `src/hook_<name>.c`). The hooks call the game's own functions and print the same comparison lines; each area
  below has its own.
- `FIXSEED=<seed>` (host; `-DTEST_SEED='"ABC"'` on the EE) gives every unseeded run that seed
  (`generate_starting_seed`, src/misc_functions_1.c). The game takes the seed of an unseeded run from the cursor's
  position and hover time, which are floats here and doubles in the oracle, so such a run could not be compared.

### What can and cannot be compared

- **The clocks.** With the default dt (1/60) the two sides first parted by a frame per timed event: an event's
  delay is a whole number of frames and `time + delay <= G.TIMERS.x` is a tie that the oracle's doubles and float
  timers round differently (about 0.6 frame per event; a scored hand ended 20 to 50 frames apart). With
  `DT=0.0166` there are no ties and the TRACE lines agree frame for frame apart from isolated single frames.
  Later the timers and the delays became doubles (section 9) and the default dt agrees too.
- While G.TIMERS was float: with `DT=0.0166` TOTAL drifts (0.0013 s behind after 2300 frames at GAMESPEED 4, 2 % of
  a step), and whenever TOTAL crosses a power of two (64, 128, 256...) the float drops low bits: an event whose
  delay is a whole number of steps (0.5 s = 8 steps at speed 4) pending across that moment completes a frame late
  (seen at TOTAL = 256, in the round evaluation). One frame late then shifts everything after it, and the event
  manager's own 1/60 s pacing (queue_dt: a pass is skipped now and then when dt < 1/60) turns one frame into two.
  The rules never differed in such a run; only the frame numbers. With `DT=0.015625` (1/64, exact in a float) the
  two agreed frame for frame much longer (0 sampled TRACE lines apart in 20000-frame runs); two of 27 tag runs
  (Investment, Juggle) still had the shift at the 256 crossing. That dt has a tie of its own: a card's flip
  (moveable.lua:440, `8*dt*T.w` is then exactly an eighth of the width) reaches 0 a frame apart, so a card caught
  mid-flip in a screenshot is a little wider or narrower. Compare rules, not frame numbers.
- **`math.random`** is shared by everything cosmetic (particles, the character's quips, the art of a tag's pack).
  The particle emitters' clock is `G.TIMERS.REAL`; after minutes an emitter is a particle ahead or behind and the
  stream is consumed in another interleaving. The quip on the game over screen (`math.random(1, 10)`) differed for
  that reason. The rules use `pseudorandom` (seeded per key) and are not affected: the deal, the scores and the
  shop were the same on both sides. TRACE's `sum` column is cosmetic too.
- **A defect of the oracle, not of the port:** Lua 5.1's `math.log` takes one argument, so the oracle computes the
  natural logarithm wherever the game writes `math.log(x, 5)` or `math.log(x, 10)` (LuaJIT, which the PC game runs
  on, takes the base; the port follows LuaJIT). Seen as: the score flames' intensity (button_callbacks.lua:2022)
  peaks at 2.27 in the oracle and 0.64 in the port for the same hand, and with it the ambient fire/organ volumes
  saved in settings.jkr (`ambient_control`). The same call decides `scale_number` (button_callbacks.lua:1910),
  `number_format`'s exponent (misc_functions.lua:961) and the rows of a challenge's banned cards
  (UI_definitions.lua:5767). There the oracle is the one that is wrong. The UI hook replaces the oracle's
  `math.log` with LuaJIT's result so these can be compared: with it the 22 values of the number format test (0 to
  1.797e308: thousands separators, the three decimal rules, e notation from 1e11, the two scale functions) are
  equal. The port's `log2(x) * (1 / log2(b))` is exact at the powers of ten the game floors; `log(x) / log(b)` is
  not.
- **The pad.** The first pad press of a run only switches the game to the pad (press twice). On the blind select
  screen the first press acts on whatever has the focus, and that differed between two scripts whose first press
  was the same (Select the first time, Run Info the second); both sides did the same each time. A pad script for
  that screen must be checked with a screenshot.
- Timing for writing cases: a Planet's level-up takes about 550 frames at DT=0.0166 and a Tarot about 300 (a
  command sent before the previous use ended is refused by `can_use_consumeable` on both sides); the cash out
  button exists about 700 frames after the win; with screenshots on, both programs draw every frame (10900 frames
  took 22 minutes on a loaded machine).
- The suites write in shared directories and run a shared binary: results taken while another suite ran or while
  the binary was being relinked showed differences ("exit 127", stray diffs) that a run alone did not. The sweep
  scripts therefore run from private copies of the binaries.

## 2. Boot and integration

Fixes made while the translated files were first linked and run together.

- `src/funcs.c` (new): the G.FUNCS registry (two open-addressing maps: element callbacks and args callbacks) and
  `funcs_register_all`, generated from the tags of `include/button_callbacks.h`: 124 plain, 21 `[args]`, 10
  `[wrap]`, plus `play_cards_from_highlighted` and the `discard_cards_from_highlighted` wrapper of state_events.h.
  The `[direct]` ones are not registered. `false_ret` and the sandbox callbacks register themselves where the Lua
  assigns them (UI_definitions_9.c, game_3.c).
- `src/main.c`: `funcs_register_all()` is called in `main()` after `classes_init()` and before `G = NEW(Game)`. It
  was called nowhere, so every `config.button` / `config.func` would have found no function.
- **A colour inside a table that reaches the save** (`recursive_table_cull` in src/misc_functions_3.c, `Card_load`
  in src/card_7.c): the first save of a run aborted in STR_PACK (a userdata at
  `.cardAreas.deck.cards.1.base.colour`). A playing card's `base.colour` is `P(float *)` (the convention for a
  colour in a table) and `Card_save` puts `base` in the save. The Lua copies the colour's table, so the cull now
  writes a V_PTR as a new table of its 4 numbers, and Card_load turns a `base.colour` that is a table back into a
  colour of its own. `src/string_packer.c`: the error for a value that cannot be packed now names the path of keys
  to it.
- **`G->CHALLENGES` was never assigned** (challenges.lua is one top-level statement and has no .c): the Stats
  screen showed "-nan% (0/0)" for Challenges and Progress, and every loop over the challenges saw none. `main()`
  now takes the key CHALLENGES of game.dat after `Game_init` (where main.lua requires "challenges"). game.dat is
  loaded once through `game_dat()` (data.h) instead of one private copy in globals.c and another in game_2.c.
- `src/love_pad.c`: the scripted pad (`PAD=`) kept 64 presses and dropped the rest without a word (the oracle has
  no limit), so a long script's last presses did nothing in the port only. Now 512. Test hook only.
- `src/main.c`: a `printf` with a real newline inside its string literal (written through a shell heredoc) stopped
  the file from compiling. A tool mistake; the reason sources are never written through heredocs.

State after this stage (optimised and sanitizer binaries, host):
- 2000 frames from boot with no input: no abort, no sanitizer report; screenshots at 300, 450, 600, 900, 1000,
  1200 and 1500 against the oracle's (same seed): at most 0.3 % of the pixels differ by more than 32/255 (the
  dissolve of the logo at frame 1000); the main menu (1200, 1500) is the same picture.
- `pad_round1.txt` + `pad_shop1.txt`, 12900 frames, `DT=0.0166`, TRACE every 10 frames: a new run, the whole
  tutorial of the first blind (run info overlay, a played hand, a discard), two more discards, a straight, a last
  hand, the win, the cash out, the shop (Joker and Empress bought, the voucher and booster steps of the tutorial):
  no sampled frame differs from the oracle in state, number of moveables or chips.
- A losing variant of the round (game over screen, New Run overlay), 8800 frames, TRACE every frame: the two differ
  on 10 isolated frames and on the 60 frames where the quip is another one.
- Options (settings tabs, stats, credits), collection (every page reached by the script), language list: same TRACE
  lines, screenshots equal to the oracle's.

## 3. Shop, booster packs, vouchers, consumables

No difference from the oracle and no crash or sanitizer report was found in this area; nothing in the game's C was
changed for it. What was added is the harness.

- `src/hook_shop.c`, `tools/hook_shop.lua`: the same commands on both sides, driven by `STEPS="frame:command ..."`
  (start a run on a seed, stake and deck without the tutorial, select the blind, win it, cash out, give money, put
  any card, pack or voucher in the shop, buy, use, sell, reroll, skip, leave, give a consumable or joker,
  highlight hand cards, play, discard, hover a card, print what the pad's cursor is on, and `dump`: every key of
  G.GAME and every card of every area as sorted `D` lines). `tools/shop_run.sh` runs both and diffs the `D` lines
  (and the TRACE lines); `tools/shop_padgen.py` writes a pad script and its steps; `tools/steps_shop_*.txt` and
  `tools/pad_shop_*.txt` are the cases. A made-up `e` for `FUNCS_buy_from_shop` / `FUNCS_sell_card` /
  `FUNCS_cash_out` is a `NEW(UIElement)` with only its config table set; `FUNCS_use_card(NULL, card, false, false)`
  needs none.
- Seen while starting: the order of `pairs(G.GAME.hands)` made `G.GAME.orbital_choices` differ (fixed in section 4,
  `lua51_pairs_order`).
- Verified equal with the hook (`DT=0.0166`, sanitizers on, every `D` line and every sampled TRACE frame; case
  names are `tools/steps_shop_<name>.txt`): `basic` (buy joker / planet / playing cards, sell, three rerolls, full
  joker slots, leave), `pad1` (the same by pad: no money, buy, buy and use, sell, rerolls 5-6-7, Buffoon pack pick,
  voucher, full slots, Standard pack skipped, next round; the cursor's target after every press), `pad2` (a round
  by pad: Tarot on two selected cards, Planet, sell, discard, play), `pad3` (Arcana mega pack by pad with hand
  cards, Standard, Celestial, Spectral skipped), `packs` and `packs2` (stake 8): the 15 kinds and sizes, picks and
  skips, `vouchers` (the 32, then 8 rerolls and the next blind), `tarot` (22), `planet` (12 and Black Hole),
  `spectral` (17), `states` (consumables used in blind select, round eval and inside Arcana / Celestial / Buffoon
  packs), `enh` (enhanced, sealed and foil cards played and discarded to the end of the round), `inter` (26
  shop-related jokers through a shop), `rerolls` (80 at stake 8: stickers and editions), `deck` with b_ghost,
  b_zodiac, b_magic, b_nebula, `hover` (the popup of every consumable, voucher and booster). Screenshots of the
  hand, the shop and the five pack screens: at most 0.11 % of the pixels differ (particles).

## 4. Saves, profiles, languages

Harness: `tools/saves.sh` (the two sides with the saves hook and a save directory each, TRACE lines pasted, the
hooks' `SUM` lines diffed), `tools/hook_saves.lua` + `src/hook_saves.c` (start a run with deck/stake/seed/
challenge, select a blind, play, continue the saved run, force the queued save, dump the save file / the profile /
the settings / every key of G.GAME sorted, change language, change and delete profile, close overlays),
`tools/saves_sweep.sh` (one new run per deck/stake/challenge/seed), `tools/saves_cont.sh` (save, continue, continue
with the two save directories swapped), `tools/saves_lang.sh` (a boot per language and a second boot from its
settings), `tools/jkrnorm.py` (a `.jkr` as text with sorted keys, to diff the files of the two sides).

- **A rule that depends on the Lua's hash order** (`lua51_pairs_order`, src/game_5.c, called in Game_start_run;
  `save_run` in src/misc_functions_3.c): the order of `pairs(G.GAME.hands)` decides rules (the hand of an Orbital
  tag, `G.GAME.orbital_choices`, was "Straight Flush" where the oracle said "High Card"; To Do List and the most
  played hand's ties read the same order) and the port walked the table in source order. G.GAME.hands is now put in
  the order Lua 5.1's pairs gives (string hash, node array, collisions as ltable.c) when a run starts: as one
  constructor fills it (a new run, and a loaded save, whose text is a constructor in the file's order). The culled
  copy that a save writes is filled key by key in the Lua and so has a third order: save_run reorders that copy
  the same way. With both, the hand order of a new run, of a continued run and of a run continued twice equal the
  oracle's, and so does the order in the file. It depends on init_game_object keeping the Lua's source order of
  the hands. The other places where a result could depend on hash order are listed in `docs/source-notes.md`; none
  of them was seen to differ.
- `Blind_save` (src/blind_2.c): `boss = false` was left out of the save (written only when true); the Lua writes
  the boolean once a blind is set and nil before. The file now has the oracle's key.
- Not fixed, only the text of the file differs (nothing reads the difference): `BLIND.triggered` is `false` in the
  oracle's file after a hand was played and absent in the port's (Blind.triggered is a bool: nil and false are
  one); colours and `config.card_w` are written with float digits (0.72549021244049 for 0.72549019607843).
- Observations: every comparison used a typed seed (and `G.GAME.seeded = nil` afterwards where the profile's
  statistics had to count). The chips' ease can start one frame apart (the event time tie); with the Plasma deck
  the eased value differs by 1 for some frames (float percent) and ends equal. Languages with a non-Latin script
  draw "?" for every character on both sides (the same picture): the runtime's font has only Latin glyphs.
- Verified equal to the oracle (SUM lines: areas' cards, jokers, tags, every key of G.GAME, the pairs order of the
  hands; TRACE state and moveables), sanitizers on in at least one run of each kind: settings / profile / meta /
  unlock_notify files written and read back on a second boot (the files' contents equal key by key); a run saved
  mid-blind, in the shop and at the blind select and continued (by hook, and once with the pad through Play /
  Continue tab / Play), a hand played after; the same with the two sides' save directories swapped; saves of
  challenge runs (eternal, pinned, negative jokers, consumables) and of held tags; a new run of each of the 15
  decks, each stake 1 to 8, 4 typed seeds, each of the 20 challenges, with the small blind selected and a hand
  played; a lost run (game over) and a won run (last ante's boss) with the profile's statistics, progress, deck
  usage and unlocks; the unlock notices (10 in a row); profile 2 (switch, a run, continue, delete) and back to
  profile 1; each of the 15 languages (changed at the menu, a hand played, then a second boot from the saved
  settings), with pictures of the menu and the run for de, ja and ru.

## 5. Blinds, bosses, tags

- `get_next_tag_key`, `get_new_boss` (src/common_events_4.c): the two debug lines that return `G.FORCE_TAG`
  (common_events.lua:1915) and `G.FORCE_BOSS` (:2347) had been left out as debug code; the members exist in game.h.
  Translated: the test hooks force the tag offered and the boss with them, as the oracle's do. Class: a skipped
  branch. It is the only change to the game's C in this area: nothing else differed.
- Harness: `src/hook_blinds.c`, `tools/hook_blinds.lua` (the same steps on both sides), `tools/both_blinds.sh`,
  `sweep_blinds.sh`, `build_blinds.sh`, `pad_blinds.sh`, `cases_blinds_{boss,tag,flow,more}.txt`. The hook starts a
  seeded run at frame 1300 (no tutorial: `tutorial_complete = true`, `tutorial_progress = nil`, else the first two
  tags are the tutorial's) and then plays by state, not by frame: at the blind select it selects, skips
  (`skip_blind` with the screen's own tag element) or rerolls the boss; in the hand it plays 5 cards, discards 3,
  wins (chips = the blind's, then a hand), loses (one hand left, one card), calls `blind:disable()`, sells joker 1,
  opens Run Info / the deck view; it cashes out, leaves the shop, skips a tag's pack, leaves the win screen
  (endless). Before every action it prints a `CMP` line: state, ante, round, chips, the blind's chips / name /
  disabled, dollars, hand size, hands and discards left, deck and playing cards, the sum of the hand levels, the
  boss chosen, the three blind states, the two tags offered, won, the tags held, jokers, consumables and hand cards
  (with d = debuffed, b = face down, h = highlighted), the shop's three rows with cost and edition, the reroll
  cost. Parameters: BOSS, FTAG, PLAN, HAND, ANTE, MONEY, JOKERS, UNSEED, STAKE, SEED, SPEED, NOAUTO (the pad plays),
  DBG=<frame> (the clocks and the head of the event queue for 30 frames). `both_blinds.sh` diffs the CMP lines
  without their frame number (the rules) and counts the frames apart and the TRACE lines that part. It runs a
  private copy of the binaries (`/root/bl-bin`), so a rebuild in the middle of a sweep does not break it.
- The boss, flow and first base sweeps ran with `DT=0.0166`, the tag sweep and the later cases with 1/64 (see "the
  clocks" in section 1).
- Verified equal to the oracle (every CMP line; sanitizers on; `SPEED=4`):
  - the run: seed BLINDS, 24000 frames, small / big / boss of antes 1 to 4 and into ante 5 (a hand, a discard, the
    win, cash out, shop left, next blind), the ante counter and the blinds' chips; two more seeds to antes 4 and 7
    with skips; stakes 5 and 8 (scaling 2 and 3, no small blind reward); antes 8 to 16 in endless (the formula past
    ante 8), the boss of ante 16 chosen by the game (a showdown boss).
  - the 28 bosses, each forced in ante 1 (and met again in ante 2) after two skips, with j_joker and j_jolly: a
    hand, a discard, a hand, `blind:disable()`, a hand, a discard, the win. Chips, dollars, hand size, hands and
    discards, debuffed and face-down cards, the debuffed joker, hand levels: equal. The Needle's one hand lost the
    run in both; a second run wins it.
  - Chicot against The Manacle and The Water; Luchador sold against The Water and Crimson Heart.
  - the 24 tags, each forced: taken by skipping the small blind, the big blind and the boss played, the shop (free
    / edition jokers, the extra voucher, coupon, D6), the packs of the five pack tags opened and skipped, Double
    with another tag and with itself, Boss (reroll), Orbital, Juggle, Investment after the boss; two runs with the
    game's own tags.
  - the boss reroll (`reroll_boss` with Retcon set, at the small blind and at the boss, and with the button of the
    next ante's screen).
  - losing (seeded, unseeded, to a boss): the game over screen; winning ante 8 (seeded, unseeded): the win screen,
    endless mode to ante 11.
  - by pad (the hook only starts the run): Select on the blind select screen, cards picked with the d-pad, a hand
    played, a discard; Run Info (poker hands and blinds tabs) and the deck view opened from the blind select.
  - pictures (port against oracle, 29 pairs): the blind select screen (fresh, with a skipped blind and a held
    tag), a boss hand with face-down cards (The Mark) and with every card debuffed (Verdant Leaf, before and after
    disable), Run Info (poker hands, blinds), the deck view, the win screen, the round evaluation after it, the
    shop of endless mode, the game over screen, hands by pad: at most 0.02 % of the pixels differ, except a card
    mid-flip (the 1/64 tie, 2 pictures, up to 0.43 %) and the quip on the game over screen (0.26 %).
- After the timers and delays became doubles (section 9): boss 28 of 28 and tag 24 of 24 run with no TRACE line and
  no CMP frame apart (before, 6 and 2 cases had lines apart).
- Not exercised: skipping a blind by pad (only through the hook), the hover popups of the blind chip and of a tag,
  the challenges' blind rules (`no_blind_reward` apart from the stakes'), a boss met with a saved and reloaded run.

## 6. Jokers and scoring

Scoring with every joker and every card modifier, compared with the oracle through hooks. No difference and no
crash of the port was found: nothing in the game's C and no shared header was changed for this area.

- `src/hook_jokers.c`, `tools/hook_jokers.lua` (the same steps on both sides through the game's own functions): at
  frame 60 a seeded run is started (`G.FUNCS.start_run`, seed JOKERS, tutorial off, game speed 4); at the first
  blind select the jokers of `CASE` (key+edition+stickers) and three consumeables are added (create_card /
  add_to_deck / emplace); round 1 is played by a list of actions (hands and discards whose cards are rewritten with
  set_base / set_ability / set_edition / set_seal, two consumeables used), the blind's chips are raised so every
  hand is played and the last one wins; then cash out, the shop (a sale, a reroll), the next blind (or a skip and
  the boss), a hand and a discard. The hook acts when the state has settled (no blocking event in the base queue,
  no lock, for 10 frames; the cash out button for 30), not at fixed frames, so a frame of drift cannot change what
  is done. Each step prints `JK` lines: chips, the hand's last chips and mult, dollars, round, ante, hands and
  discards, hand / joker / consumeable slots, deck and discard pile (count and an order-weighted sum),
  probabilities, interest, reroll cost, the round's targets (idol, mail, ancient, castle), the sum of
  G.GAME.pseudorandom (any random call out of step shows), every joker's ability table (sorted keys), sell cost,
  cost, edition, debuff, the consumeables, the hand's cards (rank, suit, enhancement, edition, seal, perma_bonus,
  debuff), the 12 hands' level/chips/mult/played, and the shop's cards and prices.
- `tools/jokers.sh` (one case, SAME/DIFF), `tools/jokers_all.sh` (the 150 jokers, keys from the oracle through
  `tools/hook_jokers_keys.lua`), `tools/jokers_cases.sh` + `tools/jokers_cases.txt` (128 named cases),
  `tools/jokers_pat.sh` (a pattern over the 150 jokers, e.g. `j_blueprint,%,j_brainstorm`). Outputs in `/root/jk`.
  A case takes 4 to 7 s in the optimised build. `ONLY=n` runs the port alone against the kept oracle lines.
- Not a bug of the port: a hook that uses a consumeable that cannot be used (Trance with no card highlighted,
  created by Sixth Sense / Seance) crashes BOTH sides (card.lua:1186 indexes a nil conv_card; the C segfaults at
  the same place). This was the segfault of the `destroy_erosion` case seen with and without the collector; the
  hooks now ask can_use_consumeable first.
- Results (optimised build): 150 of 150 jokers SAME alone; 128 of 128 cases SAME (8 enhancements played and held,
  a mix with editions and seals, 4 editions on cards and on jokers, 4 seals, 23 hand shapes covering the 12 poker
  hands, Four Fingers / Shortcut / Smeared / Splash / Pareidolia, Blueprint / Brainstorm chains, retriggers,
  destroyed cards, created cards, eternal / perishable / rental and stake 8, hand size and hands/discards jokers,
  Mr. Bones saving and not saving, the game over, a skipped blind and the boss).
  `j_blueprint,<joker>,j_brainstorm` for the 150 jokers: 150 SAME. Under -O0 with the sanitizers and the collector
  in, the port against the kept oracle lines: the 150 jokers and the 128 cases SAME, no sanitizer report.
- Not exercised: jokers whose trigger is outside this flow (Hallucination and Red Card need a booster opened or
  skipped, Invisible Joker's sale after two rounds, Yorick's 23 discards, Ring Master and Astronomer in the shop's
  pool and prices, Matador / Luchador / Chicot only against whatever boss the seed gives after the skip); one seed
  (JOKERS) and the Red Deck only; challenges and other decks; pictures (nothing was drawn in these runs).

## 7. User interface

Every screen, overlay and popup that was reached has the same UI tree as the oracle (node types, positions, sizes,
texts, scales, colours, ids, buttons, the cards of the areas shown) and the pad's cursor lands on the same node
after every press, except on the seed keyboard (below).

- Harness: `src/hook_ui.c` + `tools/hook_ui.lua` (the commands are listed at the top of the Lua file: start a run,
  give cards with editions / stickers / debuff, open any overlay by its G.FUNCS name, `ui:<box>` dumps a UIBox's
  tree as `U` lines, `pop` / `popall:<set>[:<variant>]` / `tagpop` dump popups, `tabs` and `cyc:<k>:<n>` choose
  every tab and turn every page of the open overlay and dump each, `order:<area>`, `nf` (number formats),
  `settings`; and an `F` line every time the pad's focused node changes), `tools/ui_run.sh` (both sides, then the F
  / U / D lines diffed by `tools/ui_diff.py`, which lets numbers differ by 0.01; a pad script may be written
  `@1300,40: a dpright*3 w2 hold:a:130`; `AUTO=<n>` dumps the overlay and the focused node's popup every n frames),
  `tools/ui_fuzz.sh` and `tools/ui_fuzz_loop.sh` (a random pad script per seed, from the menu, in a run, in the
  tutorial, each also with everything unlocked), `tools/ui_all.sh` (the ten written cases, one line each, 4
  minutes: run it after touching ui_*.c, text.c, UI_definitions_*.c or controller_*.c), `tools/ui_pairs.py` (native
  | oracle | difference pictures), `tools/steps_ui_*.txt` / `tools/pad_ui_*.txt` (the cases).
- One crash fixed, not a difference from the Lua: `DynaText_update_text` (src/text.c) crashed in `tget` as soon as
  a DynaText with a bound part (`{ref_table=, ref_value=}`: an option cycle, the HUD) was updated a second time,
  e.g. on opening Settings. An idle cache added for speed read `v.tab` after the block above it had put the
  composed string in `v`, so a string pointer was walked as a table. The part's table is now read again from
  `config_string[k]`. Class: an optimisation added after the translation (stale use of a reused variable). The
  scripted round never updates such a text twice, which is why the round did not find it.
- **Not fixed, a real difference:** on the seed keyboard (and any other grid whose rows are shifted by half a key)
  d-pad up or down from a key goes to the key below-left in the port and below-right in the oracle (W then down: A
  here, S there). The two candidates are at the same distance (controller.lua:1227, `dist = |x| + |y|`); what
  separates them is rounding noise of the layout, which is float here and double there. Nothing short of laying
  the UI out in doubles makes the port follow the PC on such a tie. Seen in 3 of 46 fuzz runs, always on that
  keyboard; the runs agree again as soon as the cursor leaves the tie.
- Not bugs of the port (both sides do the same): the "Continue" tab of the run setup, chosen with no saved run,
  fails on `saved_game` (UI_definitions.lua:6044; the game disables that tab's button); `G.FUNCS.unlock_all` needs
  the profile screen's overlay and `G.focused_profile`. The cards of the piles shown in the run setup and the
  collection are picked with `math.random` (not comparable; the hook prints a face-down card without its rank).
- Verified equal to the oracle (sanitizers on; U trees and F lines; pictures: at most 0.05 % of the pixels differ
  by more than 32/255, 80 screenshots in all):
  - the popup of a new card of every Joker (150), Tarot, Planet, Spectral, Voucher, Booster, enhancement and the
    base card, locked / undiscovered and unlocked; every Joker again as foil, holographic, polychrome, negative,
    eternal, perishable, rental and debuffed; the enhancements with each seal, foil and debuffed; negative Tarots;
    every tag's popup: 47 000 lines of descriptions with their variables, colours and badges;
  - every overlay opened from the menu with a fresh profile and again with everything unlocked (collection and its
    12 categories with every page, challenges list and the 20 descriptions, stats, card stats, customize deck,
    credits, languages, profiles, run setup with the 15 decks and the 8 stakes, settings, options), each with every
    tab; in a run: options, settings, run info (poker hands, blinds, vouchers), deck view (remaining, full deck),
    collection, stats (pictures of all of these);
  - by pad: the settings tabs (every cycle, slider and toggle moved; G.SETTINGS equal after), the customize deck
    screen, a round (cursor over the hand, the jokers and the consumables, cards selected with the hand preview,
    play, discard, a card dragged along the hand with the button held, the HUD at every step, button prompts), the
    in-run options, stats and collection (pictures of the round);
  - round evaluation rows with Golden Joker, Cloud 9, Rocket, Delayed Gratification, To the Moon and interest, the
    cash out and the shop's HUD; the game over and the win overlays; the unlock notices; a 1.5e13 score in the HUD;
  - 46 fuzz runs of 12 000 to 14 000 frames (menu, run, tutorial; fresh and unlocked profiles): about 150 focus
    changes and 10 000 to 80 000 tree lines each, no difference other than the keyboard tie, no sanitizer report.
- Known and not a difference: an event's time tie puts the round evaluation one frame ahead in the port from the
  frame the state changes; moveable counts then differ on the frames where a row appears.

## 8. Garbage collector

### A collector at all

The port at first never freed anything (the Lua had a collector): 2.3 KB of garbage a frame, 11 MB to 39 MB of
malloc in the 12900-frame scripted run. Now there is a collector, on in every build. `GC_OFF=1` (host) or `-DNO_GC`
turns it off (everything is libc's again); `GC_HEAP=<MB>` (host, default 64), `-DGC_HEAP_EE=<MB>` (PS2, default
12), `GC_STEP_KB`, `GC_STRESS=<n>` (a collection every n allocations, freed blocks filled with 0xDD).

- `include/gc.h`, `src/gc.c`: a conservative, non-moving mark and sweep. One arena cut in 4 KB pages; a page holds
  blocks of one of 21 size classes (16 to 2048 bytes) or belongs to a span of pages for one large block, so any
  pointer into a block finds its start, kind, mark and pin through the page table (four bit planes a page, in
  libc's memory: 140 bytes a page). Kinds: scanned word by word, atomic (never scanned), traced (a callback reports
  its pointers). Roots: the stack from the collector's frame to the address main gives gc_init (registers through
  `__builtin_unwind_init`), data and bss (`__data_start`.._end on the host, `_fdata`.._end on the PS2, minus the
  ranges of gc_skip), gc_add_root ranges. gc_frame() at the top of every frame collects when a quarter of the arena
  (or half of what the last collection left free) was allocated; an allocation that finds the arena full collects
  on the spot, and aborts with the sizes if that does not help. No finalizers: nothing needed one.
- `src/core.c`: tables are traced blocks (only V_STR/V_TAB/V_OBJ/V_FN/V_PTR values are followed) with atomic
  arrays; strings are atomic and the intern set is weak (libc's memory; unmarked strings leave it before the
  sweep); objects, NEWCTX and Fn are scanned blocks. tfree frees at once; object_free stays empty.
- `tfreeze(Table **slot)` (core.h): the static data (game.dat, the localization: 15 of the 18 thousand tables alive
  at the menu, 2.9 MB of 3.5) is pinned with all it leads to at the first collection and not walked again; a write
  barrier in the table writes puts a frozen table that takes a pointer on a dirty list that is walked as a root. A
  slot that holds another table later (a change of language) unpins everything and freezes again. Called in data.c
  (game_dat) and game_3.c (the localization).
- `include/all.h`: `malloc`, `calloc`, `realloc`, `strdup`, `free` of the game's sources are the collector's (free
  still gives libc what is libc's: the buffers of love_filesystem_read, getDirectoryItems, love.data).
- The platform's handles: Quad (one per Sprite), Image/Canvas, Font: atomic blocks; Text (DynaText letters, UI text
  drawables): a scanned block with an atomic copy of its characters, Text_release frees at once; Source: stays
  libc's (the Lua releases them itself, sound_manager.lua:121); Shader: libc's, made once at start; textures,
  samples, file buffers: libc's.
- `src/love_misc.c`: a Channel and its queue of Vals were libc's memory, the only holder of the tables waiting in a
  channel: now the collector's (the one place where the platform kept game pointers).
- `src/love_gfx.c`, `src/love_gs.c`: gc_skip for the swirl tables and the GS packet (1 MB of the PS2's 1.37 MB of
  bss). `src/main.c`: main() only gives the collector the stack's bottom and calls game_main (not inlined);
  gc_frame() at the top of love_run_frame; the PERF line prints `gc used / free / allocated KB, collections, last
  and worst ms`.
- Found on the way: (1) the address sanitizer moves address-taken locals to a fake stack in its heap, where the
  scan does not see them: gc.c turns that off (`__asan_default_options`); (2) setjmp does not write all of its
  jmp_buf: zeroed first; (3) the marking left the last blocks it looked at in its own frame, and the next
  collection took them for roots: the stack is scanned from a frame above the collector's working ones; (4) the
  write barrier ran before the allocation of the same table operation, and a collection inside that allocation
  could freeze the table in between: the barrier now comes after the last allocation (GC_STRESS=3 and 5 found it,
  as a nil field at boot; 2, 7, 11 and 20 did not).
- Verified (host): `test/gc_test.sh` (classes, interior pointers, roots, weak set, traced kind, pins, realloc, a
  random graph against a model; also under the sanitizer). The scripted run (12900 frames, TRACE every 10): the
  same 1290 TRACE lines as without the collector with GC_STRESS=1 (489107 collections), 3, 5, 20, with a 12 MB
  arena, and under the sanitizer with GC_STRESS=100; against the oracle with GC_STRESS=20: 0 lines of 1290 part.
  `tools/gc_cases.sh` (the port alone, GC_OFF against GC_STRESS=23): 9 joker cases, 13 blind cases, 4 shop step
  lists, a saved run and a second start from its files: the same output (the save files byte for byte).
- Numbers (host, 64-bit): alive after a collection 3.5 MB at the menu (2.9 pinned), 4.9 MB in the blind, 5.3 MB in
  the shop; allocation 0.07 KB a frame at the menu, about 1 KB in play with bursts of 10 to 30 at transitions, 2.85
  on average; 12 MB arena: used stays between 5 and 7 MB, 13 collections; the smallest arena that completes the run
  is 6 MB (5 runs out at frame 1700). Pause: 2.0 to 3.4 ms before freezing (18 to 23 thousand tables walked), 0.5
  ms at the menu and 0.8 to 1.8 ms in play after (2 to 7 thousand).
- Tools: `gc_run.sh`, `gc_check.sh` (one binary: off, on, 12 MB, stress), `gc_cases.sh`, `gc_prof.sh`,
  `gc_time.sh`, `gc_variant.sh`, `gc_tree.sh` (a private copy of the tree built apart).
- Known limits: a pointer one past the end of a block does not keep it; conservative retention is not measured;
  `test/core_test.c` did not compile against the current core.h at that point (its own arr_from).

### Collection in slices

The first console run: a collection every ~700 frames in play, each one pause of 36 to 51 ms (21 ms at the menu):
two or three frames lost every 12 s. The same collections on the host were 0.8 to 1.8 ms, so the console is about
30 times the host for this code, more where the memory is read out of order. The collection is now done in slices,
one per frame.

- Where a whole collection went (host, GC_HEAP=12, in play): roots hook (dirty frozen tables) 0.03 to 0.07 ms,
  stack 0.001, data and bss 0.05 to 0.09, marking 0.7 to 1.5 (5 to 7 thousand tables traced, 1000 to 1300 scanned
  blocks, 430 to 530 KB of them), weak set of the interned strings 0.07 to 0.12, sweep 0.04 to 0.09. Marking the
  tables was three quarters of it.
- `src/gc.c`, a cycle in slices (phases ROOTS, MARK, final step, WEAK, SWEEP; the comment at the top of the file
  has the argument): gc_frame runs one slice per frame, bounded by work units and not by the clock (a word read in
  a row is 1, a pointer reported by a table's tracer 9, a block taken from the mark stack 12, a slot of the string
  set 10, a page swept 48: about what each costs on the console, where a cache miss is dear). Default 60000 units
  (`GC_SLICE=<n>` on the host, `-DGC_SLICE=<n>` on the PS2).
  - Tables: every store of core.c already went through the write barrier; while `gc_barrier` is set it marks the
    stored value (and a new key) at the store. A table that was traced never comes to hold an unmarked block.
  - Objects, closures' contexts, the stack and the statics are written by plain C: nothing tells the collector. So
    the cycle ends with ONE step that is not cut: the stack, the data and bss and every marked scanned block are
    scanned again, and what that finds is marked to the end (nothing, in every cycle of the scripted round).
  - A block allocated during a cycle is born marked (until the sweep has passed its page); a block freed by hand
    may still be on the mark stack, so what is taken from it is checked to be a block of that kind.
  - The weak set is dropped in pieces after the final step: `intern()` marks an entry it finds while that is on
    (`gc_reviving`), and a rehash of the set starts the pass over.
  - The sweep is some pages per slice. A full arena, `gc_collect()` and `GC_STRESS` finish the cycle in progress
    and run a whole one at once; a cycle still on when as much as a collection's worth was allocated since it began
    is finished at once (slices too small for the allocation rate: the heap stays bounded).
  - The page table keeps the four bit planes of each 32 blocks side by side (one cache line per lookup, not four).
- PS2 roots: `.rodata` and the constants up to `.sdata` are no longer scanned (Makefile.ps2 writes gc_ee.ld, two
  symbols for the linker; gc.c skips between them when they exist: 121 KB); glyphs, flames and the flash's two
  pictures are skipped too (love_gfx.c). What is still scanned on the console: 9 KB of data and about 270 KB of
  bss, of which 96 KB are four thread stacks and 46 KB are platform buffers (a gc_skip each would take them out).
- Numbers (host, GC_HEAP=12, the scripted round): a cycle is 9 to 18 slices (frames); a slice of 60000 units takes
  0.05 to 0.10 ms; the final step is 76 to 134 thousand units and 0.09 to 0.31 ms: 20 thousand of roots (on the
  console about 70 thousand: 4-byte words, more bss) and 55 to 105 thousand for 760 to 1470 scanned blocks (430 to
  820 KB), and it found nothing to mark in any cycle. By class, late in the shop: UIElement 341 (341 KB), Sprite
  202 (126 KB), Card 58 (58 KB), UIBox 55 (27 KB), Moveable 61 (26 KB), DynaText 15 (11 KB), CardArea 9, Event 9,
  Particles 2, others (contexts, colours, letters) 509 (110 KB); tables alive 22 to 24 thousand (15 thousand of
  them frozen), 5 to 7 thousand traced per cycle. The very first cycle also freezes the static data in its first
  slice: 0.8 to 1.2 ms on the host, once (and once more after a change of language).
- Estimate for the console (not measured when this was written): 30 ns a unit gives 1.8 ms a slice and 3 to 6 ms
  for the final step. The PERF line prints what it is: `slice <units> units worst <units> final <ms> worst <ms>
  <units>`, after `last` (the longest slice of the last cycle) and `worst` (of all, in ms). If the final step is
  over the budget there, what is left to cut is its scan of the objects: UIElement is 1 KB of mostly numbers; a
  table of where the pointers are in each class would leave a sixth of it.
- Heap: the garbage made during a cycle waits for the next one (a cycle is under 20 frames, 1 to 3 KB a frame). 12
  MB: used between 3.7 and 8.3 MB over the round, 13 cycles. The smallest arena that completes the round is 7 MB; 6
  MB runs out near frame 3300 asking for 32 or 64 KB in one piece with 0.9 MB free in scattered pages (the same
  with whole collections: it is the fragmentation of a non-moving heap, not the slices).
- `GC_STAT=1` (host) or `-DGC_STAT` (PS2): a GCSTAT line per cycle (slices, the worst, units and ms by part, live
  blocks and KB by kind) and a GCLIVE line (live scanned blocks by class).
- Verified: `test/gc_test.sh`, 10 settings (5 of them with a slice at every 1 to 3 allocations of 1 to 300 units).
  `tools/gc_check.sh` (the round, TRACE against the run without a collector): 64 MB, 12 MB, GC_STRESS=20 and 3, and
  a slice at every allocation of 1, 9, 300 and 60000 units, at every 3rd of 5000 (12 MB), at every 7th of 40000,
  slices of 1000 with an 8 MB arena and poisoned frees: 1290 identical lines in each. `tools/gc_cases.sh` with
  `SLICE=700` and a slice at every allocation: the 27 hook cases and the two save runs print what they print
  without a collector (the save files byte for byte). `tools/perf_suites.sh` (all parts): the round against the
  oracle 0 of 1290 lines apart at both dts, GC_STRESS=20 round identical, blinds boss 28, tag 27 and flow 13 cases
  with no rule difference (flow: the one case with TRACE lines apart that the run before this change also has),
  the 13 shop lists, the 7 save and 9 continue cases with nothing different, gc_cases 27 SAME. `tools/ui_all.sh`:
  10 cases ok.

## 9. Performance of the game code

The game side's CPU cost on the PS2: table lookups by string key, soft-float doubles, sinf/cosf of large angles.

### Keys interned once per call site

A PS2 profile had a third of the CPU in lookups by string key (tget, hfind, intern_find, memcmp, strcmp).
tget/tset/tnum/tstr/ttab/tbool/tobj/tsetn/tsets/tsett/tsetb/tseto are now macros: where the key is a literal
(`__builtin_constant_p`) the call site keeps its interned string in a static (bss: a root, so the weak intern set
never drops it; nothing moves) and calls `*_at(t, key, &site)`, which searches by that pointer with the hash stored
in front of the characters; any other key goes the old way. The key's text is expanded twice (once unevaluated),
every other argument once. tgetv/tsetv with a string Val and the PS2's TAB (tab_from) use the interned pointer
directly (every V_STR is interned).

- Host, 12900-frame run, optimised, min of two: user 8.12 s -> 7.19 s, PERF update 0.210 -> 0.167 ms, draw 0.429 ->
  0.385 ms; built as the PS2 is (`-DTAB_ARRAYS`): user 6.59 -> 5.38 s, update 0.166 -> 0.119 ms (-28 %), draw 0.348
  -> 0.294 ms (-16 %). TRACE identical, also with GC_STRESS 3, 5, 20 and against the oracle.
- PS2 build: 51 s (no blow-up), text +89 KB, bss +30 KB (a pointer per call site).
- Tried and left out: a linear scan of the entries for tables of 8 keys or fewer (no gain on the host: 10.87 s
  against 10.36 s); `S("literal")` interned per site as well (no gain in the PS2-style build, +186 KB of text).

### First pass: lookups, floats, trigonometry

Measured on the host with the PS2-style sources (`tools/perf_game.sh <tag> [flags]` builds them, `... run <tag>`
plays the scripted round, 12900 frames, DT=0.0166; `-pg -fno-inline` + `tools/perf_callers.py` gives calls per
frame per caller). No struct member was added for a Lua field in this pass.

Lookups (calls per frame, scripted round): lookups by key and tnext 9308 -> 8042; hash probes (hfind + find_k) 6986
-> 5789; lookups with a key that is not a literal (hashed and searched among the interned strings every time) 666
-> 68; intern_find 785 -> 121; fmt 20 -> 4.4; number_tostring 6.5 -> 1.3. A literal lookup is now one call (it was
four, each returning a 16-byte Val).

- `src/core.c`: `find_k` / `find_at`: the `*_at` readers search in place and take the value by pointer. tnext: the
  key of an array slot comes from a table of 1..256 as numbers, not from an int to double conversion (a library
  call on the PS2, 2200 a frame).
- `include/core.h`: streq is inline and compares the first characters before calling strcmp; `wrap_angle(x)`.
- UI_colour (ui.h), Card_child (card.h), CardArea_child (cardarea.h), SHADERS_get (game.h), arith
  (sound_manager.c) are macros, so a literal key is interned per call site (they were functions with a key
  parameter: 194 + 247 + 17 + 25 + 49 slow lookups a frame). `__builtin_constant_p` of such a parameter is false
  after inlining with gcc and with the PS2's compiler at -O1/-O2 (checked: a site's static is never shared by two
  keys).
- Values read once instead of per element or per letter: `config.draw_after` and the draw layer
  (UIElement_draw_children), config.scale / config.object and the font's numbers (UIElement_draw_self),
  `config.type` (CardArea_draw, CardArea_align_cards), config.pop_out / pop_in / pulse / quiver per text
  (DynaText_align_letters). DynaText_update_text: prefix..tostring(value)..suffix of a bound part was formatted and
  interned every frame; the last one is kept in the part with the value (bit for bit) and the prefix and suffix it
  was made of.
- `wrap_angle` (core.h): an angle beyond 200 radians is brought to [-pi, pi] in float (2*pi in two parts) before
  sinf/cosf, instead of newlib's `__kernel_rem_pio2f`; below 200 the bits are the same. Used for the ambient tilt
  (3 calls per card drawn), the cards' sway and the other per-frame sines.
- double sin -> sinf where the result is drawing: Game_update's edition colours, update_canvas_juice's room sway (6
  a frame), Blind_align (4), AnimatedSprite_animate (3 per floating sprite). DynaText_align_letters: the letters'
  rotate / quiver / float / bump in float (it was all double, with a double sqrt and sin per letter per frame);
  the pop-in / pop-out timing and the pulse keep their doubles (thresholds).
- Effect on TRACE (scripted round, against the build before): state, moveables and chips equal on the 1290 lines;
  `sum` differs on 1053 of them, by at most 1.5e-10 relative (sway rounded in float instead of double, or reduced
  before the sine).

### G.TIMERS as doubles

- `include/game.h`: TIMERS.TOTAL / REAL / REAL_SHADER / UPTIME / BACKGROUND, SPEEDFACTOR, ACC and real_dt are
  `num`; TIMERS_get returns `num *`; Game_update, love_update and EventManager_update take a num dt. LoveConf.dt,
  love_timer_step and love_timer_getDelta are double (the host's `DT=` was read through a float). A shader send
  that points to a timer is `ref_num`; ref_float stays for float members. Per-frame code that draws or animates
  takes `(float)G->TIMERS.REAL` once and stays float; the update loop's dt * SPEEDFACTOR is computed once.
- At first the delays still reached the event as floats (about a hundred call sites), so 0.2 arrived as
  0.20000000298 and the Lua's ties were lost by a frame; a stopgap in src/event.c took a delay within 2e-7
  (relative) of a number of six decimals to be that number. It is gone (next pass).
- Against the oracle, the scripted round, 12900 frames, TRACE every 10: default dt: 0 lines of 1290 part (with num
  timers alone 44 parted, the first at frame 4850, a frame late in the scored hand; before, a frame per timed
  event); DT=0.0166: 0 of 1290. Under the sanitizers (DT=0.0166): no report, same lines.
- Left as float: Controller.overlay_timer, the particles' own clocks (last_real_time, age), the juice's start/end
  times, G.vortex_time: cosmetic, small ranges.

Host timings of the first pass (PERF means, PS2-style -O1 build, a loaded machine where the same binary varied by
a factor of two between runs): back-to-back runs, minimum of 9: update 0.145 -> 0.117 ms, draw 0.336 -> 0.291 ms
(draw is mostly the software paint here); gprof's total for the run 8.19 s -> 5.80 s, of which paint about 2 s in
both. The call counts are the robust numbers.

### Second pass: delays as doubles, table stamps, snapshots

1. **The delays are `num`.** The time arguments of `delay`, `draw_card`, `ease_value` / `_f` / `_n`, `ease_colour`,
   `play_area_status_text`, `juice_card_until`, `Card_explode`, `Card_start_dissolve`, `Card_start_materialize`,
   `FUNCS_end_consumeable`, `FUNCS_wipe_on`, `Particles_fade` are num (common_events.h, card.h, button_callbacks.h,
   node.h), `G.MIN_HOVER_TIME` too; 93 float literals and casts at their call sites in 20 files became doubles
   (`tools/delay_num.py`, re-runnable: it changes nothing when nothing is left), plus `delay_fac` of use_card and
   the two defaults inside draw_card and play_area_status_text. A test build that printed every delay within a
   float's error of a six-decimal number found the last three; over the round and the 27 hook cases of
   `tools/gc_cases.sh` none is left. Rule: a delay is written as the Lua writes it, `delay(0.2, NULL)`, never 0.2f.
2. **A stamp on every table** (core.h `tstamp`, core.c): a number that changes whenever any key of the table is
   added, removed or given another value, through any function or alias; writing the same value again leaves it; no
   stamp is given twice. Per-frame code keeps what it read with (table, stamp) and reads the table again only when
   the pair changed: exact, with no list of writers to keep. `tarr` / `tarr_next` (core.h) walk a list without a
   call per entry, asking for the array again only when the stamp changed. sizeof(Table) + 8 (it stays in its size
   class). What reads through a snapshot (members that are not the Lua's, said so in the headers):
   - game.h `SETTINGS_snap()`: SETTINGS.paused, .reduced_motion, GRAPHICS.shadows == 'On' (Moveable_move,
     Event_handle, Particles, Game_update, Sprite_draw_shader, juice_up, the UI's and the cards' shadows).
   - node.h `Moveable_refresh_movement()`: config.refresh_movement; UIElement_update_object no longer writes `true`
     over `true` every frame. Sprite.dyn_snap (sprite_pos, video, shader_tab), AnimatedSprite.anim_snap
     (offset_seconds), Moveable.shader_time (the shader's `time`, a function of the node's ID: its doubles are
     computed once per node).
   - ui.h `UI_snap(e)`: button, button_temp, button_delay, func, scale, outline, outline_colour, chosen, text,
     text_drawable, ref_table, prev_value, object, button_UIE, colour, lang, shadow, hover, force_focus, draw_after
     (draw_self, update, update_text, update_object, draw_children); `font_nums`, `lang_font`; update_text and
     update_object return at once while neither config nor the bound table / the object's dyn was written.
   - card.h `Card_kids()`, `Card_ab()`, `Card_cen()` (children by name; ability.name / set / effect / consumeable /
     eternal / rental / perishable / perma_debuff; config.center and its discovered / unlocked / demo / alerted /
     soul_pos), cardarea.h `CardArea_type()`.
   - text.c: config.string / pop_in / pop_out / pulse / quiver per DynaText; a bound part is skipped while its
     table and the table it is bound to are unwritten; config.W / H are written only when changed.
   - sound_manager.c: the sound codes with "music" and the ones with an ambient_control entry are listed once per
     state of SOURCES / ambient_control instead of walking 125 codes three times a frame.
   Two fields became real members (every reader and writer changed): `UIBox.attention_text` (ui.h) and
   `Card.send_to_shader` (card.h); both were in the object's dynamic table. Game_update's and Game_draw's walks of
   MOVEABLES, G.I.* and ANIMATIONS use tarr.
3. **Numbers** (scripted round, calls per frame): hash probes (find_k + hfind) 5789 -> 940 (6986 before the first
   pass); lookups by key and tnext 8042 -> 1879 (9308); tnext 2200 -> 869; per caller: UIElement_draw_self 1655 ->
   0, Moveable_move 706 -> 0, UIElement_update 647 -> 0, Card_draw 579 -> 142 (tnext over children), Card_update
   243 -> 0, UIElement_update_text 206 -> 36, Sprite_draw_self 199 -> 0, DynaText_update_text 174 -> 15,
   UIElement_draw_children 543 -> 371 (tnext), Game_update 1033 -> 20, MODULATE + AMBIENT 367 -> 205. TRACE of the
   round: the same 1290 lines, `sum` included, as before this pass. Host PERF means, PS2-style -O1 build, a quiet
   machine (load 1.6), five interleaved runs each within 2 %: update 0.0753 -> 0.0444 ms (-41 %), draw 0.197 ->
   0.149 ms (the baseline is the binary from before the first pass, so draw also has the platform work of section
   10).

Verified on the tree after this pass: the round against the oracle, default dt and DT=0.0166: 0 of 1290 lines
part; under the sanitizers: no report, same lines; GC_STRESS=20: same lines; `tools/ui_all.sh` with sanitizers: the
ten cases ok; blinds boss 28, tag 27, flow 13 cases: no CMP difference, no CMP frame apart, TRACE lines apart only
in `win8u` (4 of 900, single frames; the binary from before parts on the same four); shop basic, packs, packs2,
vouchers, tarot, planet, spectral, states, enh, inter, rerolls, deck, hover: 0 D lines different, 0 TRACE parts;
saves_sweep (7 cases) and saves_cont (3 cases, save / continue / cross): 0 SUM lines differing; gc_cases (GC_OFF
against GC_STRESS=20): 27 SAME, the two saves cases differ only in the directory name they print.
`tools/perf_suites.sh [parts]` runs all of it and writes one line per case.

Left: Sprite_draw's draw_steps and send_custom (about 250 lookups a frame: a step's ten keys and each send entry's
five, for the background and the flames), SET_SFX and snd per playing source (about 100), the flame handler (59),
the 68 lookups a frame with a key that is not a literal, the tnext walks of children with named keys
(UIElement_draw_children, Card_draw). Doubles still per frame: update_canvas_juice and modulate_sound (the eased
cursor and the volumes live in tables, some saved in settings.jkr: a few dozen operations once a frame),
AnimatedSprite's frame index. Neither pass was built or run for the PS2 when it was made: the cost of each change
there was inferred from the host.

## 10. Performance of the platform layer and frame pacing

### The frame's packet

The platform's share of a frame (love_gfx.c, love_gs.c, love_snd.c) on the EE. Measured in PCSX2 on the scripted
round (`-DTEST_DT=0.0166f -DPROF -DPROF_FROM=3400 -DPROF_TO=5800`), both sides built from one copy of the game's
sources so only the three files differ. Samples of the 1 kHz profile by file:

| ms a frame, frames 3400-5800 | before | after |
|---|---|---|
| love_gs.c (without the wait for the blank) | 8.08 | 1.77 |
| love_gfx.c | 5.24 | 4.61 |
| love_snd.c | 0.80 | 0.54 |
| the platform | 14.12 | 6.92 |
| busy time of a frame | 22.81 | 14.96 |
| waiting for the vertical blank | 0.17 | 2.41 |

PERF / SHOWN at the same frames: blind select (2400) 26.9 ms a frame, draw 19.4, 4233 primitives, 10.5 ms a picture
-> 17.4, draw 11.9, 1410, 0.3; a hand (3600) 21.6, draw 15.8, 2620, 10.1 ms a picture -> 16.7 (the blank), draw
9.5, 730, 0.25; the heaviest second (5520) 34.2, draw 28.1 -> 24.2, draw 18.5. The packet of a frame: 300 KB and
more -> 46 (a hand) to 114 KB (blind select).

- love_gs.c, the frames' lists: a frame is now the GIF packet itself, written while the game draws (corners in
  sixteenths of a pixel, REGLIST: 8 bytes a register, a tag per run of quads of a kind), with a small index per
  primitive (where its colour and corners are). The presenter no longer builds a packet per frame of the game from
  floats (it was 45 % of its time): it pairs and tells what moved on the packet's integers in one pass, keeps the
  own values of what moves, and patches those. A frame whose first picture is already itself (the game keeping up
  with the screen) is not paired at all.
- Polygons: `gs_fan` (love.h): a filled polygon is one triangle fan of the GS, not a triangle a corner. They were
  70 % of the primitives (232 polygons of 2986 triangles at blind select; glyphs 788, lines 420, sprites 35): 4233
  primitives -> 1410, same triangles. The software rasteriser's path is as it was.
- A picture fix that came with it: the old packet held 3072 primitives and blind select has 4233, so the
  tutorial's overlay (drawn last) was missing on the PS2 at frames 1900 and 2400. It is there now, as on the host.
- Primitives that are clear (alpha 0) or wholly off the screen are not recorded (373 at blind select); texels of
  one page drawn as they are (every glyph, most sprites) skip the page loop's four divisions, with the same floats
  to the bit; a shader recipe's texels come from a palette kept per atlas.
- love_gfx.c: a shader's uniforms the recipes read are numbered when first sent, and a sent name is found by its
  pointer first (it was about 100 strcmp a sprite: most of the profile's strcmp); a shader's recipe is found once,
  when it is made; no sine for a rotation of 0; the dissolve's edge term from a table per column and row (two
  divisions a texel less, same sums) and its tint table made when a texel first asks for it, not every blit.
- love_snd.c: the mixer's voice state in locals for a chunk (same samples).
- `-DPRIMSTAT` (love_gfx.c): a STAT line every 60 frames, primitives by kind.
- Same picture: host, 11 screenshots of the round (600 to 5290) byte for byte the same as before, TRACE and the
  sound (6000 frames) too. PCSX2, `-DSHOTS` at 10 frames from 1250 to 5600: 8 of 10 identical to the pixel; 1900
  and 2400 differ by the overlay above, where the new one is the host's.
- Tried and dropped: the background's paint in fixed point or in table units (the loop is 62 instructions a round,
  5 lookups: integers save none of them, and the texels would differ by a palette step); fewer swirl rows a frame
  (changes how fast the background moves); a separate entry for glyphs (20 % of 0.6 ms).
- Left: paint + swirl_rows are a third of the platform (2.2 ms a frame, 960 texels); dissolve_texel 0.6 ms while
  cards dissolve; text + raster + textured + quad 1.2 ms for ~1100 quads; the mixer and its decoder 3 % of the
  time whatever the frame rate. At that point only PCSX2 had run REGLIST packets and fans (they are plain GS).

### Pacing, after the first console run

The console showed a picture on about three blanks of four whenever the game's frame was over a blank, unevenly,
and a picture cost it 3 to 7 ms by the SHOWN line (0.3 in PCSX2).

- Why pictures < blanks: the presenter reached the newer frame "when the next is due, taken to last as long as the
  one before", in whole blanks. At a blank and a half a frame the frames take one blank, two, one...: after a
  one-blank frame the next picture was already final and the following blank had nothing to show; after a two-blank
  frame the motion was spread over two blanks. Now (love_gs.c, "presenting") a frame's time is the EE's count
  register when the game finished it, three finished frames are held, and at every blank the screen shows the game
  as it was `delay` before (the longest of the last four frames + a quarter of a blank), between the two frames
  around that time. While the game keeps up (30 frames in a row within a blank) the newest frame is shown as it
  is, once, as before; from one way to the other the time shown runs at half or one and a half times the clock,
  never back, never a jump.
- `-DFRAME_US=<us>` (and `-DFRAME_VARY=<us>` more on one frame of three; love_misc.c): a frame of the game lasts at
  least that long, a console's pace in PCSX2. `-DPACE`: a PACE line with each SHOWN line, how far the time shown
  moved at each of the last 32 blanks in sixteenths of a blank (16 is even). PCSX2, the scripted round: 20000 us
  (1.2 blanks): 72 pictures in 72 blanks for 60 frames; 25000 (1.5): 90 in 90; 33400 (2): 120 in 120; 41700 (2.5):
  150 in 150; 18000 with 14000 more on one frame of three: 82 in 82. Of the PACE values 84 to 89 % are 16 and the
  rest 15 (the blank's own jitter, rounded down) but for the 1 % at the changes of screen. A picture is 0.05 to 0.2
  ms there. Pictures (`-DSHOTS`, 1250 to 5100) are the ones of before but for the swirl's place in its turn.
- The SHOWN line's "ms each" was twice the truth: the count register runs at the EE's clock (294.912 MHz; a blank
  is 4917000 counts in PCSX2), not at half of it. The blank's length in counts is now measured (the shortest of the
  first two seconds') and printed (`blank N`), so the console says which it is.
- What a picture touched, and no longer does. (1) The frame's packet is written through the uncached accelerated
  window and never read: the game's writes load no line of the 8 KB data cache and the presenter flushes nothing
  (`-DNO_UCAB` puts it back through the cache if a console shows broken primitives). (2) Pairing is done once per
  frame by the game's thread on a shadow of the frame in ordinary memory (colour, texels, corners, 32 bits each: a
  third of the packet), and leaves the list of what moves with both ends of each value, so a picture walks only
  that list and writes only those words. (3) A texture's place in VRAM is written into the packet only when it
  changed. (4) The swirl's palette is a texture of its own: it changes every frame in the menu and at every colour
  change, and as the pictures' CLUT it took both pictures' 72 KB of texels to the GS again each time, the presenter
  waiting for the transfer. That is the likeliest of the console's milliseconds (the menu, where nothing is paired,
  was 3.3 "ms" a picture too).
- The swirl: one row, then two (half of SW_ROWS) while the game is not keeping up (`gs_keeping_up`): about 1.1 ms
  of a frame in PCSX2's terms, three times that on the console; the background then takes twice as long to change.
- A bug on the way: GetCop0 (a system call) in the blank's handler; an ELF built with it stops right after "BOOT
  game made" with `padman: DMA Busy`. iGetCop0 now.
- Not done: the dissolve (a card and its shadow could share one picture), a cache of static UI text.

### Frame times in a long run (PCSX2)

From the log of a 13 400-frame pad script (menu, a new run, the three blinds of ante 1, two shops, a Buffoon Pack,
the run info overlay; PERF/SHOWN lines): the rounds are at 16.67 to 16.99 ms a frame; the shop is not: 18.1 to
18.9 ms a frame through the first shop (frames 5340 to 5880, 65 to 67 blanks for 60 frames), 17.8 in the run info
overlay (6420 to 6660), 18.7 entering the second shop (9480), 19.4 to 20.3 in the third (13680 on, two Celestial
packs on sale), 21.3 at the start of the run (1560).

## 11. Text sizes and video modes

How text is sampled and placed, and the picture's modes. Nothing of the game's code or layout changed: metrics,
advances and UI trees are as they were (`tools/ui_all.sh`: the ten cases ok; `tools/both.sh` over round 1, 870
TRACE lines, no PARTS).

- **The font's levels** (src/love_gfx.c `levels_load`, `text`; `tools/font_levels.py`): the 16 px glyphs again at
  every whole pixel size from 5 to 16, as coverage (how much of each pixel the glyph's squares cover, exactly;
  raised to the power 0.5 so half-pixel strokes are not half as bright) in the alpha of a 256-entry palette. A
  file pair per picture size, beside the font's: `m6x11plus.ttf.<w>x<h>.tex` (256 wide, 847 to 1306 rows, no glyph
  across a page) and `.txt` (the boxes). A text takes the level nearest its size on screen; one that is upright
  and at rest is drawn a texel a pixel on whole pixels (the string's origin rounded once, each glyph's pen rounded
  from it, the baseline shared). Above 16 px the 16 px glyphs between texels, as before. Without the files: as
  before, and a `gfx:` line. The generator is run on the extracted game directory; the disc pack takes .tex and
  .txt as they are. They cost up to 195 KB of VRAM in a frame (3 pages and a palette).
- **At rest or not** (`resting`, fields of `Text` in love.h): a Text remembers where each of its two draws of a
  frame (shadow, glyphs) was; one that moved more than 0.05 px (or changed size) is drawn between texels, from its
  level, until it has been still for 8 frames. So DynaText's pop-in, bump, quiver and float do not go in steps,
  and the room's slow sway (hundredths of a pixel a frame) does not unsettle the rest. Rotated text (and vertical
  labels) is never put on whole pixels.
- **Half a pixel on the GS** (`HALF`): the GS samples a pixel at its corner, the software rasteriser at its middle,
  so every glyph was drawn half a texel off on the console, between two texels even at 1:1. Text is now moved half
  a pixel on the EE. Sprites still have that offset (not changed).
- The picture's scale (`out_x`, `out_y`, `love_gfx_scale`): raster() and the EE's polygon path stretch the game's
  640x448 over the mode's pixels; only text looks at the scale itself (levels, whole pixels). Host: `VIDEO=720p`
  (640x720) and `VIDEO=720p43` (960x720) draw the software picture that size; canvases follow.
- Shadows of small text: looked at both ways (`-DSHADOW_FROM` / host `SHADOW_FROM=<px>` drops a text's shadow below
  that size). Kept: on dark boxes the shadow is invisible anyway, and white text on the light grey rows (run info)
  needs it.
- **Modes** (src/love_gs.c `modes`, `gs_mode`; src/love_video.c): 480i, 480i SOFT, 480p (640x448, 32 bits, 1856 KB
  of VRAM left for textures), 720p (640x720 at 16 bits dithered, each pixel 2 wide on the 1280: 2176 KB left; the
  picture fills a 16:9 screen, which has to be set to 4:3 as for 480i) and 720p 4:3 (960x720, one to one between
  black sides: 1216 KB left). A frame of round 1 needs up to 1428 KB (`-DVRAMSTAT`), so 720p 4:3 is crowded for
  most of a round (textures loaded again for every picture, nothing placed between frames, and the swirl behind was
  not drawn in those frames); the other modes never were. 1280x720 does not fit at all (3.5 MB for two buffers at
  16 bits), and a single buffer would tear (the presenter draws the whole frame at every blank, not inside it).
- **The mode screen** (love_video.c, `love_video_pick` after love_gfx_init): in 480i always; up/down, cross; 5 s
  with no button takes the mode it opened on; any mode but 480i is asked about in that mode for 10 s (cross keeps,
  any other button or none goes back to 480i); the mode accepted with the pad is kept as `video.txt` in the saves.
  It draws through love.graphics and `gs_show` (one picture, no thread, no handler) and reads the pad with
  `love_pad_peek` (no vertical blank handler started), because handlers before the game has loaded were the
  unexplained hang described in the repository's `docs/perf.md`. Scripted test builds (`-DTEST_DT`) skip it;
  `-DVIDEO='"720p"'` sets a mode with no screen; `-DVIDEO_PICK` shows the screen. The mode code is compiled in only
  with `-DVIDEO_PICK`: the default build outputs 480i. `tools/video_pick_test.sh` drives the screen in PCSX2.
- **480i SOFT** is the flicker filter (both read circuits, the second a line lower, half and half). Not the
  default: plain, the two fields differ in every thin stroke (it blinks at 30 Hz on a screen that shows fields one
  by one); blended they agree, and the picture is softer on a screen that weaves. Which one a given screen wants is
  for the console to say, so it is a mode to pick. 480i is as it was: one 448-line picture, each field its
  alternate lines (FFMD field mode), the presenter's 60 pictures a second each shown for one field.
- love.h: `Text` has four more fields; `gs_mode_name`, `gs_mode`, `gs_show`, `love_gfx_scale`, `love_pad_peek`,
  `love_video_pick` (LOVE_INTERNAL).
- Debug switches: host `NO_LEVELS=1`, `SOFT_FROM=<frame>`, `TEXTLOG=<frames>`; EE `-DNO_LEVELS`, `-DVRAMSTAT`.
  `tools/text_shots.sh`: host screenshots of six screens with small text.
- Not verifiable in PCSX2: the true pixels of the 480 modes (its screenshots are resampled: 640x480 for 480i,
  672x448 for 480p, and 640x720 comes out as 960x720), interlace flicker, GS fill rate at 720 lines, whether an
  adapter and a screen take 480p/720p from a given console, and the picture's shape in 720p.
- Seen once and not again: an early 640x720 build died in the kernel (TLB miss at 0x80001744) 4 ms after "BOOT
  game made", before anything was drawn; the same mode built 30 minutes later, with other edits in between, ran
  the whole script. Not explained.
- PCSX2 setting used for the screenshots: `ScreenshotSize` 1 (internal resolution; window-sized screenshots were
  471x353).
- Verified in PCSX2: the scripted round in 480i, 480p and 720p with no crowded picture (720p: a frame's textures up
  to 1493 KB of 2176), in 720p 4:3 with 43 of 159 seconds crowded; the EE's cost of a picture is the same in every
  mode (0.12 to 0.13 ms; the draw 7.1 ms). The mode screen with the keyboard (a `-DPLAY -DVIDEO_PICK` build): 480p
  chosen, kept, the game starts in it; next start it opens on 480p (read from the card), takes it after 5 s, nobody
  answers, back to 480i after 10 s and the game starts; next start still opens on 480p; 720p and 720p 4:3 chosen
  and refused with circle; 480i accepted.

## 12. Saving without stopping the frame

The console stopped for 8 to 11 s at every save (a loader's virtual memory card over the network, each request
seconds); PCSX2 for about a second. Now a frame does not wait for the card.

What runs where (EE; the host keeps the synchronous path, `#ifndef _EE`):
- In the frame (main thread): the game's own work (save_run's table, save_manager.c), the packing (STR_PACK), a
  copy of the text into the cache (`saves`, src/love_fs.c: every read is answered from it) and into a queue.
  Nothing of the card.
- The card's thread (src/love_fs.c, `card_thread`, priority 60: between the presenter, 50, and the game, 64): the
  deflate, in steps of 4 KB with an 8 ms nap after each, and mkdir / open / write / close / remove, through libmc
  (the card's own server on the IOP), polled between 2 ms naps (`mcSync(0)` spins). A newer write of a file
  replaces one still waiting (only if no later job of that path follows); the rest goes in the order asked. One
  lock (`card_lock`) around every call to the card, held a job at a time. Its data are copies in libc's memory; it
  never sees a Table. `SAVE wrote <path> <bytes> in <ms> ms (queued <ms> ms)` per file, `SAVE failed to write
  <path>` and the cache marked unknown on a failure (the flag is taken by the main thread at the next look at that
  path).
- At exit (`main` now ends with `exit()`: returning from it, the EE's start-up code left without the atexit calls,
  seen in PCSX2 as a quit that lost the three files): what is still queued is written by the main thread before
  the game ends. The screen stays on its last picture meanwhile.
- At boot, before the thread: one listing of the saves' directory and of each directory in it, and every file found
  is read into the cache. After that a path not in the cache is not on the card: no existence check reaches the
  card in a frame. (The host does the same listing; with no listing, each save is looked up when first asked,
  under the lock.)

Design choices:
- A thread below the game's priority would never run: the main thread does not sleep, it spins in gs_present until
  the vertical blank. So the thread is above the game and gives the EE back by sleeping: in the card's requests and
  between the deflate's steps. If gs_present ever sleeps for the blank, the deflate can go below the game in one
  piece.
- fopen / fwrite from a second thread was not enough: the IOP's file server (and ps2sdk's lock around it) takes
  one request at a time, so the main thread's next printf (PERF, the console's log over the network), a texture
  read from the disc or the music's stream waited behind each request to the card: with the writes in a thread
  through stdio the save burst still cost 334 ms of frames in PCSX2 (frame 3360: 22.25 ms a frame). libmc has its
  own server; the reads at boot stay on stdio. If libmc does not answer at start, or a write through it fails, that
  file is written the old way (`save_write`), still from the thread.

Other card accesses that were in frames, all gone:
- `saved()` asked the card for every path the game looked up, the game's own files too (per boot: the
  localization file, the 8 fonts, "1" as a file, unlocked_jokers.jkr; mid-game: unlock_notify.jkr at the first
  unlock, save.jkr at the first save).
- mkdir of the profile's directory before every save (the game asks getInfo, which said no): made once and
  remembered (`Saved.dir`), and known from the boot's listing.
- The probe file of save_open was removed with remove(), which on the EE leaves a directory of that name: it goes
  through save_remove now. `love_filesystem_remove` queues the removal.

The packing (src/string_packer.c), which has to stay in the frame (it reads the game's tables): a run's save (56
KB of text, 6.4 KB deflated) took 54 ms of EE time in PCSX2 (1.5 ms on the host). Whole numbers are now written
without printf (the EE formats doubles in software) and strings a run at a time, not a character: 23 ms. The files
are byte for byte the same (host, before and after: settings, meta, profile, save). The key kept for the error
message was formatted for every numeric key; now only when the error is raised. New in love.h:
`love_filesystem_write_deflated` (compress_and_save_table calls it: on the host it is compress + write as before;
on the EE the cache keeps the text, which get_compressed takes as it is because it starts with "return",
string_packer.lua:58, and the card gets it deflated).

Numbers (PCSX2, scripted round, `-DPLAY -DTEST_DT=0.0166f`; PERF is the average of 60 frames):
- Before: the burst of settings + profile + run at frame 3360: 32.81 ms a frame, update 18.76 ms (a stop of about
  1 s); settings + profile at frame 1560: 31.42 ms, update 12.78 ms.
- After (150 s): the four bursts with the run (frames 3360, 5220, 7020, 8820): 16.96 to 16.97 ms a frame, update
  2.5 to 3.1 ms (2.2 to 2.3 around them: the 23 ms of packing, one frame late, and little else); frame 1560 is
  21.69 ms with update 3.15 (the run starting, not the save). `SAVE wrote settings.jkr 754 bytes in 355 ms (queued
  0 ms)`, `1/profile.jkr 607 bytes in 308 ms (queued 353 ms)`, `1/save.jkr 6432 bytes in 465 ms (queued 638 ms)`.
  No request to the card after the boot's listing other than those.
- `-DSLOW_CARD=2000` (2 s more in every write, removal and mkdir): each file takes 2.3 s, the run's 4.6 s in the
  queue; PERF stays at 16.68 ms a frame through them (16.97 at frame 3360, as without it).
- `-DSLOW_CARD=2000 -DTEST_QUIT=3380` (love.event.quit() 25 frames after the burst, the first file still being
  written): the three `SAVE wrote` lines come after the quit, 7 s, then the ELF ends.
- The boot after that quit (`-DTEST_NOTUT`, pad: Play, the run setup opening on its Continue tab, Play): settings,
  meta, profile and save are read by the listing, the menu (state 11) goes to state 1 at frame 1800 with 536
  moveables, the saved hand, and never through the blind select (7) of a new run. So the files the flush wrote are
  whole and the deflated run loads. (Without the flush, the first try, the same quit left settings.jkr cut short:
  the next boot started from default settings.)
- Deflate: 0.3 ms on the host for the run's 56 KB, so about 10 ms on the console (not measured there: it is spread
  over 14 steps now). Packing: measured, above.
- Host: saves_cont (seed=SWEEP, deck=b_ghost: save / continue / cross) and saves_sweep (deck=b_plasma, stake=8,
  challenge=3, seed=TEST1), sanitizers on: 0 SUM lines differing, 0 errors.

Build options added (EE): `-DSLOW_CARD=<ms>`, `-DSAVE_TIME` (a `SAVE packed <file> <bytes> in <ms> ms` line per
save), `-DTEST_QUIT=<frame>`, `-DTEST_NOTUT` (the tutorial counts as done, so Play opens the run setup and its
Continue tab; it is saved in the card's settings: `-DTEST_NOTUT=0` takes it back), `-DPAD_FILE=<name>` (a pad
script of one's own beside the shared `host:pad.txt`). Makefile.ps2: `-lmc`.

Not verified when this was written (needs a console):
- libmc on a loader's virtual card (the writes went through fopen until then). The fallback covers a libmc that
  fails, not one that hangs.
- How long the requests take there, and whether the disc's reads and the log really go on during a request to the
  card (they do in PCSX2).
- The quit: that the console's way out of the game goes through love.event.quit (a reset or power off loses what
  is queued, and a file being written: it is written over the old one, as before; with a slow card the window is
  the seconds the queue takes, not a frame).
- A failed write (card pulled out): the path is as before but was not exercised.

## 13. The game's text moved out of the C

The game's wording that the Lua has as string literals inside function bodies had been typed into the C by the
translation. It now comes from `data/game.dat`, which is generated from the user's own copy of the game
(`tools/dump_data.sh`; a game.dat made before this has no `LITERALS` and the port stops with a message saying so).
The repository carries none of it.

- `tools/dump_data.lua`: section `LITERALS`: a small tokenizer reads the string literals of the named functions
  from the Lua's source text (the functions are UI code and cannot be run by the tool) and the dump stops if a
  function is missing, is there twice, has another number of texts, or the credits have a string of an unknown
  kind. `DUMP_LITERALS=1` prints every string found with its kind. `include/data.h`, `src/data.c`: `lits`, `lit`,
  `lit_at`.
- Moved, with the layout code unchanged and the texts taken by index in the order of the Lua's source: the credits
  (UI_definitions_8.c: 6 tab labels, 185 texts, 24 collaborations), the rights notice of a collaboration
  (UI_definitions_7.c), the labels of the profile's high scores (game_3.c), the empty voucher slot and the deck
  button (cardarea_1.c), a round heading and a status text (state_events_1.c, state_events_2.c), the demo screens
  (game_4.c, UI_definitions_5.c, button_callbacks_3.c: two lines and the 8 links), the competition name prompt
  (UI_definitions_10.c), the crash screen (main.c); in a second pass the default content of an options box
  (UI_definitions_11.c), the beta language badge (button_callbacks_1.c) and the six labels of the sandbox's
  controls (game_3.c).
- `Game_set_language` (game_3.c) had the 15 languages' names, buttons and warnings typed in, and
  `Game_init_game_object` (game_4.c) the labels of the round scores: both were already in game.dat (`language`,
  `GAME`) and are read from there.
- Kept in the C on purpose: names the code compares (`ability.name` of jokers, cards, blinds, tags, decks, hands;
  never shown from the C, the shown name is the localization's), states and enum-like values, boot timer labels,
  placeholders a caller always replaces, key names of the on-screen keyboard, symbols, platform strings the code
  matches, version numbers it compares, file names, the interpreter's error messages. `tools/text_audit.py` lists
  the literals of the C that look like text (2727 literals before the change, 953 distinct; 2398 listed after,
  nearly all identifiers) and says whether each is in the game's Lua. Comments were not part of that audit.
- Verified: `tools/ui_run.sh` with the case `credits` (every tab, the 24 collaborations one by one, the
  languages): 7099 tree lines, none different; the case `areas` (the deck button, the empty voucher slot and the
  round heading before and after going endless; two hook commands added for it, `areaui:<area>:<child>` and
  `roundtext`): 212 lines, equal; `tools/ui_all.sh`: the ten cases ok; `tools/both.sh` with the scripted round to
  frame 12900: no PARTS; sanitizers quiet. Not reached by any case (the demo and the competition build are off,
  the crash screen is not called): the demo's screens, the competition name, the crash screen, the "not allowed"
  status text, the links, the sandbox, the beta badge.

## 14. A long scripted run on the PS2 build

Nothing of the game changed. Two test switches for a long scripted run:
- `-DTEST_SEED='"ABC"'` (EE) / `FIXSEED` (host): every unseeded run gets that seed. `NOTUT=1` (host) marks the
  tutorial done at frame 2, as `-DTEST_NOTUT` does on the EE. With both, no `-DPLAY` (no saves: a fresh profile
  every boot) and `DT=0.0166` / `-DTEST_DT=0.0166f`, the host and the EE build in PCSX2 gave the same TRACE lines
  (state, moveables, chips, every 60 frames) over 13 400 frames of a pad script: menu, a new run, the three blinds
  of ante 1, two shops, a Buffoon Pack, the run info overlay.
- The frame times seen in that run's log are in section 10.

## 15. Known differences and what was never exercised

- The d-pad tie on the seed keyboard (section 7): a real difference from the PC game, not fixed.
- Save files: `BLIND.triggered` absent instead of `false`, colours and `config.card_w` with float digits (section
  4). Nothing reads the difference.
- Cosmetic randomness (`math.random`): particles and quips are not expected to match.
- Languages with a non-Latin script show "?" for every character.
- Unported on purpose, each stops with a `TODO_PORT` message if reached (`docs/source-notes.md`, "Not ported"): the video sprite of
  the demo screen, the debug tools, the online high scores (HTTP).
- Not exercised by any suite: jokers whose trigger is outside the joker hook's flow (section 6), challenges' blind
  rules, a boss met with a reloaded run, skipping a blind by pad, the hover popups of the blind chip and of a tag,
  the demo / competition / crash screens.
- Console-only unknowns at the time of writing: listed at the end of sections 8, 11 and 12.
