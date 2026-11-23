# Performance and memory

> This document describes the first approach of the project: the game's own Lua on a Lua 5.1 runtime for the PS2
> (`runtime/`). That runtime is now only the reference the native C port (`native/`) is tested against; it is not
> the product. For the port's state and how to build it, see the top-level `README.md` and `native/docs/`.

## Phase 1: the game's Lua on the EE, headless (2026-11-18)

PCSX2, `balatro.elf` (lua_Number double) and `balatro-float.elf` (float), 1200 frames from boot with a fixed dt of
1/60 s, nothing drawn (every `love.graphics` call is an empty Lua function) and no sound. Times are the EE's
clock as PCSX2 counts it: PCSX2 does not model the EE's caches, so the console will be slower (to be measured there).
Averages over 60 frames.

| | double | float |
|---|---|---|
| Loading and compiling all the Lua (`main.lua` and its requires, from `host:`) | 1.62 s | 1.53 s |
| `love.load` | 1.15 s | 0.97 s |
| Lua heap after `love.load` | 5457 KB | 3469 KB |
| Splash, frame 300: update / draw | 28.1 / 5.7 ms | 21.6 / 4.3 ms |
| Splash, frame 600 | 114.4 / 37.3 ms | 88.2 / 28.8 ms |
| Splash, frame 660 | 138.6 / 46.0 ms | 106.3 / 35.2 ms |
| Splash, the worst (frame 720) | 176.5 / 60.5 ms | 138.5 / 47.2 ms |
| Main menu (frames 900-1200) | 10.6-19.1 / 3.6-9.8 ms | 8.6-18.2 / 2.8-7.3 ms |
| Lua heap, peak | 13105 KB (entering the menu) | 9319 KB |

"draw" is only the game's Lua side of drawing (walking the nodes, the calls); the GS work comes in phase 2.

What the numbers say:

- Boot is not a problem: under 3 s of EE time for 1.9 MB of source. No need to precompile (design D13).
- The splash (hundreds of cards flying) is far over a frame: 100-180 ms of update alone. The main menu is about
  a frame of update (10-19 ms) plus 4-10 ms of Lua draw: 30-50 fps before any rendering.
- float is ~23 % faster and its heap 36 % smaller (8-byte values instead of 16), but it is the same order of
  magnitude: the cost is the interpreter and the game's per-frame work, not the number type.
- The GC: the game stops Lua's collector and steps it for 0.3 ms a frame (`nuGC`, `misc_functions.lua:645`), with a
  300 MB safety net. On the EE 0.3 ms collects little: the heap went from 5.5 to 13.1 MB by the menu and only then
  fell back (float: 3.5 to 9.3 MB). The runtime needs its own GC policy for 32 MB (the game's Lua stays as it is).

Decision (design D3, 2026-11-18): lua_Number stays double. float rounds scores past 2^24, gives other runs for
the same seed than the PC game, and its 1-2 ms in the menu do not change what has to be done: the speed has to
come from the runtime (GC policy, what the splash spends its update on, the draw path in C).

Host (x86-64, same test): update 1-3 ms, draw ~1 ms in the menu; heap 5.4 MB after load, peak 12 MB.

## GC policy, and a run played by the bench (2026-11-18)

The runtime keeps Lua's incremental collector running (pause 130, stepmul 300); the game's
`collectgarbage("stop")` does nothing (`boot.lua`). PCSX2, double:

| | the game's GC (phase 1) | the runtime's |
|---|---|---|
| Lua heap, peak (splash) | 13105 KB | 8890 KB |
| Lua heap in the menu | 5.7-12.9 MB | 4.3-4.6 MB |
| Splash update, frame 600 | 114.4 ms | 115.2 ms |

`-O3` instead of `-O2`: no gain (116.1 ms at frame 600), dropped.

Bench mode `run` (`-DBENCH_RUN`; host: third argument `run`): from the menu a seeded run with the tutorial off,
the small blind, the first five cards of every hand, all through the functions the game's buttons call.
PCSX2, double, nothing drawn:

| State | update | draw (Lua side) | alloc | heap |
|---|---|---|---|---|
| Main menu | 11-17 ms | 4-10 ms | 0.3-0.7 KB/frame | 4.3-5.9 MB |
| Blind select | 95 ms | 43 ms | 3 KB/frame | 7.4 MB |
| Drawing to the hand | 95 ms | 44 ms | 5 KB/frame | 9.3 MB |
| Hand played / selecting | 97-108 ms (worst 446) | 55-58 ms | 1-13 KB/frame | 7.9-8.4 MB |

A frame of a run is ~150 ms of Lua before any rendering: 6-7 fps. Memory is fine (peak 9.3 MB).

Where the update goes in a run (host, `PROFILE=1000`, samples by function, state HAND_PLAYED): the game's own
engine layer every frame, for every card and UI element. `engine/moveable.lua` 31 % (`:278` move 12.4 %, `:114`
5.5 %), `engine/ui.lua` 13 % (`:663` 8.0 %), `engine/node.lua` 10 % (`:143` collides_with_point 7.5 %),
`functions/misc_functions.lua:792/824/829` 9 %, `engine/text.lua` 5 %, `engine/sprite.lua` 3 %, `card.lua` 5 %,
`cardarea.lua` 4.5 %, `game.lua:2449` (Game:update) 4 %. No hot spot in the runtime.

So about 70 % is `engine/` and its math helpers. Options, not decided: those functions in C with the same math
(by Amdahl ~2.7x if they became 10x faster), LuaJIT's interpreter ported to the R5900 (2-4x by reputation, not
measured), float (1.23x). All of it to be confirmed on the console, which PCSX2 flatters.

## Native engine functions

`runtime/src/engine.c` replaces hot functions of the game's `engine/` layer with the same math in C, installed
over the classes after the game loads (`-DNO_NATIVE` / host `NATIVE=0` turns it off). `runtime/test/native.sh`
checks that every moveable's T/VT/velocity is identical with and without it.

Slice 1 (`Moveable:move` and its `move_*` leaves, `align_to_major`, `point_translate`, `point_rotate`), PCSX2,
double, bench `run`, 1800 frames:

| State | update Lua | update native | draw (unchanged) |
|---|---|---|---|
| Splash, frame 600 | 116.7 ms | 95.8 ms | 38 ms |
| Main menu | 15.1 ms | 12.0 ms | 5 ms |
| Blind select | 94.8 ms | 70.0 ms | 43 ms |
| Hand played | 97-108 ms | 71-81 ms | 55-58 ms |

About 26 ms less per update (-26 %); a frame of a run goes from ~153 to ~127 ms. The Lua side of the draw is
untouched by this slice and is now the larger half of what is left.

Slice 2 (`get_major`, `glue_to_major`, `move_with_major`, `Node:collides_with_point`), host, 3000 frames of the
bench `run` (`runtime/test/native.sh`): update 3.95 s in Lua, 2.15 s with both slices (1.84x); draw 1.85 s either
way. In PCSX2 (measured with the GS backend already drawing): update 57-59 ms per frame in a hand, from 71-81 ms
with slice 1 alone and 97-108 ms in Lua.

What is left in the profile is the Lua side of the draw (`ui.lua:663` draw_self 14 %, `prep_draw` 10 %,
`text.lua:257`, `ui.lua:938`, `sprite.lua:73`, `node.lua:307`) and game logic (`game.lua:2449`, `cardarea.lua`,
`card.lua`). The draw functions are mostly calls into `love.graphics`, so their C versions belong with the
graphics backend (phase 2), calling it directly instead of through Lua.

## Drawing for real

With `love.graphics` in C (`gfx.c`: transforms, primitives, text glyph by glyph; nothing rasterised) the Lua side
of the draw on the host, 3000 frames of the bench `run`: 2.20 s with the game's Lua, 1.61 s with `prep_draw` and
`Node:translate_container` in C calling `gfx.c` directly. Update: 4.40 s -> 2.12 s. `native.sh` now also checks
that two frames drawn by the software rasteriser are byte-identical with and without the C functions.

Draw functions in C (`UIElement:draw_self` for text, plain boxes and elements with nothing to draw,
`draw_pixellated_rect`, `draw_children`, `DynaText:draw`), then `UIElement:update`. Lua VM instructions in the
state HAND_PLAYED over the same 1800-frame bench (host, `PROFILE=1000` samples, a measure that does not depend
on how busy the PC is): 46366 before them, 34013 with draw_self, 28956 with DynaText and draw_children, 26228
with UIElement:update. PCSX2, a hand, GS drawing: draw 60 ms -> 47 ms per frame (before UIElement:update, which
is on the update side: 58 ms).

What is left, by samples: `game.lua:2449` Game:update 12.5 %, `sprite.lua:73` draw_shader 8 % (mostly shader
uniforms nobody reads yet: to be redone with the shader recipes), `cardarea.lua:410/270` 14 %, `card.lua:4340/4126`
14 %, the rest of draw_self (buttons, objects) 7.6 %, `text.lua:174` 5 %.

## Where a frame goes on the EE (`-DPROF`)

`XF=-DPROF` builds a sampling profiler into the ELF (a 1 kHz timer alarm counting the interrupted pc, frames 1300
on; `tools/prof.py prof.bin symbols.txt` prints time per function). ps2sdk's gprof was no use: with `-pg` its call
counting doubles the frame and lands on the small hot functions, and without `-pg` objects it never samples.
The GS takes little: with `-DNO_RASTER` the draw of a hand goes from 47 to 44 ms (about 1450 primitives a frame).

A hand, PCSX2, 46k samples: double arithmetic 31 % (`__muldf3`, `__pack_d`, `__unpack_d`, `_fpadd_parts`,
`__divdf3`, comparisons: libgcc's fp-bit.c, since the FPU is single precision), the Lua VM and its table lookups
38 % (`luaV_execute` 13 %, `luaV_gettable` 8 %, `luaH_get` 7 %, `luaH_getstr` 6 %), `idx` of engine.c 7 %,
waiting for vsync 9 %.

So `vendor/soft-fp`: libgcc's faster soft-float (soft-fp), linked ahead of libgcc; same IEEE results, checked on
the host against native doubles (`runtime/test/softfp.sh`). A hand in PCSX2: update 55 -> 45 ms, draw 47 -> 40 ms.
Double arithmetic is still about 19 % of the samples (`__muldf3` 6 %).

The main menu in PCSX2 with all of the above, GS drawing (541 primitives a frame): update 7.3 ms, draw 8.9 ms.
A frame of the menu fits in 16.7 ms; a hand (86 ms) does not.

Host wall-clock times of this bench are not comparable to the EE: the game's `nuGC` runs GC steps for 0.3 ms of
real time every frame, which on a PC is half the frame and on the EE a couple of steps.

## A run played with the pad

The bot of mode `pad` in PCSX2, measured dt, GS drawing: Ante 1 (three blinds and a shop) takes 3363 frames and
377 s, 195 s of update and 127 s of draw. Selecting a hand: update 56-58 ms, draw 39 ms; a hand scoring and the
shop: update 82-89 ms, draw 51-54 ms (worst update 0.84 s, entering the shop). About 9-10 fps through a run.
Lua heap peak 14.7 MB (it was 8.9 MB in the menu-and-one-hand bench): the collector's pace has to be looked at
again with whole runs.

## Sound

The same bot run in PCSX2 with the mixer on: 2969 frames in 364 s, 122 ms a frame against 112 ms without sound
(update 64.5 against 58 ms on average, draw 39.7 against 37.9). So about 10 ms a frame for the sound: the game's
sound manager walking its 80 sources every frame (`MODULATE`), the copy of each request table, and the mixer
thread (5 music layers and up to 10 voices decoding at once). Not yet split between the three.
The sounds are 14.2 MB on disc; in memory, the 71 short ones (1.5 MB) and 32 KB per stream.

## The table lookups, the doubles and the vertical blank

A new `-DPROF` profile of a hand with sound (bench `run`, 112k samples): table lookups 31 % (`luaH_get` 11 %,
`luaH_getstr` 7 %, `luaV_gettable` 5 %, `idx` of engine.c 8 %), `luaV_execute` 16 %, double arithmetic 18 %
(`__muldf3` 6 %, division 4 % of which most in libgcc's 64-bit division, `__clzdi2` 1.6 %), waiting for the
vertical blank 8 %, the mixer 3 %. A frame of a hand was 103 ms of real time (update 49 + draw 41 + sound + wait).

What was done, each step measured in PCSX2 on the same bench (a hand being played, state HAND_PLAYED, per frame):

| Step | update | draw | real frame |
|---|---|---|---|
| Before | 49 ms | 41 ms | 103 ms |
| Field reads, array indices and field writes inline in the VM loop (`lvm.c`: `fastgetstr` through `__index` tables, `fastgetnum` from the bits of the double, `fastsetstr`) | 43 | 35.5 | |
| `idx` of engine.c inline; a multiply of doubles with four 32x32 products ahead of soft-fp's generic one; a `__clzdi2` without libgcc's table | 36.5 | 33 | 85.6 |
| The flip in the vertical blank's interrupt instead of waiting for it (`gfx_gs.c`) | 37 | 33 | 70.9 |
| Division of doubles by long division with the FPU guessing each 18-bit digit; zeros in sums and products | 35 | 31 | 66.7 |
| `Sprite:draw_shader`, `draw_self` and `draw_from` in C (the uniforms nobody reads are not computed) | 35.5 | 26 | 62.8 |
| `UIElement:draw_self` in C also for buttons, embossed boxes and objects; no count hook on the EE | 34.5 | 24 | 58.5 |

So 9.7 -> 17.1 fps in a hand. Everything is checked on the host: `runtime/test/native.sh` (the game plays the
same and draws the same pixels with and without the C functions; the trace of a run is also identical to the one
of the VM before these changes) and `runtime/test/softfp.sh` (the new arithmetic against the host's doubles,
rounding ties and the edges of overflow included).

Notes:

- Waiting for the vertical blank after drawing costs half a blank on average, and more when a frame ends just
  past one. Now the interrupt shows the finished frame and the game is already updating the next; nothing is
  sent to the GS while a frame waits to be shown, so no tearing, and a menu still runs at one frame per blank.
- The fast sum and subtraction (`vendor/soft-fp/fast.h`) measured no faster than soft-fp's own; they stay
  because they are tested and cost nothing, but they are not where the time is.
- The count hook of `boot.lua` (watchdog, profiler) is host only now: it cost every VM instruction a decrement.
- The vertical blank handler is installed with the first frame, not when the graphics start, so nothing new runs
  while the game loads (see the hang below).
- Measured and left: the game's own shadows setting Off draws 1250 primitives instead of 1380 and a frame is
  55.9 ms instead of 58.5. Not worth what the picture loses.
- Looked at and left: `CardArea:align_cards`, `Card:update` and `Card:draw` in C would give 1-2 % of a frame
  each. The cost is spread over every card of the deck and its four or five sprites, each a moveable that is
  moved, updated and asked whether to draw every frame.
- What is left in the profile of a hand (70k samples): `luaV_execute` 29 %, double arithmetic 17 % (`__muldf3`
  6 %: four `multu` each, there is no shorter way), the rest of the VM 8 %, engine.c 12 %, the mixer and its
  decoder 3.5 %, the GS 4 %. By Lua function: `Game:update`'s two loops over every moveable 15 %, the rest of
  `UIElement:draw_self` 9 %, `CardArea:align_cards` and `:draw` 16 %, `Card:draw` and `Card:update` 17 %
  (game logic: a chain of `if self.ability.name == ...` for each of the 52 cards every frame),
  `DynaText:update` 6 %.

The bot of mode `pad` in PCSX2 with all of it and sound: Ante 1 in 2970 frames and 225 s (it was 364 s), 131 s of
update and 72 s of draw: 76 ms a frame on average through a run, 13 fps (it was 122 ms, 8 fps). Heap peak 13.9 MB.

With the last row of the table too: 2809 frames in 202 s, 120 s of update and 61 s of draw, 72 ms a frame (14 fps).

Not explained: one ELF of this work (the pad bot, built before the Sprite functions) stopped twice while
loading the game's Lua, the emulator's log repeating `padman: DMA Busy`, which means the EE had stopped taking
interrupts. The same sources built again booted five times out of five, and the final tree ran the whole bot
game. If a boot ever stops like that again, keep the ELF: the first suspect is the vertical blank handler, the one
new thing that runs during the load.

## Saves

A save runs inside the frame that asks for it (the game's save manager as a coroutine). In PCSX2, with its
emulated memory card: saving the run 0.5 s (0.3 s of it writing 7 KB to the card), saving the settings and the
profile 0.6 s, saving the progress 0.6-0.9 s; nearly all of the last two is the card, about 190 ms for each
file however small. Packing the tables and deflating them is the small part. The game writes when something to save is queued and 30 s have passed since the last write, or at once when the save is forced, so each is a visible stop. Not done: the writes on an EE thread
(the packing has to stay where the Lua state is). To be measured on a real card first, which will not be
faster.

## Memory, seen while testing the saves

One PCSX2 run ended with `not enough memory` after 220 s. The cause was the test itself (it unpacked the saved
run, 50 KB of Lua source, every 0.4 s), but it shows how little room there is: at boot 17.2 MB are allocated,
5.1 MB of them the Lua heap; 7.65 MB are the textures, all loaded at boot and kept in main memory for gsKit's
texture manager (the two logos of the splash are 2.8 MB of that); and the Lua heap of a run peaks near 14 MB.
Lua 5.1 does not collect when an allocation fails, so the peak has to stay clear of the limit.

Two things done about it (branch `feat/14-memory`), measured in PCSX2 over the bot's whole run; PERF lines now
print `malloc`, everything allocated:

| | before | after |
|---|---|---|
| Allocated at boot | 17.2 MB | 9.5 MB |
| `love.load` | 2.4 s | 1.4 s |
| Lua heap, peak of the run | 13.9 MB | 11.9 MB |
| Allocated, peak of the run | about 26 MB (not printed then) | 18.8 MB |

- Texture pages are read from the atlas's file when a row of them is first drawn and freed after 600 frames
  unused (`gfx_gs.c`), instead of all 7.65 MB at boot. The first frame that shows an atlas waits for the file:
  nothing to see from `host:`, to be looked at from the disc.
- The collector starts a new cycle as soon as one ends and does ten times Lua's default work per byte allocated
  (`setpause` 100, `setstepmul` 1000; they were 130 and 300). On the host the peak of a bot run goes from 18.9 to
  15.3 MB; 2000 gives 14.9 and costs more. In the bench a frame of a hand is 59.0 ms against 58.5, and
  `propagatemark` is 0.6 % of the samples.

## From the disc

The ISO of `tools/disc.py` booted in PCSX2, which gives the emulated drive its seek and read times. The pad bot
through Ante 1, each row a whole run:

| How the pack is read | Lua loaded | `love.load` | a frame, wall time |
|---|---|---|---|
| (from `host:`, no disc) | 1.7 s | 1.4 s | 78 ms |
| The pack opened again for every file, read through stdio | 10.3 s | 12.3 s | run not finished |
| One descriptor, each read one request | 3.9 s | 6.2 s | died at 280 s: the disc's file system out of open files (a FILE per stream) |
| Streams through that descriptor too, topped up by the main thread | 3.9 s | 6.2 s | 144 ms, 64 of them waiting for the disc |
| Streams topped up by a thread of their own | 3.9 s | 6.2 s | 106 ms |
| 10 s of each stream kept ahead instead of 2.6 (128 KB each) | 3.9 s | 7.3 s | 82 ms |

So the game is on screen 11 s after the ELF starts, and a frame costs what it costs from `host:`. What made the
difference is how often the disc is asked: the game plays five music layers at once, each its own file, and
every top-up of one is a seek. Left for the console to judge: the five layers interleaved in one file would be
one sequential read; the first frame that shows an atlas waits for its texture rows (worst draw 0.8 s in a run).

## The shader recipes

The GS has no shaders: `gfx.c` does each shader's arithmetic on the EE, per texel of a sprite or on a grid, and
the GS draws the result as a texture. Measured in PCSX2, bench `run` (and `-DDRESS=1250 -DBLIND=30` for the
card shaders: seven cards of the hand and four Jokers wearing one each).

| Where | Before the recipes | First version | Now |
|---|---|---|---|
| The intro (the splash swirl and the flash), draw | (flat blocks) | 10 ms, 36 with the cards | 6.5 ms |
| The menu's logo appearing (a dissolve of 72000 texels), draw | | 150 ms | 11 to 25 ms |
| The menu, draw / frames a second | 60.0 fps | 9.5 ms / 55 fps | 7.4 ms / 60.0 fps |
| A hand, draw | 24 ms | 30 ms | 26.5 ms |
| A hand with eleven cards in card shaders, draw | | 60 ms | 39 ms (26.5 + 5 of the extra cards + 8) |

What made the difference:

- `sqrtf` and `floorf` were libm's loops: `-fno-math-errno` makes the first the FPU's instruction, the second
  is a cast. The sine is a table of 1024 steps, the nearest one taken (no interpolation, no branch).
- The swirls (`background`, `splash`) were an 80x56 grid, an eighth of its rows computed each frame, stretched
  over the screen with bilinear filtering: blotches, because the paint has detail a pixel fine. A 320x224 grid
  looks like the shader per pixel, but a texel is 3.7 us: the same eighth of its rows a frame took the menu's
  draw from 7.4 to 43.5 ms. Now (issues #27, #29) the grid is 320x224 and three rows are computed a frame, so a
  picture takes 75 frames; the screen fades between the last two finished ones. A picture is 8 bits a texel
  (how far the paint took it) and its colours are a palette made every frame, so colours and the intro's smoke
  follow their uniforms at once and only the shape lags. The menu's draw is 7.75 ms (60 fps still), a hand's
  28.7 ms (26.5 before).
- A dissolve goes in 64 steps and the picture of a step is kept (`kept[]` in `gfx_gs.c`), so a sprite that is
  dissolving is not made again every frame; its texels go through a path with no float per colour.
- A card shader's picture (a 32-bit texture of the sprite) is kept between frames. The ones that do not move
  (debuff, played, negative) are made once. The shines (foil, holo, polychrome, voucher, booster, negative_shine,
  hologram) are made again one every three frames (`EVERY`), each in its turn: one costs 15 to 20 ms (2.5 us a
  texel), which shows as the worst frame of each second (60 ms of draw against 40). With three of them on
  screen, as in a shop, each moves every nine frames.

Left: the pictures of the shines are the cost to cut next if the console says so (their factors on a coarser
grid, or a row band at a time instead of a whole picture in one frame). Memory: a kept picture of a card is 27 KB
of main memory and 48 KB of VRAM; 48 at most.

The pad bot through Ante 1 with the recipes (PCSX2, `host:`): gate passed, 3449 frames in 285 s, update 157 s and
draw 96 s: 27.8 ms of draw a frame through the run against 21.7 before (the bot does not play the same game
twice, so the totals do not compare frame by frame). 19.3 MB allocated at most (18.8 before), Lua heap peak 11.7 MB.

## Presenting at every vertical blank

A frame of a run is three to five vertical blanks of the EE, so the screen moved 12 to 20 times a second. Now
`gfx_gs.c` writes the game's primitives down, a list per frame, and a thread above the game's presents at every
blank the mix of the last two finished lists: each primitive of the newer one at its place between where the
older list had it and its own, the newer list being reached when the next frame is due (it is taken to last as
long as the one before). A primitive is paired with the next one of the older list that shows the same texels
(or, flat, lies within 48 pixels); one without a pair is drawn as it is. What is seen is the game a frame of its
own behind.

The pad bot's run in PCSX2: 2784 frames in 220 s (12.7 a second), 10256 pictures (46.6 a second); update 48.7
and draw 26.8 ms a frame against 47.3 and 29.0 before, so presenting costs the game nothing it did not already
spend sending each frame to the GS.

What it took to get there:

- Through gsKit's primitives a picture was 3.3 ms of the EE (1700 quads), a fifth of all of it at 60 a second:
  a frame of a hand went from 62.5 to 78 ms. So the newer list is one GIF packet, built once per frame of the
  game (pairs of a register's value and its number, in runs of one texture each); a picture writes again only
  the corners and the colour of the primitives that move, and each texture's place in VRAM, and sends the
  packet as it is: 0.15 ms.
- The whole room sways a little every frame, so every primitive "moved": one is placed in between only if a
  corner is half a pixel away or its colour changed (1 to 30 of 1700 in a hand at rest).
- When the newer list is on screen as it is, nothing is sent until the next one.
- All the textures of a frame have to be in VRAM at once now (gsKit's primitives were drawn between one upload
  and the next). The frame buffers leave 1.7 MB: a hand of eleven cards in card shaders does not fit, the
  texture manager gives one texture's place to another, and that frame is sent run by run, each after its
  texture is back, once, without steps (9 ms). Not seen in the bot's run. To do if it shows in play: frame
  buffers of half the lines (a field each, 1.15 MB less) or of 16 bits.
- Only the presenting thread calls gsKit. The game's side leaves it two short lists (textures whose texels
  changed, textures and memory to free) and takes a semaphore only to swap the lists.

`SHOWN` lines (every 60 frames of the game): pictures presented, blanks gone by, the EE's time per picture, and
how many frames did not fit in VRAM.
## Towards a fluid hand: the frame by sections (second round)

The game has checkpoints through its update and its draw (`timer_checkpoint`, for an overlay it does not ship).
`-DSECTIONS` (host: `SECTIONS=1`) times them: a `SECT` line with every `PERF` line. `-DPROF -DPROF_SECTION=n`
samples only one of them (1 move, 2 update, 3 primitives, 4 uiboxes, 5 rest, 6 the update's tail), so the profile
of a section is not drowned in the others. Float instead of double was measured again and left: 54.6 ms a frame
against 62.5, for runs that are no longer the PC's.

A hand being selected (bench `run`, frame 1380, 470 moveables of which 448 are at rest), PCSX2, ms a frame:

| Section | Before | Now | What changed |
|---|---|---|---|
| update: move (every moveable, `CardArea:align_cards`) | 14.0 | 11.8 | a deck's `align_cards` in C; `juice` and `zoom` read from the object's own table (no class has them); zeros not computed in `move_r` and `glue_to_major` |
| update: update (every moveable's `update`) | 9.2 | 7.4 | `Card:update` in C for the cards with nothing to do |
| update: tail (the controller: which node is under the cursor) | 6.3 | 3.2 | `Node:collides_with_point` took a sine and a cosine of the same angle for every node: the last pair is kept |
| update: sounds (`modulate_sound`) | 2.5 | 2.5 | |
| draw: uiboxes | 12.5 | 12.5 | |
| draw: rest (the card areas) | 9.5 | 6.2 | `Card:draw` and `CardArea:draw` in C for the usual cards |
| draw: primitives (the swirl) | 5.2 | 5.1 | |
| update / draw / frame | 33.5 / 29.0 / 62.5 | 25.9 / 24.7 / 50.6 | also: compares and double-to-float on the bits (`vendor/soft-fp`), no method name hashed per call |

16 -> 19.8 frames a second in a hand. The pad bot: Ante 1 in 3212 frames, 39.9 ms of update and 25.5 of draw a
frame on average (it was 47.3 and 29.0): 65 ms, 15 fps through a run, since scoring and the shop cost more than
selecting.

There is no hot spot left: every section is hundreds of small things (a table lookup is 50 cycles, a software
double operation 100 to 300, a VM instruction 80) times 470 moveables and 176 UI elements. Next, by size: the UI
boxes' draw (all C already: lookups, the GS's primitives, doubles to floats), the move of what is at rest, and
the two loops of `Game:update` themselves (Lua the runtime may not edit).
