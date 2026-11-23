#!/bin/bash
# tools/ui_run.sh <frames> <shots|none> <asan|opt> "<steps>|<case name>" ["<pad script>"]: the oracle and the port with
# the ui hook (tools/hook_ui.lua, src/hook_ui.c); then the F (focus) and U (ui trees) lines are diffed in order and the D lines sorted.
# A case name: tools/steps_ui_<name>.txt and, if no pad is given, tools/pad_ui_<name>.txt.
# A pad script "@<start>,<step>: a dpright*3 w2 b": one press every <step> frames from <start> (wN: N steps without a press, x*N: N presses,
# hold:<button>:<frames>: that button held from here on while the rest of the script goes on).
# AUTO=<n>: also dumps the overlay menu and the focused node's popup every n frames from frame 1200 (AUTO_BOXES="HUD buttons": more boxes).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
S="$4"; P="$5"
[ -f "$N/tools/pad_ui_$4.txt" ] && [ -z "$5" ] && P=$(tr '\n' ' ' < "$N/tools/pad_ui_$4.txt")
[ -f "$N/tools/steps_ui_$4.txt" ] && S=$(tr '\n' ' ' < "$N/tools/steps_ui_$4.txt")
P=$(python3 - "$P" <<'EOF'
import sys
p = sys.argv[1].strip()
if p.startswith('@'):
    head, rest = p[1:].split(':', 1)
    t, step = map(int, head.split(','))
    out = []
    for w in rest.split():
        if w[0] == 'w' and w[1:].isdigit(): t += step * int(w[1:]); continue
        if w.startswith('hold:'):  # hold:<button>:<frames>: held from here on (a press lasts 2 frames), the script goes on meanwhile
            out += ['%d:%s' % (f, w.split(':')[1]) for f in range(t, t + int(w.split(':')[2]), 2)]; continue
        b, _, n = w.partition('*')
        for _ in range(int(n or 1)): out.append('%d:%s' % (t, b)); t += step
    p = ' '.join(out)
    sys.stderr.write('pad ends at frame %d\n' % t)
print(p)
EOF
)
if [ -n "$AUTO" ]; then
  for ((f = 1200; f < $1; f += AUTO)); do S="$S $f:ui:OVERLAY_MENU $f:pop"; for b in $AUTO_BOXES; do S="$S $f:ui:$b"; done; done
fi
export TAG=ui DT=0.0166 TRACE=${TRACE:-10} HOOK=ui STEPS="$S"
mkdir -p /root/oracle-ui && cp $N/tools/hook_ui.lua /root/oracle-ui/ui   # HOOK=ui: a file for the oracle, a name for the port
[ -x ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan ] || { echo "no binary: build failed or in progress"; exit 1; }
bash $N/tools/both.sh $1 $2 $3 "$P" | grep -a '^PARTS\|^exit'
for k in F U D; do
  grep -a "^$k " /root/native-ui/out.txt > /root/ui-n-$k.txt; grep -a "^$k " /root/oracle-ui/out.txt > /root/ui-o-$k.txt
  [ $k = D ] && { sort -o /root/ui-n-D.txt /root/ui-n-D.txt; sort -o /root/ui-o-D.txt /root/ui-o-D.txt; }
  echo -n "$k lines: "; python3 $N/tools/ui_diff.py /root/ui-n-$k.txt /root/ui-o-$k.txt ${DIFF_LINES:-30}
done
grep -a 'ERROR\|TODO_PORT\|Sanitizer\|runtime error' /root/native-ui/out.txt | head -10
grep -a -A3 '^ERROR' /root/oracle-ui/out.txt | sed 's/^/oracle: /' | head -8
[ -n "$SHOW_F" ] && cat /root/ui-o-F.txt
true
