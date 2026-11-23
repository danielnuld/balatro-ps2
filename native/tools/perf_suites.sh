#!/bin/bash
# tools/perf_suites.sh [part...]: the other harnesses' cases against the tree as it is, one after the other (never more
# than two game processes), results appended to /root/perf-game/suites.txt. Parts: build round stress blinds shop
# saves gc (default: all). The binaries are copies (/root/bl-bin, /root/perf-game/bin): a relink does not break a run.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
O=/root/perf-game/suites.txt; mkdir -p /root/perf-game/bin
P="${*:-build round stress blinds shop saves gc}"
has() { [[ " $P " == *" $1 "* ]]; }
PADS="$(cat $N/tools/pad_round1.txt $N/tools/pad_shop1.txt | tr '\n' ' ')"
parts() { awk '{ if ($2 != $6 || $3 != $7 || $5 != $9) n++ } END { print n+0, "of", NR }' $1; }
echo "==== $(date +%H:%M) parts: $P" >> $O
if has build; then
  bash $N/tools/build_blinds.sh > /root/perf-game/build.txt 2>&1; tail -1 /root/perf-game/build.txt >> $O
  cp ${BALATRO_BUILD:-$HOME/balatro-native}/balatro ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan /root/perf-game/bin/
fi
if has round; then # the scripted round against the oracle: the default dt, then DT=0.0166
  ( cd $N; TAG=perf TRACE=10 LINES_OUT=1 bash tools/both.sh 12900 none opt "$PADS" > /dev/null 2>&1 )
  echo "round, default dt: lines that part: $(parts /root/both-perf.txt)" >> $O
  ( cd $N; TAG=perf DT=0.0166 TRACE=10 LINES_OUT=1 bash tools/both.sh 12900 none opt "$PADS" > /dev/null 2>&1 )
  echo "round, DT=0.0166: lines that part: $(parts /root/both-perf.txt)" >> $O
fi
if has stress; then
  GC_STRESS=20 bash $N/tools/gc_run.sh perfstress opt > /root/perf-game/stress.txt 2>&1
  echo "GC_STRESS=20 round: $(head -1 /root/perf-game/stress.txt), TRACE lines differing from the plain run: $(diff <(grep TRACE /root/native-perfstress/out.txt) <(grep TRACE /root/native-perf/out.txt) | grep -c '^<')" >> $O
fi
if has blinds; then
  for l in ${BL_LISTS:-boss tag flow}; do
    bash $N/tools/sweep_blinds.sh $N/tools/cases_blinds_$l.txt opt
    S=/root/bl-sweep-cases_blinds_$l.txt
    echo "blinds $l: cases $(grep -c '^== ' $S), exit 0: $(grep -c '^== .*exit 0' $S), with TRACE lines apart: $(grep -c '^parts total: [1-9]' $S), with rule (CMP) differences: $(grep -c '^[<>]' $S), with cmp frames apart: $(grep -c '^cmp frames apart: [1-9]' $S), error lines: $(grep -c 'ERROR\|TODO_PORT\|Sanitizer\|runtime error' $S)" >> $O
  done
fi
if has shop; then
  for c in ${SHOP_CASES:-basic packs packs2 vouchers tarot planet spectral states enh inter rerolls deck hover}; do
    [ -f $N/tools/steps_shop_$c.txt ] || continue
    bash $N/tools/shop_run.sh ${SHOP_FRAMES:-30000} none opt $c > /root/perf-game/shop-$c.txt 2>&1
    echo "shop $c: $(grep -a '^D lines' /root/perf-game/shop-$c.txt), TRACE parts $(grep -ac '^PARTS' /root/perf-game/shop-$c.txt), last step oracle $(grep -a '^STEP' /root/oracle-shop/out.txt | tail -1 | cut -c1-40) / port $(grep -a '^STEP' /root/native-shop/out.txt | tail -1 | cut -c1-40), errors $(grep -ac 'ERROR\|TODO_PORT\|Sanitizer\|runtime error' /root/native-shop/out.txt)" >> $O
  done
fi
if has saves; then
  bash $N/tools/saves_sweep.sh opt ${SAVE_CASES:-deck=b_red deck=b_plasma deck=b_abandoned stake=8 challenge=3 challenge=12 seed=PERF1} >> $O 2>&1
  bash $N/tools/saves_cont.sh opt ${CONT_CASES:-deck=b_red deck=b_magic,stake=5 challenge=7} >> $O 2>&1
fi
if has gc; then
  bash $N/tools/gc_cases.sh ${GC_BIN:-/root/perf-game/bin/balatro} 20 >> $O 2>&1
fi
echo "==== $(date +%H:%M) done" >> $O
