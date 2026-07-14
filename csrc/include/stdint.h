/* stdint.h — fixed-width integers. Base types come from <bits/alltypes.h>;
 * this header adds the least/fast aliases and the limit / constant macros.
 *
 * Every limit comes from a clang target builtin, so e.g. INTPTR_MAX is
 * 2^31-1 on ILP32 and 2^63-1 on LP64/LLP64 — the pointer-sized limits track
 * the pointer width, they are NOT hard-wired to the 64-bit maxima. */
#ifndef _STDINT_H
#define _STDINT_H

#define __NEED_int8_t
#define __NEED_int16_t
#define __NEED_int32_t
#define __NEED_int64_t
#define __NEED_intmax_t
#define __NEED_uint8_t
#define __NEED_uint16_t
#define __NEED_uint32_t
#define __NEED_uint64_t
#define __NEED_uintmax_t
#define __NEED_intptr_t
#define __NEED_uintptr_t
#include <bits/alltypes.h>   /* the fixed-width + intptr/uintptr types */

typedef int8_t   int_least8_t;
typedef int16_t  int_least16_t;
typedef int32_t  int_least32_t;
typedef int64_t  int_least64_t;
typedef uint8_t  uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t   int_fast8_t;
typedef int32_t  int_fast16_t;
typedef int32_t  int_fast32_t;
typedef int64_t  int_fast64_t;
typedef uint8_t  uint_fast8_t;
typedef uint32_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

/* ── exact-width limits ──── */
#define INT8_MAX    __INT8_MAX__
#define INT16_MAX   __INT16_MAX__
#define INT32_MAX   __INT32_MAX__
#define INT64_MAX   __INT64_MAX__
#define INT8_MIN    (-INT8_MAX-1)
#define INT16_MIN   (-INT16_MAX-1)
#define INT32_MIN   (-INT32_MAX-1)
#define INT64_MIN   (-INT64_MAX-1)
#define UINT8_MAX   __UINT8_MAX__
#define UINT16_MAX  __UINT16_MAX__
#define UINT32_MAX  __UINT32_MAX__
#define UINT64_MAX  __UINT64_MAX__

/* ── pointer-sized & greatest-width limits (track the pointer width) ──── */
#define INTPTR_MAX  __INTPTR_MAX__
#define INTPTR_MIN  (-INTPTR_MAX-1)
#define UINTPTR_MAX __UINTPTR_MAX__
#define PTRDIFF_MAX __PTRDIFF_MAX__
#define PTRDIFF_MIN (-PTRDIFF_MAX-1)
#define SIZE_MAX    __SIZE_MAX__
#define INTMAX_MAX  __INTMAX_MAX__
#define INTMAX_MIN  (-INTMAX_MAX-1)
#define UINTMAX_MAX __UINTMAX_MAX__

/* ── least / fast limits ──── */
#define INT_LEAST8_MAX   INT8_MAX
#define INT_LEAST16_MAX  INT16_MAX
#define INT_LEAST32_MAX  INT32_MAX
#define INT_LEAST64_MAX  INT64_MAX
#define INT_LEAST8_MIN   INT8_MIN
#define INT_LEAST16_MIN  INT16_MIN
#define INT_LEAST32_MIN  INT32_MIN
#define INT_LEAST64_MIN  INT64_MIN
#define UINT_LEAST8_MAX  UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX
#define INT_FAST8_MAX    INT8_MAX
#define INT_FAST16_MAX   INT32_MAX
#define INT_FAST32_MAX   INT32_MAX
#define INT_FAST64_MAX   INT64_MAX
#define INT_FAST8_MIN    INT8_MIN
#define INT_FAST16_MIN   INT32_MIN
#define INT_FAST32_MIN   INT32_MIN
#define INT_FAST64_MIN   INT64_MIN
#define UINT_FAST8_MAX   UINT8_MAX
#define UINT_FAST16_MAX  UINT32_MAX
#define UINT_FAST32_MAX  UINT32_MAX
#define UINT_FAST64_MAX  UINT64_MAX

/* ── wchar_t / wint_t / sig_atomic_t ranges ──── *
 * From clang builtins so they track the NATIVE per-OS type (see
 * <bits/alltypes.h>): wchar_t is `unsigned short` on Windows → WCHAR_MAX
 * 65535, but `int` on Linux/macOS → 2147483647. */
#define WCHAR_MAX       __WCHAR_MAX__
#ifdef __WCHAR_UNSIGNED__
#define WCHAR_MIN       (__WCHAR_TYPE__)0
#else
#define WCHAR_MIN       (-__WCHAR_MAX__ - 1)
#endif
#define WINT_MAX        __WINT_MAX__
#ifdef __WINT_UNSIGNED__
#define WINT_MIN        (__WINT_TYPE__)0
#else
#define WINT_MIN        (-__WINT_MAX__ - 1)
#endif
#define SIG_ATOMIC_MAX  __SIG_ATOMIC_MAX__
#define SIG_ATOMIC_MIN  (-__SIG_ATOMIC_MAX__ - 1)

/* ── constant-suffix macros (target-correct suffix from clang) ──── */
#define INT8_C(c)   c
#define INT16_C(c)  c
#define INT32_C(c)  c
#define UINT8_C(c)  c
#define UINT16_C(c) c
#define UINT32_C(c) c ## U
#define __c2go_cglue(a, b) a ## b
#define __c2go_cjoin(a, b) __c2go_cglue(a, b)
#define INT64_C(c)   __c2go_cjoin(c, __INT64_C_SUFFIX__)
#define UINT64_C(c)  __c2go_cjoin(c, __UINT64_C_SUFFIX__)
#define INTMAX_C(c)  __c2go_cjoin(c, __INTMAX_C_SUFFIX__)
#define UINTMAX_C(c) __c2go_cjoin(c, __UINTMAX_C_SUFFIX__)

#endif /* _STDINT_H */
