/* pthread.h — c2go pthread API.
 *
 * API-compatible with POSIX at the source level (function signatures), NOT
 * binary-compatible with any native pthread layout. A pthread maps to a
 * goroutine, and each mutex/cond/rwlock object is plain unmanaged C memory
 * holding a 64-bit `_id` into a Go-side handle table (handle.go); the state
 * object that actually parks goroutines (a sync.Mutex / channel-based cond /
 * sync.RWMutex) lives in that table, rooted there with NO managed pointer in the
 * C struct. sem_t (<semaphore.h>) uses the same _id/handle-table shape. No field
 * is a managed pointer, so these are ordinary unmanaged C memory under the
 * default world — no `#pragma c2go unmanaged` is needed (verified: the c2go
 * artifact is byte-identical with or without one). The managed carrier is
 * declared separately by <c2go/mlib/pthread.h>; see mlib/DESIGN.md. */
#ifndef _PTHREAD_H
#define _PTHREAD_H

#define __NEED_size_t
#include <bits/alltypes.h>
#include <c2go.h>
#include <time.h>   /* struct timespec (pthread_cond_timedwait) */

typedef size_t pthread_t;          /* handle id into the Go-side thread table */

/* pthread_attr_t: only `detachstate` is honored. A requested stack size is
 * accepted (returns 0) but advisory — a goroutine's stack auto-grows, so it
 * already provides at least the requested amount; priority / scope / affinity
 * are likewise not modeled. */
typedef struct {
    int           detachstate;
    int           _reserved;
    unsigned long _pad[6];
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#ifndef C2GO_PTHREAD_OMIT_SYNC

typedef struct {                        /* _id -> Go-side mutex state (handle.go) */
    size_t        _id;                  /* 0 = uninitialized; 64-bit on every target */
    unsigned long _pad[5];
} pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER { 0, {0,0,0,0,0} }

typedef struct {
    int _type;
    int _pad[5];
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

typedef struct {                        /* _id -> Go-side cond state (handle.go) */
    size_t        _id;                  /* 0 = uninitialized; 64-bit on every target */
    unsigned long _pad[5];
} pthread_cond_t;
#define PTHREAD_COND_INITIALIZER { 0, {0,0,0,0,0} }

typedef struct {
    int _reserved;
    int _pad[5];
} pthread_condattr_t;

typedef struct {                        /* _id -> Go-side rwlock state (handle.go) */
    size_t        _id;                  /* 0 = uninitialized; 64-bit on every target */
    unsigned long _pad[5];
} pthread_rwlock_t;
#define PTHREAD_RWLOCK_INITIALIZER { 0, {0,0,0,0,0} }

typedef struct {
    int _reserved;
    int _pad[5];
} pthread_rwlockattr_t;

#endif /* !C2GO_PTHREAD_OMIT_SYNC */

/* pthread_key_t is opaque (POSIX); c2go represents it as a pointer to the key's
 * heap descriptor (destructor + deleted flag), so a key carries its own
 * metadata with no global id->destructor table. */
typedef void *pthread_key_t;

typedef struct {
    int    _done;
    int    _pad;
    size_t _state;      /* 64-bit on every target: the Go side CASes it as a
                         * uintptr, so unsigned long (32-bit on Windows LLP64)
                         * would let that write spill past the struct */
} pthread_once_t;
#define PTHREAD_ONCE_INIT { 0, 0, 0 }

/* ── thread lifecycle ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCreate", C2GO_GOABI0)
int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadJoin", C2GO_GOABI0)
int pthread_join(pthread_t, void **);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadExit", C2GO_GOABI0)
void pthread_exit(void *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadSelf", C2GO_GOABI0)
pthread_t pthread_self(void);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadDetach", C2GO_GOABI0)
int pthread_detach(pthread_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadEqual", C2GO_GOABI0)
int pthread_equal(pthread_t, pthread_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadYield", C2GO_GOABI0)
int pthread_yield(void);

/* ── attributes ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrInit", C2GO_GOABI0)
int pthread_attr_init(pthread_attr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrDestroy", C2GO_GOABI0)
int pthread_attr_destroy(pthread_attr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrSetDetachState", C2GO_GOABI0)
int pthread_attr_setdetachstate(pthread_attr_t *, int);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadAttrSetStackSize", C2GO_GOABI0)
int pthread_attr_setstacksize(pthread_attr_t *, size_t);

#ifndef C2GO_PTHREAD_OMIT_SYNC
int pthread_mutex_timedlock(pthread_mutex_t *, const struct timespec *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_mutex_timedlock", C2GO_GOABI0);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_rwlock_tryrdlock", C2GO_GOABI0);
int pthread_rwlock_trywrlock(pthread_rwlock_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_rwlock_trywrlock", C2GO_GOABI0);
int pthread_mutexattr_init(pthread_mutexattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_mutexattr_init", C2GO_GOABI0);
int pthread_mutexattr_destroy(pthread_mutexattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_mutexattr_destroy", C2GO_GOABI0);
int pthread_condattr_init(pthread_condattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_condattr_init", C2GO_GOABI0);
int pthread_condattr_destroy(pthread_condattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_condattr_destroy", C2GO_GOABI0);
int pthread_rwlockattr_init(pthread_rwlockattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_rwlockattr_init", C2GO_GOABI0);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_rwlockattr_destroy", C2GO_GOABI0);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_mutexattr_settype", C2GO_GOABI0);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *)
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_mutexattr_gettype", C2GO_GOABI0);
#endif /* !C2GO_PTHREAD_OMIT_SYNC */

int pthread_atfork(void (*)(void), void (*)(void), void (*)(void))
    c2go_linkname("github.com/c2gohq/c2go_libc.pthread_atfork", C2GO_GOABI0);

#ifndef C2GO_PTHREAD_OMIT_SYNC
/* ── mutex ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadMutexInit", C2GO_GOABI0)
int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadMutexDestroy", C2GO_GOABI0)
int pthread_mutex_destroy(pthread_mutex_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadMutexLock", C2GO_GOABI0)
int pthread_mutex_lock(pthread_mutex_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadMutexTryLock", C2GO_GOABI0)
int pthread_mutex_trylock(pthread_mutex_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadMutexUnlock", C2GO_GOABI0)
int pthread_mutex_unlock(pthread_mutex_t *);

/* ── condition variable ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondInit", C2GO_GOABI0)
int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondDestroy", C2GO_GOABI0)
int pthread_cond_destroy(pthread_cond_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondWait", C2GO_GOABI0)
int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondTimedwait", C2GO_GOABI0)
int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondSignal", C2GO_GOABI0)
int pthread_cond_signal(pthread_cond_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadCondBroadcast", C2GO_GOABI0)
int pthread_cond_broadcast(pthread_cond_t *);

/* ── rwlock ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadRWLockInit", C2GO_GOABI0)
int pthread_rwlock_init(pthread_rwlock_t *, const pthread_rwlockattr_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadRWLockDestroy", C2GO_GOABI0)
int pthread_rwlock_destroy(pthread_rwlock_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadRWLockRdlock", C2GO_GOABI0)
int pthread_rwlock_rdlock(pthread_rwlock_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadRWLockWrlock", C2GO_GOABI0)
int pthread_rwlock_wrlock(pthread_rwlock_t *);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadRWLockUnlock", C2GO_GOABI0)
int pthread_rwlock_unlock(pthread_rwlock_t *);
#endif /* !C2GO_PTHREAD_OMIT_SYNC */

/* ── thread-specific data (GLS-backed) ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadKeyCreate", C2GO_GOABI0)
int pthread_key_create(pthread_key_t *, void (*)(void *));
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadKeyDelete", C2GO_GOABI0)
int pthread_key_delete(pthread_key_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadGetSpecific", C2GO_GOABI0)
void *pthread_getspecific(pthread_key_t);
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadSetSpecific", C2GO_GOABI0)
int pthread_setspecific(pthread_key_t, const void *);

/* ── once ── */
c2go_linkname("github.com/c2gohq/c2go_libc.PthreadOnce", C2GO_GOABI0)
int pthread_once(pthread_once_t *, void (*)(void));

#endif /* _PTHREAD_H */
