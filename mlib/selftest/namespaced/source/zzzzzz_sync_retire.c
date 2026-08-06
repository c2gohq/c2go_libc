/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_sync_retire_test_gc(void);

#define C2GO_MLIB_TEST_SEM mlib_sem_t
#define C2GO_MLIB_TEST_MUTEX mlib_pthread_mutex_t
#define C2GO_MLIB_TEST_COND mlib_pthread_cond_t
#define C2GO_MLIB_TEST_RWLOCK mlib_pthread_rwlock_t
#define C2GO_MLIB_TEST_SEM_INIT mlib_sem_init
#define C2GO_MLIB_TEST_SEM_DESTROY mlib_sem_destroy
#define C2GO_MLIB_TEST_MUTEX_INIT mlib_pthread_mutex_init
#define C2GO_MLIB_TEST_MUTEX_LOCK mlib_pthread_mutex_lock
#define C2GO_MLIB_TEST_MUTEX_UNLOCK mlib_pthread_mutex_unlock
#define C2GO_MLIB_TEST_MUTEX_DESTROY mlib_pthread_mutex_destroy
#define C2GO_MLIB_TEST_COND_INIT mlib_pthread_cond_init
#define C2GO_MLIB_TEST_COND_TIMEDWAIT mlib_pthread_cond_timedwait
#define C2GO_MLIB_TEST_COND_DESTROY mlib_pthread_cond_destroy
#define C2GO_MLIB_TEST_RWLOCK_INIT mlib_pthread_rwlock_init
#define C2GO_MLIB_TEST_RWLOCK_RDLOCK mlib_pthread_rwlock_rdlock
#define C2GO_MLIB_TEST_RWLOCK_UNLOCK mlib_pthread_rwlock_unlock
#define C2GO_MLIB_TEST_RWLOCK_DESTROY mlib_pthread_rwlock_destroy
#define C2GO_MLIB_TEST_EXPORT mlib_sync_retire_prefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_sync_retire_test_gc()
#include "../../source/sync_retire_fixture.inc"

#pragma c2go pop
