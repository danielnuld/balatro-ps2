#!/bin/bash
# Build the runtime in WSL. Run it through tools/wsl.sh (a copy of the sources on the WSL disk).
# usage: tools/wsl.sh bash runtime/build.sh host|ps2 [double|float] [tag]
#   host -> /root/balatro-build/host-<variant>/balatro (run by runtime/test/run.sh)
#   ps2  -> build/balatro[-float].elf, which reads the game from host:game (game/ beside it, in PCSX2)
#   float: lua_Number is float instead of double (-DLUA_FLOAT)
#   tag: the ELF is build/balatro-<tag>.elf, to keep builds side by side
#   XF in the environment: extra defines, e.g. XF="-DBENCH_RUN -DFRAMES=3600" (main.c)
set -e
S=$(cd "$(dirname "$0")/.." && pwd)
T=${1:-host}
V=${2:-double}
W=/root/balatro-build/$T-$V
rm -rf $W && mkdir -p $W
cp -r $S/runtime/src $S/runtime/Makefile $W/
cp -r $S/vendor/lua-5.1.5/src $W/lua
cp -r $S/vendor/soft-fp $W/softfp
(cd $W/src && xxd -i boot.lua > boot_lua.h)
cd $W
F=$XF; [ "$V" = float ] && F="$F -DLUA_FLOAT"
if [ "$T" = host ]; then
  gcc -O2 -Wall $F -I$W/lua -c $W/src/*.c
  gcc -O2 -w $F -o $W/balatro *.o $W/lua/*.c -lz -lm
else
  export PS2DEV=/usr/local/ps2dev PS2SDK=/usr/local/ps2dev/ps2sdk GSKIT=/usr/local/ps2dev/gsKit
  export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PS2SDK/bin:$PATH
  cp $PS2SDK/iop/irx/audsrv.irx $W/
  make -s -C $W EXTRA="$F"
  mkdir -p $S/build
  E=$S/build/balatro; [ "$V" = float ] && E=$E-float; [ -n "$3" ] && E=$E-$3
  cp $W/balatro.elf $E.elf
  mips64r5900el-ps2-elf-size $E.elf
fi
