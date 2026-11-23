# python tools/sheet.py <prefix> [frames...]: out/<prefix>shot-N.ppm (all, or those frames) as contact sheets
# out/sheet-<k>.png of 6 pictures (3 x 2), in frame order; prints which frames each holds.
import glob, os, sys
from PIL import Image
out = os.path.join(os.path.dirname(__file__), '..', 'out')
fs = sorted(glob.glob(os.path.join(out, sys.argv[1] + 'shot-*.ppm')), key=lambda f: int(f.split('-')[-1][:-4]))
if len(sys.argv) > 2: fs = [f for f in fs if f.split('-')[-1][:-4] in sys.argv[2:]]
for k in range(0, len(fs), 6):
    part = fs[k:k + 6]
    im = Image.new('RGB', (640 * 3, 448 * 2))
    for i, f in enumerate(part): im.paste(Image.open(f), ((i % 3) * 640, (i // 3) * 448))
    im.save(os.path.join(out, 'sheet-%d.png' % (k // 6)))
    print('sheet', k // 6, [f.split('-')[-1][:-4] for f in part])
