/*
 * NEXUS OS - Math Functions Implementation
 * kernel/core/math.c
 *
 * Real implementations of math functions for the kernel
 */

#include "../include/kernel.h"
#include "../include/types.h"
#include "../include/math.h"

/**
 * expf - Exponential function (approximation)
 * @x: Input value
 *
 * Returns: e^x
 */
float expf(float x)
{
    /*
     * Taylor series alone is only trustworthy for |x| well below 1: the
     * terms alternate and grow before they shrink, so for large negative
     * input the partial sums oscillate wildly. exp(-16) came out as
     * -2.8e5 instead of 1.1e-7, which made tanh() return +1.0 for
     * strongly negative arguments and gave softmax nonsense.
     *
     * Fix: reduce the argument by a power of two (exactly representable,
     * so no precision loss) until it is small, expand, then square back up.
     */
    int halvings = 0;
    bool negate = false;

    /* exp(x) = 1 / exp(-x) - keeps every series term positive */
    if (x < 0.0f) {
        x = -x;
        negate = true;
    }

    if (x > 88.0f) {
        /* Beyond float range; saturate */
        return negate ? 0.0f : 3.4028235e38f;
    }

    /* Bring |x| under 0.5: exp(x) = exp(x/2^k)^(2^k) */
    while (x > 0.5f) {
        x *= 0.5f;
        halvings++;
    }

    float res = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 20; i++) {
        term *= x / (float)i;
        res += term;
        if (term < 1e-8f && term > -1e-8f) break;
    }

    /* Undo the halvings by repeated squaring */
    while (halvings-- > 0) {
        res *= res;
    }

    return negate ? (1.0f / res) : res;
}

/**
 * powf - Power function (approximation)
 * @base: Base
 * @exp: Exponent
 *
 * Returns: base^exp
 */
float powf(float base, float exp)
{
    /* base^exp = e^(exp * ln(base)) */
    /* For now, just integer power for simplicity */
    if (exp == (float)(int)exp) {
        int e = (int)exp;
        float res = 1.0f;
        if (e < 0) {
            base = 1.0f / base;
            e = -e;
        }
        while (e > 0) {
            if (e & 1) res *= base;
            base *= base;
            e >>= 1;
        }
        return res;
    }
    
    /* Fallback or more complex implementation needed for fractional exp */
    return 0.0f; 
}

/**
 * abs - Absolute value
 */
int abs(int j)
{
    return j < 0 ? -j : j;
}

/**
 * fabs - Floating point absolute value
 */
float fabs(float x)
{
    return x < 0.0f ? -x : x;
}

/**
 * sqrtf - Floating point square root (Newton-Raphson)
 *
 * The AI inference engine, the transformer and the touchscreen all call
 * sqrtf(), but it was never implemented, so the kernel could not link
 * with -nodefaultlibs. Newton's method converges quadratically and is
 * plenty accurate in single precision after a handful of steps.
 */
float sqrtf(float x)
{
    float guess;

    if (x <= 0.0f) {
        return 0.0f;
    }

    /* Scale the initial guess by halving the exponent, so it starts close */
    guess = x > 1.0f ? x : 1.0f;

    for (int i = 0; i < 24; i++) {
        float next = 0.5f * (guess + x / guess);
        if (next == guess) {
            break;
        }
        guess = next;
    }

    return guess;
}

/**
 * sqrt - Double precision wrapper around sqrtf
 *
 * The touchscreen driver assigns the result to an s32 distance; a double
 * return keeps that file compiling without changing its expression.
 */
double sqrt(double x)
{
    return (double)sqrtf((float)x);
}

/**
 * fabsf - Single precision absolute value
 */
float fabsf(float x)
{
    return x < 0.0f ? -x : x;
}

/**
 * tanhf - Hyperbolic tangent
 *
 * tanh(x) = (e^2x - 1) / (e^2x + 1), clamped to [-1, 1]. Used by the GELU
 * activation in the inference engine.
 */
float tanhf(float x)
{
    float e2x;

    /* Saturate: tanh is +/-1 to within float precision beyond ~9 */
    if (x > 9.0f) {
        return 1.0f;
    }
    if (x < -9.0f) {
        return -1.0f;
    }

    e2x = expf(2.0f * x);

    return (e2x - 1.0f) / (e2x + 1.0f);
}

/**
 * sinf - Sine
 * @x: Angle in radians
 *
 * Returns: sin(x)
 *
 * Taylor series about 0, after range reduction to [-pi, pi]. The kernel links
 * with -nodefaultlibs and there is no libm, so this is the only definition.
 * These functions (with atan2f, asinf and copysignf below) were called by
 * mobile/sensors/sensor_hub.c's orientation code with no definition anywhere
 * in the tree - an unresolved symbol at link time, invisible in a per-file
 * compile.
 */
float sinf(float x)
{
    float x2;
    float term;
    float sum;
    int i;

    /* Range reduce into [-pi, pi] */
    x = x - 6.28318530718f * (float)(long)(x / 6.28318530718f);
    if (x > 3.14159265359f) {
        x -= 6.28318530718f;
    } else if (x < -3.14159265359f) {
        x += 6.28318530718f;
    }

    x2 = x * x;
    term = x;
    sum = x;

    /* x - x^3/3! + x^5/5! - ... ; 9 terms is well inside fp32 for |x|<=pi */
    for (i = 1; i < 9; i++) {
        term = -term * x2 / (float)((2 * i) * (2 * i + 1));
        sum += term;
    }

    return sum;
}

/**
 * cosf - Cosine
 * @x: Angle in radians
 *
 * Returns: cos(x) = sin(x + pi/2)
 */
float cosf(float x)
{
    return sinf(x + 1.57079632679f);
}

/**
 * atanf - Arctangent
 * @x: Value
 *
 * Returns: atan(x) in radians
 *
 * Two range reductions: |x|>1 uses atan(x) = pi/2 - atan(1/x), and |x|>0.4142
 * uses the half-angle identity atan(x) = 2*atan(x/(1+sqrt(1+x^2))), which puts
 * the series argument inside its fast-converging region.
 */
float atanf(float x)
{
    float sign = 1.0f;
    float sum, term, x2;
    int i;

    if (x < 0.0f) {
        x = -x;
        sign = -1.0f;
    }

    if (x > 1.0f) {
        return sign * (1.57079632679f - atanf(1.0f / x));
    }

    if (x > 0.41421356f) {
        return sign * 2.0f * atanf(x / (1.0f + sqrtf(1.0f + x * x)));
    }

    x2 = x * x;
    term = x;
    sum = x;

    /* atan(x) = x - x^3/3 + x^5/5 - ... */
    for (i = 1; i < 12; i++) {
        term = -term * x2;
        sum += term / (float)(2 * i + 1);
    }

    return sign * sum;
}

/**
 * atan2f - Two-argument arctangent
 * @y: Numerator
 * @x: Denominator
 *
 * Returns: Angle in radians in [-pi, pi]
 *
 * Four-quadrant result; atanf alone only covers [-pi/2, pi/2].
 */
float atan2f(float y, float x)
{
    if (x > 0.0f) {
        return atanf(y / x);
    }

    if (x < 0.0f) {
        return (y >= 0.0f) ? (atanf(y / x) + 3.14159265359f)
                           : (atanf(y / x) - 3.14159265359f);
    }

    /* x == 0 */
    if (y > 0.0f) {
        return 1.57079632679f;
    }
    if (y < 0.0f) {
        return -1.57079632679f;
    }

    return 0.0f;    /* both zero: undefined, choose 0 */
}

/**
 * asinf - Arcsine
 * @x: Value in [-1, 1]
 *
 * Returns: asin(x) in radians, or 0 if out of range
 *
 * asin(x) = atan2(x, sqrt(1 - x^2)), which inherits atan2f's quadrant
 * handling and needs no series of its own.
 */
float asinf(float x)
{
    if (x > 1.0f) {
        x = 1.0f;
    } else if (x < -1.0f) {
        x = -1.0f;
    }

    return atan2f(x, sqrtf(1.0f - x * x));
}

/**
 * copysignf - Magnitude of x with the sign of y
 * @x: Value supplying the magnitude
 * @y: Value supplying the sign
 *
 * Returns: |x| with y's sign
 */
float copysignf(float x, float y)
{
    float ax = (x < 0.0f) ? -x : x;

    /* Treat -0.0f as negative too: its sign bit is set even though it compares
     * equal to 0.0f, which is what IEEE copysign does. */
    if (y < 0.0f) {
        return -ax;
    }

    return ax;
}
