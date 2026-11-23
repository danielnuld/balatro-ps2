/* Ours, not libgcc's: a + b and a / b for the everyday case (normal numbers or zeros in, a normal number out),
   ahead of the generic code of adddf3.c, subdf3.c and divdf3.c, which were 8 % of a frame on the EE
   (docs/perf.md). Same rounding (to nearest, ties to even); runtime/test/softfp.sh compares them with the host's
   doubles. They return 0 for what they leave to the generic code. */
static inline int fast_add(unsigned long long x, unsigned long long y, unsigned long long *out) {
    const unsigned long long SIGN = 0x8000000000000000ULL, FRAC = 0xfffffffffffffULL, ONE = 0x10000000000000ULL;
    unsigned long long mx, my, m;
    unsigned int ex, ey, d, r;
    int e;
    if ((x & ~SIGN) < (y & ~SIGN)) m = x, x = y, y = m; /* |x| >= |y| */
    ex = (x >> 52) & 0x7ff, ey = (y >> 52) & 0x7ff;
    if (!(x << 1)) return *out = x & y, 1; /* two zeros: -0 only from two -0 */
    if (ex - 1 >= 0x7fe) return 0;
    if (!(y << 1)) return *out = x, 1; /* a normal number and a zero */
    if (ey == 0) return 0;
    e = ex, d = ex - ey;
    mx = ((x & FRAC) | ONE) << 3; /* three bits below the 53: guard, round, sticky */
    my = ((y & FRAC) | ONE) << 3;
    if (d) my = d > 55 ? 1 : (my >> d) | ((my & ((1ULL << d) - 1)) != 0);
    if (!((x ^ y) & SIGN)) {
        m = mx + my;
        if (m >> 56) m = (m >> 1) | (m & 1), e++;
    } else {
        m = mx - my;
        if (!m) return *out = 0, 1; /* x - x is +0 */
        if (!(m >> 55)) { /* cancellation; more than one bit of it only when nothing was shifted out */
            int n = __builtin_clzll(m) - 8;
            m <<= n, e -= n;
        }
    }
    if ((unsigned int)(e - 1) >= 0x7fd) return 0;
    r = m & 7, m >>= 3;
    m += r > 4 || (r == 4 && (m & 1));
    *out = (x & SIGN) | (((unsigned long long)e << 52) + (m - ONE)); /* a carry out of the 53 bits lands in the exponent */
    return 1;
}

/* The EE divides neither 64-bit integers nor doubles, and the generic code went through libgcc's 64-bit division
   twice. Here: long division, 18 bits of quotient at a time, each digit guessed with the FPU's single-precision
   division and then corrected, so the result does not depend on how exact that division is. */
static inline int fast_div(unsigned long long x, unsigned long long y, unsigned long long *out) {
    const unsigned long long SIGN = 0x8000000000000000ULL, FRAC = 0xfffffffffffffULL, ONE = 0x10000000000000ULL;
    unsigned int ex = (x >> 52) & 0x7ff, ey = (y >> 52) & 0x7ff, myl, myh, r;
    unsigned long long mx, my, q = 1, m;
    long long rem;
    float df;
    int e, i;
    if (ey - 1 >= 0x7fe) return 0;
    if (!(x << 1)) return *out = (x ^ y) & SIGN, 1; /* a zero by a normal number */
    if (ex - 1 >= 0x7fe) return 0;
    mx = (x & FRAC) | ONE, my = (y & FRAC) | ONE;
    e = (int)ex - (int)ey + 1023;
    if (mx < my) mx <<= 1, e--; /* my <= mx < 2 my: the quotient's first bit is 1 */
    if ((unsigned int)(e - 1) >= 0x7fd) return 0;
    rem = mx - my;
    df = (float)(int)(my >> 22);
    myl = my, myh = my >> 32;
    for (i = 0; i < 3; i++) {
        unsigned int d = (int)((float)(int)(rem >> 22) / df * 262144.0f);
        /* rem * 2^18 - d * my: it fits in 64 bits, so its terms may wrap */
        rem = (long long)(((unsigned long long)rem << 18) - ((unsigned long long)d * myl + ((unsigned long long)(d * myh) << 32)));
        while (rem < 0) d--, rem += my;
        while (rem >= (long long)my) d++, rem -= my;
        q = (q << 18) + d;
    }
    r = ((unsigned int)q & 3) << 1 | (rem != 0); /* q: 53 bits and two more; the remainder is the sticky bit */
    m = q >> 2;
    m += r > 4 || (r == 4 && (m & 1));
    *out = ((x ^ y) & SIGN) | (((unsigned long long)e << 52) + (m - ONE));
    return 1;
}

/* balatro-ps2: compares on the bits, ahead of the generic code (a frame of a hand makes thousands). 3 if either is
   a NaN; else -1, 0 or 1 as a is less than, equal to or greater than b. */
static inline int fast_cmp(unsigned long long x, unsigned long long y) {
    if ((x << 1) > 0xffe0000000000000ULL || (y << 1) > 0xffe0000000000000ULL) return 3;
    if (!((x | y) << 1)) return 0; /* two zeros of any sign */
    long long sx = x >> 63 ? -(long long)(x & 0x7fffffffffffffffULL) : (long long)x;
    long long sy = y >> 63 ? -(long long)(y & 0x7fffffffffffffffULL) : (long long)y;
    return sx < sy ? -1 : sx > sy;
}
