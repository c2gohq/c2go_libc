/* SPDX-License-Identifier: AGPL-3.0-only */

#include <c2go/mlib/pthread.h>
#include <pthread.h> /* managed and unmanaged synchronization APIs coexist */
#include <errno.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

static mlib_pthread_mutex_t prefixed_global_mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
static mlib_pthread_key_t prefixed_thread_key;
static int prefixed_thread_error;
static int prefixed_destructor_count;

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/namespaced.ForceGC", C2GO_GOABI0)
void c2go_mlib_test_gc(void);

static void prefixed_key_destructor(void *managed value)
{
    if (!value || *(int *)value != 137) prefixed_thread_error = 4;
    ++prefixed_destructor_count;
}

static void *managed prefixed_thread_start(void *managed value)
{
    mlib_pthread_t self = mlib_pthread_self();
    if (!self || !mlib_pthread_equal(self, self)) prefixed_thread_error = 1;
    if (mlib_pthread_setspecific(prefixed_thread_key, value) != 0)
        prefixed_thread_error = 2;
    c2go_mlib_test_gc();
    if (mlib_pthread_getspecific(prefixed_thread_key) != value)
        prefixed_thread_error = 3;
    mlib_pthread_yield();
    return value;
}

static void *managed prefixed_thread_exit(void *managed value)
{
    mlib_pthread_exit(value);
    return 0;
}

static int exercise_prefixed_thread(void)
{
    mlib_pthread_attr_t attr;
    mlib_pthread_t thread = 0;
    void *managed result = 0;
    void *managed exit_result = 0;
    int *managed value;

    prefixed_thread_error = 0;
    prefixed_destructor_count = 0;
    if (mlib_pthread_attr_init(&attr) != 0) return 1;
    if (mlib_pthread_attr_setstacksize(&attr, 64 * 1024) != 0) return 2;
    if (mlib_pthread_key_create(&prefixed_thread_key,
                                prefixed_key_destructor) != 0) return 3;
    value = (int *managed)gc_malloc((void *)0, sizeof(*value));
    if (!value) return 4;
    *value = 137;
    if (mlib_pthread_create(&thread, &attr, prefixed_thread_start, value) != 0)
        return 5;
    value = 0;
    c2go_mlib_test_gc();
    if (mlib_pthread_join(thread, &result) != 0) return 6;
    if (!result || *(int *)result != 137 || prefixed_thread_error != 0)
        return 7;
    if (prefixed_destructor_count != 1) return 8;
    if (mlib_pthread_key_delete(prefixed_thread_key) != 0) return 9;
    prefixed_thread_key = 0;

    thread = 0;
    if (mlib_pthread_create(&thread, &attr, prefixed_thread_exit, result) != 0)
        return 10;
    result = 0;
    c2go_mlib_test_gc();
    if (mlib_pthread_join(thread, &exit_result) != 0) return 11;
    if (!exit_result || *(int *)exit_result != 137) return 12;

    thread = 0;
    if (mlib_pthread_create(&thread, &attr, prefixed_thread_exit,
                            exit_result) != 0) return 13;
    if (mlib_pthread_detach(thread) != 0) return 14;
    thread = 0;
    exit_result = 0;
    c2go_mlib_test_gc();
    if (mlib_pthread_attr_destroy(&attr) != 0) return 15;
    return 0;
}

static int exercise_prefixed_mutex(mlib_pthread_mutex_t *mutex)
{
    struct timespec expired = {0, 0};
    if (mlib_pthread_mutex_init(mutex, 0) != 0) return 1;
    if (mlib_pthread_mutex_lock(mutex) != 0) return 2;
    c2go_mlib_test_gc();
    if (mlib_pthread_mutex_trylock(mutex) != EBUSY) return 3;
    if (mlib_pthread_mutex_timedlock(mutex, &expired) != ETIMEDOUT) return 4;
    if (mlib_pthread_mutex_unlock(mutex) != 0) return 5;
    if (mlib_pthread_mutex_destroy(mutex) != 0) return 6;
    return 0;
}

static int exercise_prefixed_recursive_mutex(void)
{
    mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
    mlib_pthread_mutexattr_t attr;
    int kind = -1;
    if (mlib_pthread_mutexattr_init(&attr) != 0) return 1;
    if (mlib_pthread_mutexattr_settype(&attr, MLIB_PTHREAD_MUTEX_RECURSIVE) != 0) return 2;
    if (mlib_pthread_mutexattr_gettype(&attr, &kind) != 0 ||
        kind != MLIB_PTHREAD_MUTEX_RECURSIVE) return 3;
    if (mlib_pthread_mutex_init(&mutex, &attr) != 0) return 4;
    if (mlib_pthread_mutex_lock(&mutex) != 0) return 5;
    if (mlib_pthread_mutex_lock(&mutex) != 0) return 6;
    c2go_mlib_test_gc();
    if (mlib_pthread_mutex_unlock(&mutex) != 0) return 7;
    if (mlib_pthread_mutex_unlock(&mutex) != 0) return 8;
    if (mlib_pthread_mutex_destroy(&mutex) != 0) return 9;
    if (mlib_pthread_mutexattr_destroy(&attr) != 0) return 10;
    return 0;
}

static int exercise_prefixed_cond(void)
{
    mlib_pthread_mutex_t mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
    mlib_pthread_cond_t cond = MLIB_PTHREAD_COND_INITIALIZER;
    mlib_pthread_condattr_t attr;
    struct timespec expired = {0, 0};
    if (mlib_pthread_condattr_init(&attr) != 0) return 1;
    if (mlib_pthread_mutex_init(&mutex, 0) != 0) return 2;
    if (mlib_pthread_cond_init(&cond, &attr) != 0) return 3;
    if (mlib_pthread_mutex_lock(&mutex) != 0) return 4;
    c2go_mlib_test_gc();
    if (mlib_pthread_cond_timedwait(&cond, &mutex, &expired) != ETIMEDOUT) return 5;
    c2go_mlib_test_gc(); /* cond now owns a direct state pointer too */
    if (mlib_pthread_mutex_unlock(&mutex) != 0) return 6;
    if (mlib_pthread_cond_signal(&cond) != 0) return 7;
    if (mlib_pthread_cond_broadcast(&cond) != 0) return 8;
    if (mlib_pthread_cond_destroy(&cond) != 0) return 9;
    if (mlib_pthread_mutex_destroy(&mutex) != 0) return 10;
    if (mlib_pthread_condattr_destroy(&attr) != 0) return 11;
    return 0;
}

static int exercise_prefixed_rwlock(void)
{
    mlib_pthread_rwlock_t rwlock = MLIB_PTHREAD_RWLOCK_INITIALIZER;
    mlib_pthread_rwlockattr_t attr;
    if (mlib_pthread_rwlockattr_init(&attr) != 0) return 1;
    if (mlib_pthread_rwlock_init(&rwlock, &attr) != 0) return 2;
    if (mlib_pthread_rwlock_rdlock(&rwlock) != 0) return 3;
    c2go_mlib_test_gc();
    if (mlib_pthread_rwlock_trywrlock(&rwlock) != EBUSY) return 4;
    if (mlib_pthread_rwlock_unlock(&rwlock) != 0) return 5;
    if (mlib_pthread_rwlock_wrlock(&rwlock) != 0) return 6;
    if (mlib_pthread_rwlock_tryrdlock(&rwlock) != EBUSY) return 7;
    if (mlib_pthread_rwlock_unlock(&rwlock) != 0) return 8;
    if (mlib_pthread_rwlock_destroy(&rwlock) != 0) return 9;
    if (mlib_pthread_rwlockattr_destroy(&attr) != 0) return 10;
    return 0;
}

c2go_extern int mlib_pthread_prefixed_selftest(void)
{
    mlib_pthread_mutex_t local_mutex = MLIB_PTHREAD_MUTEX_INITIALIZER;
    mlib_pthread_mutex_t *heap_mutex;
    pthread_mutex_t unmanaged_mutex = PTHREAD_MUTEX_INITIALIZER;
    int rc;

    rc = exercise_prefixed_mutex(&local_mutex);
    if (rc != 0) return 10 + rc;
    rc = exercise_prefixed_mutex(&prefixed_global_mutex);
    if (rc != 0) return 20 + rc;

    heap_mutex = gc_malloc(c2go_typeinfo(mlib_pthread_mutex_t), sizeof(*heap_mutex));
    if (!heap_mutex) return 30;
    rc = exercise_prefixed_mutex(heap_mutex);
    if (rc != 0) return 30 + rc;

    rc = exercise_prefixed_recursive_mutex();
    if (rc != 0) return 50 + rc;
    rc = exercise_prefixed_cond();
    if (rc != 0) return 70 + rc;
    rc = exercise_prefixed_rwlock();
    if (rc != 0) return 90 + rc;
    rc = exercise_prefixed_thread();
    if (rc != 0) return 110 + rc;

    if (pthread_mutex_lock(&unmanaged_mutex) != 0) return 131;
    if (pthread_mutex_unlock(&unmanaged_mutex) != 0) return 132;
    if (pthread_mutex_destroy(&unmanaged_mutex) != 0) return 133;
    return 0;
}

#pragma c2go pop
