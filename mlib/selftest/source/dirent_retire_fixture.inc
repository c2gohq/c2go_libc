/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_DIR
#error "C2GO_MLIB_TEST_DIR must name the managed DIR type"
#endif
#ifndef C2GO_MLIB_TEST_OPENDIR
#error "C2GO_MLIB_TEST_OPENDIR must name the managed opendir"
#endif
#ifndef C2GO_MLIB_TEST_CLOSEDIR
#error "C2GO_MLIB_TEST_CLOSEDIR must name the managed closedir"
#endif
#ifndef C2GO_MLIB_TEST_SCANDIR
#error "C2GO_MLIB_TEST_SCANDIR must name the managed scandir"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the retirement selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

struct c2go_mlib_dirent_retire_owner {
    C2GO_MLIB_TEST_DIR *managed stream;
    struct dirent **managed names;
};

/* Keep both public result kinds in an explicitly GC-visible heap owner. This
 * verifies the library clears DIR's internal state and that callers retire the
 * final by-value DIR/scandir owners through managed write barriers. */
c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    struct c2go_mlib_dirent_retire_owner *owner = gc_malloc(
        c2go_typeinfo(struct c2go_mlib_dirent_retire_owner), sizeof(*owner));
    int count;

    if (!owner) return 1;
    owner->stream = (C2GO_MLIB_TEST_DIR *managed)C2GO_MLIB_TEST_OPENDIR(".");
    if (!owner->stream) return 2;
    C2GO_MLIB_TEST_FORCE_GC();

    if (C2GO_MLIB_TEST_CLOSEDIR((C2GO_MLIB_TEST_DIR *)owner->stream) != 0)
        return 3;
    if (owner->stream->_state != (void *)0) return 4;
    owner->stream = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->stream) return 5;

    count = C2GO_MLIB_TEST_SCANDIR(
        ".", (struct dirent ***)&owner->names, (void *)0, (void *)0);
    if (count < 2 || !owner->names || !owner->names[0]) return 6;
    C2GO_MLIB_TEST_FORCE_GC();
    owner->names = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->names) return 7;

    owner = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_SCANDIR
#undef C2GO_MLIB_TEST_CLOSEDIR
#undef C2GO_MLIB_TEST_OPENDIR
#undef C2GO_MLIB_TEST_DIR
