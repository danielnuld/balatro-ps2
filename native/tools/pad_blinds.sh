#!/bin/bash
# tools/pad_blinds.sh <case name> <frames> <asan|opt> <shots|none> "<pad script>": as tools/both_blinds.sh, but the hook
# only starts the run (NOAUTO) and the pad script plays: the blind select screen, the hand, the overlays, as a player.
# BLBIN: the directory of the binaries (default /root/bl-bin).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
C=$1; F=$2; B=$3; SH=${4:-none}
export PAD="$5" NOAUTO=1 TRACE=${TRACE:-10} DT=0.0166
D=/root/bl-$C; mkdir -p $D/o $D/n
cd $D/o && rm -f shot-*.ppm
( export HOOK=$N/tools/hook_blinds.lua; [ "$SH" != none ] && export SHOT=$SH; ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} $F menu > out.txt 2>&1 ) &
BIN=${BLBIN:-/root/bl-bin}/balatro-asan; [ "$B" = opt ] && BIN=${BLBIN:-/root/bl-bin}/balatro
( cd $D/n && mkdir -p data && cp -u $N/data/*.dat data/ && rm -f shot-*.ppm
  [ "$SH" != none ] && export SHOT=$SH
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 HOOK=blinds TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=$F $BIN > out.txt 2>&1
  echo "exit $?" > exit.txt
  for f in shot-*.ppm; do [ -f $f ] && cp $f $N/out/nbl$C-$f; done )
wait
cd $D
for f in o/shot-*.ppm; do [ -f $f ] && cp $f $N/out/obl$C-$(basename $f); done 2>/dev/null
grep '^CMP' n/out.txt > all-n.txt; grep '^CMP' o/out.txt > all-o.txt
paste <(grep TRACE n/out.txt | awk '{print $2, $4, $6, $8, $10}') <(grep TRACE o/out.txt | awk '{print $4, $6, $8, $10}') > both.txt
echo "== $C: $(cat n/exit.txt) cmp lines n=$(wc -l < all-n.txt) o=$(wc -l < all-o.txt) last: $(tail -1 all-o.txt | cut -c1-${LASTW:-200})"
awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) print "PARTS:", $0 }' both.txt | head -${PARTS:-3}
echo "parts total: $(awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) c++ } END {print c+0}' both.txt) of $(wc -l < both.txt)"
diff all-n.txt all-o.txt | head -${DIFFL:-4} | cut -c1-${DIFFW:-900}
grep -n "ERROR\|TODO_PORT\|Sanitizer\|runtime error\|attempt to\|stack traceback" n/out.txt o/out.txt | head -8
