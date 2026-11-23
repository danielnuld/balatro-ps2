"""Convert the game's sounds to what the runtime's mixer plays (runtime/src/snd.c): SPU ADPCM, mono, 22050 Hz.

usage: python tools/sounds.py [game_dir]     (needs numpy and ffmpeg on the PATH; run after tools/extract.py)

Writes <sound>.ogg.snd beside every OGG of resources/sounds:
  uint32 rate (22050), uint32 blocks (little endian), then the blocks: 16 bytes for 28 samples
  (shift | filter << 4, flags, 28 nibbles low first; psx-spx "SPU ADPCM Samples").
The encoder: per block the filter with the smallest peak residual and the
smallest shift that holds it, encoded against the decoder's own history. A file that is up to date is skipped.
"""
import struct
import subprocess
import sys
from multiprocessing import Pool
from pathlib import Path

import numpy as np

RATE = 22050
FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


def spu_adpcm(s):
    """int samples (a multiple of 28 of them) -> SPU ADPCM blocks"""
    out = bytearray()
    h1 = h2 = 0
    for b in range(len(s) // 28):
        x = s[28 * b:28 * b + 28]
        xp = np.concatenate([[h2, h1], x])
        peak, fi = min((np.abs(x - (xp[1:29] * f0 + xp[0:28] * f1) / 64).max(), i) for i, (f0, f1) in enumerate(FILTERS))
        shift = 12
        while shift > 0 and peak > 7 << (12 - shift):
            shift -= 1
        f0, f1 = FILTERS[fi]
        nib = []
        for v in x.tolist():
            pred = (h1 * f0 + h2 * f1 + 32) >> 6
            q = round((v - pred) * (1 << shift) / 4096.0)
            q = -8 if q < -8 else 7 if q > 7 else q
            d = ((q << 12) >> shift) + pred
            d = -32768 if d < -32768 else 32767 if d > 32767 else d
            h2, h1 = h1, d
            nib.append(q & 0xF)
        out += bytes([shift | fi << 4, 0]) + bytes(nib[2 * i] | nib[2 * i + 1] << 4 for i in range(14))
    return bytes(out)


def convert(f):
    out = Path(str(f) + ".snd")
    if out.exists() and out.stat().st_mtime >= f.stat().st_mtime:
        return None
    pcm = subprocess.run(["ffmpeg", "-v", "error", "-i", str(f), "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"],
                         capture_output=True, check=True).stdout
    s = np.frombuffer(pcm, np.int16).astype(np.int64)
    s = np.concatenate([s, np.zeros(-len(s) % 28, np.int64)])
    out.write_bytes(struct.pack("<II", RATE, len(s) // 28) + spu_adpcm(s))
    return f"{f.name}: {len(s) / RATE:.1f} s"


def main(root="build/game"):
    files = sorted((Path(root) / "resources/sounds").glob("*.ogg"))
    with Pool() as pool:
        for line in pool.imap_unordered(convert, files):
            if line:
                print(line, flush=True)
    total = sum(Path(str(f) + ".snd").stat().st_size for f in files)
    print(f"{len(files)} sounds, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main(*sys.argv[1:])
