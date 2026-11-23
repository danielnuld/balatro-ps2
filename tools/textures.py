"""Convert the game's 1x atlases to what the runtime loads: 8-bit pixels and a 256-colour RGBA palette.

usage: python tools/textures.py [game_dir]     (needs Pillow and numpy; run after tools/extract.py)

Writes <atlas>.png.tex beside every PNG of resources/textures/1x:
  uint16 width, uint16 height (little endian), 256 x RGBA palette, width x height palette indices.
Prints the atlases that did not fit in 256 colours with their worst channel error.

ponytail: one palette per atlas. Jokers (12787 colours), boosters and three more lose colour this way; a palette
per sprite cell (design D5) when the picture on the console is there to compare.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def main(root="build/game"):
    for f in sorted((Path(root) / "resources/textures/1x").rglob("*.png")):
        im = Image.open(f).convert("RGBA")
        a = np.asarray(im, dtype=int)
        colours, index = np.unique(a.reshape(-1, 4), axis=0, return_inverse=True)
        if len(colours) <= 256:  # exact
            pal, pixels = colours.astype(np.uint8).tobytes().ljust(1024, b"\0"), index.astype(np.uint8).tobytes()
        else:
            q = im.quantize(256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
            pal, pixels = bytes(q.getpalette("RGBA")).ljust(1024, b"\0"), q.tobytes()
            err = np.abs(np.asarray(q.convert("RGBA"), dtype=int) - a)
            err[..., :3][a[..., 3] == 0] = 0  # the colour of a transparent pixel does not show
            print(f"{f.name}: {len(colours)} colours, worst channel error {err.max()}, mean {err.mean():.2f}")
        Path(str(f) + ".tex").write_bytes(struct.pack("<HH", *im.size) + pal + pixels)


if __name__ == "__main__":
    main(*sys.argv[1:])
