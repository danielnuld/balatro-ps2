#!/bin/bash
# tools/saves_cont.sh <asan|opt> <case>...: per case (as tools/saves_sweep.sh: deck=, stake=, challenge=, seed=) a run
# saved after its first hand (forced at frame 2350), then continued on a second boot (a hand played at 1800), then
# continued again with the two save directories swapped (the port loads the oracle's save and the other way round).
# One line per step: SUM lines differing (the save file itself is dumped at frame 1290 of the continued runs).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
B=$1; shift
line() { echo "$1: $(grep -m1 '^exit' /root/cont.txt), SUM lines $(grep -c ^SUM /root/oracle-saves/out.txt) oracle $(grep -c ^SUM /root/native-saves/out.txt) port, differing $(grep -c '^<' /root/cont.txt), TRACE parts $(grep -c ^PARTS /root/cont.txt), errors $(grep -c 'ERROR\|TODO_PORT\|runtime error\|AddressSanitizer' /root/native-saves/out.txt) port $(grep -c '^ERROR' /root/oracle-saves/out.txt) oracle"; }
for c in "$@"; do
  unset DECK STAKE CHALLENGE; SEED=SWEEP
  for kv in ${c//,/ }; do
    case $kv in deck=*) export DECK=${kv#deck=};; stake=*) export STAKE=${kv#stake=};; challenge=*) export CHALLENGE=${kv#challenge=};; seed=*) SEED=${kv#seed=};; esac
  done
  export OSAVE=/root/saves-o-cont NSAVE=/root/saves-n-cont
  rm -rf $OSAVE $NSAVE $OSAVE-0 $NSAVE-0
  NOTUT=1 SEED=$SEED START=1300 SELECT=1600 PLAY=1900 FORCESAVE=2350 SUM=100 bash $N/tools/saves.sh 2400 "" $B > /root/cont.txt 2>&1
  line "$c save"
  cp -r $OSAVE $OSAVE-0; cp -r $NSAVE $NSAVE-0
  unset DECK STAKE CHALLENGE
  NOTUT=1 DUMPSAVE=1290 CONT=1300 PLAY=1800 SUM=100 bash $N/tools/saves.sh 2300 "" $B > /root/cont.txt 2>&1
  line "$c continue"; cp /root/cont.txt /root/cont-$c-continue.txt
  rm -rf $OSAVE $NSAVE; cp -r $OSAVE-0 $NSAVE; cp -r $NSAVE-0 $OSAVE
  NOTUT=1 DUMPSAVE=1290 CONT=1300 PLAY=1800 SUM=100 bash $N/tools/saves.sh 2300 "" $B > /root/cont.txt 2>&1
  line "$c cross"; cp /root/cont.txt /root/cont-$c-cross.txt
done
