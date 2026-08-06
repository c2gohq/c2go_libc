/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed allocation policy for realpath(path, NULL). Canonicalisation stays
 * in the shared Go bridge; only ownership of the optional result differs. */

#include <c2go/mlib/stdlib.h>
#include <errno.h>
#include <limits.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef char *managed mlib_path_pointer;

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_realpath", C2GO_GOABI0)
int __c2go_mlib_realpath_bridge(const char *, char *managed,
                                unsigned long long);

c2go_extern char *managed mlib_realpath(const char *restrict path,
                                        char *managed restrict resolved_path)
{
    mlib_path_pointer buffer = resolved_path;
    int result;

    if (!path) {
        errno = EINVAL;
        return (void *)0;
    }
    if (!path[0]) {
        errno = ENOENT;
        return (void *)0;
    }

    if (!buffer) {
        buffer = (mlib_path_pointer)gc_malloc((void *)0, PATH_MAX);
        if (!buffer) {
            errno = ENOMEM;
            return (void *)0;
        }
    }

    result = __c2go_mlib_realpath_bridge(path, buffer, PATH_MAX);
    if (result < 0) {
        /* Even though GC owns an internally allocated buffer, retire the local
         * managed root explicitly on the logical failure/free path. */
        buffer = (mlib_path_pointer)0;
        errno = -result;
        return (void *)0;
    }
    return buffer;
}

#pragma c2go pop
