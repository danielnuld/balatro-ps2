#!/bin/bash
# tools/sweep_blinds.sh <list file> <asan|opt> [name filter]: runs every case of the list (lines: name frames VAR=value...)
# through tools/both_blinds.sh, one after the other; the summary goes to /root/bl-sweep-<list name>.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
L=$1; O=/root/bl-sweep-$(basename $L .txt).txt; : > $O
grep -v '^#' $L | while read name frames vars; do
  [ -z "$name" ] && continue
  [ -n "$3" ] && ! [[ $name =~ $3 ]] && continue
  env SPEED=4 $vars bash $N/tools/both_blinds.sh $name $frames $2 >> $O 2>&1 < /dev/null
done
echo SWEEP DONE >> $O
