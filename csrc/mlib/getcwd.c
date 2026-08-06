/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed allocation policy for getcwd(NULL, size). The cwd lookup remains in
 * the shared Go bridge; only ownership of the optional result differs. */

#include <c2go/mlib/unistd.h>
#include <errno.h>
#include <limits.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef char *managed mlib_cwd_pointer;

c2go_linkname("github.com/c2gohq/c2go_libc.__c2go_syscall_getcwd", C2GO_GOABI0)
int __c2go_mlib_getcwd_bridge(char *managed, unsigned long long);

c2go_extern char *managed mlib_getcwd(char *managed buffer, size_t size)
{
    mlib_cwd_pointer result = buffer;
    int status;

    if (!result) {
        /* Match root libc's musl-compatible extension: the size argument is
         * ignored when the caller requests allocation. */
        result = (mlib_cwd_pointer)gc_malloc((void *)0, PATH_MAX);
        if (!result) {
            errno = ENOMEM;
            return (void *)0;
        }
        size = PATH_MAX;
    } else if (!size) {
        errno = EINVAL;
        return (void *)0;
    }

    status = __c2go_mlib_getcwd_bridge(result, size);
    if (status < 0) {
        /* Retire a locally allocated managed root on the logical failure
         * path. Clearing this local alias does not consume a caller buffer. */
        result = (mlib_cwd_pointer)0;
        errno = -status;
        return (void *)0;
    }
    return result;
}

#pragma c2go pop
