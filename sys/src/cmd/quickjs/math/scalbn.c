#include "libm.h"
#include <stdint.h>

double scalbn(double x, int n)
{
	union {double f; uint64_t i;} u;
	double_t y = x;

	if (n > 1023) {
		y *= 8.98846567431158e+307;
		n -= 1023;
		if (n > 1023) {
			y *= 8.98846567431158e+307;
			n -= 1023;
			if (n > 1023)
				n = 1023;
		}
	} else if (n < -1022) {
		/* make sure final n < -53 to avoid double
		   rounding in the subnormal range */
		y *= 2.2250738585072014e-308 * 9007199254740992.0;
		n += 1022 - 53;
		if (n < -1022) {
			y *= 2.2250738585072014e-308 * 9007199254740992.0;
			n += 1022 - 53;
			if (n < -1022)
				n = -1022;
		}
	}
	u.i = (uint64_t)(0x3ff+n)<<52;
	x = y * u.f;
	return x;
}
