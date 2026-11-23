/* Ours, not libgcc's: the leading zeros of a 64-bit word, which soft-fp asks for after a subtraction and for
   every integer turned into a double. The EE has no instruction for it and libgcc's table loop was 1.6 % of a
   frame (docs/perf.md). x is never 0 here. */
int __clzdi2(unsigned long long x) {
    int n = 0;
    if (!(x >> 32)) n += 32, x <<= 32;
    if (!(x >> 48)) n += 16, x <<= 16;
    if (!(x >> 56)) n += 8, x <<= 8;
    if (!(x >> 60)) n += 4, x <<= 4;
    if (!(x >> 62)) n += 2, x <<= 2;
    return n + !(x >> 63);
}
