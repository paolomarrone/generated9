#include <u.h>
#include <libc.h>

static double
dvalue(double x)
{
	return x;
}

static float
fvalue(float x)
{
	return x;
}

/* Exercise values, both branch directions and two function-call operands. */
#define Check(op, name, want, evalfn)\
	if((a op b) != (want)) sysfatal("%s: %s value", type, name);\
	if(!(a op b) != !(want)) sysfatal("%s: %s negation", type, name);\
	r = 0;\
	if(a op b) r = 1;\
	if(r != (want)) sysfatal("%s: %s branch", type, name);\
	r = 1;\
	if(!(a op b)) r = 0;\
	if(r != (want)) sysfatal("%s: %s inverted branch", type, name);\
	if((evalfn(a) op evalfn(b)) != (want)) sysfatal("%s: %s calls", type, name);\
	if((a op evalfn(b)) != (want)) sysfatal("%s: %s right call", type, name);\
	if((evalfn(a) op b) != (want)) sysfatal("%s: %s left call", type, name)

#define Comparisons(evalfn)\
	Check(==, "==", eq, evalfn);\
	Check(!=, "!=", !eq, evalfn);\
	Check(<, "<", lt, evalfn);\
	Check(<=, "<=", lt || eq, evalfn);\
	Check(>, ">", gt, evalfn);\
	Check(>=, ">=", gt || eq, evalfn)

static void
dcompare(double a, double b, int eq, int lt, int gt)
{
	char *type = "double";
	int r;

	Comparisons(dvalue);
}

static void
fcompare(float a, float b, int eq, int lt, int gt)
{
	char *type = "float";
	int r;

	Comparisons(fvalue);
}

void
main(void)
{
	uvlong bits = 0x7ff8000000000000ULL;
	double d[8];
	float f;
	int i, j, a, b, eq, lt, gt, truth, r;

	setfcr(0);
	setfsr(0);
	d[0] = Inf(-1);
	d[1] = -1;
	d[2] = 0;
	d[3] = dvalue(0) / d[0];
	d[4] = 1;
	d[5] = Inf(1);
	d[6] = NaN();
	/* Quiet NaN as well as the signaling NaN returned by NaN(). */
	memmove(&d[7], &bits, sizeof d[7]);
	for(i = 0; i < nelem(d); i++){
		truth = i != 2 && i != 3;
		assert(!!d[i] == truth);
		assert(!d[i] == !truth);
		assert((d[i] == 0.0) == !truth);
		assert((d[i] != 0.0) == truth);
		assert((d[i] < 0.0) == (i < 2));
		assert((0.0 <= d[i]) == (i >= 2 && i < 6));
		assert((dvalue(d[i]) > 0.0) == (i == 4 || i == 5));
		assert(!(d[i] >= 0.0) == (i < 2 || i >= 6));
		r = 0;
		if(d[i]) r = 1;
		assert(r == truth);
		f = d[i];
		assert(!!f == truth);
		assert(!f == !truth);
		r = 0;
		if(f) r = 1;
		assert(r == truth);
		for(j = 0; j < nelem(d); j++){
			/* Integer ranks avoid using FP comparisons as the oracle. */
			a = i == 3 ? 2 : i;
			b = j == 3 ? 2 : j;
			eq = i < 6 && j < 6 && a == b;
			lt = i < 6 && j < 6 && a < b;
			gt = i < 6 && j < 6 && a > b;
			dcompare(d[i], d[j], eq, lt, gt);
			fcompare(d[i], d[j], eq, lt, gt);
		}
	}
	print("floating-point comparisons passed\n");
	exits(nil);
}
