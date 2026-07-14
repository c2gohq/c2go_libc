/* stdlib.h — general utilities.
 *
 * The functions implemented so far (source/stdlib.c: the numeric-conversion,
 * search and sort core) carry c2go_linkname naming the Go symbol they are
 * reached as + the ABI0 CC; their definitions are marked c2go_extern. The
 * declarations below all have live definitions (musl-completion campaign). */
#ifndef _STDLIB_H
#define _STDLIB_H

#define __NEED_size_t
#define __NEED_wchar_t
#define __NEED_locale_t
#include <bits/alltypes.h>
#include <c2go.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* string -> number */
int atoi(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.atoi", C2GO_GOABI0);
long atol(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.atol", C2GO_GOABI0);
long long atoll(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.atoll", C2GO_GOABI0);
double atof(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.atof", C2GO_GOABI0);
float strtof(const char *__restrict, char **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtof", C2GO_GOABI0);
double strtod(const char *__restrict, char **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtod", C2GO_GOABI0);
long double strtold(const char *__restrict, char **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtold", C2GO_GOABI0);
/* locale variants (source/locale.c). Only the FLOAT strto*_l exist — musl has no
 * integer strtol_l — and even these just forward: the decimal point is "." in
 * every locale musl supports, so the locale_t is ignored. */
float strtof_l(const char *__restrict, char **__restrict, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtof_l", C2GO_GOABI0);
double strtod_l(const char *__restrict, char **__restrict, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtod_l", C2GO_GOABI0);
long double strtold_l(const char *__restrict, char **__restrict, locale_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtold_l", C2GO_GOABI0);
long strtol(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtol", C2GO_GOABI0);
unsigned long strtoul(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtoul", C2GO_GOABI0);
long long strtoll(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtoll", C2GO_GOABI0);
unsigned long long strtoull(const char *__restrict, char **__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.strtoull", C2GO_GOABI0);

/* pseudo-random */
int rand(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.rand", C2GO_GOABI0);
void srand(unsigned)
    c2go_linkname("github.com/c2gohq/c2go_libc.srand", C2GO_GOABI0);
#define RAND_MAX (0x7fffffff)
/* extended prng families (source/prng.c, #675) */
int rand_r(unsigned *)
    c2go_linkname("github.com/c2gohq/c2go_libc.rand_r", C2GO_GOABI0);
long random(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.random", C2GO_GOABI0);
void srandom(unsigned)
    c2go_linkname("github.com/c2gohq/c2go_libc.srandom", C2GO_GOABI0);
char *initstate(unsigned, char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.initstate", C2GO_GOABI0);
char *setstate(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.setstate", C2GO_GOABI0);
double drand48(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.drand48", C2GO_GOABI0);
double erand48(unsigned short [3])
    c2go_linkname("github.com/c2gohq/c2go_libc.erand48", C2GO_GOABI0);
long lrand48(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.lrand48", C2GO_GOABI0);
long nrand48(unsigned short [3])
    c2go_linkname("github.com/c2gohq/c2go_libc.nrand48", C2GO_GOABI0);
long mrand48(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.mrand48", C2GO_GOABI0);
long jrand48(unsigned short [3])
    c2go_linkname("github.com/c2gohq/c2go_libc.jrand48", C2GO_GOABI0);
void srand48(long)
    c2go_linkname("github.com/c2gohq/c2go_libc.srand48", C2GO_GOABI0);
unsigned short *seed48(unsigned short *)
    c2go_linkname("github.com/c2gohq/c2go_libc.seed48", C2GO_GOABI0);
void lcong48(unsigned short [7])
    c2go_linkname("github.com/c2gohq/c2go_libc.lcong48", C2GO_GOABI0);
/* misc XSI utilities (source/misc.c, #675) */
long a64l(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.a64l", C2GO_GOABI0);
char *l64a(long)
    c2go_linkname("github.com/c2gohq/c2go_libc.l64a", C2GO_GOABI0);
int getsubopt(char **, char *const *, char **)
    c2go_linkname("github.com/c2gohq/c2go_libc.getsubopt", C2GO_GOABI0);
void *reallocarray(void *, size_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.reallocarray", C2GO_GOABI0);

/* memory — unmanaged libc (NOT gc_malloc, which lives in c2go.h). Implemented
 * in Go (malloc.go): per-allocation make([]byte) slices rooted in a handle
 * table until free(). C reaches the Go functions through these linknames —
 * the same C-calls-Go pattern as gc_malloc/GCMalloc. */
void *malloc(size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.Malloc", C2GO_GOABI0);
void *calloc(size_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.Calloc", C2GO_GOABI0);
void *realloc(void *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.Realloc", C2GO_GOABI0);
void  free(void *)
    c2go_linkname("github.com/c2gohq/c2go_libc.Free", C2GO_GOABI0);
void *aligned_alloc(size_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.AlignedAlloc", C2GO_GOABI0);
int   posix_memalign(void **, size_t, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.posix_memalign", C2GO_GOABI0);

/* process control */
_Noreturn void abort(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.abort", C2GO_GOABI0);
_Noreturn void exit(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.exit", C2GO_GOABI0);
_Noreturn void _Exit(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_exit", C2GO_GOABI0);
_Noreturn void quick_exit(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.quick_exit", C2GO_GOABI0);
int atexit(void (*)(void))
    c2go_linkname("github.com/c2gohq/c2go_libc.atexit", C2GO_GOABI0);
int at_quick_exit(void (*)(void))
    c2go_linkname("github.com/c2gohq/c2go_libc.at_quick_exit", C2GO_GOABI0);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* environment (via the Go bridge over the os package) */
char *getenv(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.Getenv", C2GO_GOABI0);
int setenv(const char *, const char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.Setenv", C2GO_GOABI0);
int unsetenv(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.Unsetenv", C2GO_GOABI0);
/* putenv/clearenv + the environ snapshot live in source/env.c (#675; environ
 * itself is declared in <unistd.h>). putenv COPIES — see env.c's header. */
int putenv(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.putenv", C2GO_GOABI0);
int clearenv(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.clearenv", C2GO_GOABI0);
int system(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.System", C2GO_GOABI0);

/* temp files / path canonicalisation. Both cross-platform: mkstemp/
 * mkstemps/mkdtemp in source/stdio.c (shared randname); realpath over a
 * path/filepath bridge (unix: source/fsops_posix.c; Windows:
 * source/fsops_windows.c, #647). */
int mkstemp(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.mkstemp", C2GO_GOABI0);
int mkstemps(char *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.mkstemps", C2GO_GOABI0);
char *mkdtemp(char *)
    c2go_linkname("github.com/c2gohq/c2go_libc.mkdtemp", C2GO_GOABI0);
char *realpath(const char *__restrict, char *__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc.realpath", C2GO_GOABI0);

/* search / sort (the comparator is an ordinary c2go function pointer) */
void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.bsearch", C2GO_GOABI0);
/* CONTRACT: qsort cannot sort arrays whose ELEMENTS hold managed Go pointers
 * (gc_malloc/typeinfo objects) — the void* interface erases the element type,
 * so the moves are untyped and break the concurrent-GC invariants (invisible
 * temp copies, torn pointers, no write barriers; see source/qsort.c). This is
 * a documented model constraint by decision — there is deliberately no runtime
 * rejection. Scalar/non-pointer elements (including anything in noscan
 * libc-malloc memory) are unaffected; sort managed collections on the Go side
 * or sort an index/key array instead. */
void qsort(void *, size_t, size_t, int (*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.qsort", C2GO_GOABI0);
void qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *)
    c2go_linkname("github.com/c2gohq/c2go_libc.qsort_r", C2GO_GOABI0);

/* integer arithmetic */
int abs(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.abs", C2GO_GOABI0);
long labs(long)
    c2go_linkname("github.com/c2gohq/c2go_libc.labs", C2GO_GOABI0);
long long llabs(long long)
    c2go_linkname("github.com/c2gohq/c2go_libc.llabs", C2GO_GOABI0);
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;
div_t   div(int, int)
    c2go_returntype(div_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.div", C2GO_GOABI0);
ldiv_t  ldiv(long, long)
    c2go_returntype(ldiv_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.ldiv", C2GO_GOABI0);
lldiv_t lldiv(long long, long long)
    c2go_returntype(lldiv_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.lldiv", C2GO_GOABI0);

/* multibyte / wide (non-restartable; restartable twins live in <wchar.h>) */
int    mblen(const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mblen", C2GO_GOABI0);
int    mbtowc(wchar_t *__restrict, const char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbtowc", C2GO_GOABI0);
int    wctomb(char *, wchar_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wctomb", C2GO_GOABI0);
size_t mbstowcs(wchar_t *__restrict, const char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.mbstowcs", C2GO_GOABI0);
size_t wcstombs(char *__restrict, const wchar_t *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.wcstombs", C2GO_GOABI0);
size_t __ctype_get_mb_cur_max(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.__ctype_get_mb_cur_max", C2GO_GOABI0);
#define MB_CUR_MAX (__ctype_get_mb_cur_max())

#endif /* _STDLIB_H */
