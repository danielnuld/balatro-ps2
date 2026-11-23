#!/bin/bash
# Build the native port for the PS2 in WSL: sources copied to $BALATRO_BUILD/ps2, ELF left there and in
# native/out/balatro-native[-tag].elf.
# usage (in WSL or Linux): XF=-DPLAY bash <native>/build_ps2.sh [tag]
set -e
N=$(cd "$(dirname "$0")" && pwd)
W=${PS2_W:-${BALATRO_BUILD:-$HOME/balatro-native}/ps2}   # PS2_W: a build directory of your own (two builds at once in one clash)
mkdir -p $W/src && rm -rf $W/new $W/softfp
# only what changed is copied, so make rebuilds only that (a changed header rebuilds everything)
mkdir -p $W/new && cp -r $N/include $W/new/
python3 $N/tools/tabfix.py $N/src $W/new/src   # TAB's keys as Vals (include/core.h, TAB_ARRAYS)
diff -rq $W/new/include $W/include > /dev/null 2>&1 || { rm -rf $W/include $W/src/*.o; cp -r $W/new/include $W/; }
[ "$XF" = "$(cat $W/xf.txt 2>/dev/null)" ] || rm -f $W/src/*.o; echo "$XF" > $W/xf.txt
for f in $W/new/src/*.c; do cmp -s $f $W/src/$(basename $f) || cp $f $W/src/; done
for f in $W/src/*.c; do [ -f $W/new/src/$(basename $f) ] || rm -f $f ${f%.c}.o; done
cp $N/Makefile.ps2 $W/Makefile
# the repository (vendor/soft-fp, runtime/src/irx.S): this checkout, or a copy of it on the WSL disk
SRC=${BALATRO_SRC:-$N/..}; [ -f $SRC/runtime/src/irx.S ] || SRC=/root/balatro-src
[ -d $SRC/vendor/soft-fp ] && cp -r $SRC/vendor/soft-fp $W/softfp
cp $SRC/runtime/src/irx.S $W/src/
export PS2DEV=${PS2DEV:-/usr/local/ps2dev}; export PS2SDK=${PS2SDK:-$PS2DEV/ps2sdk} GSKIT=${GSKIT:-$PS2DEV/gsKit}
export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin:$PATH
cp $PS2SDK/iop/irx/audsrv.irx $W/
cd $W && make -s -k -j3 EXTRA="-DTAB_ARRAYS $XF" 2> $W/errors.txt || { grep -E 'error|undefined' $W/errors.txt | head -40; exit 1; }
mkdir -p $N/out && cp $W/balatro.elf $N/out/balatro-native${1:+-$1}.elf
mips64r5900el-ps2-elf-size $W/balatro.elf
