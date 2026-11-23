#!/bin/bash
# tools/both.sh <frames> <shots|none> <asan|opt> "<pad script>": the oracle and the native port on the same pad
# script, their TRACE lines side by side (native | oracle) in /root/both-${TAG:-pad}.txt, the first lines that part printed.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
export PAD="$4"
mkdir -p /root/oracle-${TAG:-pad} && cd /root/oracle-${TAG:-pad} && rm -f shot-*.ppm
( export TRACE=${TRACE:-100}; [ "$2" != none ] && export SHOT=$2; ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} $1 menu > out.txt 2>&1 ) &
RUN_DIR=/root/native-${TAG:-pad} PREFIX=n${TAG:-pad}- bash $N/tools/run.sh $1 $2 $3
wait
for f in shot-*.ppm; do cp $f $N/out/o${TAG:-pad}-$f; done 2>/dev/null
paste <(grep TRACE /root/native-${TAG:-pad}/out.txt | awk '{print $2, $4, $6, $8, $10}') <(grep TRACE out.txt | awk '{print $4, $6, $8, $10}') > /root/both-${TAG:-pad}.txt
awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) print "PARTS:", $0 }' /root/both-${TAG:-pad}.txt | head -5
grep -v TRACE /root/native-${TAG:-pad}/out.txt | head -${LINES_OUT:-40}
tail -3 /root/both-${TAG:-pad}.txt
