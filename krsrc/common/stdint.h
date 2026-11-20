#ifndef KRC_STDINT_H
#define KRC_STDINT_H

/* Minimal stdint.h for krc runtime compilation. */

typedef signed char         int8_t;
typedef short               int16_t;
typedef int                 int32_t;
typedef long long           int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef int8_t   int_least8_t;
typedef int16_t  int_least16_t;
typedef int32_t  int_least32_t;
typedef int64_t  int_least64_t;
typedef uint8_t  uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t   int_fast8_t;
typedef int16_t  int_fast16_t;
typedef int32_t  int_fast32_t;
typedef int64_t  int_fast64_t;
typedef uint8_t  uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

#if defined(__KR16__)
typedef short   intptr_t;
typedef unsigned short uintptr_t;
#elif defined(__KR32__)
typedef int     intptr_t;
typedef unsigned int uintptr_t;
#else
typedef long long intptr_t;
typedef unsigned long long uintptr_t;
#endif

typedef long long intmax_t;
typedef unsigned long long uintmax_t;

/* GCC builtins used by the libc implementation. */
double __builtin_sqrt(double);
float __builtin_sqrtf(float);
double __builtin_fabs(double);
float __builtin_fabsf(float);
double __builtin_fmin(double, double);
float __builtin_fminf(float, float);
double __builtin_fmax(double, double);
float __builtin_fmaxf(float, float);

#define INT8_MIN   (-128)
#define INT16_MIN  (-32768)
#define INT32_MIN  (-2147483647 - 1)
#define INT64_MIN  (-9223372036854775807LL - 1)
#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807LL
#define UINT8_MAX  0xff
#define UINT16_MAX 0xffff
#define UINT32_MAX 0xffffffffu
#define UINT64_MAX 0xffffffffffffffffULL

#if defined(__KR16__)
#define INTPTR_MIN  (-32768)
#define INTPTR_MAX  32767
#define UINTPTR_MAX 0xffff
#define PTRDIFF_MIN (-32768)
#define PTRDIFF_MAX 32767
#define SIZE_MAX    0xffff
#elif defined(__KR32__)
#define INTPTR_MIN  (-2147483647 - 1)
#define INTPTR_MAX  2147483647
#define UINTPTR_MAX 0xffffffffu
#define PTRDIFF_MIN (-2147483647 - 1)
#define PTRDIFF_MAX 2147483647
#define SIZE_MAX    0xffffffffu
#else
#define INTPTR_MIN  (-9223372036854775807LL - 1)
#define INTPTR_MAX  9223372036854775807LL
#define UINTPTR_MAX 0xffffffffffffffffULL
#define PTRDIFF_MIN (-9223372036854775807LL - 1)
#define PTRDIFF_MAX 9223372036854775807LL
#define SIZE_MAX    0xffffffffffffffffULL
#endif

#define SIG_ATOMIC_MIN  (-9223372036854775807LL - 1)
#define SIG_ATOMIC_MAX  9223372036854775807LL

#define WCHAR_MIN  0
#define WCHAR_MAX  0xffffffffffffffffULL

#define WINT_MIN   0
#define WINT_MAX   0xffffffffffffffffULL

#endif /* KRC_STDINT_H */
