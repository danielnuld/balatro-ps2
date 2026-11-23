#!/bin/bash
# tools/gc_tree.sh <dir> [cflags]: builds a private copy of the tree (<dir>/include, <dir>/src) to <dir>/balatro,
# optimised as build.sh does: for trying a change to a shared header without making everybody rebuild.
D=$1; mkdir -p $D/obj; cd $D
ls src/*.c | xargs -P 4 -I{} bash -c 'f={}; n=$(basename $f .c); [ obj/$n.o -nt $f ] && [ obj/$n.o -nt include/core.h ] || gcc -std=gnu11 -O1 -g -w '"$2"' -Iinclude -c $f -o obj/$n.o || echo "FAILED $f"'
gcc -g -o balatro obj/*.o -lz -lm && ls -la balatro | cut -c20-
