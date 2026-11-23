// Host test of vendor/soft-fp as the EE build uses it: every routine, renamed sf_*, against the host's own
// doubles on special values and a few million random bit patterns. Run by runtime/test/softfp.sh.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

double sf_add(double, double), sf_sub(double, double), sf_mul(double, double), sf_div(double, double), sf_neg(double);
double sf_floatsi(int), sf_floatunsi(unsigned), sf_floatdi(long long), sf_floatundi(unsigned long long), sf_extend(float);
float sf_trunc(double);
int sf_fixsi(double), sf_clz(unsigned long long);
unsigned sf_fixunssi(double);
long long sf_fixdi(double);
long sf_eq(double, double), sf_ne(double, double), sf_lt(double, double), sf_le(double, double), sf_gt(double, double),
    sf_ge(double, double), sf_unord(double, double);

static uint64_t state = 88172645463325252ull;
static uint64_t rnd(void) { state ^= state << 13, state ^= state >> 7, state ^= state << 17; return state; }
static double from(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static uint64_t bits(double d) { uint64_t b; memcpy(&b, &d, 8); return b; }
static int same(double a, double b) { return bits(a) == bits(b) || (a != a && b != b); } // any NaN is a NaN
static int failures;
#define CHECK(what, cond) \
    if (!(cond) && failures++ < 20) printf("FAIL %s  a=%016llx b=%016llx\n", what, (unsigned long long)bits(a), (unsigned long long)bits(b))

static void pair(double a, double b) {
    CHECK("add", same(sf_add(a, b), a + b));
    CHECK("sub", same(sf_sub(a, b), a - b));
    CHECK("mul", same(sf_mul(a, b), a * b));
    CHECK("div", same(sf_div(a, b), a / b));
    CHECK("neg", same(sf_neg(a), -a));
    CHECK("eq", (sf_eq(a, b) == 0) == (a == b));
    CHECK("ne", (sf_ne(a, b) != 0) == (a != b));
    CHECK("lt", (sf_lt(a, b) < 0) == (a < b));
    CHECK("le", (sf_le(a, b) <= 0) == (a <= b));
    CHECK("gt", (sf_gt(a, b) > 0) == (a > b));
    CHECK("ge", (sf_ge(a, b) >= 0) == (a >= b));
    CHECK("unord", (sf_unord(a, b) != 0) == (a != a || b != b));
    CHECK("trunc", same(sf_trunc(a), (float)a));
    if (a > -2147483648.0 && a < 2147483647.0) CHECK("fixsi", sf_fixsi(a) == (int)a);
    if (a >= 0 && a < 4294967295.0) CHECK("fixunssi", sf_fixunssi(a) == (unsigned)a);
    if (a > -9e18 && a < 9e18) CHECK("fixdi", sf_fixdi(a) == (long long)a);
    uint64_t i = bits(b);
    CHECK("floatsi", same(sf_floatsi((int)i), (double)(int)i));
    CHECK("floatunsi", same(sf_floatunsi((unsigned)i), (double)(unsigned)i));
    CHECK("floatdi", same(sf_floatdi((long long)i), (double)(long long)i));
    CHECK("floatundi", same(sf_floatundi(i), (double)i));
    if (i) CHECK("clz", sf_clz(i) == __builtin_clzll(i));
    if (i >> 40) CHECK("clz", sf_clz(i >> 40) == __builtin_clzll(i >> 40));
    float f = (float)b;
    if (f == f) CHECK("extend", same(sf_extend(f), (double)f));
}

int main(void) {
    static const double special[] = {0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 3.0, 1e308, -1e308, 4.9e-324, 2.2250738585072014e-308,
                                     1.0 / 0.0, -1.0 / 0.0, 0.0 / 0.0, 16777216.0, 9007199254740993.0, 0.1, 1e-310, 123456.789};
    int n = sizeof special / sizeof *special;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) pair(special[i], special[j]);
    for (int i = 0; i < 3000000; i++) {
        uint64_t x = rnd(), y = rnd();
        pair(from(x), from(y)); // anything
        pair(from((x & 0x800fffffffffffffull) | (uint64_t)(1023 + (int)(y % 80) - 40) << 52), // everyday magnitudes
             from((y & 0x800fffffffffffffull) | (uint64_t)(1023 + (int)(x % 80) - 40) << 52));
        pair(special[x % n], from(y));
        pair((double)(x >> 11), (double)(y % 1024) / 8); // short mantissas: products that land on a rounding tie
        pair((double)(x >> 40) * (double)(y % 1024 + 1), (double)(y % 1024 + 1)); // exact quotients
        pair((double)(x >> 11) * 1e300, (double)(y % 1024) * 1e5); // and at the edge of overflow
        pair((double)(x >> 11) * 1e-300, (double)(y % 1024) * 1e-20); // and of underflow
    }
    printf(failures ? "FAIL: %d differences\n" : "PASS: soft-fp equals the host's doubles\n", failures);
    return failures != 0;
}
