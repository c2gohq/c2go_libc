/* math.h — floating-point math. Pure-C ports of musl's src/math (double + float;
 * long double == double in this data model, so the l-variants are aliases added
 * on demand, not declared here). Classification uses clang builtins so it is
 * correct without a runtime call. Each function is c2go_linkname'd to its Go
 * binding; the implementations live in source/math_*.c. */
#ifndef _MATH_H
#define _MATH_H

#include <c2go.h>

/* FLT_EVAL_METHOD == 0 on every c2go target (SSE2 / NEON), so the C99
 * evaluation types are the nominal types. */
typedef double double_t;
typedef float  float_t;

#define HUGE_VAL  (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())
#define INFINITY  (__builtin_inff())
#define NAN       (__builtin_nanf(""))

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define isfinite(x)   __builtin_isfinite(x)
#define isinf(x)      __builtin_isinf(x)
#define isnan(x)      __builtin_isnan(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)
#define isgreater(x,y)      __builtin_isgreater(x,y)
#define isgreaterequal(x,y) __builtin_isgreaterequal(x,y)
#define isless(x,y)         __builtin_isless(x,y)
#define islessequal(x,y)    __builtin_islessequal(x,y)
#define islessgreater(x,y)  __builtin_islessgreater(x,y)
#define isunordered(x,y)    __builtin_isunordered(x,y)

#define FP_ILOGB0   (-1-0x7fffffff)  /* musl: ilogb(0) sentinel (#661) */
#define FP_ILOGBNAN (-1-0x7fffffff)  /* musl: ilogb(NaN) sentinel */

extern int signgam;  /* XSI: sign of gamma(x) set by lgamma (math_gamma.c) */

#define MATH_ERRNO     1
#define MATH_ERREXCEPT 2
/* musl's value (#657): the ported musl implementations report via FP
 * exception flags, never via errno — advertising MATH_ERRNO would send
 * callers to a stale errno. (the flags themselves are
 * not queryable either — c2go's fenv is SOFT, fetestexcept is always 0;
 * this mirrors musl's contract, not a new one.) */
#define math_errhandling MATH_ERREXCEPT

/* ---- double ---- */
double floor(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.floor", C2GO_GOABI0);
double ceil(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.ceil", C2GO_GOABI0);
double trunc(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.trunc", C2GO_GOABI0);
double round(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.round", C2GO_GOABI0);
double rint(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.rint", C2GO_GOABI0);
double nearbyint(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.nearbyint", C2GO_GOABI0);
long lrint(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.lrint", C2GO_GOABI0);
long long llrint(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.llrint", C2GO_GOABI0);
long lround(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.lround", C2GO_GOABI0);
long long llround(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.llround", C2GO_GOABI0);
double modf(double, double *)
    c2go_linkname("github.com/c2gohq/c2go_libc.modf", C2GO_GOABI0);
double fabs(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fabs", C2GO_GOABI0);
double copysign(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.copysign", C2GO_GOABI0);
double frexp(double, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.frexp", C2GO_GOABI0);
double ldexp(double, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.ldexp", C2GO_GOABI0);
double scalbn(double, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.scalbn", C2GO_GOABI0);
double scalbln(double, long)
    c2go_linkname("github.com/c2gohq/c2go_libc.scalbln", C2GO_GOABI0);
int ilogb(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.ilogb", C2GO_GOABI0);
double logb(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.logb", C2GO_GOABI0);
double nextafter(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.nextafter", C2GO_GOABI0);
double nexttoward(double, long double)
    c2go_linkname("github.com/c2gohq/c2go_libc.nexttoward", C2GO_GOABI0);
double nan(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.nan", C2GO_GOABI0);
double fdim(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fdim", C2GO_GOABI0);
double fmax(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmax", C2GO_GOABI0);
double fmin(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmin", C2GO_GOABI0);
double fma(double, double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fma", C2GO_GOABI0);
double sqrt(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.sqrt", C2GO_GOABI0);
double cbrt(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.cbrt", C2GO_GOABI0);
double pow(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.pow", C2GO_GOABI0);
double hypot(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.hypot", C2GO_GOABI0);
double exp(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.exp", C2GO_GOABI0);
double exp2(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.exp2", C2GO_GOABI0);
double exp10(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.exp10", C2GO_GOABI0);
double pow10(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.pow10", C2GO_GOABI0);
double expm1(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.expm1", C2GO_GOABI0);
/* Bessel functions, first (j) and second (y) kind, orders 0/1/n
 * (source/math_bessel.c, musl/FreeBSD msun; y0/y1/yn bundle with j0/j1/jn). */
double j0(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.j0", C2GO_GOABI0);
double j1(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.j1", C2GO_GOABI0);
double jn(int, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.jn", C2GO_GOABI0);
double y0(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.y0", C2GO_GOABI0);
double y1(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.y1", C2GO_GOABI0);
double yn(int, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.yn", C2GO_GOABI0);
double log(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.log", C2GO_GOABI0);
double log2(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.log2", C2GO_GOABI0);
double log10(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.log10", C2GO_GOABI0);
double log1p(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.log1p", C2GO_GOABI0);
double sin(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.sin", C2GO_GOABI0);
double cos(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.cos", C2GO_GOABI0);
double tan(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.tan", C2GO_GOABI0);
double asin(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.asin", C2GO_GOABI0);
double acos(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.acos", C2GO_GOABI0);
double atan(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.atan", C2GO_GOABI0);
double atan2(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.atan2", C2GO_GOABI0);
void sincos(double, double *, double *)
    c2go_linkname("github.com/c2gohq/c2go_libc.sincos", C2GO_GOABI0);
double sinh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.sinh", C2GO_GOABI0);
double cosh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.cosh", C2GO_GOABI0);
double tanh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.tanh", C2GO_GOABI0);
double asinh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.asinh", C2GO_GOABI0);
double acosh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.acosh", C2GO_GOABI0);
double atanh(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.atanh", C2GO_GOABI0);
double fmod(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmod", C2GO_GOABI0);
double remainder(double, double)
    c2go_linkname("github.com/c2gohq/c2go_libc.remainder", C2GO_GOABI0);
double remquo(double, double, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.remquo", C2GO_GOABI0);
double tgamma(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.tgamma", C2GO_GOABI0);
double lgamma(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.lgamma", C2GO_GOABI0);
double lgamma_r(double, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.lgamma_r", C2GO_GOABI0);
double erf(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.erf", C2GO_GOABI0);
double erfc(double)
    c2go_linkname("github.com/c2gohq/c2go_libc.erfc", C2GO_GOABI0);

/* ---- float ---- */
float floorf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.floorf", C2GO_GOABI0);
float ceilf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.ceilf", C2GO_GOABI0);
float truncf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.truncf", C2GO_GOABI0);
float roundf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.roundf", C2GO_GOABI0);
float rintf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.rintf", C2GO_GOABI0);
float nearbyintf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.nearbyintf", C2GO_GOABI0);
long lrintf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.lrintf", C2GO_GOABI0);
long long llrintf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.llrintf", C2GO_GOABI0);
long lroundf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.lroundf", C2GO_GOABI0);
long long llroundf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.llroundf", C2GO_GOABI0);
float modff(float, float *)
    c2go_linkname("github.com/c2gohq/c2go_libc.modff", C2GO_GOABI0);
float fabsf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fabsf", C2GO_GOABI0);
float copysignf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.copysignf", C2GO_GOABI0);
float frexpf(float, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.frexpf", C2GO_GOABI0);
float ldexpf(float, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.ldexpf", C2GO_GOABI0);
float scalbnf(float, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.scalbnf", C2GO_GOABI0);
float scalblnf(float, long)
    c2go_linkname("github.com/c2gohq/c2go_libc.scalblnf", C2GO_GOABI0);
int ilogbf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.ilogbf", C2GO_GOABI0);
float logbf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.logbf", C2GO_GOABI0);
float nextafterf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.nextafterf", C2GO_GOABI0);
float nexttowardf(float, long double)
    c2go_linkname("github.com/c2gohq/c2go_libc.nexttowardf", C2GO_GOABI0);
float nanf(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.nanf", C2GO_GOABI0);
float fdimf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fdimf", C2GO_GOABI0);
float fmaxf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmaxf", C2GO_GOABI0);
float fminf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fminf", C2GO_GOABI0);
float fmaf(float, float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmaf", C2GO_GOABI0);
float sqrtf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.sqrtf", C2GO_GOABI0);
float cbrtf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.cbrtf", C2GO_GOABI0);
float powf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.powf", C2GO_GOABI0);
float hypotf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.hypotf", C2GO_GOABI0);
float expf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.expf", C2GO_GOABI0);
float exp2f(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.exp2f", C2GO_GOABI0);
float expm1f(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.expm1f", C2GO_GOABI0);
float logf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.logf", C2GO_GOABI0);
float log2f(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.log2f", C2GO_GOABI0);
float log10f(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.log10f", C2GO_GOABI0);
float log1pf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.log1pf", C2GO_GOABI0);
float sinf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.sinf", C2GO_GOABI0);
float cosf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.cosf", C2GO_GOABI0);
float tanf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.tanf", C2GO_GOABI0);
float asinf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.asinf", C2GO_GOABI0);
float acosf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.acosf", C2GO_GOABI0);
float atanf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.atanf", C2GO_GOABI0);
float atan2f(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.atan2f", C2GO_GOABI0);
void sincosf(float, float *, float *)
    c2go_linkname("github.com/c2gohq/c2go_libc.sincosf", C2GO_GOABI0);
float sinhf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.sinhf", C2GO_GOABI0);
float coshf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.coshf", C2GO_GOABI0);
float tanhf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.tanhf", C2GO_GOABI0);
float asinhf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.asinhf", C2GO_GOABI0);
float acoshf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.acoshf", C2GO_GOABI0);
float atanhf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.atanhf", C2GO_GOABI0);
float fmodf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.fmodf", C2GO_GOABI0);
float remainderf(float, float)
    c2go_linkname("github.com/c2gohq/c2go_libc.remainderf", C2GO_GOABI0);
float remquof(float, float, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.remquof", C2GO_GOABI0);
float tgammaf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.tgammaf", C2GO_GOABI0);
float lgammaf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.lgammaf", C2GO_GOABI0);
float lgammaf_r(float, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.lgammaf_r", C2GO_GOABI0);
float erff(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.erff", C2GO_GOABI0);
float erfcf(float)
    c2go_linkname("github.com/c2gohq/c2go_libc.erfcf", C2GO_GOABI0);


/* ── long double variants (#664): long double == double in the c2go data
 * model (see the header note), so every *l IS its double twin — object-like
 * macro aliases keep calls AND address-taking (&sinl == &sin) working. musl
 * arranges the same equivalence via weak aliases where the formats match. */
#define floorl floor
#define ceill ceil
#define truncl trunc
#define roundl round
#define rintl rint
#define nearbyintl nearbyint
#define lrintl lrint
#define llrintl llrint
#define lroundl lround
#define llroundl llround
#define modfl modf
#define fabsl fabs
#define copysignl copysign
#define frexpl frexp
#define ldexpl ldexp
#define scalbnl scalbn
#define scalblnl scalbln
#define ilogbl ilogb
#define logbl logb
#define nextafterl nextafter
#define nexttowardl nexttoward
#define nanl nan
#define fdiml fdim
#define fmaxl fmax
#define fminl fmin
#define fmal fma
#define sqrtl sqrt
#define cbrtl cbrt
#define powl pow
#define hypotl hypot
#define expl exp
#define exp2l exp2
#define expm1l expm1
#define logl log
#define log2l log2
#define log10l log10
#define log1pl log1p
#define sinl sin
#define cosl cos
#define tanl tan
#define asinl asin
#define acosl acos
#define atanl atan
#define atan2l atan2
#define sinhl sinh
#define coshl cosh
#define tanhl tanh
#define asinhl asinh
#define acoshl acosh
#define atanhl atanh
#define fmodl fmod
#define remainderl remainder
#define remquol remquo
#define tgammal tgamma
#define lgammal lgamma
#define erfl erf
#define erfcl erfc

#endif /* _MATH_H */
