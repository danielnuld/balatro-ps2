#!/bin/bash
# Host run of the native port: tools/run.sh [frames] [shots] [asan|opt]  -> /root/native-run/shot-<n>.ppm, out.txt
# Environment passed on: PAD="frame:button ...", RUN_DIR, PREFIX (of the copies in native/out).
# Copies the screenshots to native/out as PNG-convertible PPMs (see tools/ppm2png.py on the Windows side).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
R=${RUN_DIR:-/root/native-run}; mkdir -p $R && cd $R
[ -d data ] || mkdir data
cp -u $N/data/*.dat data/
BIN=${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan; [ "$3" = opt ] && BIN=${BALATRO_BUILD:-$HOME/balatro-native}/balatro
rm -f shot-*.ppm
export SHOT=${2:-300,900}; [ "$SHOT" = none ] && unset SHOT   # none: nothing is drawn (fast)
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 TRACE=${TRACE:-100} TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=${1:-900} $BIN > out.txt 2>&1
echo "exit $?"
for f in shot-*.ppm; do cp $f $N/out/${PREFIX}$f; done 2>/dev/null
