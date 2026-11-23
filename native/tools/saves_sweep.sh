#!/bin/bash
# tools/saves_sweep.sh <asan|opt> <case>...: a new run per case on the oracle and the port (fresh save directories,
# seed SWEEP, tutorial off): started at frame 1300, small blind selected at 1600, first 5 cards played at 1900, 2400
# frames; one line per case: how many SUM lines (every 100 frames) differ and the first TRACE lines that part.
# A case is deck=<b_key>, stake=<n>, challenge=<index>, seed=<text>, or several joined by commas.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
B=$1; shift
for c in "$@"; do
  unset DECK STAKE CHALLENGE; SEED=SWEEP
  for kv in ${c//,/ }; do
    case $kv in deck=*) export DECK=${kv#deck=};; stake=*) export STAKE=${kv#stake=};; challenge=*) export CHALLENGE=${kv#challenge=};; seed=*) SEED=${kv#seed=};; esac
  done
  rm -rf /root/saves-o-sweep /root/saves-n-sweep
  OSAVE=/root/saves-o-sweep NSAVE=/root/saves-n-sweep NOTUT=1 SEED=$SEED START=1300 SELECT=1600 PLAY=1900 SUM=100 \
    bash $N/tools/saves.sh ${FRAMES:-2400} "" $B > /root/sweep-$c.txt 2>&1
  cp /root/native-saves/out.txt /root/sweep-$c-n.txt; cp /root/oracle-saves/out.txt /root/sweep-$c-o.txt
  echo "$c: $(grep -m1 '^exit' /root/sweep-$c.txt), SUM lines $(grep -c ^SUM /root/sweep-$c-o.txt) oracle $(grep -c ^SUM /root/sweep-$c-n.txt) port, differing $(grep -c '^<' /root/sweep-$c.txt), TRACE parts $(grep -c ^PARTS /root/sweep-$c.txt), errors $(grep -c 'ERROR\|TODO_PORT\|runtime error\|AddressSanitizer' /root/sweep-$c-n.txt) port $(grep -c '^ERROR' /root/sweep-$c-o.txt) oracle"
done
