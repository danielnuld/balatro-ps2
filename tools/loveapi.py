"""Inventory of the LÖVE API the extracted game uses (phase 0.2 / 0.3). Prints markdown.

usage: python tools/loveapi.py [game_dir] > docs/inventory-api.md
"""
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

LOVE = re.compile(r"\blove\.(\w+(?:\.\w+)?)")
METHOD = re.compile(r":(\w+)\s*\(")
DEFINED = re.compile(r"function\s+[\w.]+:(\w+)")
COMMENT = re.compile(r"--\[\[.*?\]\]|--[^\n]*", re.S)
# LuaJIT-only things PUC Lua 5.1 lacks (design D2)
RISKS = {
    "bit.*": r"\bbit\.\w+",
    "goto": r"\bgoto\s+\w+|::\w+::",
    "ffi": r"\bffi\b",
    "jit.*": r"\bjit\.\w+",
    "require": r"\brequire\s*\(?\s*[\"'][^\"']+[\"']",
    "love.thread": r"love\.thread\.\w+",
    "love.data": r"love\.data\.\w+",
    "string.buffer / table.new": r"string\.buffer|table\.new|table\.clear",
}


def main(root="build/game"):
    love, methods, defined = Counter(), Counter(), set()
    where = defaultdict(set)
    risks = defaultdict(Counter)
    files = sorted(Path(root).rglob("*.lua"))
    lines = 0
    for f in files:
        src = COMMENT.sub("", f.read_text(encoding="utf-8", errors="replace"))
        rel = f.relative_to(root).as_posix()
        lines += src.count("\n")
        for m in LOVE.findall(src):
            love[m] += 1
            where[m].add(rel)
        methods.update(METHOD.findall(src))
        defined.update(DEFINED.findall(src))
        for name, pat in RISKS.items():
            for m in re.findall(pat, src):
                risks[name][m if name != "goto" else "goto/label"] += 1

    print(f"# LÖVE API inventory\n\n{len(files)} Lua files, {lines} lines (comments stripped).\n")
    print(f"## love.* ({len(love)} names)\n\n| Name | Uses | Files |\n|---|---|---|")
    for name, n in sorted(love.items()):
        print(f"| love.{name} | {n} | {', '.join(sorted(where[name]))} |")
    # methods the game does not define itself: LÖVE objects' and Lua strings' / files'
    ext = {k: v for k, v in methods.items() if k not in defined}
    print(f"\n## Methods not defined by the game ({len(ext)})\n\n| Method | Uses |\n|---|---|")
    for name, n in sorted(ext.items()):
        print(f"| :{name} | {n} |")
    print("\n## LuaJIT / runtime risks\n")
    for name in RISKS:
        got = risks.get(name)
        print(f"- **{name}**: " + (", ".join(f"`{k}` x{v}" for k, v in sorted(got.items())) if got else "none"))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    main(*sys.argv[1:2])
