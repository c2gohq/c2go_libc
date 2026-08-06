/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_DIRENT_H
#define C2GO_MLIB_DIRENT_H

#include <c2go.h>
#include <c2go/mlib/names.h>

/* Replacement mode needs struct dirent and DT_* from the ordinary header but
 * must not admit its handle-based DIR or stream functions. Namespaced mode may
 * expose both ABIs in one translation unit. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _DIRENT_H
#error "c2go mlib dirent replacement must be included before <dirent.h>"
#endif
#define C2GO_DIRENT_OMIT_STREAM 1
#include <dirent.h>
#undef C2GO_DIRENT_OMIT_STREAM
#else
#include <dirent.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* The managed stream contains its direct Go state pointer and the stable
 * entry buffer returned by readdir. Heap instances are created inside mlib's
 * opendir/fdopendir with typed gc_malloc; closedir clears the state pointer and
 * leaves object reclamation to the Go GC. Because closedir receives the handle
 * by value, callers must discard and preferably assign NULL to their DIR *
 * owner after a successful close. */
typedef struct {
    void *managed _state;
    /* Keep the embedded no-pointer record as bytes. Header-defined struct
     * dirent is intentionally opaque in the generated Go API; spelling this
     * field as struct dirent would therefore produce an 8-byte Go carrier and
     * an invalid runtime type descriptor for the real 288-byte allocation. */
    unsigned char _entry[sizeof(struct dirent)];
} C2GO_MLIB_NAME(DIR);

/* Both namespace modes route to the one prefixed implementation compiled into
 * package mlib. C2GO_MLIB_UNPREFIXED changes only the caller-facing C name and
 * carrier typedef; it does not build a second library instance. */
C2GO_MLIB_NAME(DIR) *C2GO_MLIB_NAME(opendir)(const char *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_opendir", C2GO_GOABI0);
struct dirent *C2GO_MLIB_NAME(readdir)(C2GO_MLIB_NAME(DIR) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_readdir", C2GO_GOABI0);
int C2GO_MLIB_NAME(readdir_r)(C2GO_MLIB_NAME(DIR) *__restrict,
    struct dirent *__restrict, struct dirent **__restrict)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_readdir_r", C2GO_GOABI0);
int C2GO_MLIB_NAME(closedir)(C2GO_MLIB_NAME(DIR) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_closedir", C2GO_GOABI0);
void C2GO_MLIB_NAME(rewinddir)(C2GO_MLIB_NAME(DIR) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_rewinddir", C2GO_GOABI0);

#if !defined(_WIN32)
int C2GO_MLIB_NAME(dirfd)(C2GO_MLIB_NAME(DIR) *)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_dirfd", C2GO_GOABI0);
C2GO_MLIB_NAME(DIR) *C2GO_MLIB_NAME(fdopendir)(int)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_fdopendir", C2GO_GOABI0);
#endif

/* The result graph returned by managed scandir is allocated with gc_malloc:
 * both the pointer array and every dirent are Go-GC-owned. Keep the returned
 * pointers only in managed storage and assign the last owning pointer NULL
 * when finished; never pass either level to free(). The selector and comparator
 * are ordinary c2go internal-ABI callbacks, as with the root libc scandir API. */
int C2GO_MLIB_NAME(scandir)(const char *, struct dirent ***,
    int (*)(const struct dirent *),
    int (*)(const struct dirent **, const struct dirent **))
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_scandir", C2GO_GOABI0);

/* These comparators are stateless, so mlib reuses root libc's implementations
 * rather than compiling managed duplicates. Wrap either in a local comparator
 * before passing it to scandir because the public declaration uses Go ABI0. */
int C2GO_MLIB_NAME(alphasort)(const struct dirent **,
    const struct dirent **)
    c2go_linkname("github.com/c2gohq/c2go_libc.alphasort", C2GO_GOABI0);
int C2GO_MLIB_NAME(versionsort)(const struct dirent **,
    const struct dirent **)
    c2go_linkname("github.com/c2gohq/c2go_libc.versionsort", C2GO_GOABI0);

#pragma c2go pop

#endif /* C2GO_MLIB_DIRENT_H */
