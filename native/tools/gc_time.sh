#!/bin/bash
# tools/gc_time.sh <binary> <tag> [more binaries and tags...]: the scripted run of gc_run.sh (12900 frames, nothing
# drawn) with each binary in turn, twice over; prints the user CPU time and the mean of the PERF lines' update and
# draw ms after frame 1200. The TRACE lines of each are kept in /root/native-key/trace-<tag>.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
mkdir -p /root/native-key/data && cp -u $N/data/*.dat /root/native-key/data/ && cd /root/native-key
export PAD="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')" DT=0.0166 TRACE=10 TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=12900
for round in 1 2; do
  set -- "$@"
  for ((i = 1; i < $#; i += 2)); do
    bin=${!i}; j=$((i + 1)); tag=${!j}
    /usr/bin/time -f "%U" -o time-$tag.txt $bin > out-$tag.txt 2>&1
    grep TRACE out-$tag.txt > trace-$tag.txt
    echo "$tag: user $(cat time-$tag.txt) s  $(grep -a PERF out-$tag.txt | awk '$3 > 1200 { u += $9; d += $12; n++ } END { printf "update %.3f ms draw %.3f ms", u / n, d / n }')"
  done
done
