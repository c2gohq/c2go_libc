/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_UNISTD_H
#define C2GO_MLIB_UNISTD_H

#include <c2go.h>
#include <c2go/mlib/names.h>
#include <unistd.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* A null buffer selects a GC-owned no-pointer result. Never pass that result
 * to free(); assign its final managed owner NULL after use. A non-null buffer
 * must also be managed no-pointer storage, not a C stack array or root malloc
 * allocation, so the conditional return has one precise pointer type. Root
 * getcwd remains available for ordinary stack buffers. */
char *managed mlib_getcwd(char *managed, size_t)
    c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_getcwd", C2GO_GOABI0);

/* Replacement mode changes only getcwd; every other <unistd.h> interface
 * continues to use root libc. */
#ifdef C2GO_MLIB_UNPREFIXED
#define getcwd mlib_getcwd
#endif

#pragma c2go pop

#endif /* C2GO_MLIB_UNISTD_H */
