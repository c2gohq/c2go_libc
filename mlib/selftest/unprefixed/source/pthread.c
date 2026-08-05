/* SPDX-License-Identifier: AGPL-3.0-only */

#define C2GO_MLIB_UNPREFIXED 1
#include <c2go/mlib/pthread.h>
#include <pthread.h> /* claimed guard retains pthread_once and pthread_atfork */
#include <errno.h>

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

static pthread_mutex_t unprefixed_global_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t unprefixed_once = PTHREAD_ONCE_INIT;
static int unprefixed_once_count;
static pthread_key_t unprefixed_thread_key;
static int unprefixed_thread_error;
static int unprefixed_destructor_count;

c2go_linkname("github.com/c2gohq/c2go_libc/mlib/selftest/unprefixed.ForceGC", C2GO_GOABI0)
void c2go_mlib_test_gc(void);

static void unprefixed_key_destructor(void *managed value)
{
    if (!value || *(int *)value != 137) unprefixed_thread_error = 4;
    ++unprefixed_destructor_count;
}

static void *managed unprefixed_thread_start(void *managed value)
{
    pthread_t self = pthread_self();
    if (!self || !pthread_equal(self, self)) unprefixed_thread_error = 1;
    if (pthread_setspecific(unprefixed_thread_key, value) != 0)
        unprefixed_thread_error = 2;
    c2go_mlib_test_gc();
    if (pthread_getspecific(unprefixed_thread_key) != value)
        unprefixed_thread_error = 3;
    pthread_yield();
    return value;
}

static void *managed unprefixed_thread_exit(void *managed value)
{
    pthread_exit(value);
    return 0;
}

static int exercise_unprefixed_thread(void)
{
    pthread_attr_t attr;
    pthread_t thread = 0;
    void *managed result = 0;
    void *managed exit_result = 0;
    int *managed value;

    unprefixed_thread_error = 0;
    unprefixed_destructor_count = 0;
    if (pthread_attr_init(&attr) != 0) return 1;
    if (pthread_attr_setstacksize(&attr, 64 * 1024) != 0) return 2;
    if (pthread_key_create(&unprefixed_thread_key,
                           unprefixed_key_destructor) != 0) return 3;
    value = (int *managed)gc_malloc((void *)0, sizeof(*value));
    if (!value) return 4;
    *value = 137;
    if (pthread_create(&thread, &attr, unprefixed_thread_start, value) != 0)
        return 5;
    value = 0;
    c2go_mlib_test_gc();
    if (pthread_join(thread, &result) != 0) return 6;
    if (!result || *(int *)result != 137 || unprefixed_thread_error != 0)
        return 7;
    if (unprefixed_destructor_count != 1) return 8;
    if (pthread_key_delete(unprefixed_thread_key) != 0) return 9;
    unprefixed_thread_key = 0;

    thread = 0;
    if (pthread_create(&thread, &attr, unprefixed_thread_exit, result) != 0)
        return 10;
    result = 0;
    c2go_mlib_test_gc();
    if (pthread_join(thread, &exit_result) != 0) return 11;
    if (!exit_result || *(int *)exit_result != 137) return 12;

    thread = 0;
    if (pthread_create(&thread, &attr, unprefixed_thread_exit,
                       exit_result) != 0) return 13;
    if (pthread_detach(thread) != 0) return 14;
    thread = 0;
    exit_result = 0;
    c2go_mlib_test_gc();
    if (pthread_attr_destroy(&attr) != 0) return 15;
    return 0;
}

static void unprefixed_once_init(void)
{
    unprefixed_once_count++;
}

static int exercise_unprefixed_mutex(pthread_mutex_t *mutex)
{
    struct timespec expired = {0, 0};
    if (pthread_mutex_init(mutex, 0) != 0) return 1;
    if (pthread_mutex_lock(mutex) != 0) return 2;
    c2go_mlib_test_gc();
    if (pthread_mutex_trylock(mutex) != EBUSY) return 3;
    if (pthread_mutex_timedlock(mutex, &expired) != ETIMEDOUT) return 4;
    if (pthread_mutex_unlock(mutex) != 0) return 5;
    if (pthread_mutex_destroy(mutex) != 0) return 6;
    return 0;
}

static int exercise_unprefixed_cond(void)
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    struct timespec expired = {0, 0};
    if (pthread_mutex_init(&mutex, 0) != 0) return 1;
    if (pthread_cond_init(&cond, 0) != 0) return 2;
    if (pthread_mutex_lock(&mutex) != 0) return 3;
    c2go_mlib_test_gc();
    if (pthread_cond_timedwait(&cond, &mutex, &expired) != ETIMEDOUT) return 4;
    c2go_mlib_test_gc(); /* cond now owns a direct state pointer too */
    if (pthread_mutex_unlock(&mutex) != 0) return 5;
    if (pthread_cond_destroy(&cond) != 0) return 6;
    if (pthread_mutex_destroy(&mutex) != 0) return 7;
    return 0;
}

static int exercise_unprefixed_rwlock(void)
{
    pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
    if (pthread_rwlock_init(&rwlock, 0) != 0) return 1;
    if (pthread_rwlock_rdlock(&rwlock) != 0) return 2;
    c2go_mlib_test_gc();
    if (pthread_rwlock_trywrlock(&rwlock) != EBUSY) return 3;
    if (pthread_rwlock_unlock(&rwlock) != 0) return 4;
    if (pthread_rwlock_wrlock(&rwlock) != 0) return 5;
    if (pthread_rwlock_tryrdlock(&rwlock) != EBUSY) return 6;
    if (pthread_rwlock_unlock(&rwlock) != 0) return 7;
    if (pthread_rwlock_destroy(&rwlock) != 0) return 8;
    return 0;
}

c2go_extern int mlib_pthread_unprefixed_selftest(void)
{
    pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *heap_mutex;
    pthread_t self;
    int rc;

    rc = exercise_unprefixed_mutex(&local_mutex);
    if (rc != 0) return 10 + rc;
    rc = exercise_unprefixed_mutex(&unprefixed_global_mutex);
    if (rc != 0) return 20 + rc;

    heap_mutex = gc_malloc(c2go_typeinfo(pthread_mutex_t), sizeof(*heap_mutex));
    if (!heap_mutex) return 30;
    rc = exercise_unprefixed_mutex(heap_mutex);
    if (rc != 0) return 30 + rc;

    rc = exercise_unprefixed_cond();
    if (rc != 0) return 50 + rc;
    rc = exercise_unprefixed_rwlock();
    if (rc != 0) return 70 + rc;

    rc = exercise_unprefixed_thread();
    if (rc != 0) return 90 + rc;

    /* The replacement header must not truncate the shared once surface. */
    self = pthread_self();
    if (!pthread_equal(self, self)) return 111;
    if (pthread_once(&unprefixed_once, unprefixed_once_init) != 0) return 112;
    if (pthread_once(&unprefixed_once, unprefixed_once_init) != 0) return 113;
    if (unprefixed_once_count != 1) return 114;
    return 0;
}

#pragma c2go pop
