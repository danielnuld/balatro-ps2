# Balatro for the PlayStation 2 (unofficial fan port)

A port of the card game Balatro to the PlayStation 2: the whole game reimplemented in C, with a platform layer for
the console, running from a disc image on a real PS2 and in PCSX2.

**This is an unofficial fan project.** Balatro is by LocalThunk and is published by Playstack. This project is not
affiliated with or endorsed by either of them, or by Sony. **There is no game content in this repository**: no art,
sound, text or code of the game. To build and play the port you need your own copy of Balatro (the PC version,
1.0.1o); the tools here extract and convert everything from it on your machine. What you build contains the game
and is yours alone: do not share it. See [NOTICE.md](NOTICE.md), which also explains why the code carries no open
licence.

## Status

The game is playable from the main menu through full runs: blinds and bosses, the shop, booster packs, vouchers,
jokers, consumables, tags, the collection, challenges, the options, sound and music, and saves.

Tested on a real console (SCPH-75001, started from a loader) and in PCSX2. Numbers from the console's own log,
averages over 60 frames:

| Where | Time a frame |
|---|---|
| Main menu | 16.7 ms (60 fps) |
| Playing a hand | about 17 ms (16.6 to 20) |
| Shop | 22 to 31 ms (the slowest screen) |

The picture is presented at every vertical blank whatever a frame of the game takes, so the slower screens still
scroll evenly.

- Saves (settings, profiles, the run in progress) go to the memory card in slot 1 (`mc0:/BALATROC`), written from
  a thread so the game does not stop while the card is busy.
- All 15 languages of the game load and save.

Known limits:

- **480i only.** The port outputs 640x448 interlaced. A loader that forces a progressive mode (for example
  Neutrino's or OPL's GSM) gives 480p. The port's own mode screen (480p, 720p) exists in the code behind
  `-DVIDEO_PICK` and has only been verified in PCSX2.
- **Small text.** The game was designed for a much larger picture; on a 640x448 screen a lot of its text is only a few
  pixels tall. It is drawn from versions of the font made for each pixel size, but it is still small on a CRT.
- **Languages with non-Latin scripts** (Chinese, Japanese, Korean, Russian) show "?" for every character: only the
  game's pixel font is converted.
- **No icon in the console's browser**: the save has no icon file, so the browser does not show it as a proper
  save. The game itself reads and writes it without trouble.
- The demo video screen, the debug tools and the online high scores are not ported.
- One known difference from the PC game in pad navigation (a tie between up and down on the seed keyboard).

The details, with what was measured and what was never exercised, are in
[native/docs/development-log.md](native/docs/development-log.md).

## Two generations

The repository holds two approaches to the same port.

- **`native/`: the port.** The game's Lua translated to C function by function, on a small core (values, tables,
  strings, a garbage collector) and a platform layer that gives the game what it takes from LÖVE: graphics on the
  GS through gsKit, the shader effects as CPU recipes, a software mixer, the pad, files, the memory card.
- **`runtime/`: the reference.** The first approach: the game's own unmodified Lua on a Lua 5.1 runtime written
  for the PS2. It runs the game, but a hand took about 50 ms a frame even after its hot paths were moved to C,
  which is why the native port exists. It is kept because it is what the native port is tested against (below). `docs/` describes this
  runtime, not the native port.

## How the port is verified

The reference build runs the game's own Lua on the host with double precision numbers; the port must do what it
does. Both take the same scripted pad presses and the same seed, and both print, every few frames, the game's
state, the number of objects on screen, a sum over every object's position and velocity, and the score. The lines
are compared (`native/tools/both.sh`). For what a pad script takes too long to reach, a hook on each side runs the
same commands and prints the same facts, and the outputs are diffed:

- every joker on its own (150 of 150 the same), named scoring cases (128 of 128) and copy chains;
- the 28 bosses, the 24 tags, antes up to 16 in endless mode;
- the shop, packs, vouchers and consumables by step lists;
- saves written by one side and continued by the other; runs started on every deck, stake and challenge;
- the UI tree of every overlay and popup reached, and fuzzed pad input.

The host build also runs under the address and undefined-behaviour sanitizers, and with the collector forced to
run far more often than it needs to. The PS2 build printed the same lines as the host build over a 13,400-frame scripted run in
PCSX2. The scripts are in `native/tools/`; the counts and the differences found are in the development log.

## Building

Nothing here has prebuilt binaries, and none will be published: a build contains the game.

Requirements:

- Linux, or Windows with WSL (the port was developed on Windows with the builds in WSL; the disc script as written
  calls `wsl.exe`).
- gcc, make, zlib headers, `patch`, `flock`, `mkisofs` (genisoimage) on the Linux side.
- The PS2 toolchain: [ps2dev](https://github.com/ps2dev/ps2dev) with ps2sdk, gsKit and the zlib port, at
  `/usr/local/ps2dev` or wherever `PS2DEV` points.
- Python 3 with Pillow, numpy and fontTools; ffmpeg on the PATH for the sounds.
- Your copy of Balatro 1.0.1o for PC (`Balatro.exe`).

Steps, from the repository's root:

1. **Extract the game** into `build/game/` (ignored by git):
   `python tools/extract.py "<path>/Balatro.exe"`
2. **Convert the assets** beside the extracted files:
   `python tools/textures.py` (atlases to 8-bit pixels and a palette), `python tools/font.py` (the pixel font),
   `python native/tools/font_levels.py build/game` (the font at each small pixel size),
   `python tools/sounds.py` (sounds to SPU ADPCM; minutes the first time).
3. **Dump the game's data** into `native/data/` (ignored by git): `bash native/tools/dump_data.sh`.
   It builds a Lua 5.1 from `vendor/` that remembers the order of table keys and runs the game's own tables and
   localization files through it. The port reads its static data, and every text it shows, from these files.
4. **Host build and tests** (optional, but it is how changes are checked):
   `bash native/build.sh link` builds `$BALATRO_BUILD/balatro`; `bash native/test/gc_test.sh` tests the collector;
   `bash native/tools/run.sh 900` runs 900 frames and leaves screenshots. The comparison suites also need the
   reference binary: `bash runtime/build.sh host`, then `bash native/tools/oracle_hook.sh`.
5. **PS2 build**: `XF="-DPLAY -DDISC" bash native/build_ps2.sh disc` leaves `native/out/balatro-native-disc.elf`.
   `-DPLAY` is the real pad, measured time and saves; `-DDISC` reads the game from the disc's pack.
6. **Disc image**: `python native/tools/disc_native.py . native/out/balatro-native-disc.elf balatro.iso`
   packs the converted game files and the data files with the ELF.

The scripts take their locations from the environment:

| Variable | What | Default |
|---|---|---|
| `BALATRO_NATIVE` | the `native/` directory | where the script is |
| `BALATRO_BUILD` | host and PS2 build products | `$HOME/balatro-native` |
| `BALATRO_GAME` | the extracted game | `/root/balatro-src/build/game` (`build/game` for `dump_data.sh`) |
| `BALATRO_ORACLE` | the reference binary | `/root/balatro-build/host-double/balatro` |
| `BALATRO_SRC` | the repository, for `build_ps2.sh` and `dump_data.sh` | this checkout |
| `RUN_DIR` | where `run.sh` runs | `/root/native-run` |
| `BALATRO_RUN` | where the PCSX2 scripts keep ELFs and images | `%LOCALAPPDATA%/balatro-ps2` |

The comparison scripts in `native/tools/` are the development harness as it was used: they assume WSL as root, keep
their run directories under `/root/`, and expect the repository's copy and the extracted game under
`/root/balatro-src` (`tools/wsl.sh` puts them there). Set the variables above, or read the script, before running
one elsewhere.

What was checked for this README, on a clean copy of the repository's files: the host build (87 files, links
with nothing undefined), `gc_test.sh`, and the PS2 build with `-DPLAY -DDISC`. **Not checked end to end from a
clean checkout:** the extraction and conversion steps, the data dump, the disc image and the comparison suites
(they were all run during development, from the author's working directories, before the paths in the scripts
were made relative). `native/test/core_test.sh` does not compile at the moment: a helper in the test has the name
of a function added to `core.h` since.

## Running

- **PCSX2**: boot the ISO as a disc. A memory card in slot 1 holds the saves.
- **A console**: with a loader that starts disc images, such as Open PS2 Loader or Neutrino, from USB, the network
  or a hard disk, as you would any image you made yourself. The image boots as `BLTR_000.01`.

Controls are the game's own controller scheme: the DualShock 2 is presented to the game as a standard gamepad, and
the game shows its button prompts as it does on PC with a pad.

## Repository layout

```
native/           the port
  src/ include/     the game in C (one file per Lua file, big ones split) and the platform layer (love_*.c, gc.c)
  test/             unit tests of the core and of the collector
  tools/            data dump, comparison suites, pad scripts, profiling, disc image
  docs/             development log, conventions of the code, notes per header and per source file
  build.sh          host build         build_ps2.sh, Makefile.ps2   PS2 build
runtime/          the reference: a Lua 5.1 runtime for the PS2 that runs the game's own Lua
tools/            extraction and conversion of the game's files, disc image, PCSX2 and console log helpers
docs/             notes and measurements of the Lua runtime
vendor/           Lua 5.1.5 and the soft-float routines of the GNU C Library
```

## Credits

- **Balatro** is by LocalThunk, published by Playstack. Buy it: the port is nothing without it.
- [ps2sdk](https://github.com/ps2dev/ps2sdk) and [gsKit](https://github.com/ps2dev/gsKit) (Academic Free License
  2.0), and the ps2dev toolchain. `native/src/gs_screen.c` is derived from gsKit's screen set-up.
- [Lua](https://www.lua.org) 5.1.5 by PUC-Rio (MIT licence), in `vendor/lua-5.1.5`: the reference runtime and the
  data dump.
- The soft-float routines of the GNU C Library (LGPL 2.1 or later with a linking exception), in `vendor/soft-fp`:
  the console's FPU is single precision and the game's rules compute in doubles.
- zlib, for the saves.
- [LÖVE](https://love2d.org), the framework the game is written for, whose behaviour the platform layer follows.
- [PCSX2](https://pcsx2.net), where most of the testing was done, and the authors of Open PS2 Loader and Neutrino.

## Licence

There is no open licence on the port's code, on purpose: read [NOTICE.md](NOTICE.md). Third-party code keeps its
own licences, listed there.
