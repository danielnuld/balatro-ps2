#!/bin/bash
# tools/jokers_cases.sh [asan|opt] [regex]: every case of tools/jokers_cases.txt (or those whose name matches) through
# tools/jokers.sh. Results in /root/jk/cases-<mode>.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
mode=${1:-opt}; re=${2:-.}
mkdir -p /root/jk
cp ${BALATRO_BUILD:-$HOME/balatro-native}/balatro$([ "$mode" = asan ] && echo -asan) /root/jk/bin-$mode 2>/dev/null; export JKBIN=/root/jk/bin-$mode
grep -v '^#' $N/tools/jokers_cases.txt | tr -d '\r' | while IFS='|' read -r name rest; do
  [ -n "$name" ] && echo "$name" | grep -q -E "$re" || continue
  # the environment is NAME=value words; a value may hold '|' (the held cards), so only the first '|' splits the line
  env $rest bash $N/tools/jokers.sh $name $mode < /dev/null
done 2>&1 | tee /root/jk/cases-$mode.txt | grep -a -c '^SAME'
grep -a '^DIFF' /root/jk/cases-$mode.txt | awk '{print $2}' | tr '\n' ' '
