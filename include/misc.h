#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

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
#define per_thread_local __declspec(thread)
#endif


static inline int CLZ_U32(u32 X) {
#if defined(_MSC_VER) && !defined(__clang__)
	u64 Index;
	if (_BitScanReverse(&Index, X))
		return 31 - Index;
	else
		return 32;
#elif defined(__clang__)
	return X ? __builtin_clz(X) : 32;
#else
	#error "CLZ_U32 is not defined for this compiler"
#endif
}

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

static inline int CLZ_U64(u64 X) {
#if defined(_MSC_VER) && !defined(__clang__)
	u64 Index;
	if (_BitScanReverse64(&Index, X))
		return 63 - Index;
	else
		return 64;
#elif defined(__clang__)
	return X ? __builtin_clzll(X) : 64;
#else
	#error "CLZ_U64 is not defined for this compiler"
#endif
}

static inline int CTZ_U32(u32 X) {
#if defined(_MSC_VER) && !defined(__clang__)
    u64 Index;
    if (_BitScanForward(&Index, X))
        return Index;
    else
        return 32;
#elif defined(__clang__)
    return X ? __builtin_ctz(X) : 32;
#else
    #error "CTZ_U32 is not defined for this compiler"
#endif
}

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

static inline int CTZ_U64(u64 X) {
#if defined(_MSC_VER) && !defined(__clang__)
    u64 Index;
    if (_BitScanForward64(&Index, X))
        return Index;
    else
        return 64;
#elif defined(__clang__)
    return X ? __builtin_ctzll(X) : 64;
#else
    #error "CTZ_U64 is not defined for this compiler"
#endif
}

#define Stringify_(S) #S
#define Stringify(S) Stringify_(S)
#define Concat_(A,B) A##B
#define Concat(A,B) Concat_(A,B)

#define Align(Value, Alignment) ((Value + (Alignment - 1)) & ~(Alignment - 1))

#define Assert assert

#define array(T) struct { T *Items; s64 Count; }
