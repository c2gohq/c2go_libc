/* semaphore.h — POSIX unnamed semaphores, backed by a Go channel-based state
 * named by a 64-bit `_id` into the handle table (same _id/handle-table shape as
 * <pthread.h>; the sem_t itself holds no managed pointer). */
#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <c2go.h>
#include <time.h>   /* struct timespec (sem_timedwait) */

typedef struct {                /* _id -> Go-side semaphore state (handle.go) */
    size_t        _id;          /* 0 = uninitialized; 64-bit on every target */
    unsigned long _pad[3];
} sem_t;

c2go_linkname("github.com/c2gohq/c2go_libc.SemInit", C2GO_GOABI0)
int sem_init(sem_t *, int, unsigned);
c2go_linkname("github.com/c2gohq/c2go_libc.SemDestroy", C2GO_GOABI0)
int sem_destroy(sem_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.SemWait", C2GO_GOABI0)
int sem_wait(sem_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.SemTrywait", C2GO_GOABI0)
int sem_trywait(sem_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.SemTimedwait", C2GO_GOABI0)
int sem_timedwait(sem_t *, const struct timespec *);
c2go_linkname("github.com/c2gohq/c2go_libc.SemPost", C2GO_GOABI0)
int sem_post(sem_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.SemGetvalue", C2GO_GOABI0)
int sem_getvalue(sem_t *, int *);

#endif /* _SEMAPHORE_H */
