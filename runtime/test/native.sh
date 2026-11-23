#!/bin/bash
# Host test of engine.c: the bench plays the same run with the game's Lua engine functions and with the C ones;
# every moveable's transform must be the same every 10 frames. Then the CPU time of each.
# usage: tools/wsl.sh bash runtime/test/native.sh [frames]
set -e
S=$(cd "$(dirname "$0")/../.." && pwd)
bash $S/runtime/build.sh host
B=/root/balatro-build/host-double/balatro
N=${1:-3000}
export TRACE=10
# setarch -R: no address randomization, so tables keyed by objects iterate in the same order in both runs
NATIVE=0 setarch -R $B $S/build/game $N run > /root/native-lua.log
NATIVE=1 setarch -R $B $S/build/game $N run > /root/native-c.log
grep -h TOTAL /root/native-lua.log /root/native-c.log
# and the same picture: two frames drawn by the software rasteriser, a card dissolving in the first and one of
# every card shader in the second (DRESS, boot.lua)
for n in 0 1; do
  (mkdir -p /root/native-shot$n && cd /root/native-shot$n && NATIVE=$n DRESS=1250 SHOT=900,1340 setarch -R $B $S/build/game 1340 run > /dev/null)
done
cmp /root/native-shot0/shot-900.ppm /root/native-shot1/shot-900.ppm
cmp /root/native-shot0/shot-1340.ppm /root/native-shot1/shot-1340.ppm
echo "PASS: screenshots equal"
if diff <(grep TRACE /root/native-lua.log) <(grep TRACE /root/native-c.log) > /root/native.diff; then
  echo "PASS: $(grep -c TRACE /root/native-c.log) trace lines equal"
else
  echo "FAIL: first differences"; head -6 /root/native.diff; exit 1
fi
