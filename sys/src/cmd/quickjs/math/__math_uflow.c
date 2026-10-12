#include "libm.h"

double __math_uflow(uint32_t sign)
{
	return __math_xflow(sign, 1.2882297539194267e-231);
}
