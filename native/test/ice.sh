#!/bin/bash
# Which construct makes the PS2 compiler die ("maximum number of generated reload insns")? Small probes.
cd ${BALATRO_BUILD:-$HOME/balatro-native}/ps2
export PS2DEV=/usr/local/ps2dev PS2SDK=/usr/local/ps2dev/ps2sdk GSKIT=/usr/local/ps2dev/gsKit PATH=/usr/local/ps2dev/ee/bin:$PATH
CC="mips64r5900el-ps2-elf-gcc -D_EE -G0 -std=gnu11 -O2 -DTAB_ARRAYS -Wno-unused -Iinclude -I$PS2SDK/ee/include -I$PS2SDK/common/include -I$GSKIT/include -I$PS2SDK/ports/include -c -o /tmp/ice.o"
probe() {
  printf '#include "all.h"\n%s\n' "$2" > /tmp/ice.c
  if $CC /tmp/ice.c 2>&1 | grep -q 'internal compiler error'; then echo "ICE  $1"; else echo "ok   $1"; fi
}
probe "v_or" 'static inline Val v_or(Val a, Val b) { return truthy(a) ? a : b; }
Table *f(Table *a){ return TAB(S("a"), v_or(tget(a, "x"), N(7)), S("b"), v_or(tget(a, "y"), S("z"))); }'
probe "v_or nested" 'static inline Val v_or(Val a, Val b) { return truthy(a) ? a : b; }
Table *f(Table *a){ return TAB(S("n"), N(1), S("config"), T(TAB(S("a"), S("cm"), S("p"), v_or(tget(a, "x"), N(0.2)), S("m"), v_or(tget(a, "y"), N(7))))); }'
probe "float*5" 'Table *f(Table *a){ return TAB(S("a"), N(G->ROOM->T->w * 5), S("b"), N(G->ROOM->T->h * 5), S("c"), N(0.1)); }'
probe "truthy assign" 'float *new_colour(float, float, float, float); Table *f(Table *a){ Val c = tget(a, "c"); if (!truthy(c)) c = P(new_colour(G->C.GREY[0], G->C.GREY[1], G->C.GREY[2], 0.7f)); return TAB(S("a"), c); }'
probe "event" 'static bool ev(void *c) { return true; } void f(Table *a){ E_add(G->E_MANAGER, Event_new((EventCfg){.not_blocking = true, .not_blockable = true, .timer = "REAL", .func = ev, .ctx = a}), NULL, false); }'
# the real functions: each failing file cut at its first failing function is too slow by hand; list the lines
for f in UI_definitions_11 UI_definitions_2; do
  $CC src/$f.c 2>&1 | grep -B1 'internal compiler error' | head -4
done
