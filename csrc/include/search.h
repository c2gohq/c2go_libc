/* search.h — POSIX search tables/trees/lists (musl include/search.h).
 * Impl source/{tsearch,tfind,tdelete,twalk,tdestroy,hsearch,lsearch,insque}.c
 * (#668).
 *
 * CONTRACT (#668, same model decision as qsort's #650 — documented,
 * deliberately NO runtime rejection mechanism): every pointer HANDED TO this
 * family for STORAGE (tsearch/tdelete keys, hsearch ENTRY key/data, lsearch
 * elements, insque nodes) must point at STABLE UNMANAGED storage — libc-malloc
 * memory, C globals, or string literals. The containers themselves live in
 * noscan libc-malloc memory, so stored pointers are invisible to the GC:
 *   1. a pointer to a STACK object goes stale on the next copystack (the
 *      noscan container is not adjusted — dangling after growth);
 *   2. a pointer to a MANAGED Go object (gc_malloc/typeinfo) does not keep it
 *      alive or pinned — the object can be collected or moved while the
 *      container still references it.
 * This is a storage-duration contract (stronger than qsort's sort-duration
 * one): it must hold for the whole lifetime of the entry, not just the call.
 * lsearch additionally inherits qsort's memcpy caveat — the appended element
 * is an untyped byte copy, so elements must not hold managed pointers either.
 * musl itself has no such caveat only because it has no GC. */
#ifndef _SEARCH_H
#define _SEARCH_H

#include <c2go.h>

#define __NEED_size_t
#include <bits/alltypes.h>

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;

typedef struct entry {
	char *key;
	void *data;
} ENTRY;

int hcreate(size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc.hcreate", C2GO_GOABI0);
void hdestroy(void)
    c2go_linkname("github.com/c2gohq/c2go_libc.hdestroy", C2GO_GOABI0);
ENTRY *hsearch(ENTRY, ACTION)
    c2go_linkname("github.com/c2gohq/c2go_libc.hsearch", C2GO_GOABI0);

#ifdef _GNU_SOURCE
struct hsearch_data {
	struct __tab *__tab;
	unsigned int __unused1;
	unsigned int __unused2;
};

int hcreate_r(size_t, struct hsearch_data *)
    c2go_linkname("github.com/c2gohq/c2go_libc.hcreate_r", C2GO_GOABI0);
void hdestroy_r(struct hsearch_data *)
    c2go_linkname("github.com/c2gohq/c2go_libc.hdestroy_r", C2GO_GOABI0);
int hsearch_r(ENTRY, ACTION, ENTRY **, struct hsearch_data *)
    c2go_linkname("github.com/c2gohq/c2go_libc.hsearch_r", C2GO_GOABI0);
#endif

void insque(void *, void *)
    c2go_linkname("github.com/c2gohq/c2go_libc.insque", C2GO_GOABI0);
void remque(void *)
    c2go_linkname("github.com/c2gohq/c2go_libc.remque", C2GO_GOABI0);

void *lsearch(const void *, void *, size_t *, size_t,
	int (*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.lsearch", C2GO_GOABI0);
void *lfind(const void *, const void *, size_t *, size_t,
	int (*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.lfind", C2GO_GOABI0);

void *tdelete(const void *__restrict, void **__restrict, int(*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.tdelete", C2GO_GOABI0);
void *tfind(const void *, void *const *, int(*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.tfind", C2GO_GOABI0);
void *tsearch(const void *, void **, int (*)(const void *, const void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.tsearch", C2GO_GOABI0);
void twalk(const void *, void (*)(const void *, VISIT, int))
    c2go_linkname("github.com/c2gohq/c2go_libc.twalk", C2GO_GOABI0);

#ifdef _GNU_SOURCE
struct qelem {
	struct qelem *q_forw, *q_back;
	char q_data[1];
};

void tdestroy(void *, void (*)(void *))
    c2go_linkname("github.com/c2gohq/c2go_libc.tdestroy", C2GO_GOABI0);
#endif

#endif /* _SEARCH_H */
