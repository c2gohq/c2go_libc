/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed allocation variants of strdup and strndup. String traversal and
 * copying stay on the shared stateless libc surface; only ownership differs. */

#include <c2go/mlib/string.h>
#include <errno.h>
#include <stdint.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_extern char *managed mlib_strdup(const char *source)
{
    size_t length = strlen(source);
    char *managed copy;

    if (length == SIZE_MAX) {
        errno = ENOMEM;
        return (void *)0;
    }
    copy = (char *managed)gc_malloc((void *)0, length + 1);
    if (!copy) {
        errno = ENOMEM;
        return (void *)0;
    }
    memcpy((char *)copy, source, length + 1);
    return copy;
}

c2go_extern char *managed mlib_strndup(const char *source, size_t limit)
{
    size_t length = strnlen(source, limit);
    char *managed copy;

    if (length == SIZE_MAX) {
        errno = ENOMEM;
        return (void *)0;
    }
    copy = (char *managed)gc_malloc((void *)0, length + 1);
    if (!copy) {
        errno = ENOMEM;
        return (void *)0;
    }
    memcpy((char *)copy, source, length);
    copy[length] = 0;
    return copy;
}

#pragma c2go pop
