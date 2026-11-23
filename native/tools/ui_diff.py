# tools/ui_diff.py <native lines> <oracle lines> [max]: diff of two hook outputs where numbers may differ by 0.011 or 1e-5 relative
# (the port's float geometry against the oracle's doubles, printed with two decimals).
import sys, re, difflib

NUM = re.compile(r'-?\d+\.\d+(?:e[-+]?\d+)?')

def same(a, b):
    if a == b: return True
    if NUM.sub('#', a) != NUM.sub('#', b): return False
    return all(abs(float(x) - float(y)) <= max(0.0101, 1e-5 * abs(float(y))) for x, y in zip(NUM.findall(a), NUM.findall(b)))

a = open(sys.argv[1], errors='replace').read().splitlines()
b = open(sys.argv[2], errors='replace').read().splitlines()
mx = int(sys.argv[3]) if len(sys.argv) > 3 else 30
out = []
if len(a) == len(b):
    for x, y in zip(a, b):
        if not same(x, y): out += ['< ' + x, '> ' + y]
else:
    ka, kb = [NUM.sub('#', x) for x in a], [NUM.sub('#', x) for x in b]
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, ka, kb, autojunk=False).get_opcodes():
        if tag == 'equal':
            for x, y in zip(a[i1:i2], b[j1:j2]):
                if not same(x, y): out += ['< ' + x, '> ' + y]
        else:
            out += ['< ' + x for x in a[i1:i2]] + ['> ' + y for y in b[j1:j2]]
print('native %d oracle %d, different: %d' % (len(a), len(b), len(out)))
print('\n'.join(out[:mx]))
