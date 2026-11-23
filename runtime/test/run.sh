#!/bin/bash
# Host test: the game's Lua loads, love.load runs and N frames of update / draw step, with nothing on screen.
# usage: tools/wsl.sh [game] bash runtime/test/run.sh [double|float] [frames] [menu|run]
set -e
S=$(cd "$(dirname "$0")/../.." && pwd)
V=${1:-double}
bash $S/runtime/build.sh host $V
/root/balatro-build/host-$V/balatro $S/build/game ${2:-600} ${3:-menu}
