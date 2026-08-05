/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_FTW_H
#define C2GO_MLIB_FTW_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Replacement mode needs the standard constants and record layouts without
 * admitting root libc's handle-based walk functions. Namespaced mode exposes
 * both APIs in the same translation unit. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _FTW_H
#error "c2go mlib ftw replacement must be included before <ftw.h>"
#endif
#define C2GO_FTW_OMIT_FUNCTIONS 1
#include <ftw.h>
#undef C2GO_FTW_OMIT_FUNCTIONS
#else
#include <ftw.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* The callbacks use c2go's internal ABI and are invoked synchronously. Their
 * path/stat/FTW pointers are borrowed stack views and must not escape the
 * callback. The walk itself uses mlib's direct-pointer DIR carrier. */
int C2GO_MLIB_NAME(ftw)(const char *,
    int (*)(const char *, const struct stat *, int), int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_ftw", C2GO_GOABI0);
int C2GO_MLIB_NAME(nftw)(const char *,
    int (*)(const char *, const struct stat *, int, struct FTW *), int, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_nftw", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_FTW_H */
