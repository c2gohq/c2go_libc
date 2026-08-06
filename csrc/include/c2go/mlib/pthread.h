/* SPDX-License-Identifier: AGPL-3.0-only
 * Also available under a separate commercial agreement. */
#ifndef C2GO_MLIB_PTHREAD_H
#define C2GO_MLIB_PTHREAD_H

#include <c2go.h>
#include <time.h>
#include <c2go/mlib/names.h>

/* Replacement mode keeps pthread_once/atfork but replaces thread lifecycle,
 * thread keys, and synchronization carriers. Namespaced mode keeps both ABIs. */
#ifdef C2GO_MLIB_UNPREFIXED
#ifdef _PTHREAD_H
#error "c2go mlib pthread replacement must be included before <pthread.h>"
#endif
#define C2GO_PTHREAD_OMIT_SYNC 1
#define C2GO_PTHREAD_OMIT_THREAD 1
#define C2GO_PTHREAD_OMIT_KEY 1
#include <pthread.h>
#undef C2GO_PTHREAD_OMIT_SYNC
#undef C2GO_PTHREAD_OMIT_THREAD
#undef C2GO_PTHREAD_OMIT_KEY
#else
#include <pthread.h>
#endif

#pragma c2go managed(C2GO_PTR | C2GO_RECORD) push

/* Thread and key descriptors are direct Go pointers. The thread argument and
 * result values below are managed too because they may live in Go state while
 * the new goroutine runs or until pthread_join publishes the result.
 * pthread_join, pthread_detach, and pthread_key_delete take descriptors by
 * value, so they cannot clear the caller's variable. After a successful call,
 * discard that value and preferably assign NULL to the managed descriptor. */
typedef void *managed C2GO_MLIB_NAME(pthread_t);

typedef struct {
    int detachstate;
    int _reserved;
    unsigned long _pad[6];
} C2GO_MLIB_NAME(pthread_attr_t);

typedef void *managed (*C2GO_MLIB_NAME(pthread_start_routine_t))(void *managed);

typedef void *managed C2GO_MLIB_NAME(pthread_key_t);
typedef void (*C2GO_MLIB_NAME(pthread_key_destructor_t))(void *managed);

/* A managed synchronization object is one GC-visible pointer. Explicit
 * managed pointer types retain AS1 provenance; the pointed-to Go state is
 * shared with root libc's algorithm, while root libc stores an ID in its
 * larger unmanaged carrier instead. */
typedef struct {
    void *managed _state;
} C2GO_MLIB_NAME(pthread_mutex_t);

typedef struct {
    int _type;
    int _pad[5];
} C2GO_MLIB_NAME(pthread_mutexattr_t);

typedef struct {
    void *managed _state;
} C2GO_MLIB_NAME(pthread_cond_t);

typedef struct {
    int _reserved;
    int _pad[5];
} C2GO_MLIB_NAME(pthread_condattr_t);

typedef struct {
    void *managed _state;
} C2GO_MLIB_NAME(pthread_rwlock_t);

typedef struct {
    int _reserved;
    int _pad[5];
} C2GO_MLIB_NAME(pthread_rwlockattr_t);

#ifdef C2GO_MLIB_UNPREFIXED
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_MUTEX_INITIALIZER  { 0 }
#define PTHREAD_COND_INITIALIZER   { 0 }
#define PTHREAD_RWLOCK_INITIALIZER { 0 }
#define PTHREAD_MUTEX_NORMAL       0
#define PTHREAD_MUTEX_RECURSIVE    1
#define PTHREAD_MUTEX_ERRORCHECK   2
#define PTHREAD_MUTEX_DEFAULT      PTHREAD_MUTEX_NORMAL
#else
#define MLIB_PTHREAD_CREATE_JOINABLE 0
#define MLIB_PTHREAD_CREATE_DETACHED 1
#define MLIB_PTHREAD_MUTEX_INITIALIZER  { 0 }
#define MLIB_PTHREAD_COND_INITIALIZER   { 0 }
#define MLIB_PTHREAD_RWLOCK_INITIALIZER { 0 }
#define MLIB_PTHREAD_MUTEX_NORMAL       0
#define MLIB_PTHREAD_MUTEX_RECURSIVE    1
#define MLIB_PTHREAD_MUTEX_ERRORCHECK   2
#define MLIB_PTHREAD_MUTEX_DEFAULT      MLIB_PTHREAD_MUTEX_NORMAL
#endif

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_create", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_create)(C2GO_MLIB_NAME(pthread_t) *,
    const C2GO_MLIB_NAME(pthread_attr_t) *,
    C2GO_MLIB_NAME(pthread_start_routine_t), void *managed);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_join", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_join)(C2GO_MLIB_NAME(pthread_t), void *managed *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_exit", C2GO_GOABI0)
void C2GO_MLIB_NAME(pthread_exit)(void *managed);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_self", C2GO_GOABI0)
C2GO_MLIB_NAME(pthread_t) C2GO_MLIB_NAME(pthread_self)(void);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_detach", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_detach)(C2GO_MLIB_NAME(pthread_t));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_equal", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_equal)(C2GO_MLIB_NAME(pthread_t),
    C2GO_MLIB_NAME(pthread_t));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_yield", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_yield)(void);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_attr_init", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_attr_init)(C2GO_MLIB_NAME(pthread_attr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_attr_destroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_attr_destroy)(C2GO_MLIB_NAME(pthread_attr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_attr_setdetachstate", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_attr_setdetachstate)(
    C2GO_MLIB_NAME(pthread_attr_t) *, int);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_attr_setstacksize", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_attr_setstacksize)(
    C2GO_MLIB_NAME(pthread_attr_t) *, size_t);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_key_create", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_key_create)(C2GO_MLIB_NAME(pthread_key_t) *,
    C2GO_MLIB_NAME(pthread_key_destructor_t));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_key_delete", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_key_delete)(C2GO_MLIB_NAME(pthread_key_t));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_getspecific", C2GO_GOABI0)
void *managed C2GO_MLIB_NAME(pthread_getspecific)(
    C2GO_MLIB_NAME(pthread_key_t));
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.mlib_pthread_setspecific", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_setspecific)(C2GO_MLIB_NAME(pthread_key_t),
    const void *managed);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_init)(C2GO_MLIB_NAME(pthread_mutex_t) *,
    const C2GO_MLIB_NAME(pthread_mutexattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_destroy)(C2GO_MLIB_NAME(pthread_mutex_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexLock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_lock)(C2GO_MLIB_NAME(pthread_mutex_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexTryLock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_trylock)(C2GO_MLIB_NAME(pthread_mutex_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexTimedlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_timedlock)(C2GO_MLIB_NAME(pthread_mutex_t) *,
    const struct timespec *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexUnlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutex_unlock)(C2GO_MLIB_NAME(pthread_mutex_t) *);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexAttrInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutexattr_init)(C2GO_MLIB_NAME(pthread_mutexattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexAttrDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutexattr_destroy)(C2GO_MLIB_NAME(pthread_mutexattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexAttrSetType", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutexattr_settype)(C2GO_MLIB_NAME(pthread_mutexattr_t) *, int);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadMutexAttrGetType", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_mutexattr_gettype)(const C2GO_MLIB_NAME(pthread_mutexattr_t) *, int *);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_init)(C2GO_MLIB_NAME(pthread_cond_t) *,
    const C2GO_MLIB_NAME(pthread_condattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_destroy)(C2GO_MLIB_NAME(pthread_cond_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondWait", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_wait)(C2GO_MLIB_NAME(pthread_cond_t) *,
    C2GO_MLIB_NAME(pthread_mutex_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondTimedwait", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_timedwait)(C2GO_MLIB_NAME(pthread_cond_t) *,
    C2GO_MLIB_NAME(pthread_mutex_t) *, const struct timespec *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondSignal", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_signal)(C2GO_MLIB_NAME(pthread_cond_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondBroadcast", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_cond_broadcast)(C2GO_MLIB_NAME(pthread_cond_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondAttrInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_condattr_init)(C2GO_MLIB_NAME(pthread_condattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadCondAttrDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_condattr_destroy)(C2GO_MLIB_NAME(pthread_condattr_t) *);

c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_init)(C2GO_MLIB_NAME(pthread_rwlock_t) *,
    const C2GO_MLIB_NAME(pthread_rwlockattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_destroy)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockRdlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_rdlock)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockWrlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_wrlock)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRwlockTryrdlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_tryrdlock)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRwlockTrywrlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_trywrlock)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockUnlock", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlock_unlock)(C2GO_MLIB_NAME(pthread_rwlock_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockAttrInit", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlockattr_init)(C2GO_MLIB_NAME(pthread_rwlockattr_t) *);
c2go_linkname("github.com/c2gohq/c2go_libc/mlib.PthreadRWLockAttrDestroy", C2GO_GOABI0)
int C2GO_MLIB_NAME(pthread_rwlockattr_destroy)(C2GO_MLIB_NAME(pthread_rwlockattr_t) *);

#pragma c2go pop

#endif /* C2GO_MLIB_PTHREAD_H */
