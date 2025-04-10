#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef float     f32;
typedef double    f64;
typedef uint8_t   b8;
typedef uint16_t  b16;
typedef uint32_t  b32;
typedef uint64_t  b64;


#define KiloBytes(X) (1024ll*X)
#define MegaBytes(X) (1024ll*KiloBytes(X))
#define GigaBytes(X) (1024ll*MegaBytes(X))

#define False 0
#define True (!False)
#define Null (void*)0

#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))

#define ArrayCount(Array) (sizeof(Array)/sizeof(Array[0]))

#if defined(__APPLE__)
#define per_thread_local __thread
#elif(_WIN32)
#endif

#define array(T) struct { T *Items; s64 Count; }
