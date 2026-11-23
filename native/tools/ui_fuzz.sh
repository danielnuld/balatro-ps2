#!/bin/bash
# tools/ui_fuzz.sh <seed> <frames> <menu|run|tut|menuall|runall> [asan|opt]: a random pad script (python's random with <seed>) on both sides
# through tools/ui_run.sh, the overlay and the focused popup dumped every 30 frames. menu: from the main menu, the tutorial off;
# run: a run started by the hook at 1250 and its blind selected; tut: a fresh profile (the first run is the tutorial).
# Every unseeded run gets the seed FUZZ<seed> (FIXSEED). The pad script is left in /root/ui-fuzz-pad.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
PADS=$(python3 - $1 $2 $3 <<'PY'
import sys, random
seed, frames, mode = sys.argv[1], int(sys.argv[2]), sys.argv[3]
r = random.Random(seed)
B = ['dpup'] * 12 + ['dpdown'] * 14 + ['dpleft'] * 12 + ['dpright'] * 14 + ['a'] * 22 + ['b'] * 6 + ['x'] * 3 + ['y'] * 3 + \
    ['leftshoulder'] * 3 + ['rightshoulder'] * 4 + ['triggerleft', 'triggerright', 'start', 'start']
t, out = (1900 if mode.startswith('run') else 1300), []
while t < frames - 100:
    out.append('%d:%s' % (t, r.choice(B)))
    t += r.choice([20, 30, 30, 40, 40, 60, 90])
print(' '.join(out))
PY
)
echo "$PADS" > /root/ui-fuzz-pad.txt
S="1200:notut"; [ $3 = run ] && S="1250:start:FUZZ$1 1600:blind"; [ $3 = tut ] && S=""
[ $3 = menuall ] && S="1200:notut 1200:f:profile_select 1210:f:unlock_all 1220:f:unlock_all 1230:f:exit_overlay_menu"; [ $3 = runall ] && S="1200:f:profile_select 1210:f:unlock_all 1220:f:unlock_all 1230:f:exit_overlay_menu 1250:start:FUZZ$1 1600:blind"   # everything unlocked and discovered
FIXSEED=FUZZ$1 AUTO=30 AUTO_BOXES="$AUTO_BOXES" bash $N/tools/ui_run.sh $2 ${SHOTS:-none} ${4:-asan} "$S" "$PADS"
