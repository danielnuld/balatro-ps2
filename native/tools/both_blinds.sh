#!/bin/bash
# tools/both_blinds.sh <case name> <frames> <asan|opt> [shots]: the oracle and the port driven by the blinds hook
# (tools/hook_blinds.lua, src/hook_blinds.c); the case's parameters come from the environment (BOSS, FTAG, PLAN, HAND,
# ANTE, MONEY, JOKERS, UNSEED, SPEED, SEED). Results in /root/bl-<case>/{n,o}/out.txt, cmp-n.txt, cmp-o.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
C=$1; F=$2; B=$3; SH=${4:-none}
D=/root/bl-$C; mkdir -p $D/o $D/n
cd $D/o && rm -f shot-*.ppm
( export TRACE=${TRACE:-10} DT=${DT:-0.0166} HOOK=$N/tools/hook_blinds.lua; [ "$SH" != none ] && export SHOT=$SH; ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} $F menu > out.txt 2>&1 ) &
# the port, as tools/run.sh runs it, but from a copy of the binary (/root/bl-bin, made by tools/build_blinds.sh): the
# a relink of the shared one while a sweep runs would break it
BIN=/root/bl-bin/balatro-asan; [ "$B" = opt ] && BIN=/root/bl-bin/balatro
( cd $D/n && mkdir -p data && cp -u $N/data/*.dat data/ && rm -f shot-*.ppm
  [ "$SH" != none ] && export SHOT=$SH
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 TRACE=${TRACE:-10} DT=${DT:-0.0166} HOOK=blinds TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=$F $BIN > out.txt 2>&1
  echo "exit $?" > exit.txt
  for f in shot-*.ppm; do [ -f $f ] && cp $f $N/out/nbl$C-$f; done )
wait
cd $D
for f in o/shot-*.ppm; do [ -f $f ] && cp $f $N/out/obl$C-$(basename $f); done 2>/dev/null
grep '^CMP' n/out.txt | grep -v ' tick ' > cmp-n.txt; grep '^CMP' o/out.txt | grep -v ' tick ' > cmp-o.txt
grep '^CMP' n/out.txt > all-n.txt; grep '^CMP' o/out.txt > all-o.txt
paste <(grep TRACE n/out.txt | awk '{print $2, $4, $6, $8, $10}') <(grep TRACE o/out.txt | awk '{print $4, $6, $8, $10}') > both.txt
echo "== $C: $(cat n/exit.txt) cmp lines n=$(wc -l < cmp-n.txt) o=$(wc -l < cmp-o.txt) last: $(tail -1 cmp-o.txt | cut -c1-60)"
awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) print "PARTS:", $0 }' both.txt | head -${PARTS:-3}
echo "parts total: $(awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) c++ } END {print c+0}' both.txt) of $(wc -l < both.txt)"
# the rules: the CMP lines without their frame number (a timed event may fire a frame apart: the port's clocks are floats)
diff <(cut -d' ' -f3- cmp-n.txt) <(cut -d' ' -f3- cmp-o.txt) | head -${DIFFL:-4} | cut -c1-${DIFFW:-900}
echo "cmp frames apart: $(paste -d' ' <(cut -d' ' -f2 cmp-n.txt) <(cut -d' ' -f2 cmp-o.txt) | awk '{ if ($1 != $2) c++; d = $1 - $2 } END { print c+0, "last", d+0 }')"
grep -n "ERROR\|TODO_PORT\|Sanitizer\|runtime error\|attempt to\|stack traceback" n/out.txt o/out.txt | head -8
