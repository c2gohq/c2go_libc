// sem.go — POSIX unnamed semaphores (<semaphore.h>), a counting semaphore over
// the shared handle table (handle.go).
//
// A sem_t is plain unmanaged C memory holding a 64-bit _id into semTab. Unlike a
// pthread mutex there is no static initializer — POSIX requires sem_init before
// use — so SemInit allocates the state eagerly (carrying the initial count) and
// wait/post resolve it with get() (a zero/unknown _id is EINVAL). A blocked
// sem_wait parks on a per-waiter buffered channel; sem_post hands a permit
// directly to the oldest waiter (without bumping the count) or, with none
// waiting, increments the count.
//
// Portable Go (sync/channels/handle table + per-OS errno constants), so it
// builds on Windows too — <semaphore.h> declares the API for every target.

package libc

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type semState struct {
	mu      sync.Mutex
	count   int
	waiters []chan struct{}
}

var semTab handleTable[semState]

func semID(p unsafe.Pointer) *uint64 { return (*uint64)(p) }

//go:linkname SemInit
func SemInit(s unsafe.Pointer, pshared int32, value uint32) int32 {
	// pshared != 0 (process-shared via shared memory) can't be honored in a
	// single Go process; the semaphore is process-local regardless.
	st := &semState{count: int(value)}
	atomic.StoreUint64(semID(s), semTab.alloc(st))
	return 0
}

//go:linkname SemDestroy
func SemDestroy(s unsafe.Pointer) int32 {
	semTab.free(atomic.SwapUint64(semID(s), 0))
	return 0
}

//go:linkname SemWait
func SemWait(s unsafe.Pointer) int32 { return semWait(s, nil) }

//go:linkname SemTimedwait
func SemTimedwait(s, abstime unsafe.Pointer) int32 { return semWait(s, abstime) }

func semWait(s, abstime unsafe.Pointer) int32 {
	if abstime != nil && !tsValid(abstime) {
		return errEINVAL // musl sem_timedwait.c rejects tv_nsec out of range (#657)
	}
	st := semTab.get(atomic.LoadUint64(semID(s)))
	if st == nil {
		return errEINVAL
	}
	st.mu.Lock()
	if st.count > 0 {
		st.count--
		st.mu.Unlock()
		return 0
	}
	w := make(chan struct{}, 1)
	st.waiters = append(st.waiters, w)
	st.mu.Unlock()

	if abstime == nil {
		<-w // woken by a post that handed us the permit
		return 0
	}
	if waitWall(w, abstime) { // wall-clock slices: a realtime step is observed (#651)
		return 0
	}
	st.mu.Lock()
	if i := waiterIndex(st.waiters, w); i >= 0 {
		st.waiters = append(st.waiters[:i], st.waiters[i+1:]...)
		st.mu.Unlock()
		return errETIMEDOUT
	}
	st.mu.Unlock()
	<-w // a post already handed us the permit
	return 0
}

//go:linkname SemTrywait
func SemTrywait(s unsafe.Pointer) int32 {
	st := semTab.get(atomic.LoadUint64(semID(s)))
	if st == nil {
		return errEINVAL
	}
	st.mu.Lock()
	if st.count > 0 {
		st.count--
		st.mu.Unlock()
		return 0
	}
	st.mu.Unlock()
	return errEAGAIN
}

//go:linkname SemPost
func SemPost(s unsafe.Pointer) int32 {
	st := semTab.get(atomic.LoadUint64(semID(s)))
	if st == nil {
		return errEINVAL
	}
	st.mu.Lock()
	if len(st.waiters) > 0 {
		w := st.waiters[0]
		st.waiters = st.waiters[1:]
		w <- struct{}{} // direct permit handoff: count stays put
	} else {
		st.count++
	}
	st.mu.Unlock()
	return 0
}

//go:linkname SemGetvalue
func SemGetvalue(s, out unsafe.Pointer) int32 {
	st := semTab.get(atomic.LoadUint64(semID(s)))
	if st == nil {
		return errEINVAL
	}
	st.mu.Lock()
	v := st.count // >0 implies no waiters; Linux likewise reports 0 when waiters exist
	st.mu.Unlock()
	*(*int32)(out) = int32(v)
	return 0
}
