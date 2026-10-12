#include "libm.h"

double __math_oflow(uint32_t sign)
{
	return __math_xflow(sign, 3.105036184601418e+231);
}
