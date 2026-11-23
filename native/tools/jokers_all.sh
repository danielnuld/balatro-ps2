#!/bin/bash
# tools/jokers_all.sh [asan|opt] [keys...]: tools/jokers.sh for every joker (CASE=<key>), or for the keys given.
# Results in /root/jk/results-<mode>.txt (SAME / DIFF lines).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
mode=${1:-opt}; shift
mkdir -p /root/jk
if [ ! -s /root/jk/keys.txt ]; then
  (cd /root/jk && HOOK=$N/tools/hook_jokers_keys.lua ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} 100 menu 2>&1 | grep -a '^KEY ' | cut -d' ' -f2 > keys.txt)
fi
cp ${BALATRO_BUILD:-$HOME/balatro-native}/balatro$([ "$mode" = asan ] && echo -asan) /root/jk/bin-$mode 2>/dev/null; export JKBIN=/root/jk/bin-$mode
keys="$@"; [ -z "$keys" ] && keys=$(cat /root/jk/keys.txt)
for k in $keys; do CASE=$k bash $N/tools/jokers.sh $k $mode; done 2>&1 | tee /root/jk/results-$mode.txt | grep -a -c '^SAME' 
grep -a '^DIFF' /root/jk/results-$mode.txt | awk '{print $2}' | tr '\n' ' '
