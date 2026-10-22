#ifndef QUICKJS_NATIVE_H
#define QUICKJS_NATIVE_H

#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>

typedef s8int int8_t;
typedef s16int int16_t;
typedef s32int int32_t;
typedef s64int int64_t;
typedef u8int uint8_t;
typedef u16int uint16_t;
typedef u32int uint32_t;
typedef u64int uint64_t;
typedef intptr intptr_t;
typedef uintptr uintptr_t;
typedef uintptr size_t;
typedef intptr ptrdiff_t;
typedef vlong time_t;

#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MIN (-32767-1)
#define INT16_MAX 32767
#define UINT16_MAX 65535
#define INT32_MIN (-2147483647-1)
#define INT32_MAX 2147483647
#define UINT32_MAX 0xffffffffU
#define INT64_MIN (-9223372036854775807LL-1)
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 0xffffffffffffffffULL
#define SIZE_MAX ((size_t)-1)
#if defined(_amd64) || defined(_arm64) || defined(_power64)
#define INTPTR_MAX INT64_MAX
#else
#define INTPTR_MAX INT32_MAX
#endif
#define INT_MAX INT32_MAX
#define INT_MIN INT32_MIN
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define DBL_MANT_DIG 53
#define LDBL_MANT_DIG 53
#define DBL_MAX 1.7976931348623157e308
#define DBL_MIN 2.2250738585072014e-308
#define DBL_EPSILON 2.2204460492503131e-16
#define M_PI PI
#define M_LN2 0.69314718055994530942
#define isnan(x) isNaN(x)
#define isinf(x) isInf(x, 0)
#define va_copy(d, s) ((d) = (s))

struct timeval {
	vlong tv_sec;
	long tv_usec;
};
double fmin(double, double);
double log2(double);

int gettimeofday(struct timeval *, void *);
int qjs_snprintf(char *, int, const char *, ...);
int qjs_vsnprintf(char *, int, const char *, va_list);
#define snprintf qjs_snprintf
#define vsnprintf qjs_vsnprintf

#endif
