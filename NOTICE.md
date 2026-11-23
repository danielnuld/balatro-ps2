# Notice

This is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or supported by
LocalThunk, Playstack or Sony Interactive Entertainment. Balatro is by LocalThunk and is published by Playstack;
"PlayStation" and "PS2" are trademarks of Sony Interactive Entertainment. All rights in the game belong to their
owners.

## You need your own copy of the game

Nothing of the game is in this repository: no art, no sound, no music, no fonts, no text and none of the game's Lua
code. The tools here read all of that from a copy of Balatro that you own, on your own machine, and convert it
there. What they produce (the extracted files, the data files, the ELF with the data beside it, the disc image)
contains the game and must not be shared, uploaded or committed. Do not ask for, or offer, game files or built
images in this repository's issues.

## No open licence on the port's code

The code of the port (`native/`, `runtime/`, `tools/` and the documents) is published to be read and to be built
for personal use with your own copy of the game. It carries no open-source licence: the native port is a
reimplementation in C that follows the game's own code closely, so it is a work derived from the game and its
author cannot grant rights over it that are not his to give. No permission is given to redistribute it, in source
or built form, to sell it, or to use it in another product. All rights not held by the game's owners are reserved
by the author of the port.

## Third-party code keeps its own licence

- `vendor/lua-5.1.5/`: Lua 5.1.5, Copyright (C) 1994-2012 Lua.org, PUC-Rio. MIT licence
  (`vendor/lua-5.1.5/COPYRIGHT`).
- `vendor/soft-fp/`: the software floating-point routines of the GNU C Library, Copyright (C) Free Software
  Foundation, Inc. GNU Lesser General Public License 2.1 or later, with the exception that allows linking the
  compiled files into other programs without restriction (stated at the top of each file).
- `native/src/gs_screen.c`: derived from gsKit's `ee/gs/src/gsInit.c` (https://github.com/ps2dev/gsKit), Academic
  Free License 2.0; the differences are listed at the top of the file.
- `native/tools/lua51_keyorder.patch` is a patch to Lua 5.1.5 and falls under Lua's licence.
- A built ELF links ps2sdk (Academic Free License 2.0), gsKit (Academic Free License 2.0) and zlib (zlib licence).

## Takedown and contact

If you hold rights in Balatro and want something here changed or removed, open an issue in this repository
(https://github.com/danielnuld/balatro-ps2/issues) and it will be dealt with promptly; no formal notice is needed.
