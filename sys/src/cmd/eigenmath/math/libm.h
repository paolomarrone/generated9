#ifndef EIGENMATH_LIBM_H
#define EIGENMATH_LIBM_H

#include <u.h>
#include <libc.h>

typedef u32int uint32_t;
typedef int int32_t;
typedef uvlong uint64_t;
typedef double double_t;

#define INFINITY Inf(1)
#define log emlog
#define predict_false(x) (x)
#define FORCE_EVAL(x) do { volatile double y = (x); USED(y); } while(0)
#define GET_HIGH_WORD(h, x) do { \
	union { double f; uint64_t i; } u; \
	u.f = (x); (h) = u.i >> 32; \
} while(0)
#define SET_LOW_WORD(x, l) do { \
	union { double f; uint64_t i; } u; \
	u.f = (x); u.i = (u.i & 0xffffffff00000000ULL) | (uint32_t)(l); (x) = u.f; \
} while(0)

double log1p(double);
double __sin(double, double, int);
double __cos(double, double);
double __tan(double, double, int);
double scalbn(double, int);
int __rem_pio2(double, double*);
int __rem_pio2_large(double*, double*, int, int, int);
double fp_neg(double);
double __lgamma_r(double, int*);
double emlog(double);

#endif
