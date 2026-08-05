/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_SEMAPHORE_H
#define C2GO_MLIB_SEMAPHORE_H

#include <c2go.h>
#include <time.h>
#include <c2go/mlib/names.h>

/* In replacement mode, managed and unmanaged sem_t cannot coexist in one
 * translation unit. Claim the standard header guard so a later
 * <semaphore.h> include cannot silently replace this ABI; fail explicitly if
 * the unmanaged header was included first. Namespaced mode may coexist with
 * <semaphore.h> and therefore does not touch its guard. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _SEMAPHORE_H
#error "c2go mlib semaphore replacement must be included before <semaphore.h>"
#endif
#define _SEMAPHORE_H
#endif

/* The carrier contains the direct Go-heap state pointer. Its explicit managed
 * type preserves AS1 provenance; C2GO_RECORD makes every use of the carrier
 * participate in C2Go's stack/global/typeinfo GC metadata. */
#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

typedef struct {
    void *managed _state;
} C2GO_MLIB_NAME(sem_t);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_init)(C2GO_MLIB_NAME(sem_t) *, int, unsigned);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_destroy)(C2GO_MLIB_NAME(sem_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemWait", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_wait)(C2GO_MLIB_NAME(sem_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemTrywait", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_trywait)(C2GO_MLIB_NAME(sem_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemTimedwait", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_timedwait)(C2GO_MLIB_NAME(sem_t) *, const struct timespec *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemPost", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_post)(C2GO_MLIB_NAME(sem_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.SemGetvalue", C2GO_GOABI0)
int C2GO_MLIB_NAME(sem_getvalue)(C2GO_MLIB_NAME(sem_t) *, int *);

#pragma c2go pop

#endif /* C2GO_MLIB_SEMAPHORE_H */
