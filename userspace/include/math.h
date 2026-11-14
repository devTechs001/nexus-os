/*
 * NEXUS OS - userspace math.h
 * userspace/include/math.h
 *
 * Declarations for the floating point helpers used by libc. Under
 * -Wno-implicit-function-declaration a call to an undeclared pow() is assumed
 * to return int, which truncates every value atof() computes - the conversion
 * would compile and produce wrong numbers.
 */

#ifndef _NEXUS_USERSPACE_MATH_H
#define _NEXUS_USERSPACE_MATH_H

double pow(double base, double exp);
double sqrt(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double fmod(double x, double y);
double exp(double x);
double log(double x);
double log10(double x);
double sin(double x);
double cos(double x);
double tan(double x);

#endif /* _NEXUS_USERSPACE_MATH_H */
