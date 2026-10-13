#ifndef EIGENMATH_PLAN9_H
#define EIGENMATH_PLAN9_H

#include <u.h>
#include <libc.h>
#include <ctype.h>

typedef uchar uint8_t;
typedef u32int uint32_t;
typedef uvlong uint64_t;
typedef vlong int64_t;

/* Eigenmath uses PI as a symbol-table index. */
#undef PI
#define M_PI 3.141592653589793238462643383279502884
#define M_E 2.718281828459045235360287471352662498
#define NULL nil
#define RAND_MAX 32767
#define INFINITY Inf(1)
#define isfinite(x) (!isNaN(x) && !isInf(x, 0))
#define snprintf snprint
#define exit emexit
#define log emlog

void emexit(int);
double acosh(double);
double asinh(double);
double atanh(double);
double erf(double);
double erfc(double);
double lgamma(double);
double tgamma(double);
double scalbn(double, int);
double trunc(double);
double emlog(double);

#endif
