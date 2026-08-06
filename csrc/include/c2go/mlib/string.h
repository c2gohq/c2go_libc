/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_STRING_H
#define C2GO_MLIB_STRING_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Stateless string functions continue to come from root libc. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _STRING_H
#error "c2go mlib string replacement must be included before <string.h>"
#endif
#endif
#include <string.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* Results are no-pointer Go-heap buffers. They are reclaimed after all
 * managed references disappear: never call free(), and preferably assign
 * NULL to the owning pointer immediately after its final use. */
char *managed mlib_strdup(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_strdup", C2GO_GOABI0);
char *managed mlib_strndup(const char *, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_strndup", C2GO_GOABI0);

/* strdup/strndup are compiler-recognized libcalls. A second direct route for
 * those exact IR names would conflict with root libc's route when several
 * translation units are merged. Replacement mode therefore aliases the C
 * source spelling to the one prefixed implementation before IR is emitted. */
#ifdef C2GO_MLIB_UNPREFIXED
#define strdup mlib_strdup
#define strndup mlib_strndup
#endif

#pragma c2go pop

#endif /* C2GO_MLIB_STRING_H */
