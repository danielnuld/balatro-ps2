#!/bin/bash
# tools/jokers.sh <name> [asan|opt]: one case of the jokers comparison. The case is in the environment (CASE, CONS,
# SEED, ACT<i>, STAKE: see tools/hook_jokers.lua). Runs the oracle and the port with their hooks, keeps the "JK" lines in
# /root/jk/<name>.o.txt and .n.txt (the frame field removed) and prints SAME or DIFF with the first lines that part.
# ONLY=n runs the port alone against the kept oracle output (after a fix).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
name=$1; mode=${2:-opt}
mkdir -p /root/jk /root/jk/run-o-$name /root/jk/run-n-$name
export DT=0.0166 TIME=1700000000 PAD=""
if [ "$ONLY" != n ]; then
  ( cd /root/jk/run-o-$name && HOOK=$N/tools/hook_jokers.lua timeout 600 ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} ${FRAMES:-40000} menu > out.txt 2>&1 ) &
fi
cd /root/jk/run-n-$name; [ -d data ] || mkdir data; cp -u $N/data/*.dat data/
BIN=${BALATRO_BUILD:-$HOME/balatro-native}/balatro; [ "$mode" = asan ] && BIN=${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan
[ -n "$JKBIN" ] && BIN=$JKBIN   # a private copy: the shared binary disappears while another build links
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 HOOK=jokers GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=${FRAMES:-40000} timeout 900 $BIN > out.txt 2>&1
rc=$?
wait
grep -a '^JK ' /root/jk/run-n-$name/out.txt | sed 's/ f=[0-9]*$//' > /root/jk/$name.n.txt
[ "$ONLY" != n ] && grep -a '^JK ' /root/jk/run-o-$name/out.txt | sed 's/ f=[0-9]*$//' > /root/jk/$name.o.txt
bad=$(grep -a -c 'ERROR\|TODO_PORT\|runtime error\|AddressSanitizer\|Assertion\|stack traceback' /root/jk/run-n-$name/out.txt)
if cmp -s /root/jk/$name.o.txt /root/jk/$name.n.txt && [ -s /root/jk/$name.o.txt ] && grep -q '^JK END \(done\|gameover\)' /root/jk/$name.o.txt && [ "$bad" = 0 ]; then
  echo "SAME $name ($(wc -l < /root/jk/$name.o.txt) lines)"
else
  echo "DIFF $name rc=$rc bad=$bad oracle_end='$(grep -a '^JK END' /root/jk/$name.o.txt)' native_end='$(grep -a '^JK END' /root/jk/$name.n.txt)'"
  diff /root/jk/$name.o.txt /root/jk/$name.n.txt | head -${DIFFLINES:-6} | cut -c1-${DIFFCOLS:-900}
  grep -a 'ERROR\|TODO_PORT\|runtime error\|AddressSanitizer\|Assertion' /root/jk/run-n-$name/out.txt | head -3
fi
