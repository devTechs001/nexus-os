/*
 * NEXUS OS - Math Library Declarations
 * kernel/include/math.h
 *
 * The kernel is linked with -nodefaultlibs, so there is no libm to fall
 * back on. These are the math functions NEXUS implements itself in
 * kernel/core/math.c. Without this header every caller in the tree
 * (AI inference, the transformer, the touchscreen driver, the renderer)
 * had to call them without a prototype - which in C silently assumes an
 * int return type and mangles float results.
 */

#ifndef _NEXUS_MATH_H
#define _NEXUS_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- exponential / logarithmic --- */
float expf(float x);

/* --- roots --- */
float sqrtf(float x);
double sqrt(double x);

/* --- absolute value --- */
float fabsf(float x);
/* Pre-existing definition in math.c returns float, not the C standard's
 * double; declare it as-is so no caller silently mis-reads a float return. */
float fabs(float x);

/* --- hyperbolic --- */
float tanhf(float x);

/* --- integer / generic --- */
int abs(int x);

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_MATH_H */
