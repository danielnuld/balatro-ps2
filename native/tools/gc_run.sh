#!/bin/bash
# The collector's scripted run: tools/gc_run.sh <dir tag> <opt|asan> [frames]  with GC_* in the environment.
# The first blind and the first shop (pad_round1 + pad_shop1), TRACE every 10 frames, nothing drawn; the run is in
# /root/native-<tag>. Prints the exit status, the last PERF line and any ERROR/sanitizer line.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
PAD="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')" DT=0.0166 RUN_DIR=/root/native-$1 TRACE=10 bash $N/tools/run.sh ${3:-12900} none $2
cd /root/native-$1
grep PERF out.txt | tail -1
grep -vE '^(TRACE|PERF)' out.txt | grep -E 'ERROR|TODO_PORT|gc:|Sanitizer|runtime error|Aborted|Segmentation' | head -5
grep TRACE out.txt > trace.txt; wc -l < trace.txt
