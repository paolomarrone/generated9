#include <u.h>
#include <libc.h>

/* Keep the wide product live while a narrow operation borrows AX, DX or CX. */
static vlong divc(vlong x, long n, long d) { USED(d); return x*1000 + n/1000; }
static vlong divv(vlong x, long n, long d) { return x*1000 + n/d; }
static vlong modc(vlong x, long n, long d) { USED(d); return x*1000 + n%1000; }
static vlong modv(vlong x, long n, long d) { return x*1000 + n%d; }
static vlong udiv(vlong x, long n, long d) { return x*1000 + (ulong)n/(ulong)d; }
static vlong umod(vlong x, long n, long d) { return x*1000 + (ulong)n%(ulong)d; }
static vlong asdiv(vlong x, long n, long d) { return x*1000 + (n /= d); }
static vlong asmod(vlong x, long n, long d) { return x*1000 + (n %= d); }
static vlong shift(vlong x, long n, long d) { return x*1000 + (n << d); }
static vlong asshift(vlong x, long n, long d) { return x*1000 + (n <<= d); }
static vlong mix(vlong x, long n, long d) { return x*1000 + n/d + n%d; }

static void
check(char *name, vlong (*f)(vlong, long, long), long n, long d, long answer)
{
	vlong x, got, want;
	int i;

	for(i = 0; i < 2; i++){
		x = i ? -1792688000LL : 1792688000LL;
		want = x*1000;
		want += answer;
		got = f(x, n, d);
		if(got != want)
			sysfatal("%s: %lld, want %lld", name, got, want);
	}
}

void
main(void)
{
	check("divc", divc, 123456, 0, 123);
	check("divc negative", divc, -123456, 0, -123);
	check("divv", divv, 123456, 1000, 123);
	check("divv negative", divv, -123456, 1000, -123);
	check("divv negative divisor", divv, 123456, -1000, -123);
	check("modc", modc, 123456, 0, 456);
	check("modv", modv, -123456, 1000, -456);
	check("udiv", udiv, -1, 1000, 4294967);
	check("umod", umod, -1, 1000, 295);
	check("asdiv", asdiv, -123456, 1000, -123);
	check("asmod", asmod, -123456, 1000, -456);
	check("shift", shift, 123456, 3, 987648);
	check("asshift", asshift, 123456, 3, 987648);
	check("mix", mix, 123456, 1000, 579);
	print("register spill checks passed\n");
	exits(nil);
}
