/* SPDX-License-Identifier: AGPL-3.0-only */

/* Shared managed-regex fixture. The including TU supplies the namespace and
 * ForceGC hook so the same source validates prefixed and replacement modes. */

#ifndef C2GO_MLIB_TEST_REGEX_T
#error "C2GO_MLIB_TEST_REGEX_T is required"
#endif
#ifndef C2GO_MLIB_TEST_REGCOMP
#error "C2GO_MLIB_TEST_REGCOMP is required"
#endif
#ifndef C2GO_MLIB_TEST_REGEXEC
#error "C2GO_MLIB_TEST_REGEXEC is required"
#endif
#ifndef C2GO_MLIB_TEST_REGFREE
#error "C2GO_MLIB_TEST_REGFREE is required"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT is required"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC is required"
#endif

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    C2GO_MLIB_TEST_REGEX_T expression = {0};
    regmatch_t matches[3];
    int iteration;
    int result;

    result = C2GO_MLIB_TEST_REGCOMP(&expression, "^(a+)(b)$", REG_EXTENDED);
    if (result != REG_OK) return 10 + result;
    if (!expression.__opaque || !expression.__c2go_arena) return 30;
    if (expression.re_nsub != 2) return 31;

    C2GO_MLIB_TEST_FORCE_GC();
    result = C2GO_MLIB_TEST_REGEXEC(&expression, "aaab", 3, matches, 0);
    if (result != REG_OK) return 40 + result;
    if (matches[0].rm_so != 0 || matches[0].rm_eo != 4) return 50;
    if (matches[1].rm_so != 0 || matches[1].rm_eo != 3) return 51;
    if (matches[2].rm_so != 3 || matches[2].rm_eo != 4) return 52;

    /* Repeated matching creates and drops an independent temporary arena on
     * every call. Forced GC proves the compiled arena remains rooted solely by
     * the managed regex carrier. */
    for (iteration = 0; iteration < 16; ++iteration) {
        C2GO_MLIB_TEST_FORCE_GC();
        if (C2GO_MLIB_TEST_REGEXEC(&expression, "ab", 0, (void *)0, 0)
            != REG_OK)
            return 60;
    }

    C2GO_MLIB_TEST_REGFREE(&expression);
    if (expression.__opaque || expression.__c2go_arena || expression.re_nsub)
        return 70;
    C2GO_MLIB_TEST_FORCE_GC();

    /* A failed compile must not publish a half-built arena. */
    result = C2GO_MLIB_TEST_REGCOMP(&expression, "[", REG_EXTENDED);
    if (result == REG_OK) return 80;
    if (expression.__opaque || expression.__c2go_arena || expression.re_nsub)
        return 81;
    return 0;
}
