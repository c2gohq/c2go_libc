/* SPDX-License-Identifier: AGPL-3.0-only */

#ifndef C2GO_MLIB_TEST_SEM
#error "C2GO_MLIB_TEST_SEM must name the managed semaphore type"
#endif
#ifndef C2GO_MLIB_TEST_MUTEX
#error "C2GO_MLIB_TEST_MUTEX must name the managed mutex type"
#endif
#ifndef C2GO_MLIB_TEST_COND
#error "C2GO_MLIB_TEST_COND must name the managed condition type"
#endif
#ifndef C2GO_MLIB_TEST_RWLOCK
#error "C2GO_MLIB_TEST_RWLOCK must name the managed rwlock type"
#endif
#ifndef C2GO_MLIB_TEST_EXPORT
#error "C2GO_MLIB_TEST_EXPORT must name the retirement selftest"
#endif
#ifndef C2GO_MLIB_TEST_FORCE_GC
#error "C2GO_MLIB_TEST_FORCE_GC must trigger a Go collection"
#endif

#include <errno.h>

struct c2go_mlib_sync_retire_owner {
    C2GO_MLIB_TEST_SEM *managed sem;
    C2GO_MLIB_TEST_MUTEX *managed mutex;
    C2GO_MLIB_TEST_COND *managed cond;
    C2GO_MLIB_TEST_RWLOCK *managed rwlock;
};

/* Publishing the owner makes this a real heap-retention test rather than a
 * stack-promotion test. The final store of NULL also verifies retirement of a
 * propagated managed root. */
static struct c2go_mlib_sync_retire_owner *managed
    c2go_mlib_sync_retire_root;

/* Every destroy operation retires the state inside its carrier. The carrier
 * itself remains caller-owned, so a typed heap owner must then clear that last
 * pointer through a managed write barrier. */
c2go_extern int C2GO_MLIB_TEST_EXPORT(void)
{
    struct c2go_mlib_sync_retire_owner *managed owner =
        (struct c2go_mlib_sync_retire_owner *managed)gc_malloc(
            c2go_typeinfo(struct c2go_mlib_sync_retire_owner), sizeof(*owner));
    struct timespec expired = {0, 0};

    if (!owner) return 1;
    c2go_mlib_sync_retire_root = owner;

    owner->sem = (C2GO_MLIB_TEST_SEM *managed)gc_malloc(
        c2go_typeinfo(C2GO_MLIB_TEST_SEM), sizeof(*owner->sem));
    if (!owner->sem) return 2;
    if (C2GO_MLIB_TEST_SEM_INIT(
            (C2GO_MLIB_TEST_SEM *)owner->sem, 0, 1) != 0) return 3;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!owner->sem->_state) return 4;
    if (C2GO_MLIB_TEST_SEM_DESTROY(
            (C2GO_MLIB_TEST_SEM *)owner->sem) != 0) return 5;
    if (owner->sem->_state) return 6;
    owner->sem = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->sem) return 7;

    owner->mutex = (C2GO_MLIB_TEST_MUTEX *managed)gc_malloc(
        c2go_typeinfo(C2GO_MLIB_TEST_MUTEX), sizeof(*owner->mutex));
    owner->cond = (C2GO_MLIB_TEST_COND *managed)gc_malloc(
        c2go_typeinfo(C2GO_MLIB_TEST_COND), sizeof(*owner->cond));
    if (!owner->mutex || !owner->cond) return 8;
    if (C2GO_MLIB_TEST_MUTEX_INIT(
            (C2GO_MLIB_TEST_MUTEX *)owner->mutex, (void *)0) != 0) return 9;
    if (C2GO_MLIB_TEST_COND_INIT(
            (C2GO_MLIB_TEST_COND *)owner->cond, (void *)0) != 0) return 10;
    if (C2GO_MLIB_TEST_MUTEX_LOCK(
            (C2GO_MLIB_TEST_MUTEX *)owner->mutex) != 0) return 11;
    if (C2GO_MLIB_TEST_COND_TIMEDWAIT(
            (C2GO_MLIB_TEST_COND *)owner->cond,
            (C2GO_MLIB_TEST_MUTEX *)owner->mutex,
            &expired) != ETIMEDOUT) return 12;
    if (C2GO_MLIB_TEST_MUTEX_UNLOCK(
            (C2GO_MLIB_TEST_MUTEX *)owner->mutex) != 0) return 13;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!owner->mutex->_state || !owner->cond->_state) return 14;
    if (C2GO_MLIB_TEST_COND_DESTROY(
            (C2GO_MLIB_TEST_COND *)owner->cond) != 0) return 15;
    if (C2GO_MLIB_TEST_MUTEX_DESTROY(
            (C2GO_MLIB_TEST_MUTEX *)owner->mutex) != 0) return 16;
    if (owner->cond->_state || owner->mutex->_state) return 17;
    owner->cond = (void *)0;
    owner->mutex = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->cond || owner->mutex) return 18;

    owner->rwlock = (C2GO_MLIB_TEST_RWLOCK *managed)gc_malloc(
        c2go_typeinfo(C2GO_MLIB_TEST_RWLOCK), sizeof(*owner->rwlock));
    if (!owner->rwlock) return 19;
    if (C2GO_MLIB_TEST_RWLOCK_INIT(
            (C2GO_MLIB_TEST_RWLOCK *)owner->rwlock,
            (void *)0) != 0) return 20;
    if (C2GO_MLIB_TEST_RWLOCK_RDLOCK(
            (C2GO_MLIB_TEST_RWLOCK *)owner->rwlock) != 0) return 21;
    if (C2GO_MLIB_TEST_RWLOCK_UNLOCK(
            (C2GO_MLIB_TEST_RWLOCK *)owner->rwlock) != 0) return 22;
    C2GO_MLIB_TEST_FORCE_GC();
    if (!owner->rwlock->_state) return 23;
    if (C2GO_MLIB_TEST_RWLOCK_DESTROY(
            (C2GO_MLIB_TEST_RWLOCK *)owner->rwlock) != 0) return 24;
    if (owner->rwlock->_state) return 25;
    owner->rwlock = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (owner->rwlock) return 26;

    c2go_mlib_sync_retire_root = (void *)0;
    owner = (void *)0;
    C2GO_MLIB_TEST_FORCE_GC();
    if (c2go_mlib_sync_retire_root) return 27;
    return 0;
}

#undef C2GO_MLIB_TEST_FORCE_GC
#undef C2GO_MLIB_TEST_EXPORT
#undef C2GO_MLIB_TEST_RWLOCK_DESTROY
#undef C2GO_MLIB_TEST_RWLOCK_UNLOCK
#undef C2GO_MLIB_TEST_RWLOCK_RDLOCK
#undef C2GO_MLIB_TEST_RWLOCK_INIT
#undef C2GO_MLIB_TEST_COND_DESTROY
#undef C2GO_MLIB_TEST_COND_TIMEDWAIT
#undef C2GO_MLIB_TEST_COND_INIT
#undef C2GO_MLIB_TEST_MUTEX_DESTROY
#undef C2GO_MLIB_TEST_MUTEX_UNLOCK
#undef C2GO_MLIB_TEST_MUTEX_LOCK
#undef C2GO_MLIB_TEST_MUTEX_INIT
#undef C2GO_MLIB_TEST_SEM_DESTROY
#undef C2GO_MLIB_TEST_SEM_INIT
#undef C2GO_MLIB_TEST_RWLOCK
#undef C2GO_MLIB_TEST_COND
#undef C2GO_MLIB_TEST_MUTEX
#undef C2GO_MLIB_TEST_SEM
