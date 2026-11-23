#!/bin/bash
# tools/text_shots.sh <prefix> [VAR=value ...]: host screenshots of screens with small text (the ui hook's steps of
# tools/steps_ui_shots1.txt: a hand with jokers, a joker's popup, run info, the deck, options, settings), as
# native/out/<prefix>-shot-<frame>.ppm. The variables are love_gfx.c's: NO_LEVELS=1 (text as it was), VIDEO=720p,
# SHADOW_FROM=<pixels>, TEXTLOG=<frame>.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
P=$1; shift
for v in "$@"; do export "$v"; done
export HOOK=ui STEPS="$(tr '\n' ' ' < $N/tools/steps_ui_shots1.txt)" DT=0.0166 SOFT_FROM=1980
RUN_DIR=/root/native-$P PREFIX=$P- bash $N/tools/run.sh 2330 1995,2030,2100,2180,2260,2320 opt
grep -a 'ERROR\|^gfx' /root/native-$P/out.txt | head -5
