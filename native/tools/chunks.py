"""Cut the game's Lua files into chunks of whole top-level statements: the units the C files were translated from.

usage: python chunks.py <game dir> [max lines]  -> JSON on stdout: [{file, stem, part, parts, start, end, lines}]
A chunk never splits a top-level function; a function longer than the limit is a chunk of its own.
"""
import json
import re
import sys
from pathlib import Path

SKIP = {'conf.lua', 'functions/test_functions.lua', 'engine/http_manager.lua', 'engine/profile.lua'}
TOP = re.compile(r'^(function |local function |[A-Za-z_][A-Za-z0-9_.]*\s*=\s*function|G\.FUNCS\.|G\.UIDEF\.|[A-Za-z_][A-Za-z0-9_.\[\]\'"]*\s*=)')


def main(root, limit=650):
    out = []
    for f in sorted(Path(root).rglob('*.lua')):
        rel = f.relative_to(root).as_posix()
        if rel.startswith('localization/') or rel.startswith('resources/') or rel in SKIP:
            continue
        lines = f.read_text(encoding='utf-8', errors='replace').splitlines()
        starts = [i + 1 for i, line in enumerate(lines) if TOP.match(line)]
        if not starts or starts[0] != 1:
            starts.insert(0, 1)
        # a top-level statement begins at each start; pull the comment lines above it into it
        for n, s in enumerate(starts):
            while s > 1 and lines[s - 2].startswith('--') and (n == 0 or s - 1 > starts[n - 1]):
                s -= 1
            starts[n] = s
        starts = sorted(set(starts))
        bounds = starts + [len(lines) + 1]
        chunks, a = [], 0
        for n in range(1, len(bounds)):
            if bounds[n] - bounds[a] > limit and n - 1 > a:
                chunks.append((bounds[a], bounds[n - 1] - 1))
                a = n - 1
        chunks.append((bounds[a], len(lines)))
        stem = rel[:-4].replace('engine/', '').replace('functions/', '')
        for k, (s, e) in enumerate(chunks):
            out.append({'file': rel, 'stem': stem, 'part': k + 1, 'parts': len(chunks), 'start': s, 'end': e, 'lines': e - s + 1})
    json.dump(out, sys.stdout, indent=0)


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 650)
