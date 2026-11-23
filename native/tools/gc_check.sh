#!/bin/bash
# tools/gc_check.sh <binary> [stress n...]: the scripted run of gc_run.sh (12900 frames) with one binary: without the
# collector (GC_OFF, the reference), with it (64 MB and 12 MB arenas), in stress mode for each n given (a whole
# collection every n allocations) and with a slice of the collection at every allocation or so, of several sizes;
# the TRACE lines of every run must be the reference's. Two runs at a time. Runs in /root/gck-*.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
BIN=$1; shift
export PAD="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')" DT=0.0166 TRACE=10 TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=12900
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1
go() { # tag, then VAR=value words
  local d=/root/gck-$1; shift
  mkdir -p $d/data && cp -u $N/data/*.dat $d/data/ && cd $d && env "$@" $BIN > out.txt 2>&1; echo "exit $?" >> out.txt
  grep -a TRACE out.txt > trace.txt
}
go off GC_OFF=1 & go on GC_ON=1 & wait
go h12 GC_HEAP=12 &
for s in "$@"; do go s$s GC_STRESS=$s & wait; done
wait
# the collection in slices: one at every allocation (every 3rd, 7th), of 1 work unit, of a few, of the default
go a1u1 GC_STRESS_SLICE=1 GC_SLICE=1 & go a1u300 GC_STRESS_SLICE=1 GC_SLICE=300 & wait
go a1 GC_STRESS_SLICE=1 & go a3u5000 GC_STRESS_SLICE=3 GC_SLICE=5000 GC_HEAP=12 & wait
go a7u40000 GC_STRESS_SLICE=7 GC_SLICE=40000 & go u1000 GC_SLICE=1000 GC_HEAP=8 GC_POISON=1 & wait
for t in on h12 $(printf 's%s ' "$@") a1u1 a1u300 a1 a3u5000 a7u40000 u1000; do
  d=/root/gck-$t
  echo "$t: $(tail -1 $d/out.txt), $(cmp -s $d/trace.txt /root/gck-off/trace.txt && echo "TRACE identical ($(wc -l < $d/trace.txt) lines)" || echo "TRACE DIFFERS"), $(grep -a PERF $d/out.txt | tail -1 | grep -o 'gc used.*')"
  grep -avE '^(TRACE|PERF)' $d/out.txt | grep -aE 'ERROR|TODO_PORT|gc:|Sanitizer|runtime error' | head -3
done
