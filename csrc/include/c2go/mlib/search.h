/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_SEARCH_H
#define C2GO_MLIB_SEARCH_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Keep lsearch/lfind from the root header: their byte-array API erases the
 * element type and therefore cannot safely copy pointer-bearing elements.
 * Managed tree, hash, and queue containers have known pointer layouts and are
 * replaced below. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _SEARCH_H
#error "c2go mlib search replacement must be included before <search.h>"
#endif
#define C2GO_SEARCH_OMIT_HASH 1
#define C2GO_SEARCH_OMIT_TREE 1
#define C2GO_SEARCH_OMIT_QUEUE 1
#include <search.h>
#undef C2GO_SEARCH_OMIT_HASH
#undef C2GO_SEARCH_OMIT_TREE
#undef C2GO_SEARCH_OMIT_QUEUE
#else
#include <search.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* Unlike root <search.h>, these records retain keys, values, tree nodes, and
 * queue links as GC-visible pointers. Stored objects must themselves live in
 * managed storage. The implementation allocates internal nodes and hash
 * arrays with typed gc_malloc and releases them by clearing roots, never by
 * calling malloc/realloc/free. */
typedef char *managed C2GO_MLIB_NAME(search_key_t);
typedef void *managed C2GO_MLIB_NAME(search_value_t);

typedef struct C2GO_MLIB_NAME(entry) {
    C2GO_MLIB_NAME(search_key_t) key;
    C2GO_MLIB_NAME(search_value_t) data;
} C2GO_MLIB_NAME(ENTRY);

struct mlib_hsearch_table;
struct C2GO_MLIB_NAME(hsearch_data) {
    struct mlib_hsearch_table *managed __tab;
    unsigned int __unused1;
    unsigned int __unused2;
};

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hcreate", C2GO_GOABI0)
int C2GO_MLIB_NAME(hcreate)(size_t);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hdestroy", C2GO_GOABI0)
void C2GO_MLIB_NAME(hdestroy)(void);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hsearch", C2GO_GOABI0)
C2GO_MLIB_NAME(ENTRY) *managed C2GO_MLIB_NAME(hsearch)(
    C2GO_MLIB_NAME(ENTRY), ACTION);

#ifdef _GNU_SOURCE
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hcreate_r", C2GO_GOABI0)
int C2GO_MLIB_NAME(hcreate_r)(size_t,
    struct C2GO_MLIB_NAME(hsearch_data) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hdestroy_r", C2GO_GOABI0)
void C2GO_MLIB_NAME(hdestroy_r)(struct C2GO_MLIB_NAME(hsearch_data) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_hsearch_r", C2GO_GOABI0)
int C2GO_MLIB_NAME(hsearch_r)(C2GO_MLIB_NAME(ENTRY), ACTION,
    C2GO_MLIB_NAME(ENTRY) *managed *,
    struct C2GO_MLIB_NAME(hsearch_data) *);
#endif

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_tdelete", C2GO_GOABI0)
void *managed C2GO_MLIB_NAME(tdelete)(const void *managed,
    void *managed *, int (*)(const void *managed, const void *managed));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_tfind", C2GO_GOABI0)
void *managed C2GO_MLIB_NAME(tfind)(const void *managed,
    void *managed const *, int (*)(const void *managed, const void *managed));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_tsearch", C2GO_GOABI0)
void *managed C2GO_MLIB_NAME(tsearch)(const void *managed,
    void *managed *, int (*)(const void *managed, const void *managed));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_twalk", C2GO_GOABI0)
void C2GO_MLIB_NAME(twalk)(const void *managed,
    void (*)(const void *managed, VISIT, int));

/* Comparator, walk, and destroy callbacks are synchronous c2go internal-ABI
 * callbacks. tdelete/tdestroy unlink and clear managed roots; detached nodes
 * are reclaimed by the GC and must not be passed to free(). tdestroy receives
 * the root by value, so the caller must assign its root variable NULL after
 * destruction. */

#ifdef _GNU_SOURCE
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_tdestroy", C2GO_GOABI0)
void C2GO_MLIB_NAME(tdestroy)(void *managed,
    void (*)(void *managed));

struct C2GO_MLIB_NAME(qelem) {
    struct C2GO_MLIB_NAME(qelem) *managed q_forw;
    struct C2GO_MLIB_NAME(qelem) *managed q_back;
    char q_data[1];
};
#endif

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_insque", C2GO_GOABI0)
void C2GO_MLIB_NAME(insque)(void *managed, void *managed);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_remque", C2GO_GOABI0)
void C2GO_MLIB_NAME(remque)(void *managed);

/* lsearch/lfind deliberately remain the root declarations included above.
 * Their size-based byte-copy API cannot describe a pointer bitmap, so it is
 * safe only for pointer-free elements and has no managed replacement. */

#pragma c2go pop

#endif /* C2GO_MLIB_SEARCH_H */
