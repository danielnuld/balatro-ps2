#!/bin/bash
# What gsKit's screen set-up and vertical blank wait really do (the library has no source here): the calls of
# gsKit_init_screen in order, and gsKit_vsync_wait's code.
export PATH=/usr/local/ps2dev/ee/bin:$PATH
mips64r5900el-ps2-elf-objdump -d ${1:-${BALATRO_BUILD:-$HOME/balatro-native}/ps2-main/balatro.elf} > /tmp/gs.dis   # (the library is LTO objects: read the linked ELF)
for f in gsKit_vsync_wait gsKit_sync_flip gsKit_init_screen; do
  echo "== $f"
  awk -v f="<$f>:" '$2 == f {on = 1} on && /^$/ {exit} on' /tmp/gs.dis > /tmp/gs.fn
  wc -l < /tmp/gs.fn
  if [ $f = gsKit_init_screen ]; then grep -E "jal|syscall" /tmp/gs.fn | cut -c1-90; else cut -c1-80 /tmp/gs.fn | head -45; fi
done
