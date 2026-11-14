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

/* u32 and bool; this header had no dependency of its own and relied on the
 * including translation unit having pulled in types.h already. */
#include "types.h"

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

/* --- trigonometric ---
 * Declared here because there is no libm; defined in kernel/core/math.c.
 * sensor_hub.c's orientation filter called all of these with no declaration,
 * which in C assumes an int return type. */
float sinf(float x);
float cosf(float x);
float tanf(float x);
float atanf(float x);
float atan2f(float y, float x);
float asinf(float x);
float acosf(float x);
float copysignf(float x, float y);

/* --- integer / generic --- */
int abs(int x);

/* --- special values ---
 *
 * The kernel has no <float.h> and no <math.h> of its own, so the softmax and
 * max-score loops in ai_ml/models/transformer.c had no definition of INFINITY
 * to use and did not compile. Build the values bit-exactly rather than relying
 * on a division: 1.0f/0.0f at compile time is a constant-expression error in a
 * freestanding build, and -ffast-math (which some targets here use) would fold
 * a runtime 0.0f/0.0f to 0.
 *
 * A union is used instead of a cast because strict C cannot reinterpret float
 * bits through a pointer without a trap representation.
 */
#define NEXUS_INF_BITS   0x7F800000u   /* +inf (fp32) */
#define NEXUS_QNAN_BITS  0x7FC00000u   /* quiet NaN (fp32) */

typedef union nexus_f32_bits {
    u32 bits;
    float value;
} nexus_f32_bits_t;

/* Function-like so these stay compile-time constants usable as initialisers. */
#define nexus_f32_from_bits(b)  (((nexus_f32_bits_t){ .bits = (b) }).value)

#define INFINITY    nexus_f32_from_bits(NEXUS_INF_BITS)
#define NAN         nexus_f32_from_bits(NEXUS_QNAN_BITS)
#define HUGE_VALF   INFINITY

/* Pi. Used by the transformer's position encodings and by the neural engine's
 * initialisers; the kernel has no libm and no <float.h>, so there is nowhere
 * else for these to come from. */
#define M_PI        3.14159265358979323846
#define M_PI_F      3.14159265f
#define M_E         2.71828182845904523536
#define M_SQRT2     1.41421356237309504880

/* Classification without libm. A value is a NaN when it is not equal to
 * itself; infinities are the exponent all-ones with a zero mantissa. */
static inline bool isnan(float x) { return x != x; }

static inline bool isinf(float x)
{
    nexus_f32_bits_t u;
    u.value = x;
    return (u.bits & 0x7FFFFFFFu) == NEXUS_INF_BITS;
}

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_MATH_H */
