"""Measure the extracted assets (phase 0.4): atlases, sounds, fonts, shaders. Prints markdown.

usage: python tools/assets.py [game_dir] >> docs/inventory.md   (needs Pillow, numpy and ffprobe)
"""
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def main(root="build/game"):
    res = Path(root) / "resources"
    print("## Textures (1x)\n\n| Atlas | Size | Colours (RGBA) | Alpha levels | PSMT8 KB | PSMCT32 KB |\n|---|---|---|---|---|---|")
    t8 = t32 = 0
    for f in sorted((res / "textures" / "1x").rglob("*.png")):
        a = np.asarray(Image.open(f).convert("RGBA"))
        h, w = a.shape[:2]
        colours = len(np.unique(a.reshape(-1, 4), axis=0))
        alphas = len(np.unique(a[..., 3]))
        t8 += w * h + 1024
        t32 += w * h * 4
        print(f"| {f.relative_to(res / 'textures' / '1x').as_posix()} | {w}x{h} | {colours} | {alphas} "
              f"| {(w * h + 1024) // 1024} | {w * h * 4 // 1024} |")
    print(f"\nAll 1x atlases: {t8 // 1024} KB as PSMT8 + CLUT, {t32 // 1024} KB as PSMCT32 (VRAM is 4096 KB).\n")

    print("## Sounds\n\n| File | Seconds | Hz | Ch | ADPCM KB (mono, same Hz) |\n|---|---|---|---|---|")
    tot_s = tot_kb = 0
    for f in sorted((res / "sounds").iterdir()):
        out = subprocess.run(
            ["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries",
             "stream=sample_rate,channels:format=duration", "-of", "csv=p=0", str(f)],
            capture_output=True, text=True).stdout.split()
        hz, ch = out[0].split(",")[:2]
        sec = float(out[1])
        kb = sec * int(hz) / 28 * 16 / 1024  # SPU2 ADPCM: 16 bytes per 28 samples
        tot_s += sec
        tot_kb += kb
        print(f"| {f.name} | {sec:.2f} | {hz} | {ch} | {kb:.0f} |")
    print(f"\nAll sounds: {tot_s:.0f} s, {tot_kb:.0f} KB as mono ADPCM at the source rate (SPU2 RAM is 2048 KB).\n")

    print("## Fonts and shaders\n")
    for d in ("fonts", "shaders"):
        print(f"- {d}: " + ", ".join(f"{f.name} ({f.stat().st_size // 1024} KB)" for f in sorted((res / d).iterdir())))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    main(*sys.argv[1:2])
