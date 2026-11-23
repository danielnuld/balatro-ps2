#!/bin/bash
# tools/gc_variant.sh <file.c> [cflags]: links the optimised build's objects with another copy of one source (same base name
# as the object it replaces, e.g. /tmp/gcd/core.c) as $BALATRO_BUILD/balatro-gcvar: for experiments on the
# collector without touching the shared tree.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
b=$(basename $1 .c)
gcc -std=gnu11 -O1 -g $2 -I$N/include -c $1 -o /tmp/gcd/var_$b.o &&
  gcc -g -o ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-gcvar $(ls ${BALATRO_BUILD:-$HOME/balatro-native}/obj/*.o | grep -v "/$b.o") /tmp/gcd/var_$b.o -lz -lm
