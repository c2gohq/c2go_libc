/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_STDLIB_H
#define C2GO_MLIB_STDLIB_H

#include <c2go.h>
#include <c2go/mlib/names.h>
#include <stdlib.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* With resolved_path == NULL, the result is a GC-owned no-pointer buffer.
 * Never pass it to free(); assign its last managed owner NULL after use. To
 * keep the conditional return type precise, a non-null resolved_path must also
 * be a managed no-pointer buffer (normally gc_malloc(NULL, PATH_MAX)), not a C
 * stack array or root malloc allocation. The buffer is borrowed only for this
 * synchronous call. On failure the function returns NULL; assign the result
 * directly to an owner when replacing an earlier managed path so the old root
 * is retired. Root realpath remains available for ordinary stack buffers. */
char *managed mlib_realpath(const char *__restrict,
                            char *managed __restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_realpath", C2GO_GOABI0);

/* Replacement mode routes the standard source spelling to the same single
 * prefixed implementation, while the rest of <stdlib.h> remains root libc. */
#ifdef C2GO_MLIB_UNPREFIXED
#define realpath mlib_realpath
#endif

#pragma c2go pop

#endif /* C2GO_MLIB_STDLIB_H */
