# python tools/ui_pairs.py [prefix n] [prefix o]: for every out/nui-shot-N.ppm with its oui- pair: the difference, and
# out/ui-pair-N.png (native | oracle, the pixels that differ by more than 32 marked red on a third panel).
import glob, os, sys
from PIL import Image, ImageChops
out = os.path.join(os.path.dirname(__file__), '..', 'out')
a_ = sys.argv[1] if len(sys.argv) > 1 else 'nui-'
b_ = sys.argv[2] if len(sys.argv) > 2 else 'oui-'
for f in sorted(glob.glob(os.path.join(out, a_ + 'shot-*.ppm')), key=lambda f: int(f.split('-')[-1][:-4])):
    n = f.split('-')[-1][:-4]
    g = os.path.join(out, b_ + 'shot-' + n + '.ppm')
    if not os.path.exists(g): continue
    a = Image.open(f).convert('RGB'); b = Image.open(g).convert('RGB')
    d = ImageChops.difference(a, b).convert('L'); h = d.histogram()
    tot = sum(h)
    m = d.point(lambda v: 255 if v > 32 else 0)
    print(n, 'mean diff %.2f' % (sum(i * c for i, c in enumerate(h)) / tot), 'pixels >32: %.2f%%' % (100 * sum(h[32:]) / tot), 'bbox', m.getbbox())
    im = Image.new('RGB', (a.width * 3, a.height))
    im.paste(a, (0, 0)); im.paste(b, (a.width, 0))
    red = Image.composite(Image.new('RGB', a.size, (255, 0, 0)), a.point(lambda v: v // 3), m)
    im.paste(red, (a.width * 2, 0))
    im.save(os.path.join(out, 'ui-pair-%s.png' % n))
