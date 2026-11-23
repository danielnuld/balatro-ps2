#!/bin/bash
# tools/shop_run.sh <frames> <shots|none> <asan|opt> "<steps>|<case name>" ["<pad script>"]: the oracle and the port with the
# shop hook (tools/hook_shop.lua, src/hook_shop.c) on the same STEPS; then the sorted "D" lines of both are compared.
# ORACLE_ONLY=1: only the oracle (to work out the frames of a new case).
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
S="$4"; [ -f "$N/tools/steps_shop_$4.txt" ] && S=$(tr '\n' ' ' < "$N/tools/steps_shop_$4.txt")   # a case name: tools/steps_shop_<name>.txt
export TAG=shop DT=0.0166 TRACE=${TRACE:-10} HOOK=shop STEPS="$S"
mkdir -p /root/oracle-shop && cp $N/tools/hook_shop.lua /root/oracle-shop/shop   # HOOK=shop: a file for the oracle, a name for the port
if [ -n "$ORACLE_ONLY" ]; then
  cd /root/oracle-shop && PAD="$5" ${BALATRO_ORACLE:-/root/balatro-build/host-double/balatro} ${BALATRO_GAME:-/root/balatro-src/build/game} $1 menu > out.txt 2>&1
  grep -a 'STEP\|rror\|traceback' out.txt | head -60; exit
fi
[ -x ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan ] || { echo "no binary: build failed or in progress"; exit 1; }
bash $N/tools/both.sh $1 $2 $3 "$5" | grep -av '^PERF\|^STEP\|^D '
grep -a '^D ' /root/native-shop/out.txt | sort > /root/shop-n.txt
grep -a '^D ' /root/oracle-shop/out.txt | sort > /root/shop-o.txt
echo "D lines: native $(wc -l < /root/shop-n.txt) oracle $(wc -l < /root/shop-o.txt), different: $(diff /root/shop-n.txt /root/shop-o.txt | grep -c '^[<>]')"
diff /root/shop-n.txt /root/shop-o.txt | head -${DIFF_LINES:-40}
grep -a 'ERROR\|TODO_PORT\|Sanitizer\|runtime error' /root/native-shop/out.txt | head -10
grep -a 'rror\|traceback' /root/oracle-shop/out.txt | head -5
