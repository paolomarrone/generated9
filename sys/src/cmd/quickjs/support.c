#include "plan9.h"
#ifndef PLAN9_NATIVE
#include <lib9.h>
#endif

void
p9_init(void)
{
	/* JavaScript requires nearest rounding, subnormals and masked traps. */
	setfcr(0);
	setfsr(0);
}

/* Keep allocation accounting independent of APE's private malloc layout. */
typedef union Header Header;
union Header {
	size_t size;
	double align;
	void *pointer;
};

void *
p9_malloc(size_t size)
{
	Header *h;

	if(size > SIZE_MAX - sizeof *h)
		return NULL;
	h = malloc(sizeof *h + size);
	if(h == NULL)
		return NULL;
	h->size = size;
	return h + 1;
}

size_t
p9_msize(const void *p)
{
	return ((const Header *)p)[-1].size;
}

void
p9_free(void *p)
{
	if(p != NULL)
		free((Header *)p - 1);
}

void *
p9_realloc(void *p, size_t size)
{
	Header *h;

	if(p == NULL)
		return p9_malloc(size);
	if(size == 0){
		p9_free(p);
		return NULL;
	}
	if(size > SIZE_MAX - sizeof *h)
		return NULL;
	h = realloc((Header *)p - 1, sizeof *h + size);
	if(h == NULL)
		return NULL;
	h->size = size;
	return h + 1;
}

int
p9_signbit(double x)
{
	union { double d; uint64_t u; } v;

	v.d = x;
	return v.u >> 63;
}

int
__builtin_clz(unsigned int x)
{
	int n;

	if(x == 0)
		return 32;
	for(n = 0; (x & 0x80000000U) == 0; n++)
		x <<= 1;
	return n;
}

int
__builtin_ctz(unsigned int x)
{
	int n;

	if(x == 0)
		return 32;
	for(n = 0; (x & 1) == 0; n++)
		x >>= 1;
	return n;
}

int
__builtin_clzll(uint64_t x)
{
	return x >> 32 ? __builtin_clz(x >> 32) : 32 + __builtin_clz(x);
}

int
__builtin_ctzll(uint64_t x)
{
	return (uint32_t)x ? __builtin_ctz(x) : 32 + __builtin_ctz(x >> 32);
}

/* QuickJS uses lrint only for nonnegative values within a 32-bit long. */
long
lrint(double x)
{
	double i, f;

	i = floor(x);
	f = x - i;
	if(f > 0.5 || (f == 0.5 && fmod(i, 2.0) != 0))
		i++;
	return i;
}

double
fmax(double x, double y)
{
	if(isnan(x))
		return y;
	if(isnan(y))
		return x;
	if(x == y)
		return p9_signbit(x) ? y : x;
	return x > y ? x : y;
}
