#!/bin/bash
# Regenerates native/data/ (game.dat, loc_<lang>.dat, ref/*.txt, skipped.txt) from the user's copy of the game.
# The game's data is never kept in this tree as source: it is extracted here, at build time.
#
# In WSL or Linux:
#   bash <native>/tools/dump_data.sh
#
# What it does, all of it under $WORK (/root/balatro-dump), so a slow source disk is read once:
#   1. copies vendor/lua-5.1.5 and the game's .lua files there (only the first time; REFRESH=1 copies again);
#   2. applies tools/lua51_keyorder.patch to that Lua copy and builds tools/dump_host.c with it: a Lua 5.1.5 whose
#      tables remember the order their keys were first set in (see dump_host.c);
#   3. runs tools/dump_data.lua on it for the game's tables and every localization/*.lua.
#
# Environment: REPO or BALATRO_SRC (the repository: default this checkout, or /root/balatro-src), BALATRO_GAME (the
# extracted game, default $REPO/build/game), WORK, REFRESH=1, and DUMP_OS (default ps4: what
# love.system.getOS() answers while globals.lua sets its F_* flags; the Lua port's boot.lua answers the same).
set -euo pipefail
NATIVE=$(cd "$(dirname "$0")/.." && pwd)
REPO=${REPO:-${BALATRO_SRC:-$NATIVE/..}}; [ -d "$REPO/vendor/lua-5.1.5" ] || REPO=/root/balatro-src
WORK=${WORK:-/root/balatro-dump}

if [ -n "${REFRESH:-}" ] || [ ! -d "$WORK/game" ] || [ ! -d "$WORK/lua" ]; then
    rm -rf "$WORK/game" "$WORK/lua"
    mkdir -p "$WORK/game"
    cp -r "$REPO/vendor/lua-5.1.5" "$WORK/lua"
    (cd "${BALATRO_GAME:-$REPO/build/game}" && cp -r ./*.lua engine functions localization "$WORK/game/")
    (cd "$WORK/lua" && patch -p1 < "$NATIVE/tools/lua51_keyorder.patch")
fi
gcc -std=gnu99 -O1 -w -I"$WORK/lua/src" -o "$WORK/dump_host" "$NATIVE/tools/dump_host.c" "$WORK"/lua/src/*.c -lm

rm -rf "$NATIVE/data"
mkdir -p "$NATIVE/data/ref"
langs=$(cd "$WORK/game/localization" && ls ./*.lua | sed 's|^\./||; s|\.lua$||')
# en-us first: it is the one whose table sizes are printed in full
"$WORK/dump_host" "$NATIVE/tools/dump_data.lua" "$WORK/game" "$NATIVE/data" en-us $(echo "$langs" | grep -vx en-us)
