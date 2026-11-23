#!/bin/bash
# tools/ui_all.sh [asan|opt]: every ui case (tools/steps_ui_<name>.txt, with tools/pad_ui_<name>.txt where there is one) on both
# sides, one line each: "ok" or the first lines that differ. About 4 minutes.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
run() { # name frames [AUTO [boxes]]
  r=$(AUTO=$3 AUTO_BOXES="$4" bash $N/tools/ui_run.sh $2 none ${BIN:-asan} $1 2>&1)
  bad=$(echo "$r" | grep -a 'different: [1-9]\|ERROR\|TODO_PORT\|Sanitizer\|runtime error\|^exit [1-9]\|no binary' | head -4 | cut -c1-200 | tr '\n' '|')
  echo "$1: F $(wc -l < /root/ui-o-F.txt) U $(wc -l < /root/ui-o-U.txt) D $(wc -l < /root/ui-o-D.txt) ${bad:-ok}"
}
BIN=$1
run settings 5200 20
run popups 2400
run eval 3700
run lose 3700
run wingame 3700
run drag 3600 40 "HUD buttons"
run runpad 6400 20 "HUD buttons"
run sweep 6500
run setup 3520
run misc 3400
