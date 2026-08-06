/* SPDX-License-Identifier: AGPL-3.0-only */

/* Managed allocation variants of asprintf and vasprintf. Formatting remains
 * on root libc's stateless vsnprintf engine; only allocation, publication, and
 * logical retirement differ. */

#include <c2go/mlib/stdio.h>
#include <errno.h>
#include <stdarg.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef char *managed mlib_formatted_pointer;
typedef mlib_formatted_pointer *managed mlib_formatted_slot;

__attribute__((noinline))
static void mlib_store_formatted_pointer(mlib_formatted_slot slot,
                                         mlib_formatted_pointer value)
{
    *slot = value;
}

c2go_extern int mlib_vasprintf(char **restrict output,
                               const char *restrict format,
                               va_list arguments)
{
    mlib_formatted_slot slot;
    mlib_formatted_pointer buffer;
    va_list measure_arguments;
    int length;
    int result;

    if (!output) {
        errno = EINVAL;
        return -1;
    }

    /* This is both the failure-state contract and logical retirement of any
     * prior GC-owned result held by the caller's slot. */
    slot = (mlib_formatted_slot)output;
    mlib_store_formatted_pointer(slot, (mlib_formatted_pointer)0);

    va_copy(measure_arguments, arguments);
    length = vsnprintf((void *)0, 0, format, measure_arguments);
    va_end(measure_arguments);
    if (length < 0) return -1;

    buffer = (mlib_formatted_pointer)gc_malloc((void *)0,
                                               (size_t)length + 1);
    if (!buffer) {
        errno = ENOMEM;
        return -1;
    }

    result = vsnprintf((char *)(void *)buffer, (size_t)length + 1,
                       format, arguments);
    if (result < 0 || result > length) {
        if (result > length) errno = EOVERFLOW;
        return -1;
    }

    mlib_store_formatted_pointer(slot, buffer);
    return result;
}

c2go_extern int mlib_asprintf(char **restrict output,
                              const char *restrict format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = mlib_vasprintf(output, format, arguments);
    va_end(arguments);
    return result;
}

#pragma c2go pop
