#ifndef RAYLIB_PLAN9_H
#define RAYLIB_PLAN9_H
#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>

typedef Rune wchar_t;
typedef uintptr size_t;
typedef intptr ptrdiff_t;
typedef u8int uint8_t;
typedef s8int int8_t;
typedef u16int uint16_t;
typedef s16int int16_t;
typedef u32int uint32_t;
typedef s32int int32_t;
typedef u64int uint64_t;
typedef s64int int64_t;
typedef uintptr uintptr_t;
typedef intptr intptr_t;
#define UINT8_MAX 255
#define UINT16_MAX 65535
#define INT16_MIN (-32767-1)
#define INT16_MAX 32767
#define SHRT_MAX INT16_MAX
#define SHRT_MIN INT16_MIN
#define UINT32_MAX 0xffffffffU
#define UINT64_MAX 0xffffffffffffffffULL
#define INT64_MAX 9223372036854775807LL
#if defined(_amd64) || defined(_arm64) || defined(_power64)
#define INTPTR_MAX INT64_MAX
#define RAYLIB_64BIT
#else
#define INTPTR_MAX INT32_MAX
#endif
#define INT32_MAX 2147483647
#define INT_MAX INT32_MAX
#define INT_MIN (-INT_MAX-1)
#define UINT_MAX UINT32_MAX
#define SIZE_MAX ((size_t)-1)
#define ULONG_MAX 0xffffffffUL
#define FLT_MAX 3.402823466e38F
#define FLT_MIN 1.175494351e-38F
#define FLT_EPSILON 1.192092896e-7F
#define DBL_MAX 1.7976931348623157e308
#define DBL_MIN 2.2250738585072014e-308
#define DBL_EPSILON 2.2204460492503131e-16
#define RAND_MAX 32767
#define va_copy(dst, src) ((dst) = (src))
#define M_PI PI
#define NAN NaN()
#define INFINITY Inf(1)
#define HUGE_VAL Inf(1)
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define exit(n) exits((n) ? "raylib" : nil)
#define abort() sysfatal("abort")
#define strcasecmp cistrcmp
#define strncasecmp cistrncmp
#define __inline__ inline
#include <raymath9.h>
#endif
