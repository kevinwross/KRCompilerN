/* math.c — Integer math functions (plan.md §14.3).
 *
 * KR16 has no hardware floating point, so all math functions are
 * integer-based. sqrt uses a simple integer square root algorithm.
 * For KR32, these could be replaced with hardware FP instructions.
 */

#include "hal.h"

#include <stddef.h>

/* Integer absolute value (long long variant). */
long long llabs(long long j) {
    return j < 0 ? -j : j;
}

/* Integer square root (floor). Returns the largest r such that r*r <= n.
 * Returns 0 for negative input. */
unsigned long isqrt(unsigned long n) {
    if (n == 0) return 0;

    /* Newton's method for integer square root. */
    unsigned long x = n;
    while (true) {
        unsigned long y = (x + n / x) / 2;
        if (y >= x) return x;
        x = y;
    }
}

/* Integer power: base^exp for non-negative exp. Returns 1 for exp==0. */
unsigned long ipow(unsigned long base, unsigned long exp) {
    unsigned long result = 1;
    while (exp--) result *= base;
    return result;
}

/* Integer divide with rounding: (n + d/2) / d for positive d. */
unsigned long idiv_round(unsigned long n, unsigned long d) {
    if (d == 0) return 0;
    return (n + d / 2) / d;
}

/* Greatest common divisor (Euclidean algorithm). */
unsigned long igcd(unsigned long a, unsigned long b) {
    while (b) {
        unsigned long t = a % b;
        a = b;
        b = t;
    }
    return a;
}
