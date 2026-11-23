#!/bin/bash
# tools/ui_fuzz_loop.sh <mode> <frames> <seed>...: tools/ui_fuzz.sh for each seed, one summary line each; the outputs of a seed that
# differs are kept in /root/ui-fuzz-keep/<mode>-<seed>/.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
mode=$1; frames=$2; shift 2
for s in "$@"; do
  r=$(bash $N/tools/ui_fuzz.sh $s $frames $mode 2>&1)
  bad=$(echo "$r" | grep -a 'PARTS\|different: [1-9]\|ERROR\|TODO_PORT\|Sanitizer\|runtime error\|rror\|traceback\|^exit [1-9]' | head -4 | tr '\n' '|')
  echo "$mode $s: F $(wc -l < /root/ui-o-F.txt) U $(wc -l < /root/ui-o-U.txt) ${bad:-ok}"
  if [ -n "$bad" ]; then mkdir -p /root/ui-fuzz-keep/$mode-$s; cp /root/ui-[no]-[FUD].txt /root/ui-fuzz-pad.txt /root/both-ui.txt /root/ui-fuzz-keep/$mode-$s/; cp /root/native-ui/out.txt /root/ui-fuzz-keep/$mode-$s/n-out.txt; cp /root/oracle-ui/out.txt /root/ui-fuzz-keep/$mode-$s/o-out.txt; fi
done
