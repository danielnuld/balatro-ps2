#!/bin/bash
# tools/gc_cases.sh <binary> <stress n> [name filter]: the other harnesses' cases (jokers, blinds, shop, saves) run by
# the port alone, twice with the same binary: without the collector (GC_OFF) and with it in stress mode (a collection
# every n allocations, freed blocks filled with 0xDD). Everything the two runs print must be the same, PERF lines
# apart: a difference or a crash is a pointer the collector does not see. Runs in /root/gcx (nobody else's).
# SLICE=<units> in the environment: instead, a slice of that many work units of the collection every n allocations.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
BIN=$1; S=$2; RE=${3:-.}
one() { # name, frames, then VAR=value words (@M@ in one: the mode, for a save directory each)
  local name=$1 frames=$2; shift 2
  echo "$name" | grep -q -E "$RE" || return
  for m in off gc; do
    local d=/root/gcx/$name-$m; mkdir -p $d/data; cp -u $N/data/*.dat $d/data/
    ( cd $d && if [ $m = off ]; then export GC_OFF=1; elif [ -n "$SLICE" ]; then export GC_STRESS_SLICE=$S GC_SLICE=$SLICE; else export GC_STRESS=$S; fi
      env DT=0.0166 TIME=1700000000 TRACE=10 GAME_DIR=${BALATRO_GAME:-/root/balatro-src/build/game} FRAMES=$frames "${@//@M@/$m}" timeout 3000 $BIN > out.txt 2>&1; echo "exit $?" >> out.txt ) &
  done
  wait
  if cmp -s <(grep -av '^PERF' /root/gcx/$name-off/out.txt) <(grep -av '^PERF' /root/gcx/$name-gc/out.txt) && grep -q '^exit 0' /root/gcx/$name-gc/out.txt; then
    echo "SAME $name ($(grep -avc '^PERF' /root/gcx/$name-gc/out.txt) lines, $(grep -a PERF /root/gcx/$name-gc/out.txt | tail -1 | grep -o 'collections [0-9]*'))"
  else
    echo "DIFF $name: $(tail -1 /root/gcx/$name-off/out.txt) / $(tail -1 /root/gcx/$name-gc/out.txt)"
    diff <(grep -av '^PERF' /root/gcx/$name-off/out.txt) <(grep -av '^PERF' /root/gcx/$name-gc/out.txt) | head -6 | cut -c1-300
  fi
}
# jokers (tools/jokers_cases.txt: name|VAR=value ...), every 12th case
grep -v '^#' $N/tools/jokers_cases.txt | tr -d '\r' | awk 'NF && NR % 12 == 0' | while IFS='|' read -r name rest; do
  [ -n "$name" ] && one jk_$name ${JK_FRAMES:-40000} HOOK=jokers PAD= $rest < /dev/null
done
# blinds (name frames VAR=value ...), every 6th case of each list
for l in boss flow tag; do
  grep -v '^#' $N/tools/cases_blinds_$l.txt | tr -d '\r' | awk 'NF && NR % 6 == 1' | while read name frames vars; do
    one bl_$name $frames HOOK=blinds SPEED=4 $vars < /dev/null
  done
done
# the shop's step lists
for c in packs spectral vouchers rerolls; do
  [ -f $N/tools/steps_shop_$c.txt ] && one shop_$c ${SHOP_FRAMES:-9000} HOOK=shop STEPS="$(tr '\n' ' ' < $N/tools/steps_shop_$c.txt)" < /dev/null
done
# saves: the first blind and shop with a save directory (the run is saved as it goes), then a second start that finds
# the settings, the profile and the run of the first
rm -rf /root/gcx/save-off /root/gcx/save-gc; mkdir -p /root/gcx/save-off /root/gcx/save-gc
ROUND="$(cat $N/tools/pad_round1.txt | tr '\n' ' ')"; SHOP="$(cat $N/tools/pad_shop1.txt | tr '\n' ' ')"
one saves 12900 SAVE_DIR=/root/gcx/save-@M@ PAD="$ROUND $SHOP" < /dev/null
one saves_again 3000 SAVE_DIR=/root/gcx/save-@M@ PAD="$ROUND" < /dev/null
