// pthread.go — the pthread mutex family, goroutine-backed over the shared
// handle table (handle.go).
//
// A pthread_mutex_t is plain unmanaged C memory holding a 64-bit _id (0 =
// uninitialized, so PTHREAD_MUTEX_INITIALIZER's zero bytes mean "not yet
// used"). The sync.Mutex that actually parks goroutines lives in mutexTab,
// rooted there so the GC keeps it alive while the C object references it — the
// C struct itself holds no managed pointer. See <pthread.h> and
// project_pthread_sem_handle_table for the data model.
//
// All three mutex kinds are supported: NORMAL (no owner tracking), RECURSIVE and
// ERRORCHECK (owner tracked by goid via routine.Goid()). Real pthreads
// (thread.go) exercise these concurrently in addition to the Go-side callers in
// pthread_test.go.
//
// pthread functions return the error number DIRECTLY (0 on success), unlike the
// errno-setting libc surface — so these never touch the C errno.
//
// The implementation is portable Go (sync primitives + the handle table); the
// only per-OS piece is the errno constants (errno_unix.go / errno_windows.go),
// since the returned numbers must match c2go-libc's <bits/errno.h> for the
// target — and MinGW's values differ from Go's windows syscall.Errno.

package libc

import (
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/timandy/routine"
)

// Mutex kinds, matching <pthread.h>'s PTHREAD_MUTEX_* values.
const (
	cMutexNormal     = 0
	cMutexRecursive  = 1
	cMutexErrorcheck = 2
)

// mutexState is the Go object behind one pthread_mutex_t, held live by mutexTab.
// For RECURSIVE/ERRORCHECK the owning goroutine is tracked by goid: owner is
// read/written atomically (any goroutine may probe it to decide block-vs-recurse),
// while count is touched ONLY by the current owner — ownership is handed off
// through mu, which supplies the happens-before — so it needs no atomic.
//
// DEAD-OWNER contract (#651, documented — deliberately no mechanism): a
// goroutine that dies holding the mutex (pthread_exit / Goexit / panic
// without unlock) leaves it locked forever and later lockers block — exactly
// glibc/musl behaviour for a NON-robust mutex (owner death while holding is
// POSIX-undefined). Robust mutexes (EOWNERDEAD recovery) are not implemented;
// silently auto-unlocking would fake robustness and mask the bug. Goids are
// monotonic and NEVER reused, so a recycled g cannot be misidentified as the
// dead owner — the failure mode is an honest hang, never corruption.
type mutexState struct {
	mu    sync.Mutex
	typ   int32  // cMutex{Normal,Recursive,Errorcheck}; set at init, then read-only
	owner uint64 // goid of the holder, 0 = unowned (RECURSIVE/ERRORCHECK only)
	count int32  // recursion depth (RECURSIVE only)
}

func newMutexState() *mutexState { return &mutexState{} } // NORMAL (typ zero value)

var mutexTab handleTable[mutexState]

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
	switch typ {
	case cMutexNormal:
		// Reset to "uninitialized": a freshly malloc'd mutex has garbage _id and a
		// static PTHREAD_MUTEX_INITIALIZER is already 0; either way the first lock
		// lazily allocates a NORMAL state. Re-init of a still-live mutex leaks its
		// old state (POSIX UB), the same latitude malloc takes for use-after-free.
		atomic.StoreUint64(mutexID(m), 0)
	case cMutexRecursive, cMutexErrorcheck:
		// No static initializer exists for these, so allocate eagerly to carry the
		// type into the state the lock path reads.
		atomic.StoreUint64(mutexID(m), mutexTab.alloc(&mutexState{typ: typ}))
	default:
		return errEINVAL
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
	st := mutexTab.lazyInit(mutexID(m), newMutexState)
	if st.typ == cMutexNormal {
		st.mu.Lock() // no owner tracking, no goid cost on the common path
		return 0
	}
	self := routine.Goid()
	if atomic.LoadUint64(&st.owner) == self {
		if st.typ == cMutexErrorcheck {
			return errEDEADLK // relock by owner: report instead of deadlocking
		}
		st.count++ // RECURSIVE: recurse without re-taking mu
		return 0
	}
	st.mu.Lock()
	atomic.StoreUint64(&st.owner, self) // own it only after mu is held
	st.count = 1
	return 0
}

//go:linkname PthreadMutexTryLock
func PthreadMutexTryLock(m unsafe.Pointer) int32 {
	st := mutexTab.lazyInit(mutexID(m), newMutexState)
	if st.typ == cMutexNormal {
		if st.mu.TryLock() {
			return 0
		}
		return errEBUSY
	}
	self := routine.Goid()
	if atomic.LoadUint64(&st.owner) == self {
		if st.typ == cMutexRecursive {
			st.count++
			return 0
		}
		return errEBUSY // ERRORCHECK: already held; trylock never deadlocks
	}
	if st.mu.TryLock() {
		atomic.StoreUint64(&st.owner, self)
		st.count = 1
		return 0
	}
	return errEBUSY
}

//go:linkname PthreadMutexUnlock
func PthreadMutexUnlock(m unsafe.Pointer) int32 {
	st := mutexTab.get(atomic.LoadUint64(mutexID(m)))
	if st == nil {
		return errEPERM // unlock of an uninitialized/destroyed mutex
	}
	if st.typ == cMutexNormal {
		st.mu.Unlock() // NORMAL: unlocking a mutex you don't hold is UB
		return 0
	}
	if atomic.LoadUint64(&st.owner) != routine.Goid() {
		return errEPERM // not the owner
	}
	if st.typ == cMutexRecursive {
		st.count--
		if st.count > 0 {
			return 0 // still held by an outer lock
		}
	}
	atomic.StoreUint64(&st.owner, 0) // release ownership before mu, still exclusive
	st.mu.Unlock()
	return 0
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

type rwlockState struct {
	rw        sync.RWMutex
	writeHeld atomic.Bool
}

func newRWLockState() *rwlockState { return &rwlockState{} }

var rwlockTab handleTable[rwlockState]

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
	rwlockTab.lazyInit(rwlockID(rwl), newRWLockState).rw.RLock()
	return 0
}

//go:linkname PthreadRWLockWrlock
func PthreadRWLockWrlock(rwl unsafe.Pointer) int32 {
	st := rwlockTab.lazyInit(rwlockID(rwl), newRWLockState)
	st.rw.Lock()
	st.writeHeld.Store(true)
	return 0
}

//go:linkname PthreadRWLockUnlock
func PthreadRWLockUnlock(rwl unsafe.Pointer) int32 {
	st := rwlockTab.get(atomic.LoadUint64(rwlockID(rwl)))
	if st == nil {
		return errEPERM
	}
	if st.writeHeld.Load() {
		st.writeHeld.Store(false) // clear before Unlock, still exclusive
		st.rw.Unlock()
	} else {
		st.rw.RUnlock()
	}
	return 0
}

// ────────────────────────── condition variable ─────────────────────────
//
// Channel-based (not sync.Cond, which has no timedwait): each waiter enqueues a
// fresh buffered channel, releases the associated mutex, then blocks on it.
// signal pops one waiter and sends; broadcast drains them all. The enqueue
// happens before the mutex is released, so a signal that runs after the release
// always finds the waiter — no missed wakeup.

type condState struct {
	mu      sync.Mutex
	waiters []chan struct{}
}

func newCondState() *condState { return &condState{} }

var condTab handleTable[condState]

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
	if abstime != nil && !tsValid(abstime) {
		return errEINVAL // musl pthread_cond_timedwait.c (#657)
	}
	cs := condTab.lazyInit(condID(c), newCondState)
	ms := mutexTab.get(atomic.LoadUint64(mutexID(m)))
	if ms == nil {
		return errEINVAL // must hold an initialized mutex
	}
	w := make(chan struct{}, 1)
	cs.mu.Lock()
	cs.waiters = append(cs.waiters, w)
	cs.mu.Unlock()

	// A RECURSIVE/ERRORCHECK mutex carries owner/count bookkeeping that the raw
	// ms.mu.Unlock below bypasses. Save and clear it so the wait fully releases
	// the mutex (a signaler on another goroutine may legitimately take it while we
	// block), then restore the same recursion depth to the waking goroutine on
	// re-acquire — otherwise the caller's later pthread_mutex_unlock would see a
	// stale/zero owner and wrongly return EPERM. NORMAL mutexes have no such state.
	// Safe without extra locking: the caller owns ms.mu here, so count is its own.
	savedCount := int32(0)
	if ms.typ != cMutexNormal {
		savedCount = ms.count
		ms.count = 0
		atomic.StoreUint64(&ms.owner, 0)
	}
	ms.mu.Unlock() // release the associated mutex (enqueue already happened)
	rc := int32(0)
	if abstime == nil {
		<-w
	} else if !waitWall(w, abstime) {
		cs.mu.Lock()
		if i := waiterIndex(cs.waiters, w); i >= 0 {
			cs.waiters = append(cs.waiters[:i], cs.waiters[i+1:]...)
			rc = errETIMEDOUT
		} else {
			<-w // a signal already dequeued us and sent the token
		}
		cs.mu.Unlock()
	}
	ms.mu.Lock() // re-acquire before returning, per POSIX
	if ms.typ != cMutexNormal {
		atomic.StoreUint64(&ms.owner, routine.Goid()) // this goroutine owns it again
		ms.count = savedCount
	}
	return rc
}

//go:linkname PthreadCondSignal
func PthreadCondSignal(c unsafe.Pointer) int32 {
	cs := condTab.get(atomic.LoadUint64(condID(c)))
	if cs == nil {
		return 0 // never waited on -> no-op
	}
	cs.mu.Lock()
	if len(cs.waiters) > 0 {
		w := cs.waiters[0]
		cs.waiters = cs.waiters[1:]
		w <- struct{}{} // buffered cap-1: never blocks
	}
	cs.mu.Unlock()
	return 0
}

//go:linkname PthreadCondBroadcast
func PthreadCondBroadcast(c unsafe.Pointer) int32 {
	cs := condTab.get(atomic.LoadUint64(condID(c)))
	if cs == nil {
		return 0
	}
	cs.mu.Lock()
	ws := cs.waiters
	cs.waiters = nil
	cs.mu.Unlock()
	for _, w := range ws {
		w <- struct{}{}
	}
	return 0
}

// ─────────────────────────── shared helpers ────────────────────────────

// waitWall waits on w until the CLOCK_REALTIME deadline in ts, re-deriving
// the remaining duration from the WALL clock in bounded slices (#651): POSIX
// timedwait deadlines are absolute realtime instants, but Go timers run on
// the monotonic clock — a single timer armed with the initial delta goes
// stale when the wall clock is STEPPED (forward step: the wait overshoots;
// backward step: it fires early). Re-reading time.Until(deadline) once per
// slice bounds the error window to one slice after any step; an un-stepped
// wait keeps full precision (the final slice is the exact remainder).
// Untestable without stepping the system clock — reviewed logic, documented.
// Returns true if w fired, false on deadline expiry.
// tsValid: musl __timedwait rejects a timespec with tv_nsec outside
// [0, 1e9) as EINVAL before any waiting (#657). Both timedwait entry points
// check this first.
func tsValid(ts unsafe.Pointer) bool {
	if ts == nil {
		return false
	}
	nsec := *(*int64)(unsafe.Add(ts, 8))
	return nsec >= 0 && nsec < 1_000_000_000
}

func waitWall(w chan struct{}, ts unsafe.Pointer) bool {
	const slice = 250 * time.Millisecond
	sec := *(*int64)(ts)
	nsec := *(*int64)(unsafe.Add(ts, 8))
	deadline := time.Unix(sec, nsec)
	t := time.NewTimer(slice) // one timer, Reset per slice (#661)
	defer t.Stop()
	for {
		d := time.Until(deadline)
		if d <= 0 {
			return false
		}
		if d > slice {
			d = slice
		}
		t.Reset(d)
		select {
		case <-w:
			return true
		case <-t.C:
		}
	}
}


// waiterIndex finds w in a waiter FIFO, or -1. Shared by cond and sem timeout
// paths to remove a timed-out waiter.
func waiterIndex(ws []chan struct{}, w chan struct{}) int {
	for i, x := range ws {
		if x == w {
			return i
		}
	}
	return -1
}

// ── #664: timed / try variants (musl pthread_mutex_timedlock.c and
// pthread_rwlock_try*.c semantics over the handle-table states). The timed
// forms poll TryLock in waitWall-style 10ms slices against the CLOCK_REALTIME
// deadline — POSIX permits the latency, and a realtime STEP is observed the
// same way waitWall observes it (#651). ──────────────────────────────────────

//go:linkname PthreadMutexTimedlock
func PthreadMutexTimedlock(m, abstime unsafe.Pointer) int32 {
	if abstime == nil || !tsValid(abstime) {
		return errEINVAL
	}
	st := mutexTab.lazyInit(mutexID(m), newMutexState)
	if st == nil {
		return errEINVAL
	}
	if r := PthreadMutexTryLock(m); r != errEBUSY {
		return r // 0, EDEADLK, or another immediate answer
	}
	sec := *(*int64)(abstime)
	nsec := *(*int64)(unsafe.Add(abstime, 8))
	deadline := time.Unix(sec, nsec)
	for {
		if time.Until(deadline) <= 0 {
			return errETIMEDOUT
		}
		time.Sleep(10 * time.Millisecond)
		if r := PthreadMutexTryLock(m); r != errEBUSY {
			return r
		}
	}
}

//go:linkname PthreadRwlockTryrdlock
func PthreadRwlockTryrdlock(rw unsafe.Pointer) int32 {
	st := rwlockTab.lazyInit(rwlockID(rw), newRWLockState)
	if st == nil {
		return errEINVAL
	}
	if st.rw.TryRLock() {
		return 0
	}
	return errEBUSY
}

//go:linkname PthreadRwlockTrywrlock
func PthreadRwlockTrywrlock(rw unsafe.Pointer) int32 {
	st := rwlockTab.lazyInit(rwlockID(rw), newRWLockState)
	if st == nil {
		return errEINVAL
	}
	if st.rw.TryLock() {
		st.writeHeld.Store(true)
		return 0
	}
	return errEBUSY
}
