/*
 * NEXUS OS - Math Functions Implementation
 * kernel/core/math.c
 *
 * Real implementations of math functions for the kernel
 */

#include "../include/kernel.h"
#include "../include/types.h"

/**
 * expf - Exponential function (approximation)
 * @x: Input value
 *
 * Returns: e^x
 */
float expf(float x)
{
    /* Simple Taylor series expansion for e^x: 1 + x + x^2/2! + x^3/3! + ... */
    float res = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 15; i++) {
        term *= x / (float)i;
        res += term;
        /* Convergence check */
        if (term < 1e-7f && term > -1e-7f) break;
    }
    return res;
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
