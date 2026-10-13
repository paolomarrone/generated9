#include "../plan9.h"

static void
check(int ok, char *what)
{
	if(!ok)
		sysfatal("%s", what);
}

static int
negative(double x)
{
	union { double f; uvlong i; } u;

	u.f = x;
	return u.i >> 63;
}

void
main(void)
{
	union { double f; uvlong i; } zero;
	double inf, nan;

	setfcr(0);
	setfsr(0);
	zero.i = 0x8000000000000000ULL;
	inf = Inf(1);
	nan = NaN();
	check(cos(0) == 1, "cos(0)");
	check(sin(0) == 0 && tan(0) == 0, "sin/tan(0)");
	check(negative(sin(zero.f)) && negative(tan(zero.f)), "trig signed zero");
	check(negative(asinh(zero.f)) && negative(atanh(zero.f)), "hyperbolic signed zero");
	check(negative(trunc(-0.5)), "trunc signed zero");
	check(isInf(log(0), -1) && isInf(log(inf), 1), "log limits");
	check(isNaN(log(-1)) && isNaN(log(nan)), "log domain");
	check(isInf(lgamma(0), 1) && isInf(lgamma(inf), 1), "lgamma limits");
	check(isNaN(lgamma(nan)), "lgamma NaN");
	check(isInf(tgamma(0), 1) && isInf(tgamma(zero.f), -1), "tgamma poles");
	check(isNaN(tgamma(-1)) && isNaN(tgamma(nan)), "tgamma domain");
	check(tgamma(6) == 120 && fabs(tgamma(-0.5) + 2*sqrt(M_PI)) < 1e-14, "tgamma values");
	check(isInf(asinh(inf), 1) && isInf(asinh(-inf), -1), "asinh limits");
	check(isInf(acosh(inf), 1), "acosh limit");
	check(isNaN(acosh(0)), "acosh domain");
	check(isInf(atanh(1), 1) && isInf(atanh(-1), -1), "atanh limits");
	check(erf(inf) == 1 && erf(-inf) == -1, "erf limits");
	check(erfc(inf) == 0 && erfc(-inf) == 2, "erfc limits");
	check(isNaN(sin(inf)) && isNaN(cos(inf)) && isNaN(tan(inf)), "trig domain");
	check(scalbn(1, -1074) > 0 && scalbn(1, -1075) == 0, "subnormal scaling");
	print("Eigenmath numeric boundary tests passed\n");
	exits(nil);
}
