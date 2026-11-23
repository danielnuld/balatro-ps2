#!/bin/bash
# The PS2-style sources (tools/tabfix.py, -DTAB_ARRAYS) built for the host in a private directory, for profiling.
# usage: bash tools/perf_game.sh <tag> [extra gcc flags]   -> /root/perf-game/balatro-<tag>
#   tag pg:  bash tools/perf_game.sh pg -pg -fno-inline     (gprof: call counts per caller)
#   tag o:   bash tools/perf_game.sh o                      (timings: the PERF line)
# run:  bash tools/perf_game.sh run <tag> [frames]  (the scripted round, DT=0.0166, TRACE=10; out in /root/perf-game/run-<tag>)
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
W=/root/perf-game
if [ "$1" = run ]; then
  R=$W/run-$2; mkdir -p $R/data && cd $R && cp -u $N/data/*.dat data/
  PAD="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')" DT=0.0166 TRACE=10 TIME=1700000000 \
    GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=${3:-12900} $W/balatro-$2 > out.txt 2>&1
  echo "exit $?"; grep -c TRACE out.txt; grep TRACE out.txt | md5sum; grep -v -e TRACE -e PERF out.txt | head -5
  grep PERF out.txt | awk '{ for (i = 1; i < NF; i++) { if ($i == "update") u += $(i+1); if ($i == "draw") d += $(i+1) } n++ } END { printf "PERF mean of %d lines: update %.4f ms  draw %.4f ms\n", n, u / n, d / n }'
  exit
fi
TAG=$1; shift
mkdir -p $W/obj-$TAG $W/new-$TAG
rm -rf $W/new-$TAG/src.tmp; python3 $N/tools/tabfix.py $N/src $W/new-$TAG/src.tmp
mkdir -p $W/new-$TAG/src $W/new-$TAG/include
# only what changed is copied, so only that is compiled again (a changed header: everything)
diff -rq $N/include $W/new-$TAG/include > /dev/null 2>&1 || { rm -rf $W/new-$TAG/include $W/obj-$TAG/*.o; cp -r $N/include $W/new-$TAG/; }
for f in $W/new-$TAG/src.tmp/*.c; do cmp -s $f $W/new-$TAG/src/$(basename $f) || cp $f $W/new-$TAG/src/; done
for f in $W/new-$TAG/src/*.c; do [ -f $W/new-$TAG/src.tmp/$(basename $f) ] || rm -f $f $W/obj-$TAG/$(basename $f .c).o; done
cd $W/new-$TAG
for f in src/*.c; do o=$W/obj-$TAG/$(basename $f .c).o; [ -f $o ] && [ $o -nt $f ] || echo $f; done | xargs -r -P 4 -I{} bash -c 'f={}; gcc -std=gnu11 -O1 -g -w -DTAB_ARRAYS '"$*"' -Iinclude -c $f -o '$W/obj-$TAG'/$(basename $f .c).o || echo "FAILED $f"'
gcc -g "$@" -o $W/balatro-$TAG $W/obj-$TAG/*.o -lz -lm && echo linked
