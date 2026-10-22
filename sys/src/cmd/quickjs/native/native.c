#include "../plan9.h"

/* QuickJS needs the C99 return value even when the output is truncated. */
int
qjs_vsnprintf(char *buf, int size, const char *fmt, va_list ap)
{
	FILE *f;
	char *s;
	int n, copy;

	if(size > 0)
		buf[0] = 0;
	f = sopenw();
	if(f == nil)
		return -1;
	n = vfprintf(f, fmt, ap);
	s = sclose(f);
	if(s == nil)
		return -1;
	if(n >= 0 && size > 0){
		copy = n < size ? n : size-1;
		memcpy(buf, s, copy);
		buf[copy] = 0;
	}
	free(s);
	return n;
}

int
qjs_snprintf(char *buf, int size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = qjs_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

int
gettimeofday(struct timeval *tv, void *tz)
{
	vlong t;

	USED(tz);
	t = nsec();
	tv->tv_sec = t / 1000000000;
	tv->tv_usec = t / 1000 % 1000000;
	return 0;
}

double
fmin(double x, double y)
{
	if(isNaN(x))
		return y;
	if(isNaN(y))
		return x;
	if(x == y)
		return p9_signbit(x) ? x : y;
	return x < y ? x : y;
}
