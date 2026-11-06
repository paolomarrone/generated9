#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>

typedef uintptr size_t;
typedef uintptr uintptr_t;
typedef intptr intptr_t;
typedef u32int uint32_t;
typedef u16int uint16_t;
typedef s16int int16_t;
typedef u8int uint8_t;
typedef s8int int8_t;

#define SQLITE_OS_PLAN9 1
#define SQLITE_OS_OTHER 1
#define SQLITE_THREADSAFE 0
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_WAL 1
#define SQLITE_OMIT_LOCALTIME 1
#define SQLITE_MAX_MMAP_SIZE 0
#define SQLITE_POWERSAFE_OVERWRITE 0
#define SQLITE_DISABLE_INTRINSIC 1
#if defined(_amd64) || defined(_arm64) || defined(_power64)
#define SQLITE_PTRSIZE 8
#else
#define SQLITE_PTRSIZE 4
#endif
#define SQLITE_INT64_TYPE vlong
#define SQLITE_UINT64_TYPE uvlong
#define SQLITE_MAX_ALLOCATION_SIZE 0x3fffffff
#define NDEBUG 1
#undef assert
#define assert(x) ((void)0)
#define UINT64_C(x) x##ULL
#define INFINITY Inf(1)
