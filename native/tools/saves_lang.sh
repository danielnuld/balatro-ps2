#!/bin/bash
# tools/saves_lang.sh <asan|opt> <language key>...: per language, a first boot that changes to it at the main menu
# (frame 1300) and plays a hand of a new run, then a second boot from the settings it saved (the language must come
# back) with a new run. One line per boot. SHOT=<frames> keeps screenshots of the last language run.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
B=$1; shift
line() { echo "$1: $(grep -m1 '^exit' /root/lang.txt), SUM lines $(grep -c ^SUM /root/oracle-saves/out.txt) oracle $(grep -c ^SUM /root/native-saves/out.txt) port, differing $(grep -c '^<' /root/lang.txt), TRACE parts $(grep -c ^PARTS /root/lang.txt), errors $(grep -c 'ERROR\|TODO_PORT\|runtime error\|AddressSanitizer' /root/native-saves/out.txt) port $(grep -c '^ERROR' /root/oracle-saves/out.txt) oracle, language $(grep -m1 -o 'S.language .*' /root/native-saves/out.txt) / $(grep -m1 -o 'S.language .*' /root/oracle-saves/out.txt)"; }
for l in "$@"; do
  export OSAVE=/root/saves-o-lang NSAVE=/root/saves-n-lang
  rm -rf $OSAVE $NSAVE
  NOTUT=1 LANGAT=1300 GLANG=$l SEED=SWEEP START=1700 SELECT=2000 PLAY=2300 UNLOCKS=2690 SUM=100 bash $N/tools/saves.sh 2700 "" $B > /root/lang.txt 2>&1
  line "$l first boot"; cp /root/lang.txt /root/lang-$l-1.txt; cp /root/native-saves/out.txt /root/lang-$l-1n.txt; cp /root/oracle-saves/out.txt /root/lang-$l-1o.txt
  NOTUT=1 SEED=SWEEP UNLOCKS=1250 START=1300 SELECT=1600 PLAY=1900 SUM=100 bash $N/tools/saves.sh 2300 "" $B > /root/lang.txt 2>&1
  line "$l second boot"; cp /root/lang.txt /root/lang-$l-2.txt
done
