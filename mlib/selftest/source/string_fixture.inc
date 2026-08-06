/* SPDX-License-Identifier: AGPL-3.0-only */

/* Shared managed-string fixture. The including TU supplies the namespace and
 * ForceGC hook so the same source validates prefixed and replacement modes. */

#ifndef C2GO_MLIB_TEST_STRDUP
#error "C2GO_MLIB_TEST_STRDUP is required"
#endif
#ifndef C2GO_MLIB_TEST_STRNDUP
#error "C2GO_MLIB_TEST_STRNDUP is required"
#endif
#ifndef C2GO_MLIB_TEST_WCSDUP
#error "C2GO_MLIB_TEST_WCSDUP is required"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT is required"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC is required"
#endif

c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    char *managed narrow = C2GO_MLIB_TEST_STRDUP("managed string");
    char *managed bounded = C2GO_MLIB_TEST_STRNDUP("bounded", 5);
    wchar_t *managed wide = C2GO_MLIB_TEST_WCSDUP(L"wide");

    if (!narrow || !bounded || !wide) return 1;
    C2GO_MLIB_TEST_FORCE_GC();
    if (narrow[0] != 'm' || narrow[7] != ' ' || narrow[14] != 0) return 2;
    if (bounded[0] != 'b' || bounded[4] != 'd' || bounded[5] != 0) return 3;
    if (wide[0] != L'w' || wide[3] != L'e' || wide[4] != 0) return 4;

    /* Managed release means severing roots, not passing GC storage to free. */
    narrow = (void *)0;
    bounded = (void *)0;
    wide = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}
