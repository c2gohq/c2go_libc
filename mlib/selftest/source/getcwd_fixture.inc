/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_GETCWD
#error "C2GO_MLIB_TEST_GETCWD is required"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT is required"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC is required"
#endif

struct c2go_mlib_getcwd_result {
    char *managed allocated;
    char *managed caller_buffer;
};

static int c2go_mlib_getcwd_equal(const char *managed left,
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
    struct c2go_mlib_getcwd_result *result = gc_malloc(
        c2go_typeinfo(struct c2go_mlib_getcwd_result), sizeof(*result));
    char *managed caller_result;
    if (!result) return 1;

    result->caller_buffer = (char *managed)gc_malloc((void *)0, PATH_MAX);
    if (!result->caller_buffer) return 2;

    /* As in root musl, size is ignored for the allocating extension. */
    result->allocated = C2GO_MLIB_TEST_GETCWD((void *)0, 0);
    if (!result->allocated) return 3;
    C2GO_MLIB_TEST_FORCE_GC();

    caller_result = C2GO_MLIB_TEST_GETCWD(result->caller_buffer, PATH_MAX);
    if (caller_result != result->caller_buffer ||
        !c2go_mlib_getcwd_equal(result->allocated,
                                result->caller_buffer)) return 4;

    /* A caller-owned buffer is borrowed, not consumed, on failure. */
    caller_result = C2GO_MLIB_TEST_GETCWD(result->caller_buffer, 0);
    if (caller_result != (void *)0 ||
        !result->caller_buffer[0]) return 5;

    /* Direct replacement with a failed result cuts the old managed root. */
    result->allocated = C2GO_MLIB_TEST_GETCWD(result->allocated, 0);
    if (result->allocated != (void *)0) return 6;
    C2GO_MLIB_TEST_FORCE_GC();

    result->allocated = C2GO_MLIB_TEST_GETCWD((void *)0, 1);
    if (!result->allocated ||
        !c2go_mlib_getcwd_equal(result->allocated,
                                result->caller_buffer)) return 7;
    C2GO_MLIB_TEST_FORCE_GC();

    /* Managed release severs every final owner; GC owns the allocations. */
    result->allocated = (void *)0;
    result->caller_buffer = (void *)0;
    caller_result = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_GETCWD
