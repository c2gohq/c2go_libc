/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>
#include <semaphore.h> /* claimed guard keeps the unmanaged ABI out */
#include <errno.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

static sem_t unprefixed_global_sem;

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_test_gc(void);

static int exercise_unprefixed_sem(sem_t *sem)
{
    int value = -1;
    struct timespec expired = {0, 0};
    if (sem_init(sem, 0, 1) != 0) return 1;
    c2go_mlib_test_gc();
    if (sem_wait(sem) != 0) return 2;
    if (sem_timedwait(sem, &expired) != ETIMEDOUT) return 3;
    if (sem_trywait(sem) != EAGAIN) return 4;
    if (sem_post(sem) != 0) return 5;
    if (sem_getvalue(sem, &value) != 0 || value != 1) return 6;
    if (sem_destroy(sem) != 0) return 7;
    return 0;
}

c2go_extern int mlib_sem_unprefixed_selftest(void)
{
    sem_t local_sem = {0};
    int rc = exercise_unprefixed_sem(&local_sem);
    if (rc != 0) return 10 + rc;

    rc = exercise_unprefixed_sem(&unprefixed_global_sem);
    if (rc != 0) return 20 + rc;
    return 0;
}

#pragma c2go pop
