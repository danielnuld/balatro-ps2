#!/bin/bash
# Build the native port on the host (Linux, in WSL): every src/*.c to an object in $BALATRO_BUILD/obj, in
# parallel, then link. Errors per file are kept in $BALATRO_BUILD/err/<file>.txt (empty files removed).
# usage (in WSL or Linux): bash <native>/build.sh [link]
cd "$(dirname "$0")"
W=${BALATRO_BUILD:-$HOME/balatro-native}
# build.sh link asan: -O0 with the address and undefined sanitizers, objects in obj-asan, binary balatro-asan
OBJ=obj; BIN=balatro; OPT="-O1"; LD=""
if [ "$2" = asan ]; then OBJ=obj-asan; BIN=balatro-asan; OPT="-O0 -fsanitize=address,undefined -fno-omit-frame-pointer"; LD="-fsanitize=address,undefined"; fi
mkdir -p $W/$OBJ $W/err
exec 9> $W/build.lock; flock 9   # one build at a time: concurrent builds share this directory
rm -f $W/err/*
FLAGS="-std=gnu11 $OPT -g -Wall -Wno-unused -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -Iinclude"
# incremental: a file is compiled when its object is older than it or than the newest header
H=$(ls -t include/*.h include/needs/*.h | head -1)
for f in src/*.c; do o=$W/$OBJ/$(basename $f .c).o; [ -f $o ] && [ $o -nt $f ] && [ $o -nt $H ] || echo $f; done | xargs -r -P 4 -I{} bash -c 'f={}; n=$(basename $f .c); gcc '"$FLAGS"' -c $f -o '$W/$OBJ'/$n.o 2> '$W'/err/$n.txt || rm -f '$W/$OBJ'/$n.o; [ -s '$W'/err/$n.txt ] || rm -f '$W'/err/$n.txt'
echo "objects: $(ls $W/$OBJ/*.o 2>/dev/null | wc -l) of $(ls src/*.c | wc -l); files with errors: $(grep -l ' error' $W/err/*.txt 2>/dev/null | wc -l)"
if [ "$1" = link ]; then
  gcc -g $LD -o $W/$BIN $W/$OBJ/*.o -lz -lm 2> $W/link.txt; echo "link: $(grep -c 'undefined reference' $W/link.txt) undefined, $(grep -c 'multiple definition' $W/link.txt) multiple"
fi
