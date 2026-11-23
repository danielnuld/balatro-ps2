#!/bin/bash
# The PS2 build's sources (keys wrapped by tools/tabfix.py, -DTAB_ARRAYS) compiled for the host: is a difference
# between the PS2 build and the host one in that rewrite? Needs build_ps2.sh run once (it leaves ps2/new/src).
# usage: bash tools/host_tabfix.sh [extra gcc flags, e.g. -m32]   -> $BALATRO_BUILD/balatro-tabfix
W=${BALATRO_BUILD:-$HOME/balatro-native}
mkdir -p $W/obj-tabfix && cd $W/ps2/new
ls src/*.c | xargs -P 4 -I{} bash -c 'f={}; gcc -std=gnu11 -O1 -g -w -DTAB_ARRAYS '"$*"' -Iinclude -c $f -o '$W'/obj-tabfix/$(basename $f .c).o || echo "FAILED $f"'
gcc -g "$@" -o $W/balatro-tabfix $W/obj-tabfix/*.o -lz -lm && echo linked
