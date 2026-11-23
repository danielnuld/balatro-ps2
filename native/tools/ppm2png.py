# python tools/ppm2png.py: every out/*.ppm to a .png next to it
import glob, os
from PIL import Image
for f in glob.glob(os.path.join(os.path.dirname(__file__), '..', 'out', '*.ppm')):
    Image.open(f).save(f[:-4] + '.png')
