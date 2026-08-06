/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_FILE
#error "C2GO_MLIB_TEST_FILE must name the managed FILE type"
#endif
#ifndef C2GO_MLIB_TEST_FCLOSE
#error "C2GO_MLIB_TEST_FCLOSE must name the managed fclose"
#endif
#ifndef C2GO_MLIB_TEST_STDERR
#error "C2GO_MLIB_TEST_STDERR must name the managed standard error stream"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the retirement selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

struct c2go_mlib_stdio_retire_owner {
    C2GO_MLIB_TEST_FILE *managed closed;
    C2GO_MLIB_TEST_FILE *managed replacement;
};

/* A managed standard stream has a separate package-global root. fclose must
 * clear it so the next macro lookup creates a fresh live carrier. Keeping the
 * closed local alive across GC makes pointer inequality a reliable check. */
c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    struct c2go_mlib_stdio_retire_owner *owner = gc_malloc(
        c2go_typeinfo(struct c2go_mlib_stdio_retire_owner), sizeof(*owner));
    if (!owner) return 1;
    owner->closed = (C2GO_MLIB_TEST_FILE *managed)C2GO_MLIB_TEST_STDERR;
    if (!owner->closed) return 2;
    if (C2GO_MLIB_TEST_FCLOSE((C2GO_MLIB_TEST_FILE *)owner->closed) != 0)
        return 3;
    C2GO_MLIB_TEST_FORCE_GC();
    owner->replacement =
        (C2GO_MLIB_TEST_FILE *managed)C2GO_MLIB_TEST_STDERR;
    if (!owner->replacement || owner->replacement == owner->closed) return 4;
    if (C2GO_MLIB_TEST_FCLOSE(
            (C2GO_MLIB_TEST_FILE *)owner->replacement) != 0) return 5;

    /* fclose cannot rewrite a by-value FILE pointer. Retire both caller roots
     * explicitly even though the GC performs physical reclamation. */
    owner->closed = (void *)0;
    owner->replacement = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->closed || owner->replacement) return 6;
    owner = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_STDERR
#undef C2GO_MLIB_TEST_FCLOSE
#undef C2GO_MLIB_TEST_FILE
