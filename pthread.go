// pthread.go — the pthread mutex family, goroutine-backed over the shared
// handle table (handle.go).
//
// A pthread_mutex_t is plain unmanaged C memory holding a 64-bit _id (0 =
// uninitialized, so PTHREAD_MUTEX_INITIALIZER's zero bytes mean "not yet
// used"). The posixsync state that actually parks goroutines lives in mutexTab,
// rooted there so the GC keeps it alive while the C object references it. The
// same state algorithm also serves mlib's direct managed-pointer carrier; see
// <pthread.h> and mlib/DESIGN.md.
//
// All three mutex kinds are supported: NORMAL (no owner tracking), RECURSIVE and
// ERRORCHECK (owner tracked by goid via routine.Goid()). Real pthreads
// (thread.go) exercise these concurrently in addition to the Go-side callers in
// pthread_test.go.
//
// pthread functions return the error number DIRECTLY (0 on success), unlike the
// errno-setting libc surface — so these never touch the C errno.
//
// The implementation is portable Go (shared posixsync state + handle table); the
// only per-OS piece is the errno constants (errno_unix.go / errno_windows.go),
// since the returned numbers must match c2go-libc's <bits/errno.h> for the
// target — and MinGW's values differ from Go's windows syscall.Errno.

package libc

import (
	"sync/atomic"
	"unsafe"

	"github.com/c2gohq/c2go_libc/internal/posixsync"
)

// Mutex kinds, matching <pthread.h>'s PTHREAD_MUTEX_* values.
const (
	cMutexNormal     = int32(posixsync.MutexNormal)
	cMutexRecursive  = int32(posixsync.MutexRecursive)
	cMutexErrorcheck = int32(posixsync.MutexErrorcheck)
)

func newMutexState() *posixsync.Mutex {
	return posixsync.NewMutex(posixsync.MutexNormal)
}

var mutexTab handleTable[posixsync.Mutex]

func pthreadSyncResultCode(result posixsync.SyncResult) int32 {
	switch result {
	case posixsync.SyncOK:
		return 0
	case posixsync.SyncBusy:
		return errEBUSY
	case posixsync.SyncPermission:
		return errEPERM
	case posixsync.SyncDeadlock:
		return errEDEADLK
	case posixsync.SyncTimedOut:
		return errETIMEDOUT
	default:
		return errEINVAL
	}
}

// mutexID reinterprets the pthread_mutex_t's leading _id field. m must point at
// stable memory (a mutex shared across goroutines lives in a C global or on the
// malloc heap, never a moving goroutine stack); the pointer is threaded through
// the handle table as a *uint64 — a live pointer, so copystack still relocates
// it if the object is on a stack — and never laundered through uintptr across a
// safepoint.
func mutexID(m unsafe.Pointer) *uint64 { return (*uint64)(m) }

//go:linkname PthreadMutexInit
func PthreadMutexInit(m, attr unsafe.Pointer) int32 {
	typ := int32(cMutexNormal)
	if attr != nil {
		typ = *(*int32)(attr) // pthread_mutexattr_t._type is the leading field
	}
	kind := posixsync.MutexKind(typ)
	if !posixsync.ValidMutexKind(kind) {
		return errEINVAL
	}
	switch kind {
	case posixsync.MutexNormal:
		// Reset to "uninitialized": a freshly malloc'd mutex has garbage _id and a
		// static PTHREAD_MUTEX_INITIALIZER is already 0; either way the first lock
		// lazily allocates a NORMAL state. Re-init of a still-live mutex leaks its
		// old state (POSIX UB), the same latitude malloc takes for use-after-free.
		atomic.StoreUint64(mutexID(m), 0)
	case posixsync.MutexRecursive, posixsync.MutexErrorcheck:
		// No static initializer exists for these, so allocate eagerly to carry the
		// type into the state the lock path reads.
		atomic.StoreUint64(mutexID(m), mutexTab.alloc(posixsync.NewMutex(kind)))
	}
	return 0
}

//go:linkname PthreadMutexDestroy
func PthreadMutexDestroy(m unsafe.Pointer) int32 {
	mutexTab.free(atomic.SwapUint64(mutexID(m), 0)) // free(0) is a no-op
	return 0
}

//go:linkname PthreadMutexLock
func PthreadMutexLock(m unsafe.Pointer) int32 {
	return pthreadSyncResultCode(mutexTab.lazyInit(mutexID(m), newMutexState).Lock())
}

//go:linkname PthreadMutexTryLock
func PthreadMutexTryLock(m unsafe.Pointer) int32 {
	return pthreadSyncResultCode(mutexTab.lazyInit(mutexID(m), newMutexState).TryLock())
}

//go:linkname PthreadMutexUnlock
func PthreadMutexUnlock(m unsafe.Pointer) int32 {
	st := mutexTab.get(atomic.LoadUint64(mutexID(m)))
	if st == nil {
		return errEPERM // unlock of an uninitialized/destroyed mutex
	}
	return pthreadSyncResultCode(st.Unlock())
}

//go:linkname PthreadMutexAttrSetType
func PthreadMutexAttrSetType(a unsafe.Pointer, typ int32) int32 {
	if a == nil {
		return errEINVAL
	}
	switch typ {
	case cMutexNormal, cMutexRecursive, cMutexErrorcheck:
		*(*int32)(a) = typ // consumed by pthread_mutex_init (NORMAL / RECURSIVE / ERRORCHECK all supported)
		return 0
	default:
		return errEINVAL
	}
}

// ─────────────────────────────── rwlock ───────────────────────────────
//
// sync.RWMutex has separate RUnlock/Unlock, but POSIX pthread_rwlock_unlock is
// one call that must release whichever mode the caller holds. writeHeld bridges
// it WITHOUT tracking an owner goid: a write lock is exclusive, so when
// writeHeld is true the only goroutine that can be unlocking is the writer;
// when it is false the caller is a reader. The flag is only ever written under
// the exclusive write lock, so the read in unlock is unraced.

func newRWLockState() *posixsync.RWLock { return posixsync.NewRWLock() }

var rwlockTab handleTable[posixsync.RWLock]

func rwlockID(p unsafe.Pointer) *uint64 { return (*uint64)(p) }

//go:linkname PthreadRWLockInit
func PthreadRWLockInit(rwl, attr unsafe.Pointer) int32 {
	atomic.StoreUint64(rwlockID(rwl), 0)
	return 0
}

//go:linkname PthreadRWLockDestroy
func PthreadRWLockDestroy(rwl unsafe.Pointer) int32 {
	rwlockTab.free(atomic.SwapUint64(rwlockID(rwl), 0))
	return 0
}

//go:linkname PthreadRWLockRdlock
func PthreadRWLockRdlock(rwl unsafe.Pointer) int32 {
	return pthreadSyncResultCode(rwlockTab.lazyInit(rwlockID(rwl), newRWLockState).ReadLock())
}

//go:linkname PthreadRWLockWrlock
func PthreadRWLockWrlock(rwl unsafe.Pointer) int32 {
	return pthreadSyncResultCode(rwlockTab.lazyInit(rwlockID(rwl), newRWLockState).WriteLock())
}

//go:linkname PthreadRWLockUnlock
func PthreadRWLockUnlock(rwl unsafe.Pointer) int32 {
	st := rwlockTab.get(atomic.LoadUint64(rwlockID(rwl)))
	if st == nil {
		return errEPERM
	}
	return pthreadSyncResultCode(st.Unlock())
}

// ────────────────────────── condition variable ─────────────────────────
//
// Channel-based (not sync.Cond, which has no timedwait): each waiter enqueues a
// fresh buffered channel, releases the associated mutex, then blocks on it.
// signal pops one waiter and sends; broadcast drains them all. The enqueue
// happens before the mutex is released, so a signal that runs after the release
// always finds the waiter — no missed wakeup.

func newCondState() *posixsync.Cond { return posixsync.NewCond() }

var condTab handleTable[posixsync.Cond]

func condID(p unsafe.Pointer) *uint64 { return (*uint64)(p) }

//go:linkname PthreadCondInit
func PthreadCondInit(c, attr unsafe.Pointer) int32 {
	atomic.StoreUint64(condID(c), 0)
	return 0
}

//go:linkname PthreadCondDestroy
func PthreadCondDestroy(c unsafe.Pointer) int32 {
	condTab.free(atomic.SwapUint64(condID(c), 0))
	return 0
}

//go:linkname PthreadCondWait
func PthreadCondWait(c, m unsafe.Pointer) int32 { return condWait(c, m, nil) }

//go:linkname PthreadCondTimedwait
func PthreadCondTimedwait(c, m, abstime unsafe.Pointer) int32 { return condWait(c, m, abstime) }

func condWait(c, m, abstime unsafe.Pointer) int32 {
	if abstime != nil && !posixsync.TimespecValid(abstime) {
		return errEINVAL // musl pthread_cond_timedwait.c (#657)
	}
	cs := condTab.lazyInit(condID(c), newCondState)
	ms := mutexTab.get(atomic.LoadUint64(mutexID(m)))
	if ms == nil {
		return errEINVAL // must hold an initialized mutex
	}
	return pthreadSyncResultCode(cs.Wait(ms, abstime))
}

//go:linkname PthreadCondSignal
func PthreadCondSignal(c unsafe.Pointer) int32 {
	cs := condTab.get(atomic.LoadUint64(condID(c)))
	if cs == nil {
		return 0 // never waited on -> no-op
	}
	cs.Signal()
	return 0
}

//go:linkname PthreadCondBroadcast
func PthreadCondBroadcast(c unsafe.Pointer) int32 {
	cs := condTab.get(atomic.LoadUint64(condID(c)))
	if cs == nil {
		return 0
	}
	cs.Broadcast()
	return 0
}

// ── #664: timed / try variants (musl pthread_mutex_timedlock.c and
// pthread_rwlock_try*.c semantics over the handle-table states). The timed
// forms poll TryLock in waitWall-style 10ms slices against the CLOCK_REALTIME
// deadline — POSIX permits the latency, and a realtime STEP is observed the
// same way waitWall observes it (#651). ──────────────────────────────────────

//go:linkname PthreadMutexTimedlock
func PthreadMutexTimedlock(m, abstime unsafe.Pointer) int32 {
	if abstime == nil || !posixsync.TimespecValid(abstime) {
		return errEINVAL
	}
	st := mutexTab.lazyInit(mutexID(m), newMutexState)
	return pthreadSyncResultCode(st.TimedLock(abstime))
}

//go:linkname PthreadRwlockTryrdlock
func PthreadRwlockTryrdlock(rw unsafe.Pointer) int32 {
	st := rwlockTab.lazyInit(rwlockID(rw), newRWLockState)
	if st == nil {
		return errEINVAL
	}
	return pthreadSyncResultCode(st.TryReadLock())
}

//go:linkname PthreadRwlockTrywrlock
func PthreadRwlockTrywrlock(rw unsafe.Pointer) int32 {
	st := rwlockTab.lazyInit(rwlockID(rw), newRWLockState)
	if st == nil {
		return errEINVAL
	}
	return pthreadSyncResultCode(st.TryWriteLock())
}
