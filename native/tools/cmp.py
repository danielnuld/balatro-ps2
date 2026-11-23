# python tools/cmp.py <native prefix> <oracle prefix>: compares out/<prefix>shot-N.ppm pairs, writes PNGs of both
import glob, os, sys
from PIL import Image, ImageChops
out = os.path.join(os.path.dirname(__file__), '..', 'out')
a_, b_ = sys.argv[1], sys.argv[2]
for f in sorted(glob.glob(os.path.join(out, a_ + 'shot-*.ppm')), key=lambda f: int(f.split('-')[-1][:-4])):
    n = f.split('-')[-1][:-4]
    g = os.path.join(out, b_ + 'shot-' + n + '.ppm')
    if not os.path.exists(g): continue
    a = Image.open(f).convert('RGB'); b = Image.open(g).convert('RGB')
    d = ImageChops.difference(a, b).convert('L'); h = d.histogram()
    tot = sum(h)
    print(n, 'mean diff %.2f' % (sum(i * c for i, c in enumerate(h)) / tot), 'pixels >32: %.2f%%' % (100 * sum(h[32:]) / tot),
          'bbox', d.point(lambda v: 255 if v > 32 else 0).getbbox())
    a.save(f[:-4] + '.png'); b.save(g[:-4] + '.png')
