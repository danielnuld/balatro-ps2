# The Lua runtime: where it stood

> This document describes the first approach of the project: the game's own Lua on a Lua 5.1 runtime for the PS2
> (`runtime/`). That runtime is now only the reference the native C port (`native/`) is tested against; it is not
> the product. For the port's state and how to build it, see the top-level `README.md` and `native/docs/`.

Short on purpose: the state, the rules, how to run things, what is next. Numbers are in `docs/perf.md`.

## State

- Phase 0 done: `docs/inventory.md` (Balatro 1.0.1o-FULL; no blocker for PUC Lua 5.1).
- Phase 1 done: the game's unmodified Lua loads on Lua 5.1.5 with a headless `love` module (`runtime/src/boot.lua`),
  runs `love.load` and reaches the main menu on the host and in PCSX2 (`docs/perf.md`). lua_Number is double.
- Phase 2 done: `love.graphics` in C (`runtime/src/gfx.c`) over the GS (`gfx_gs.c`, gsKit; 8-bit atlases in
  256x256 pages) and a software rasteriser on the host for screenshots; the bitmap font; the game's one canvas is
  the frame buffer. The main menu and a hand draw in PCSX2 as on the host.
- Phase 3 done: the DualShock 2 is LÖVE's gamepad (`runtime/src/pad.c`, events queued by `love.event.pump` in
  `boot.lua`), so the game's own controller mode drives every screen; SELECT is the game's "guide" button. A bot
  (mode `pad`) starts a run, beats Ante 1 and buys a Joker with pad presses alone, on the host and in PCSX2.
  `math.random` is LuaJIT's generator (`main.c`), checked at boot against numbers from the game's `lua51.dll`:
  PUC Lua's own made every fractional seed of the game the same sequence.
- Phase 4 (sound) works by its logs, NOT YET LISTENED TO on the PS2 side: `runtime/src/snd.c` is a software mixer
  (mono, 22050 Hz, 24 voices, SPU ADPCM decoded as it plays, volume and pitch per voice) into audsrv's stream
  from its own EE thread; music and ambiences are streamed from their files. The game's sound manager runs
  unmodified as a coroutine fed by its channel (`boot.lua`, "threads"). In a run in PCSX2 the loudest music
  layer follows the game (music1, music4 in the shop, music5 on the boss) and the chips' pitch climbs.
  The host mixer can write its output to a file to listen to (`SND_RAW`, below).
- Phase 5 done: what the game writes (settings, profiles, the run in progress) goes to `mc0:/BALATRO/` through
  `runtime/src/save.c`, deflated with zlib as `love.data` does it, when the game is played (mode `play`); benches
  and tests keep it in memory. The game's save manager runs unmodified as a coroutine, like the sound one, so a
  save happens inside the frame that asks for it. A bot (mode `saves`) plays a hand, waits for the game to save,
  and on the next boot continues that run from the card: on the host and in PCSX2. No `icon.sys` yet (the
  console's browser will call the save corrupted data).
- Phase 6 (shaders) under way: the GS has none, so `gfx.c` has a recipe per shader file ("shaders" there), each
  the shader's own arithmetic. The card shaders (`dissolve` with its shadows, `debuff`, `played`, `negative`,
  `negative_shine`, `voucher`, `booster`, `foil`, `holo`, `polychrome`, `hologram`) run per texel of the sprite into a 32-bit
  texture, which `gfx_gs.c` keeps between frames; the swirls (`background`, `splash`) are 320x224 pictures made three rows
  a frame, the screen fading from each to the next (their colours a palette made every frame), and `flash` an
  80x56 grid; `flame` on a 32x32 grid over its sprite. The vertex half of the card
  shaders (a card leans under the cursor, the Jokers sway) moves the sprite's corners: a quadrilateral of two
  triangles. Without a recipe, so passing through: `CRT` (off). The host draws the same recipes.
- The screen is presented at every vertical blank, whatever a frame of the game takes: `gfx_gs.c`
  keeps the primitives of the last two frames and a thread draws the mix of them for each blank (`docs/perf.md`).
  The game is seen a frame of its own behind. Only that thread calls gsKit.
- A disc image boots (plan 7.1): `tools/disc.py` puts the game's files in one pack (`GAME.PAK`), which the runtime
  reads through `runtime/src/pack.c` when built with `-DDISC`, and makes the ISO with the ELF and `SYSTEM.CNF`.
  The pad bot beats Ante 1 from the ISO in PCSX2. Not yet on the console.
- The game's hot `engine/` functions run in C (`runtime/src/engine.c`), installed over the classes after load;
  the game's Lua files are untouched. `runtime/test/native.sh` proves them equal to the Lua ones.
- The runtime has its own GC policy, and texture pages are in main memory only while they are being drawn:
  a bot run peaks at 18.8 MB allocated (11.9 of Lua heap) of the 32. OPEN, the port's main risk: speed. In PCSX2 the menu
  fits in a frame, but a hand is 58 ms (17 fps; it was 103 ms before the first speed work) and a whole run averages 14 fps;
  the console will be slower (`docs/perf.md`).
- The vendored Lua and soft-fp carry changes of ours, each marked `balatro-ps2`: table accesses inline in the VM
  loop (`lvm.c`, `ltable.h`) and fast paths for the double arithmetic (`vendor/soft-fp/fast.h`, `muldf3.c`).
- A finished frame is shown by the vertical blank's interrupt (`gfx_gs.c`), not by waiting for it.

## Rules

- The game's Lua is never edited or rewritten: the runtime adapts to it. Where the PS2 cannot do what LÖVE does
  (shaders, 32 MB, the FPU), the runtime fills the gap with a `ponytail:` comment naming the limit.
- Nothing of the game in git: code and assets come from the user's `Balatro.exe` through `tools/extract.py` into
  `build/` (ignored). Check `git status` before each commit.
- The builds run on the WSL disk, not through `/mnt/<drive>`: `tools/wsl.sh` pipes the sources there; PCSX2 runs
  from a directory of its own (`BALATRO_RUN`).
- One PCSX2 at a time: `tools/pcsx2.sh` takes a lock directory (`PCSX2_LOCK`, default `/tmp/pcsx2.lock`) and waits
  for it.
- PCSX2 passing is not the console passing: timings and crashes are confirmed on the real PS2.

## How to run (Git Bash, from the repo)

| What | Command |
|---|---|
| Extract the game | `python tools/extract.py "<...>/Balatro.exe"` -> `build/game/` |
| Convert the atlases | `python tools/textures.py` -> a `.png.tex` (8-bit + palette) beside every 1x PNG; then `tools/wsl.sh game ...` once |
| Convert the sounds | `python tools/sounds.py` -> a `.ogg.snd` (SPU ADPCM, mono, 22050 Hz) beside every OGG (ffmpeg on the PATH; minutes the first time); then `tools/wsl.sh game ...` once and copy them to the run directory's `game/resources/sounds/` |
| Rasterise the font | `python tools/font.py` -> `m6x11plus.ttf.tex` and `.lua` (glyphs at 16 px, LÖVE's advances at size 200) |
| Host test (load + N frames, headless) | `tools/wsl.sh [game] bash runtime/test/run.sh [double\|float] [frames]` (`game` the first time: copies `build/game` to WSL) |
| EE build | `tools/wsl.sh bash runtime/build.sh ps2 [float]` -> `%LOCALAPPDATA%/balatro-ps2/balatro[-float].elf` |
| Bench that plays a run | host: `... run.sh double 3000 run`; EE: `tools/wsl.sh "XF='-DBENCH_RUN -DFRAMES=3000' bash runtime/build.sh ps2 double run"` (XF inside the quoted command: the environment of Git Bash does not reach WSL); profile on the host with `PROFILE=1000` |
| Screenshots on the host | `SHOT=900,1600` in the environment of the host binary: `shot-<frame>.ppm` in the working directory (`/root/balatro-src`), drawn by the software rasteriser of `runtime/src/gfx.c` |
| Screenshots in PCSX2 | `-DSHOTS='\"900,1600\"'` in `XF`: the ELF holds those frames 3 s and prints `SHOT n`; `tools/pcsx2.sh` presses F8 (`tools/shot.ps1`) -> `Documents/PCSX2/snaps/*.png`. Copy the `.tex` and font files to the run directory's `game/` first |
| C engine functions equal to the Lua ones | `tools/wsl.sh bash runtime/test/native.sh [frames]` |
| One of every card shader on screen | host: `DRESS=1250` in the environment of a `run` (`BLIND=30` too: the first hand wins and the flames light); EE: `-DDRESS=1250 -DBLIND=30` in `XF`. `native.sh` compares its screenshot between the Lua and the C draw paths |
| The pad bot (gate of phase 3) | host: `... run.sh double 90000 pad` (`DT=0.1` for frames as long as the EE's); EE: `XF="-DBENCH_PAD -DFRAMES=8000"`. Prints `BOT` lines and `GATE PASSED` / `GATE FAILED` |
| The saves gate (phase 5) | two runs of mode `saves`, the second continues the run the first saved. Host: `SAVE_DIR=/root/savetest ... balatro build/game 60000 saves` twice; EE: `XF="-DBENCH_SAVES -DFRAMES=8000"`, `tools/pcsx2.sh` twice (it writes `mc0:/BALATEST`). Prints `GATE HALF`, then `GATE PASSED` |
| A disc image | `XF="-DDISC -DPLAY -DFRAMES=0 -DOSD"` (or a bench mode; `-DOSD` puts the frame's numbers on screen, `OSD=1` on the host) and `build.sh ps2 double disc`, then `python tools/disc.py` -> `<run dir>/balatro.iso`; `tools/pcsx2.sh balatro.iso` boots it. Nothing of it goes in git |
| The pack against the directory | `python tools/disc.py pack && tools/wsl.sh game bash runtime/test/pack.sh`: the same trace, screenshot and sound from both |
| Listen to the host | `SND_RAW=/root/balatro-src/sound.raw` in the environment: the mixer's output, raw signed 16-bit mono 22050 Hz; `SND_LOG=1`: a line per sound started |
| Scripted pad presses | host: `PAD="1300:dpright 1500:a"` (frame:button, LÖVE's names) in the environment |
| Play (measured dt, the real pad) | EE: `XF="-DPLAY -DFRAMES=0"` (0 = until the game quits). PCSX2's pad keys can be pressed from a script: `tools/shot.ps1 <vk> <scan> <ms>` |
| Where a frame goes, by the game's own sections | `-DSECTIONS` in `XF` (host: `SECTIONS=1`): a `SECT` line with every `PERF` line; `-DPROF -DPROF_SECTION=n` profiles one section (`docs/perf.md`) |
| The console's log | `python tools/udptty.py [out.log]` on a PC of the console's network while the game runs: the loader's udptty (Neutrino from ORBIT) broadcasts the runtime's printf (BOOT, PERF, ERROR lines) on UDP 18194. Not yet tried on the console |
| Run in PCSX2 | `tools/pcsx2.sh [balatro.elf] [timeout s]` -> BOOT / PERF / ERROR lines, log beside the ELF |
| Inventories | `python tools/loveapi.py`, `python tools/assets.py` |

A step stuck for 120 s (`WATCHDOG` in the environment, host only) aborts with its traceback.
