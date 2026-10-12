#ifndef QUICKJS_PLAN9_H
#define QUICKJS_PLAN9_H

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifdef _STDINT_ARCH_H_
#define INTPTR_MAX INT64_MAX
#else
#define INTPTR_MAX INT32_MAX
#endif
#define UINT64_C(n) n##ULL
#define CONFIG_VERSION "2026-06-04"
#define __attribute(x)
#define __attribute__(x)
#define __builtin_expect(x, y) (x)
#define NAN NaN()
#define INFINITY Inf(1)
#define isfinite(x) (!isnan(x) && !isinf(x))
#define signbit(x) p9_signbit(x)

void p9_init(void);
char *realpath(const char *, char *);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
extern char **environ;
#define _utimbuf utimbuf
#define _utime utime

int p9_signbit(double);
double scalbn(double, int);
double trunc(double);
double round(double);
double acosh(double);
double asinh(double);
double atanh(double);
double expm1(double);
double log1p(double);
double cbrt(double);
double fmax(double, double);
long lrint(double);
int __builtin_clz(unsigned int);
int __builtin_ctz(unsigned int);
int __builtin_clzll(uint64_t);
int __builtin_ctzll(uint64_t);

void *p9_malloc(size_t);
void *p9_realloc(void *, size_t);
void p9_free(void *);
size_t p9_msize(const void *);

#endif
