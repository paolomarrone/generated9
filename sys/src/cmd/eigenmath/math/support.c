#include "libm.h"
#undef log

/* Plan 9 log returns NaN at zero and does not handle positive infinity. */
double
emlog(double x)
{
	if(x == 0)
		return Inf(-1);
	if(isNaN(x) || isInf(x, 1))
		return x;
	return log(x);
}

double
lgamma(double x)
{
	int sign;

	return __lgamma_r(x, &sign);
}

/* Preserve signed zero when compiled by the Plan 9 C compilers. */
double
fp_neg(double x)
{
	union { double f; uint64_t i; } u;

	u.f = x;
	u.i ^= 0x8000000000000000ULL;
	return u.f;
}
