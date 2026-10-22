#include "libm.h"
#ifndef PLAN9_NATIVE
#include <stdint.h>
#endif

double fabs(double x)
{
	union {double f; uint64_t i;} u = {x};
	u.i &= -1ULL/2;
	return u.f;
}
