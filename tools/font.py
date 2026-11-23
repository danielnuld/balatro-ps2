"""Rasterise the game's pixel font to what the runtime loads (design D8).

usage: python tools/font.py [game_dir]     (needs Pillow; run after tools/extract.py)

m6x11plus.ttf is a pixel font whose grid is 1 px at size 16. Writes beside it:
  m6x11plus.ttf.tex   every glyph at size 16, as a texture of tools/textures.py (index 0 clear, 1 white)
  m6x11plus.ttf.lua   a Lua table: the size the game asks for (200) and, per code point,
                      {x, y, w, h in the texture, x and y offset of that box from the pen at size 16,
                       advance at size 200 as FreeType rounds it, which is what LÖVE measures with}
The font has no kerning table. The other fonts (Noto, for CJK, Cyrillic...) are not converted.
"""
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

NATIVE, ASKED = 16, 200  # game.lua:969: render_scale = TILESIZE * 10


def main(root="build/game"):
    path = Path(root) / "resources/fonts/m6x11plus.ttf"
    small, big = ImageFont.truetype(str(path), NATIVE), ImageFont.truetype(str(path), ASKED)
    from fontTools.ttLib import TTFont
    codes = sorted(TTFont(str(path)).getBestCmap())
    W = H = 256
    atlas = Image.new("1", (W, H), 0)
    x = y = row = 0
    lines = [f"return {{size = {ASKED}, native = {NATIVE}, height = {big.font.height}, glyphs = {{"]
    for cp in codes:
        c = chr(cp)
        box = small.getbbox(c)
        w, h = (box[2] - box[0], box[3] - box[1]) if box else (0, 0)
        if x + w > W:
            x, y, row = 0, y + row + 1, 0
        assert y + h <= H, "atlas full"
        if w and h:
            ImageDraw.Draw(atlas).text((x - box[0], y - box[1]), c, font=small, fill=1)
        lines.append(f"[{cp}] = {{{x}, {y}, {w}, {h}, {box[0] if box else 0}, {box[1] if box else 0}, {big.getlength(c):g}}},")
        x, row = x + w + 1, max(row, h)
    lines.append("}}")
    Path(str(path) + ".lua").write_text("\n".join(lines) + "\n", newline="\n")
    pal = bytes([255, 255, 255, 0, 255, 255, 255, 255]).ljust(1024, bytes(1))
    Path(str(path) + ".tex").write_bytes(struct.pack("<HH", W, H) + pal + atlas.convert("L").point(lambda v: 1 if v else 0).tobytes())
    print(f"{len(codes)} glyphs, {y + row} of {H} rows used")


if __name__ == "__main__":
    main(*sys.argv[1:])
