#!/bin/bash
# tools/saves.sh <frames> "<pad script>" [asan|opt]: the oracle and the port with the saves hook and a save directory
# each (OSAVE, NSAVE; default /root/saves-o and /root/saves-n; they are NOT wiped: a second call continues from them).
# CASE and the other variables of tools/hook_saves.lua / src/hook_saves.c pass through. Prints the first TRACE lines
# that part and the diff of the hooks' SUM lines. SHOT=<frames> for screenshots (native/out/nsaves-*, osaves-*).
# The port's binary is run from a copy in /root/saves-bin (a relink mid-run does not break the run).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
O=/root/oracle-saves; R=/root/native-saves
export PAD="$2" TRACE=${TRACE:-10} DT=0.0166
mkdir -p $O $R/data /root/saves-bin ${OSAVE:=/root/saves-o} ${NSAVE:=/root/saves-n} && cd $O && rm -f shot-*.ppm
( HOOK=$N/tools/hook_saves.lua SAVE_DIR=$OSAVE ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} $1 menu > out.txt 2>&1 ) &
B=balatro-asan; [ "$3" = opt ] && B=balatro
[ -x ${BALATRO_BUILD:-$HOME/balatro-native}/$B ] && cp -u ${BALATRO_BUILD:-$HOME/balatro-native}/$B /root/saves-bin/$B
( cd $R && cp -u $N/data/*.dat data/ && rm -f shot-*.ppm
  [ -z "$SHOT" ] && unset SHOT
  HOOK=saves SAVE_DIR=$NSAVE ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 TIME=1700000000 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=$1 /root/saves-bin/$B > out.txt 2>&1
  echo "exit $?"
  for f in shot-*.ppm; do cp $f $N/out/nsaves-$f; done 2>/dev/null )
wait
for f in shot-*.ppm; do cp $f $N/out/osaves-$f; done 2>/dev/null
paste <(grep ^TRACE $R/out.txt | awk '{print $2, $4, $6, $8, $10}') <(grep ^TRACE out.txt | awk '{print $4, $6, $8, $10}') > /root/both-saves.txt
awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) print "PARTS:", $0 }' /root/both-saves.txt | head -${PARTS:-5}
grep -v "^TRACE\|^SUM\|^PERF\|^FLM\|^BOOT" $R/out.txt | head -${LINES_OUT:-30}
grep "rror\|traceback" out.txt | head -5
tail -1 /root/both-saves.txt
diff <(grep ^SUM $R/out.txt) <(grep ^SUM out.txt) > /root/sum-saves.diff && echo "SUM equal ($(grep -c ^SUM out.txt) lines)" || head -${DIFF_OUT:-20} /root/sum-saves.diff
