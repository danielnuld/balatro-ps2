"""The font's small sizes, for the native port (src/love_gfx.c, "levels").

usage: python tools/font_levels.py <game_dir> [more game dirs...]     (needs numpy; after the repo's tools/font.py)

The repo's tools/font.py leaves the pixel font at its own 16 px, one bit a texel. Most text on a 640x448 screen is
smaller than that, and a 16 px glyph shrunk by sampling between texels loses strokes. This writes, beside the
font's files, every glyph again at each whole pixel size from FIRST to LAST: the exact share of each screen pixel
its squares cover (the font is squares: no rasteriser needed), 0 to 255, which is the palette's alpha. The port
draws a text from the level of its size on screen, a texel a pixel.

A file pair per picture the port has (love_gs.c's modes; the game's 640x448 is stretched over it, so the pixels of
960x720 are not square: 1.5 across and 1.607 down for one of the game's):
  m6x11plus.ttf.<w>x<h>.tex   a texture of tools/textures.py: 256 wide, the levels one under another, no glyph
                              across a multiple of 256 rows (the port's pages); palette entry i is white, alpha i
  m6x11plus.ttf.<w>x<h>.txt   whole numbers: the levels; then per level: the pixels of 16 texels across and down,
                              its glyphs; then per glyph: code point, x, y, w, h in the texture, x and y from the
                              pen (at a whole pixel) to that box
BOOST: a stroke half a pixel wide is half as bright as the font's own; coverage is raised by that power (< 1).
"""
import re
import struct
import sys
from math import ceil, floor
from pathlib import Path

import numpy as np

FIRST, LAST, W, PAGE = 5, 16, 256, 256
PICTURES = {"640x448": 1.0, "640x720": 720 / 448, "960x720": (720 / 448) / (960 / 640)}  # a level's pixels down for one across
BOOST = 0.5


def overlap(n0, n1, off, size, k):
    """[pixel, texel]: how much of pixel n0+i the texel off+j (k pixels wide) covers"""
    m = np.zeros((n1 - n0, size))
    for i in range(n1 - n0):
        for j in range(size):
            lo, hi = max(n0 + i, (off + j) * k), min(n0 + i + 1, (off + j + 1) * k)
            m[i, j] = max(0.0, hi - lo)
    return m


def levels(root):
    path = Path(root) / "resources/fonts/m6x11plus.ttf"
    w, h = struct.unpack("<HH", Path(str(path) + ".tex").read_bytes()[:4])
    base = np.frombuffer(Path(str(path) + ".tex").read_bytes()[4 + 1024:], np.uint8).reshape(h, w).astype(float)
    glyphs = [tuple(map(int, m.groups())) for m in
              re.finditer(r"\[(\d+)\] = \{(\d+), (\d+), (\d+), (\d+), (-?\d+), (-?\d+),", Path(str(path) + ".lua").read_text())]
    glyphs = [g for g in glyphs if g[3] and g[4]]
    for lines, tall in PICTURES.items():
        rows, text = [], [str(LAST - FIRST + 1)]
        atlas = np.zeros((PAGE * 64, W), np.uint8)
        x = y = row = 0
        for nx in range(FIRST, LAST + 1):
            ny = round(nx * tall)
            kx, ky = nx / 16, ny / 16
            text.append(f"{nx} {ny} {len(glyphs)}")
            x, y, row = 0, y + row + 1 if y else 0, 0
            for cp, gx, gy, gw, gh, ox, oy in glyphs:
                x0, x1, y0, y1 = floor(ox * kx), ceil((ox + gw) * kx), floor(oy * ky), ceil((oy + gh) * ky)
                cover = overlap(y0, y1, oy, gh, ky) @ base[gy:gy + gh, gx:gx + gw] @ overlap(x0, x1, ox, gw, kx).T
                cw, ch = x1 - x0, y1 - y0
                if x + cw > W:
                    x, y, row = 0, y + row + 1, 0
                if y // PAGE != (y + ch) // PAGE:  # (with the clear row under it)
                    x, y, row = 0, (y // PAGE + 1) * PAGE, 0
                atlas[y:y + ch, x:x + cw] = np.rint(np.clip(cover, 0, 1) ** BOOST * 255)
                text.append(f"{cp} {x} {y} {cw} {ch} {x0} {y0}")
                x, row = x + cw + 1, max(row, ch)
        height = y + row + 1
        pal = bytes(b for i in range(256) for b in (255, 255, 255, i))
        Path(f"{path}.{lines}.tex").write_bytes(struct.pack("<HH", W, height) + pal + atlas[:height].tobytes())
        Path(f"{path}.{lines}.txt").write_text("\n".join(text) + "\n", newline="\n")
        print(f"{root}: {lines}: {LAST - FIRST + 1} levels of {len(glyphs)} glyphs, {W}x{height} texels")


if __name__ == "__main__":
    # the smallest check: a pixel-wide stroke at half size covers half a pixel, wherever it falls
    assert abs(overlap(0, 4, 3, 1, 0.5).sum() - 0.5) < 1e-9
    for root in sys.argv[1:]:
        levels(root)
