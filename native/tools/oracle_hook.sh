#!/bin/bash
# Give the oracle (the Lua runtime's host build, from the WSL copy of the repo) a test hook: HOOK=<file.lua> in the
# environment; the file is a chunk called once per frame with the frame number, after the game's update and draw.
# Idempotent. tools/wsl.sh of the repo wipes the copy: run this again after it.
set -e
B=${BALATRO_SRC:-/root/balatro-src}/runtime/src/boot.lua
grep -q 'os.getenv("HOOK")' $B || python3 - $B <<'EOF'
import sys
p = sys.argv[1]
s = open(p, encoding='latin-1', newline='').read()
a = 'local peak, allocated = 0, sys.allocated()\n'
b = '  if TRACE and i % TRACE == 0 then trace(i) end\n'
assert s.count(a) == 1 and s.count(b) == 1
s = s.replace(a, 'local HOOK = os.getenv("HOOK") and assert(loadfile(os.getenv("HOOK")))\n' + a)
s = s.replace(b, '  if HOOK then HOOK(i) end\n' + b)
open(p, 'w', encoding='latin-1', newline='').write(s)
EOF
bash ${BALATRO_SRC:-/root/balatro-src}/runtime/build.sh host
