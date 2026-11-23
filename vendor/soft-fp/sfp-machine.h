/* Ours, not libgcc's: the machine description soft-fp wants, for the EE (and any 64-bit host, for the test).
   After libgcc/config/mips/sfp-machine.h, its 64-bit word branch, without the FPU's control register: round to
   nearest, no exception flags. The rest of this directory is libgcc/soft-fp and include/longlong.h of GCC 15.2.0;
   runtime/Makefile says why it is here. Ours too: fast.h and clzdi2.c, and the blocks marked balatro-ps2 at the
   top of adddf3.c, subdf3.c, muldf3.c and divdf3.c, which take the everyday cases before libgcc's generic code. */
#define _FP_W_TYPE_SIZE 64
#define _FP_W_TYPE unsigned long long
#define _FP_WS_TYPE signed long long
#define _FP_I_TYPE long long

typedef int TItype __attribute__((mode(TI)));
typedef unsigned int UTItype __attribute__((mode(TI)));
#define TI_BITS (__CHAR_BIT__ * (int)sizeof(TItype))

#define _FP_MUL_MEAT_S(R, X, Y) _FP_MUL_MEAT_1_wide(_FP_WFRACBITS_S, R, X, Y, umul_ppmm)
#define _FP_MUL_MEAT_D(R, X, Y) _FP_MUL_MEAT_1_wide(_FP_WFRACBITS_D, R, X, Y, umul_ppmm)
#define _FP_DIV_MEAT_S(R, X, Y) _FP_DIV_MEAT_1_udiv_norm(S, R, X, Y)
#define _FP_DIV_MEAT_D(R, X, Y) _FP_DIV_MEAT_1_udiv_norm(D, R, X, Y)

#define _FP_NANFRAC_S (_FP_QNANBIT_S - 1)
#define _FP_NANFRAC_D (_FP_QNANBIT_D - 1)
#define _FP_NANSIGN_S 0
#define _FP_NANSIGN_D 0
#define _FP_KEEPNANFRACP 1
#define _FP_QNANNEGATEDP 1
#define _FP_CHOOSENAN(fs, wc, R, X, Y, OP)                                                \
    do {                                                                                  \
        if ((_FP_FRAC_HIGH_RAW_##fs(X) | _FP_FRAC_HIGH_RAW_##fs(Y)) & _FP_QNANBIT_##fs) { \
            R##_s = _FP_NANSIGN_##fs;                                                     \
            _FP_FRAC_SET_##wc(R, _FP_NANFRAC_##fs);                                       \
        } else {                                                                          \
            R##_s = X##_s;                                                                \
            _FP_FRAC_COPY_##wc(R, X);                                                     \
        }                                                                                 \
        R##_c = FP_CLS_NAN;                                                               \
    } while (0)

typedef int __gcc_CMPtype __attribute__((mode(__libgcc_cmp_return__)));
#define CMPtype __gcc_CMPtype

#define FP_EX_INVALID (1 << 4)
#define FP_EX_DIVZERO (1 << 3)
#define FP_EX_OVERFLOW (1 << 2)
#define FP_EX_UNDERFLOW (1 << 1)
#define FP_EX_INEXACT (1 << 0)
#define _FP_TININESS_AFTER_ROUNDING 1

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#define strong_alias(name, aliasname) _strong_alias(name, aliasname)
#define _strong_alias(name, aliasname) extern __typeof(name) aliasname __attribute__((alias(#name)));
