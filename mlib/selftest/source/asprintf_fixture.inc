/* SPDX-License-Identifier: AGPL-3.0-only */

#include <stdarg.h>

#ifndef C2GO_MLIB_TEST_ASPRINTF
#error "C2GO_MLIB_TEST_ASPRINTF is required"
#endif
#ifndef C2GO_MLIB_TEST_VASPRINTF
#error "C2GO_MLIB_TEST_VASPRINTF is required"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT is required"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC is required"
#endif

struct c2go_mlib_asprintf_result {
    char *formatted;
};

__attribute__((noinline))
static int c2go_mlib_asprintf_test_vcall(char **output,
                                        const char *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = C2GO_MLIB_TEST_VASPRINTF(output, format, arguments);
    va_end(arguments);
    return result;
}

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    struct c2go_mlib_asprintf_result *result = gc_malloc(
        c2go_typeinfo(struct c2go_mlib_asprintf_result), sizeof(*result));
    if (!result) return 1;

    if (C2GO_MLIB_TEST_ASPRINTF(&result->formatted, "managed-%s:%d",
                                "format", 42) != 17) return 2;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!result->formatted ||
        strcmp(result->formatted, "managed-format:42")) return 3;

    /* A failing replacement retires the previous root before doing work. */
    if (C2GO_MLIB_TEST_ASPRINTF(&result->formatted, "%") != -1 ||
        result->formatted != (void *)0) return 4;

    if (c2go_mlib_asprintf_test_vcall(&result->formatted,
                                      "managed-%s:%d", "va", 73) != 13)
        return 5;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!result->formatted ||
        strcmp(result->formatted, "managed-va:73")) return 6;

    /* Managed release severs the final root; GC owns the storage. */
    result->formatted = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_VASPRINTF
#undef C2GO_MLIB_TEST_ASPRINTF
