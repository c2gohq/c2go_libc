/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/semaphore.h>
#include <semaphore.h> /* namespaced mlib and unmanaged libc may coexist */
#include <errno.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

static mlib_sem_t prefixed_global_sem;

/* ForceGC is an ordinary Go function in this test package. Calling it here
 * validates that the surrounding C frame/global/gc_malloc object carries the
 * managed semaphore state through an actual collection. */
c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_test_gc(void);

static int exercise_prefixed_sem(mlib_sem_t *sem)
{
    int value = -1;
    struct timespec expired = {0, 0};
    if (mlib_sem_init(sem, 0, 1) != 0) return 1;
    c2go_mlib_test_gc();
    if (mlib_sem_wait(sem) != 0) return 2;
    if (mlib_sem_timedwait(sem, &expired) != ETIMEDOUT) return 3;
    if (mlib_sem_trywait(sem) != EAGAIN) return 4;
    if (mlib_sem_post(sem) != 0) return 5;
    if (mlib_sem_getvalue(sem, &value) != 0 || value != 1) return 6;
    if (mlib_sem_destroy(sem) != 0) return 7;
    return 0;
}

c2go_extern int mlib_sem_prefixed_selftest(void)
{
    mlib_sem_t local_sem = {0};
    mlib_sem_t *heap_sem;
    int rc;

    rc = exercise_prefixed_sem(&local_sem);
    if (rc != 0) return 10 + rc;

    rc = exercise_prefixed_sem(&prefixed_global_sem);
    if (rc != 0) return 20 + rc;

    heap_sem = gc_malloc(c2go_typeinfo(mlib_sem_t), sizeof(*heap_sem));
    if (!heap_sem) return 30;
    rc = exercise_prefixed_sem(heap_sem);
    if (rc != 0) return 30 + rc;

    return 0;
}

#pragma c2go pop
