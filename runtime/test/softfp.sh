#!/bin/bash
# Host test of the soft-fp double routines the EE build links (vendor/soft-fp), against the host's doubles.
# usage: tools/wsl.sh bash runtime/test/softfp.sh
set -e
S=$(cd "$(dirname "$0")/../.." && pwd)
W=/root/balatro-build/softfp
mkdir -p $W && cd $W
R="-D__adddf3=sf_add -D__subdf3=sf_sub -D__muldf3=sf_mul -D__divdf3=sf_div -D__negdf2=sf_neg -D__eqdf2=sf_eq -D__nedf2=sf_ne"
R="$R -D__ltdf2=sf_lt -D__ledf2=sf_le -D__gtdf2=sf_gt -D__gedf2=sf_ge -D__unorddf2=sf_unord -D__fixdfsi=sf_fixsi"
R="$R -D__fixunsdfsi=sf_fixunssi -D__fixdfdi=sf_fixdi -D__fixunsdfdi=sf_fixunsdi -D__floatsidf=sf_floatsi"
R="$R -D__floatunsidf=sf_floatunsi -D__floatdidf=sf_floatdi -D__floatundidf=sf_floatundi -D__extendsfdf2=sf_extend"
R="$R -D__truncdfsf2=sf_trunc -D__clzdi2=sf_clz"
gcc -O2 -w $R -I$S/vendor/soft-fp -c $S/vendor/soft-fp/*.c
gcc -O2 -Wall -ffp-contract=off -o softfp $S/runtime/test/softfp.c *.o
./softfp
