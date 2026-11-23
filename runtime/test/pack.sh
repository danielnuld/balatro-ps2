#!/bin/bash
# Host test of the pack of a disc (runtime/src/pack.c, tools/disc.py): the bench plays the same run from the
# directory of game files and from the pack; the trace, a screenshot and the mixer's output must be the same.
# usage: python tools/disc.py pack && tools/wsl.sh game bash runtime/test/pack.sh [frames]
set -e
S=$(cd "$(dirname "$0")/../.." && pwd)
bash $S/runtime/build.sh host
B=/root/balatro-build/host-double/balatro
N=${1:-1300}
export TRACE=10
for way in dir pak; do
  mkdir -p /root/pack-$way && cd /root/pack-$way
  if [ $way = pak ]; then export PACK=$S/build/GAME.PAK; G=pak:; else G=$S/build/game; fi
  SHOT=$N SND_RAW=sound.raw setarch -R $B $G $N run | grep -E "TRACE|ERROR" > trace.log
done
cmp /root/pack-dir/shot-$N.ppm /root/pack-pak/shot-$N.ppm
cmp /root/pack-dir/sound.raw /root/pack-pak/sound.raw
diff /root/pack-dir/trace.log /root/pack-pak/trace.log > /root/pack.diff || { echo "FAIL: first differences"; head -6 /root/pack.diff; exit 1; }
echo "PASS: $(grep -c TRACE /root/pack-pak/trace.log) trace lines, the screenshot and the sound ($(stat -c %s /root/pack-pak/sound.raw) bytes) equal from the pack"
