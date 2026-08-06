/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_REGEX_H
#define C2GO_MLIB_REGEX_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Import POSIX flags, error values, regoff_t, and regmatch_t from the ordinary
 * header. Replacement mode suppresses only root regex_t and root functions so
 * the managed carrier can own the standard names without a redeclaration. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _REGEX_H
#error "c2go mlib regex replacement must be included before <regex.h>"
#endif
#define C2GO_REGEX_OMIT_TYPE 1
#define C2GO_REGEX_OMIT_FUNCTIONS 1
#include <regex.h>
#undef C2GO_REGEX_OMIT_FUNCTIONS
#undef C2GO_REGEX_OMIT_TYPE
#else
#include <regex.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* TRE stores an opaque pointer graph. __c2go_arena is the direct GC-visible
 * owner of every gc_malloc block in that graph; __opaque is the TNFA entry
 * point. regfree clears both roots and the GC reclaims the complete graph. */
typedef struct C2GO_MLIB_NAME(regex) {
	size_t re_nsub;
	void *managed __opaque;
	void *managed __c2go_arena;
} C2GO_MLIB_NAME(regex_t);

int C2GO_MLIB_NAME(regcomp)(C2GO_MLIB_NAME(regex_t) *__restrict,
                            const char *__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_regcomp", C2GO_GOABI0);
int C2GO_MLIB_NAME(regexec)(const C2GO_MLIB_NAME(regex_t) *__restrict,
                            const char *__restrict, size_t,
                            regmatch_t *__restrict, int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_regexec", C2GO_GOABI0);
void C2GO_MLIB_NAME(regfree)(C2GO_MLIB_NAME(regex_t) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_regfree", C2GO_GOABI0);

/* regerror is stateless and ignores the carrier. Keep one root implementation
 * while preserving the selected public spelling. */
size_t C2GO_MLIB_NAME(regerror)(int,
                                const C2GO_MLIB_NAME(regex_t) *__restrict,
                                char *__restrict, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.regerror", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_REGEX_H */
