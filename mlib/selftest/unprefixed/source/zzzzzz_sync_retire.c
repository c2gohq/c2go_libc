/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/semaphore.h>
#include <c2go/mlib/pthread.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_sync_retire_test_gc(void);

#define C2GO_MLIB_TEST_SEM sem_t
#define C2GO_MLIB_TEST_MUTEX pthread_mutex_t
#define C2GO_MLIB_TEST_COND pthread_cond_t
#define C2GO_MLIB_TEST_RWLOCK pthread_rwlock_t
#define C2GO_MLIB_TEST_SEM_INIT sem_init
#define C2GO_MLIB_TEST_SEM_DESTROY sem_destroy
#define C2GO_MLIB_TEST_MUTEX_INIT pthread_mutex_init
#define C2GO_MLIB_TEST_MUTEX_LOCK pthread_mutex_lock
#define C2GO_MLIB_TEST_MUTEX_UNLOCK pthread_mutex_unlock
#define C2GO_MLIB_TEST_MUTEX_DESTROY pthread_mutex_destroy
#define C2GO_MLIB_TEST_COND_INIT pthread_cond_init
#define C2GO_MLIB_TEST_COND_TIMEDWAIT pthread_cond_timedwait
#define C2GO_MLIB_TEST_COND_DESTROY pthread_cond_destroy
#define C2GO_MLIB_TEST_RWLOCK_INIT pthread_rwlock_init
#define C2GO_MLIB_TEST_RWLOCK_RDLOCK pthread_rwlock_rdlock
#define C2GO_MLIB_TEST_RWLOCK_UNLOCK pthread_rwlock_unlock
#define C2GO_MLIB_TEST_RWLOCK_DESTROY pthread_rwlock_destroy
#define C2GO_MLIB_TEST_EXPORT mlib_sync_retire_unprefixed_selftest
#define C2GO_MLIB_TEST_FORCE_GC() c2go_mlib_sync_retire_test_gc()
#include "../../source/sync_retire_fixture.inc"

#pragma c2go pop
