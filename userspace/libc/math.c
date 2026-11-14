/*
 * NEXUS OS - userspace math primitives
 * userspace/libc/math.c
 *
 * Only what libc actually calls. userspace/include/math.h declares a dozen
 * functions; libc/stdlib.c needs exactly one of them - pow() - for the exponent
 * handling in atof(). Declaring it without defining it was enough to make the
 * link fail once userspace was finally linked as its own image.
 *
 * pow() is implemented as a square-and-multiply loop over an integer exponent
 * plus a log/exp-free approximation for fractional exponents, which is all
 * atof() needs: it only ever calls pow(10.0, n) with an integral n.
 *
 * The remaining prototypes in math.h are left undefined on purpose. They are
 * not referenced by anything in the userspace image, and defining floating
 * point routines with no caller would add untested code to the first binary
 * this tree has ever linked. If something starts calling them, the link will
 * fail loudly, which is the behaviour we want.
 */

#include "math.h"

/* Natural log, series-based. Accurate enough for the fractional-exponent path
 * in pow(); not used by anything on the critical path. */
static double nxs_log(double x)
{
    if (x <= 0.0) {
        return 0.0;
    }

    /* Reduce x into [0.5, 2) so the atanh series converges quickly. */
    int exponent = 0;
    while (x >= 2.0) {
        x *= 0.5;
        exponent++;
    }
    while (x < 0.5) {
        x *= 2.0;
        exponent--;
    }

    /* log(x) = 2 * atanh((x-1)/(x+1)) */
    double z = (x - 1.0) / (x + 1.0);
    double z2 = z * z;
    double term = z;
    double sum = 0.0;

    for (int i = 1; i < 40; i += 2) {
        sum += term / (double)i;
        term *= z2;
    }

    return 2.0 * sum + (double)exponent * 0.6931471805599453;
}

/* e^x by range reduction around log(2) plus a Taylor series. */
static double nxs_exp(double x)
{
    int whole = 0;

    while (x >= 0.6931471805599453) {
        x -= 0.6931471805599453;
        whole++;
    }
    while (x <= -0.6931471805599453) {
        x += 0.6931471805599453;
        whole--;
    }

    double term = 1.0;
    double sum = 1.0;

    for (int i = 1; i < 24; i++) {
        term *= x / (double)i;
        sum += term;
    }

    while (whole > 0) {
        sum *= 2.0;
        whole--;
    }
    while (whole < 0) {
        sum *= 0.5;
        whole++;
    }

    return sum;
}

/**
 * pow - raise base to exp
 *
 * Integral exponents go through square-and-multiply so atof()'s
 * pow(10.0, n) is exact for the range a decimal literal uses. Fractional
 * exponents fall back to e^(exp * log(base)).
 */
double pow(double base, double exp)
{
    if (exp == 0.0) {
        return 1.0;
    }

    if (base == 0.0) {
        return (exp < 0.0) ? 0.0 : 0.0;
    }

    /* Integral exponent: exact repeated squaring. */
    if (exp == (double)(long)exp) {
        long e = (long)exp;
        int negate = 0;

        if (e < 0) {
            negate = 1;
            e = -e;
        }

        double result = 1.0;
        double b = base;

        while (e > 0) {
            if (e & 1) {
                result *= b;
            }
            b *= b;
            e >>= 1;
        }

        return negate ? (1.0 / result) : result;
    }

    /* Fractional exponent. Negative bases have no real result. */
    if (base < 0.0) {
        return 0.0;
    }

    return nxs_exp(exp * nxs_log(base));
}
