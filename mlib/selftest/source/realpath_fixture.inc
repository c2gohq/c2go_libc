/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_REALPATH
#error "C2GO_MLIB_TEST_REALPATH is required"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT is required"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC is required"
#endif

struct c2go_mlib_realpath_result {
    char *managed allocated;
    char *managed caller_buffer;
};

static int c2go_mlib_realpath_equal(const char *managed left,
                                    const char *managed right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    struct c2go_mlib_realpath_result *result = gc_malloc(
        c2go_typeinfo(struct c2go_mlib_realpath_result), sizeof(*result));
    char *managed caller_result;
    if (!result) return 1;

    result->caller_buffer = (char *managed)gc_malloc((void *)0, PATH_MAX);
    if (!result->caller_buffer) return 2;

    result->allocated = C2GO_MLIB_TEST_REALPATH(".", (void *)0);
    if (!result->allocated) return 3;
    C2GO_MLIB_TEST_FORCE_GC();

    caller_result = C2GO_MLIB_TEST_REALPATH(".", result->caller_buffer);
    if (caller_result != result->caller_buffer ||
        !c2go_mlib_realpath_equal(result->allocated,
                                  result->caller_buffer)) return 4;

    /* Assigning a failed replacement directly to the owner retires the prior
     * GC root and leaves the public descriptor NULL. */
    result->allocated = C2GO_MLIB_TEST_REALPATH("", (void *)0);
    if (result->allocated != (void *)0) return 5;
    C2GO_MLIB_TEST_FORCE_GC();

    result->allocated = C2GO_MLIB_TEST_REALPATH(".", (void *)0);
    if (!result->allocated ||
        !c2go_mlib_realpath_equal(result->allocated,
                                  result->caller_buffer)) return 6;
    C2GO_MLIB_TEST_FORCE_GC();

    /* Managed release severs the final root; GC owns the allocation. */
    result->allocated = (void *)0;
    result->caller_buffer = (void *)0;
    caller_result = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_REALPATH
