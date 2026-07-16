/* libm.h — c2go shim standing in for musl's src/internal/libm.h, consumed by
 * the verbatim-musl src/math TUs whose `#include "libm.h"` falls through to
 * the include path (nothing next to the TU). Distilled from musl's header: Trimmed to the 64-bit-double / 32-bit-float world c2go
 * targets: long double == double (LDBL_MANT_DIG == 53), so NO ldshape / 80-bit
 * or 113-bit unions and NO l-variant helpers. The bit-access macros work on the
 * uint64/uint32 VALUE (asuint64 then shift), so they are endian-independent.
 *
 * IMPORTANT for ports (the @llvm.floor->libcall self-recursion trap): do NOT use
 * __builtin_floor/ceil/round/trunc/rint/nearbyint in those functions' bodies —
 * on a target without SSE4.1 the backend lowers @llvm.floor to a CALL to floor(),
 * i.e. infinite recursion. Use musl's bit-manipulation instead. __builtin_sqrt
 * (@llvm.sqrt -> sqrtsd/fsqrt), __builtin_fabs and __builtin_copysign (bit ops)
 * are safe — they lower to hardware with no library call. */
#ifndef _C2GO_LIBM_H
#define _C2GO_LIBM_H

#include <stdint.h>
#include <float.h>
#include <math.h>
#include <endian.h>
#include <c2go.h>

/* double_t/float_t come from <math.h> (FLT_EVAL_METHOD == 0 everywhere). */

/* type-punning accessors (no aliasing UB — compound-literal unions) */
#define asuint(f)   ((union{float _f; uint32_t _i;}){f})._i
#define asfloat(i)  ((union{uint32_t _i; float _f;}){i})._f
#define asuint64(f) ((union{double _f; uint64_t _i;}){f})._i
#define asdouble(i) ((union{uint64_t _i; double _f;}){i})._f

#define EXTRACT_WORDS(hi,lo,d) do { uint64_t __u = asuint64(d); (hi)=__u>>32; (lo)=(uint32_t)__u; } while (0)
#define GET_HIGH_WORD(hi,d)    do { (hi) = asuint64(d) >> 32; } while (0)
#define GET_LOW_WORD(lo,d)     do { (lo) = (uint32_t)asuint64(d); } while (0)
#define INSERT_WORDS(d,hi,lo)  do { (d) = asdouble(((uint64_t)(hi)<<32) | (uint32_t)(lo)); } while (0)
#define SET_HIGH_WORD(d,hi)    INSERT_WORDS(d, hi, (uint32_t)asuint64(d))
#define SET_LOW_WORD(d,lo)     INSERT_WORDS(d, asuint64(d)>>32, lo)
#define GET_FLOAT_WORD(w,d)    do { (w) = asuint(d); } while (0)
#define SET_FLOAT_WORD(d,w)    do { (d) = asfloat(w); } while (0)

/* Support non-nearest rounding modes; no signaling NaNs. */
#define WANT_ROUNDING 1
#define issignalingf_inline(x) 0
#define issignaling_inline(x) 0

#ifdef __GNUC__
#define predict_true(x)  __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)
#else
#define predict_true(x)  (x)
#define predict_false(x) (x)
#endif

static inline float  eval_as_float(float x)   { float  y = x; return y; }
static inline double eval_as_double(double x)  { double y = x; return y; }

static inline float  fp_barrierf(float x)  { volatile float  y = x; return y; }
static inline double fp_barrier(double x)  { volatile double y = x; return y; }

static inline void fp_force_evalf(float x) { volatile float  y; y = x; (void)y; }
static inline void fp_force_eval(double x) { volatile double y; y = x; (void)y; }
#define FORCE_EVAL(x) do { \
	if (sizeof(x) == sizeof(float))       fp_force_evalf(x); \
	else                                  fp_force_eval(x);  \
} while (0)

/* Error-path helpers (musl src/math/__math_*.c), inlined so every cluster that
 * needs them is self-contained. They raise the right FP exception and return the
 * IEEE result; float variants mirror them. */
static inline double __math_invalid(double x)      { return (x - x) / (x - x); }
static inline float  __math_invalidf(float x)      { return (x - x) / (x - x); }
static inline double __math_divzero(uint32_t sign) { return fp_barrier(sign ? -1.0 : 1.0) / 0.0; }
static inline float  __math_divzerof(uint32_t sign){ return fp_barrierf(sign ? -1.0f : 1.0f) / 0.0f; }
static inline double __math_xflow(uint32_t sign, double y)  { return eval_as_double(fp_barrier(sign ? -y : y) * y); }
static inline float  __math_xflowf(uint32_t sign, float y)  { return eval_as_float(fp_barrierf(sign ? -y : y) * y); }
static inline double __math_uflow(uint32_t sign) { return __math_xflow(sign, 0x1p-767); }
static inline float  __math_uflowf(uint32_t sign){ return __math_xflowf(sign, 0x1p-95f); }
static inline double __math_oflow(uint32_t sign) { return __math_xflow(sign, 0x1p769); }
static inline float  __math_oflowf(uint32_t sign){ return __math_xflowf(sign, 0x1p97f); }

/* ---- cross-TU internal surface (trig wave) ----------------------------------
 * musl declares these `hidden` in src/internal/libm.h; in the c2go model each
 * is a linkname declaration here + a C2GO_KEEPCASE definition in its own TU
 * (the __fesetround / __rand48_step pattern). */
int __rem_pio2_large(double *x, double *y, int e0, int nx, int prec)
    c2go_linkname("github.com/c2gohq/c2go_libc.__rem_pio2_large", C2GO_GOABI0);
int __rem_pio2(double x, double *y)
    c2go_linkname("github.com/c2gohq/c2go_libc.__rem_pio2", C2GO_GOABI0);
double __sin(double x, double y, int iy)
    c2go_linkname("github.com/c2gohq/c2go_libc.__sin", C2GO_GOABI0);
double __cos(double x, double y)
    c2go_linkname("github.com/c2gohq/c2go_libc.__cos", C2GO_GOABI0);
double __tan(double x, double y, int odd)
    c2go_linkname("github.com/c2gohq/c2go_libc.__tan", C2GO_GOABI0);
int __rem_pio2f(float x, double *y)
    c2go_linkname("github.com/c2gohq/c2go_libc.__rem_pio2f", C2GO_GOABI0);
float __sindf(double x)
    c2go_linkname("github.com/c2gohq/c2go_libc.__sindf", C2GO_GOABI0);
float __cosdf(double x)
    c2go_linkname("github.com/c2gohq/c2go_libc.__cosdf", C2GO_GOABI0);
float __tandf(double x, int odd)
    c2go_linkname("github.com/c2gohq/c2go_libc.__tandf", C2GO_GOABI0);

#endif /* _C2GO_LIBM_H */
