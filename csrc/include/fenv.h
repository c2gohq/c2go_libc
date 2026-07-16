/* fenv.h — c2go's SOFT fenv model (musl include/fenv.h + the soft-arch bits
 * inlined: FE_ALL_EXCEPT == 0, FE_TONEAREST == 0). The Go runtime owns the
 * FPU control state, so this libc neither reads nor writes real FP flags —
 * musl's dummy src/fenv/fenv.c serves every call (fegetround is always
 * FE_TONEAREST, fetestexcept always 0). musl's math code is soft-float-
 * guarded (#ifdef FE_INEXACT around all bookkeeping), so fmaf/lrint & co
 * compile verbatim and return round-to-nearest-identical values. */
#ifndef _FENV_H
#define _FENV_H

#include <c2go.h>

#define FE_ALL_EXCEPT   0
#define FE_TONEAREST    0

typedef unsigned int fexcept_t;
typedef unsigned int fenv_t;

#define FE_DFL_ENV      ((const fenv_t *) -1)

int feclearexcept(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.feclearexcept", C2GO_GOABI0);
int feraiseexcept(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.feraiseexcept", C2GO_GOABI0);
int fetestexcept(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fetestexcept", C2GO_GOABI0);
int fegetround(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.fegetround", C2GO_GOABI0);
int fesetround(int)
    c2go_linkname("github.com/c2gohq/c2go_libc.fesetround", C2GO_GOABI0);
int fegetenv(fenv_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fegetenv", C2GO_GOABI0);
int fesetenv(const fenv_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.fesetenv", C2GO_GOABI0);

#endif
