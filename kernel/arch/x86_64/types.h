#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef	__cplusplus
# define restrict
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

#if defined(__GNUC__)

#define _packed                 __attribute__((__packed__))
#define _const                 __attribute__((__const__))
#define __pure                  __attribute__((__pure__))
#define _malloc                __attribute__((__malloc__))
#define _alloc_align(pi)       __attribute__((__alloc_align__(pi)))
#define _use_result            __attribute__((__warn_unused_result__))
#define _leaf                   __attribute__((__leaf__))
#define __aligned(n)            __attribute__((__aligned__(n)))
#define _always_inline         __attribute__((__always_inline__)) inline
#define _noreturn              __attribute__((__noreturn__))
#define _used                  __attribute__((__used__))
#define _returns_twice         __attribute__((__returns_twice__))
#define _vector_size(n)        __attribute__((__vector_size__(n)))
#define _noinline              __attribute__((__noinline__))
#define _assume_aligned(n)     __attribute__((__assume_aligned__(n)))
#define _printf_format(m,n)    __attribute__((__format__(printf, m, n)))
#define _artificial            __attribute__((__artificial__))
#define _no_instrument         __attribute__((__no_instrument_function__))
#define _no_asan               __attribute__((__no_address_safety_analysis__))
#define _constructor(prio)     __attribute__((__constructor__ (prio)))
#define _destructor(prio)      __attribute__((__destructor__ (prio)))

#define _section(name)         __attribute__((__section__(name)))

#define _generic_target \
    __attribute__(( \
    __target__("no-sse3"), \
    __target__("no-ssse3"), \
    __target__("no-sse4.1"), \
    __target__("no-sse4.2"), \
    __target__("no-avx"), \
    __target__("no-avx2") \
    ))

typedef int_fast64_t off_t;
typedef __SIZE_TYPE__ size_t;
typedef long ssize_t;

#endif

typedef decltype(nullptr) nullptr_t;

/// SSE vectors
typedef int8_t _vector_size(16) __i8_vec16;
typedef int16_t _vector_size(16) __i16_vec8;
typedef int32_t _vector_size(16) __i32_vec4;
typedef int64_t _vector_size(16) __i64_vec2;
typedef uint8_t _vector_size(16) __u8_vec16;
typedef uint16_t _vector_size(16) __u16_vec8;
typedef uint32_t _vector_size(16) __u32_vec4;
typedef uint64_t _vector_size(16) __u64_vec2;
typedef float _vector_size(16) __f32_vec4;
typedef double _vector_size(16) __d64_vec2;

// Some builtins unnecessarily insist on long long types
typedef long long _vector_size(16) __i64_vec2LL;
typedef unsigned long long _vector_size(16) __ivec2ULL;

template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

//#define countof(arr) (sizeof((arr))/sizeof(*(arr)))

typedef int pid_t;

struct process_t;
