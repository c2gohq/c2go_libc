/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed allocation variant of wcsdup. The wchar_t element width remains the
 * target C ABI width: UTF-32 on Unix and UTF-16 on Windows. */

#include <c2go/mlib/wstring.h>
#include <wchar.h>
#include <errno.h>
#include <stdint.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_extern wchar_t *managed mlib_wcsdup(const wchar_t *source)
{
    size_t length = wcslen(source);
    size_t elements;
    wchar_t *managed copy;

    if (length == SIZE_MAX || length + 1 > SIZE_MAX / sizeof(wchar_t)) {
        errno = ENOMEM;
        return (void *)0;
    }
    elements = length + 1;
    copy = (wchar_t *managed)gc_malloc((void *)0,
                                      elements * sizeof(wchar_t));
    if (!copy) {
        errno = ENOMEM;
        return (void *)0;
    }
    wmemcpy((wchar_t *)copy, source, elements);
    return copy;
}

#pragma c2go pop
