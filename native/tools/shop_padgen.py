# python tools/shop_padgen.py <name> "<actions>" [round]: writes tools/steps_shop_<name>.txt and tools/pad_shop_<name>.txt for
# tools/shop_run.sh. An action is a pad button, a hook command prefixed with '!' (see tools/hook_shop.lua) or 'wN'
# (wait N frames). Every action takes 60 frames and is followed by the hook's 'focus' (what the cursor is on).
import sys, os
name, acts = sys.argv[1], sys.argv[2].split()
d = os.path.dirname(__file__)
steps, pad, t = ['1250:start:PADSHOP 1600:blind 2100:win'], ['3100:a 3200:a'], 3600   # to the first shop
if len(sys.argv) > 3: steps, pad, t = ['1250:start:PADRND 1600:blind'], [], 2000   # any third argument: to the first hand
for a in acts:
    if a[0] == 'w': t += int(a[1:]); continue
    if a[0] == '!': steps.append('%d:%s' % (t, a[1:]))
    else: pad.append('%d:%s' % (t, a))
    steps.append('%d:focus' % (t + 50))
    t += 60
open(os.path.join(d, 'steps_shop_%s.txt' % name), 'w', newline='\n').write(' '.join(steps) + '\n')
open(os.path.join(d, 'pad_shop_%s.txt' % name), 'w', newline='\n').write(' '.join(pad) + '\n')
print(t + 60)
