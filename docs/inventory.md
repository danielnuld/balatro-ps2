# Phase 0 inventory

> This document describes the first approach of the project: the game's own Lua on a Lua 5.1 runtime for the PS2
> (`runtime/`). That runtime is now only the reference the native C port (`native/`) is tested against; it is not
> the product. For the port's state and how to build it, see the top-level `README.md` and `native/docs/`.

Measured 2026-11-18 on Balatro 1.0.1o-FULL (Steam, `Balatro.exe` sha1 `b37282b28749c6b1d057de10efc8d43a4fade95a`,
304 zip entries). Regenerate with `tools/extract.py`, `tools/loveapi.py`, `tools/assets.py`; the full tables are
`inventory-api.md` and `inventory-assets.md`.

## Lua

- 47 files, 98k lines. Without the 15 locale files: 32 files, ~1.75 MB of source; one locale adds ~150 KB
  (`localization/es_419.lua`, `en-us.lua`). The biggest: `functions/UI_definitions.lua` 350 KB, `card.lua` 243 KB,
  `game.lua` 237 KB.
- No `goto`, no `ffi`, no `string.buffer` / `table.new`: nothing blocks PUC Lua 5.1 (design D2).
- `bit`: `bxor`, `lshift`, `rshift`, once, in a hash in `functions/misc_functions.lua:292`. A three-function C module.
- `jit.*`: `jit.arch` / `jit.off()` only behind `love.system.getOS() == 'OS X'` (never evaluated when `getOS`
  returns something else), plus `engine/profile.lua` (the debug profiler). No `jit` table needed.
- `require`: the game's own modules, `bit`, `love.*` submodules (threads), `luasteam` and `https` (to stub or skip).

## LÖVE API

106 `love.*` names, of which ~35 are `love.graphics` functions and the rest callbacks, window / mouse / system calls
that can be constants or no-ops on a console. Object methods: Source (`play`, `stop`, `pause`, `seek`, `isPlaying`,
`setVolume`, `setPitch`), Shader `send` (25 call sites), Image / Font / Canvas (`getWidth`, `getHeight`,
`getDimensions`, `setFilter`, `hasGlyphs`), Quad `setViewport`, Text `set`, Joystick (`isGamepad`, `getGamepadAxis`,
`setVibration`, `getGamepadMappingString`), Channel (`push`, `pop`, `demand`), Thread `start`.

- Threads: three (`engine/sound_manager.lua`, `engine/save_manager.lua`, `engine/http_manager.lua`), all through
  named channels. HTTP is not needed.
- `love.graphics.newVideo`: two call sites (a demo CTA and test functions); not part of the full game's flow.
- `love.data.compress` / `decompress`: only in `engine/string_packer.lua` (saves).

## Textures (1x)

- 20 atlases + 48 optional collab skins (213x95 each). All of them as PSMT8: 7.8 MB; as PSMCT32: 30.8 MB. They live
  in EE RAM and go to VRAM by use.
- The big ones: `Jokers.png` 710x1520 (12787 colours), `BlindChips.png` 714x1054 (547), `8BitDeck.png` 923x380
  (131), `Tarots.png` 710x570 (219), `boosters.png` 284x855 (18018), `Enhancers.png` 497x475 (1024),
  `Vouchers.png` 639x380 (659).
- One 256-colour palette per atlas is not enough for Jokers, boosters, Enhancers, Vouchers, BlindChips: the palette
  has to be per page or per sprite cell (design D5 corrected), with the quantization error reported.
- `playstack-logo.png` (1417x1417, 256 alpha levels) and `localthunk-logo.png` (1390x560) are splash logos over the
  GS's 1024 limit: scale them down offline.
- Alpha: most atlases have 2-16 levels; `balatro.png` and `icons.png` have ~200 (soft edges).

## Sounds

- 80 OGG files, 44.1 kHz, almost all stereo, 1124 s in total.
- Music: `music1`..`music5`, 173.6 s each, the same length (layers played together and cross-faded). 4.3 MB each as
  mono ADPCM at 44.1 kHz: streamed from the disc, never in SPU2 RAM.
- Long ambients: `ambientFire1-3` (37.9 s), `introPad1` (28.6 s), `splash_buildup` (24.1 s), `ambientOrgan1`
  (22.8 s): streamed or at a low rate.
- The other 69 effects: ~1.6 MB as mono ADPCM at 44.1 kHz, ~0.8 MB at 22.05 kHz, which fits the SPU2's 2 MB.

## Fonts and shaders

- `m6x11plus.ttf` (34 KB) is the game's font for Latin languages; the other seven (62 MB) are CJK / other scripts
  and stay out (design D8).
- 19 shaders (`background`, `booster`, `CRT`, `debuff`, `dissolve`, `flame`, `flash`, `foil`, `gold_seal`, `holo`,
  `hologram`, `negative`, `negative_shine`, `played`, `polychrome`, `skew`, `splash`, `vortex`, `voucher`); four of
  them are empty or near-empty files (`flash`, `gold_seal`, `skew`, `vortex`).

## Gate 0.5

No blocker for D2. Open for phase 1: the Lua heap after `love.load`, the load time of 1.9 MB of source, and
`double` against `float` in `love.update` (D3; the seeded random and the hash use number arithmetic, so `float`
would also change what a seed gives against the PC game).
