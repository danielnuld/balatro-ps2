#!/bin/bash
# tools/gc_prof.sh <prof.py>: links the optimised build's objects with an instrumented copy of src/gc.c (made by the given
# script as /tmp/gcd/gc_prof.c) and runs the scripted run of gc_run.sh with it: a GCPROF line per collection.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
python3 "$1" && gcc -std=gnu11 -O1 -g -I$N/include -c /tmp/gcd/gc_prof.c -o /tmp/gcd/gc_prof.o &&
  gcc -g -o ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-gcprof $(ls ${BALATRO_BUILD:-$HOME/balatro-native}/obj/*.o | grep -v '/gc.o') /tmp/gcd/gc_prof.o -lz -lm || exit 1
mkdir -p /root/native-gcprof/data && cd /root/native-gcprof && cp -u $N/data/*.dat data/
PAD="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')" GC_PROF=1 DT=0.0166 TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} \
  FRAMES=${FRAMES:-12900} ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-gcprof 2>&1 | grep -E 'GCPROF|gc:|ERROR'
