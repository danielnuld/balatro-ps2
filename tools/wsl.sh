#!/bin/bash
# Run a command in WSL on a copy of the sources kept on the WSL disk, not through /mnt/<drive>: on a slow disk
# WSL's drvfs on it stalls for minutes when anything else is writing there.
# usage (Git Bash, anywhere):  tools/wsl.sh [game] <command...>     e.g.  tools/wsl.sh bash runtime/test/run.sh
#   game: also copy build/game (the extracted game) over; needed once, and after tools/extract.py runs again
#         (and build/GAME.PAK, the pack of tools/disc.py, when there is one)
# ELFs the command leaves in build/ come back to the run directory $BALATRO_RUN (default
# %LOCALAPPDATA%/balatro-ps2, on C:), where tools/pcsx2.ps1 starts them with game/ beside them.
set -e
cd "$(dirname "$0")/.."
export MSYS_NO_PATHCONV=1
R=/root/balatro-src
SRC="runtime vendor"
[ "$1" = game ] && { shift; SRC="$SRC build/game"; [ -f build/GAME.PAK ] && SRC="$SRC build/GAME.PAK"; }
wsl.exe -e bash -c "mkdir -p $R && rm -rf $R/runtime $R/vendor $R/build/*.elf"
tar -c $SRC | wsl.exe -e tar -x -C $R
wsl.exe -e bash -c "cd $R && $*"
RUN=${BALATRO_RUN:-$(cygpath -u "$LOCALAPPDATA")/balatro-ps2}
mkdir -p "$RUN"
for e in $(wsl.exe -e bash -c "ls $R/build/*.elf 2>/dev/null"); do
  wsl.exe -e cat $e > "$RUN/$(basename $e)"
done
