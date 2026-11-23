#!/bin/bash
# Builds and runs the tests of src/gc.c on the host:
#   bash <native>/test/gc_test.sh
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/gc_test}
gcc -std=gnu11 -O1 -g -Wall -Iinclude src/gc.c test/gc_test.c -o "$OUT"
gcc -std=gnu11 -O0 -g -Wall -fsanitize=address,undefined -Iinclude src/gc.c test/gc_test.c -o "$OUT-asan"
GC_HEAP=16 GC_STRESS=1000000 "$OUT"                # collections when the arena fills and at gc_frame
GC_HEAP=4 GC_STRESS=1000000 "$OUT"                 # a tight arena
GC_HEAP=16 GC_STRESS=64 CHAIN=20000 ROUNDS=60000 "$OUT"        # a collection every 64 allocations
GC_HEAP=16 GC_STRESS=1 CHAIN=2000 ROUNDS=3000 "$OUT"          # at every allocation
ASAN_OPTIONS=detect_leaks=0 GC_HEAP=16 GC_STRESS=200 CHAIN=20000 ROUNDS=20000 "$OUT-asan"
# the collection in slices: one at every allocation (and at every 3rd), of 1, 7 and 300 work units
GC_HEAP=16 GC_STRESS=1000000 GC_STRESS_SLICE=1 GC_SLICE=1 "$OUT"
GC_HEAP=16 GC_STRESS=1000000 GC_STRESS_SLICE=1 GC_SLICE=7 "$OUT"
GC_HEAP=16 GC_STRESS=1000000 GC_STRESS_SLICE=3 GC_SLICE=300 "$OUT"
GC_HEAP=4 GC_STRESS=1000000 GC_STRESS_SLICE=2 GC_SLICE=40 "$OUT"
ASAN_OPTIONS=detect_leaks=0 GC_HEAP=16 GC_STRESS=90000 GC_STRESS_SLICE=1 GC_SLICE=25 CHAIN=20000 ROUNDS=40000 "$OUT-asan"
