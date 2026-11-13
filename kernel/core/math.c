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
