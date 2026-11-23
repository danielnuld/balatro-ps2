#!/bin/bash
# Builds and runs the tests of src/core.c on the host:
#   bash <native>/test/core_test.sh
set -e
cd "$(dirname "$0")/.."
# The expected results come from real Lua 5.1; the generated header is kept, so the interpreter is optional.
LUA=${LUA:-/root/lua51/lua}
if [ -x "$LUA" ]; then "$LUA" test/sort_gen.lua > test/sort_cases.h; fi
# The binary goes outside /mnt/c: faster, and it keeps build products out of the source tree
OUT=${OUT:-/tmp/core_test}
gcc -std=gnu99 -Wall -Wextra -Wno-unused-parameter -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Iinclude src/core.c src/gc.c test/core_test.c -lm -o "$OUT"
# Tables and strings are never freed by design (docs/conventions.md, Memory), so the leak report would only be noise
ASAN_OPTIONS=detect_leaks=0 "$OUT"
