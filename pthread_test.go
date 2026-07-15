//go:build unix

package libc

import (
	"sync"
	"syscall"
	"testing"
	"time"
	"unsafe"
)

// cMutex / cCond / cRWLock mirror the C struct layouts { size_t _id; unsigned
// long _pad[5]; } — 6 words, _id at offset 0. Heap-allocated (via new) so the
// address is stable and shareable across goroutines, exactly as a real shared
// object lives in a C global / on the malloc heap (never a moving goroutine
// stack).
type cMutex [6]uint64
type cCond [6]uint64
type cRWLock [6]uint64

func mptr(m *cMutex) unsafe.Pointer  { return unsafe.Pointer(m) }
func cptr(c *cCond) unsafe.Pointer   { return unsafe.Pointer(c) }
func rptr(r *cRWLock) unsafe.Pointer { return unsafe.Pointer(r) }

// absTS builds a POSIX absolute struct timespec { int64 tv_sec; int64 tv_nsec }
// at now+d, for the *_timedwait entry points.
func absTS(d time.Duration) unsafe.Pointer {
	t := time.Now().Add(d)
	ts := new([2]int64)
	ts[0] = t.Unix()
	ts[1] = int64(t.Nanosecond())
	return unsafe.Pointer(ts)
}

// TestPthreadMutexMutualExclusion is the load-bearing e2e for the foundation:
// M goroutines first-use the SAME zero-initialized mutex (racing lazyInit),
// then each increments a shared counter N times under the lock. A final value
// of M*N proves real mutual exclusion through the _id -> handle-table -> one
// sync.Mutex path. Run with -race, which also flags any unsynchronized access.
func TestPthreadMutexMutualExclusion(t *testing.T) {
	m := new(cMutex) // PTHREAD_MUTEX_INITIALIZER equivalent: all-zero, _id == 0
	const M, N = 16, 5000
	var counter int
	var wg sync.WaitGroup
	start := make(chan struct{})
	for g := 0; g < M; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			<-start
			for i := 0; i < N; i++ {
				if r := PthreadMutexLock(mptr(m)); r != 0 {
					t.Errorf("lock returned %d", r)
					return
				}
				counter++
				if r := PthreadMutexUnlock(mptr(m)); r != 0 {
					t.Errorf("unlock returned %d", r)
					return
				}
			}
		}()
	}
	close(start)
	wg.Wait()
	if counter != M*N {
		t.Fatalf("counter = %d, want %d (mutual exclusion violated)", counter, M*N)
	}
	if PthreadMutexDestroy(mptr(m)) != 0 {
		t.Fatal("destroy failed")
	}
	if m[0] != 0 {
		t.Fatalf("destroy left _id = %d, want 0", m[0])
	}
}

func TestPthreadMutexTryLock(t *testing.T) {
	m := new(cMutex)
	if r := PthreadMutexLock(mptr(m)); r != 0 {
		t.Fatalf("lock returned %d", r)
	}
	// A different goroutine must see the lock as held (TryLock from the SAME
	// goroutine is undefined for a plain sync.Mutex, so probe from another).
	res := make(chan int32, 1)
	go func() { res <- PthreadMutexTryLock(mptr(m)) }()
	if r := <-res; r != int32(syscall.EBUSY) {
		t.Fatalf("trylock on held mutex = %d, want EBUSY(%d)", r, syscall.EBUSY)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != 0 {
		t.Fatalf("unlock returned %d", r)
	}
	// Now uncontended: trylock succeeds, then release.
	if r := PthreadMutexTryLock(mptr(m)); r != 0 {
		t.Fatalf("trylock on free mutex = %d, want 0", r)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != 0 {
		t.Fatalf("unlock returned %d", r)
	}
	PthreadMutexDestroy(mptr(m))
}

func TestPthreadMutexUnlockUninitialized(t *testing.T) {
	m := new(cMutex) // never locked -> _id stays 0 -> no state in the table
	if r := PthreadMutexUnlock(mptr(m)); r != int32(syscall.EPERM) {
		t.Fatalf("unlock of uninitialized mutex = %d, want EPERM(%d)", r, syscall.EPERM)
	}
}

func TestPthreadMutexInitAttr(t *testing.T) {
	m := new(cMutex)
	// NULL attr = default (NORMAL): init succeeds.
	if r := PthreadMutexInit(mptr(m), nil); r != 0 {
		t.Fatalf("init(NULL attr) = %d, want 0", r)
	}
	// A pthread_mutexattr_t is { int _type; int _pad[5]; }; settype records the
	// kind, and NORMAL round-trips through init.
	var attr [6]int32
	ap := unsafe.Pointer(&attr)
	if r := PthreadMutexAttrSetType(ap, cMutexNormal); r != 0 {
		t.Fatalf("settype(NORMAL) = %d, want 0", r)
	}
	if r := PthreadMutexInit(mptr(m), ap); r != 0 {
		t.Fatalf("init(NORMAL) = %d, want 0", r)
	}
	// RECURSIVE round-trips through settype and init.
	if r := PthreadMutexAttrSetType(ap, cMutexRecursive); r != 0 {
		t.Fatalf("settype(RECURSIVE) = %d, want 0", r)
	}
	if r := PthreadMutexInit(mptr(m), ap); r != 0 {
		t.Fatalf("init(RECURSIVE) = %d, want 0", r)
	}
	PthreadMutexDestroy(mptr(m))
	// An out-of-range type is rejected at settype.
	if r := PthreadMutexAttrSetType(ap, 99); r != int32(syscall.EINVAL) {
		t.Fatalf("settype(99) = %d, want EINVAL(%d)", r, syscall.EINVAL)
	}
}

// initTyped is a helper: a fresh heap mutex initialized to the given kind.
func initTyped(t *testing.T, typ int32) *cMutex {
	t.Helper()
	m := new(cMutex)
	var attr [6]int32
	ap := unsafe.Pointer(&attr)
	if r := PthreadMutexAttrSetType(ap, typ); r != 0 {
		t.Fatalf("settype(%d) = %d", typ, r)
	}
	if r := PthreadMutexInit(mptr(m), ap); r != 0 {
		t.Fatalf("init(%d) = %d", typ, r)
	}
	return m
}

// TestPthreadMutexRecursive: the same goroutine locks `depth` deep without
// deadlocking, and a second goroutine cannot enter until the owner unlocks all
// the way out — so M goroutines recursing over a shared counter still land at
// M*N (mutual exclusion holds despite recursion).
func TestPthreadMutexRecursive(t *testing.T) {
	m := initTyped(t, cMutexRecursive)
	const M, N, depth = 8, 2000, 3
	counter := 0
	var wg sync.WaitGroup
	for g := 0; g < M; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := 0; i < N; i++ {
				for d := 0; d < depth; d++ {
					if r := PthreadMutexLock(mptr(m)); r != 0 {
						t.Errorf("recursive lock = %d", r)
						return
					}
				}
				counter++
				for d := 0; d < depth; d++ {
					if r := PthreadMutexUnlock(mptr(m)); r != 0 {
						t.Errorf("recursive unlock = %d", r)
						return
					}
				}
			}
		}()
	}
	wg.Wait()
	if counter != M*N {
		t.Fatalf("counter = %d, want %d (recursion broke mutual exclusion)", counter, M*N)
	}
	PthreadMutexDestroy(mptr(m))
}

// TestPthreadMutexRecursiveNonOwner: a goroutine that does not own the mutex
// gets EPERM from unlock.
func TestPthreadMutexRecursiveNonOwner(t *testing.T) {
	m := initTyped(t, cMutexRecursive)
	if r := PthreadMutexLock(mptr(m)); r != 0 {
		t.Fatalf("lock = %d", r)
	}
	res := make(chan int32, 1)
	go func() { res <- PthreadMutexUnlock(mptr(m)) }()
	if r := <-res; r != int32(syscall.EPERM) {
		t.Fatalf("non-owner unlock = %d, want EPERM(%d)", r, syscall.EPERM)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != 0 { // owner releases
		t.Fatalf("owner unlock = %d", r)
	}
	PthreadMutexDestroy(mptr(m))
}

// TestPthreadMutexErrorcheck: relock by the owner returns EDEADLK (not a
// deadlock); a non-owner unlock and an unlock-when-unlocked both return EPERM.
func TestPthreadMutexErrorcheck(t *testing.T) {
	m := initTyped(t, cMutexErrorcheck)
	if r := PthreadMutexLock(mptr(m)); r != 0 {
		t.Fatalf("lock = %d", r)
	}
	if r := PthreadMutexLock(mptr(m)); r != int32(syscall.EDEADLK) {
		t.Fatalf("owner relock = %d, want EDEADLK(%d)", r, syscall.EDEADLK)
	}
	res := make(chan int32, 1)
	go func() { res <- PthreadMutexUnlock(mptr(m)) }()
	if r := <-res; r != int32(syscall.EPERM) {
		t.Fatalf("non-owner unlock = %d, want EPERM(%d)", r, syscall.EPERM)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != 0 {
		t.Fatalf("owner unlock = %d", r)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != int32(syscall.EPERM) {
		t.Fatalf("unlock of unlocked = %d, want EPERM(%d)", r, syscall.EPERM)
	}
	PthreadMutexDestroy(mptr(m))
}

// TestPthreadCondSignal exercises the wait/signal handshake plus the mutex
// hand-off: a waiter blocks in cond_wait until the main goroutine sets a
// predicate under the mutex and signals. It must wake, re-acquire the mutex,
// and observe the predicate.
func TestPthreadCondSignal(t *testing.T) {
	m, c := new(cMutex), new(cCond)
	ready := false
	done := make(chan struct{})
	go func() {
		PthreadMutexLock(mptr(m))
		for !ready {
			PthreadCondWait(cptr(c), mptr(m))
		}
		PthreadMutexUnlock(mptr(m))
		close(done)
	}()
	time.Sleep(15 * time.Millisecond) // let the waiter park (exercise the blocking path)
	PthreadMutexLock(mptr(m))
	ready = true
	PthreadCondSignal(cptr(c))
	PthreadMutexUnlock(mptr(m))
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("cond_wait not woken by signal (deadlock)")
	}
}

// TestPthreadCondBroadcast: every waiter must wake on a single broadcast.
func TestPthreadCondBroadcast(t *testing.T) {
	m, c := new(cMutex), new(cCond)
	const M = 8
	ready := false
	var wg sync.WaitGroup
	for i := 0; i < M; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			PthreadMutexLock(mptr(m))
			for !ready {
				PthreadCondWait(cptr(c), mptr(m))
			}
			PthreadMutexUnlock(mptr(m))
		}()
	}
	time.Sleep(20 * time.Millisecond)
	PthreadMutexLock(mptr(m))
	ready = true
	PthreadCondBroadcast(cptr(c))
	PthreadMutexUnlock(mptr(m))
	waited := make(chan struct{})
	go func() { wg.Wait(); close(waited) }()
	select {
	case <-waited:
	case <-time.After(2 * time.Second):
		t.Fatal("broadcast did not wake all waiters")
	}
}

// TestPthreadCondTimedwait: with no signal the wait must time out AND re-acquire
// the mutex (the trailing unlock would fail/panic otherwise).
func TestPthreadCondTimedwait(t *testing.T) {
	m, c := new(cMutex), new(cCond)
	PthreadMutexLock(mptr(m))
	r := PthreadCondTimedwait(cptr(c), mptr(m), absTS(20*time.Millisecond))
	if r != int32(syscall.ETIMEDOUT) {
		t.Fatalf("timedwait = %d, want ETIMEDOUT(%d)", r, syscall.ETIMEDOUT)
	}
	if r := PthreadMutexUnlock(mptr(m)); r != 0 {
		t.Fatalf("mutex not re-held after timeout: unlock = %d", r)
	}
}

// TestPthreadCondErrorcheckMutex: cond_wait on an ERRORCHECK (owner-tracked)
// mutex must hand ownership back to the waking goroutine so its trailing unlock
// succeeds. Before condWait saved/restored owner+count, the raw ms.mu
// unlock/relock left a stale owner and this unlock wrongly returned EPERM.
func TestPthreadCondErrorcheckMutex(t *testing.T) {
	m := initTyped(t, cMutexErrorcheck)
	c := new(cCond)
	ready := false
	unlockRC := make(chan int32, 1)
	go func() {
		PthreadMutexLock(mptr(m))
		for !ready {
			PthreadCondWait(cptr(c), mptr(m))
		}
		unlockRC <- PthreadMutexUnlock(mptr(m)) // must be 0, not EPERM
	}()
	time.Sleep(15 * time.Millisecond) // let the waiter park inside cond_wait
	PthreadMutexLock(mptr(m))
	ready = true
	PthreadCondSignal(cptr(c))
	PthreadMutexUnlock(mptr(m))
	select {
	case r := <-unlockRC:
		if r != 0 {
			t.Fatalf("errorcheck unlock after cond_wait = %d, want 0 (stale owner)", r)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("waiter never returned from cond_wait")
	}
	PthreadMutexDestroy(mptr(m))
}

// TestPthreadKeyRootTracking: a pthread_key_t is a *pthreadKey stored in
// unmanaged C memory (GC-invisible), so the descriptor must be rooted in
// keyRoots to survive GC between create and use. White-box: verify the registry
// gains the descriptor on create and drops it on delete (the reachability that
// keeps it alive). A true use-after-free needs the key to live ONLY in C memory,
// which a Go test cannot stage, so this guards the rooting mechanism directly.
func TestPthreadKeyRootTracking(t *testing.T) {
	var k unsafe.Pointer
	if r := PthreadKeyCreate(&k, nil); r != 0 {
		t.Fatalf("key_create = %d", r)
	}
	desc := (*pthreadKey)(k)
	keyRoots.mu.Lock()
	_, rooted := keyRoots.m[desc]
	keyRoots.mu.Unlock()
	if !rooted {
		t.Fatal("descriptor not rooted after create (GC could reclaim it)")
	}
	if r := PthreadKeyDelete(k); r != 0 {
		t.Fatalf("key_delete = %d", r)
	}
	keyRoots.mu.Lock()
	_, stillRooted := keyRoots.m[desc]
	keyRoots.mu.Unlock()
	if stillRooted {
		t.Fatal("descriptor still rooted after delete")
	}
}

// TestPthreadRWLock: writers mutually exclude (counter == M*N), and the single
// pthread_rwlock_unlock correctly dispatches read vs write releases. Run with
// -race: a reader concurrent with a writer would trip it.
func TestPthreadRWLock(t *testing.T) {
	r := new(cRWLock)
	const M, N = 8, 3000
	counter := 0
	var wg sync.WaitGroup
	for i := 0; i < M; i++ {
		wg.Add(1)
		go func() { // writer
			defer wg.Done()
			for j := 0; j < N; j++ {
				PthreadRWLockWrlock(rptr(r))
				counter++
				PthreadRWLockUnlock(rptr(r))
			}
		}()
		wg.Add(1)
		go func() { // reader
			defer wg.Done()
			for j := 0; j < N; j++ {
				PthreadRWLockRdlock(rptr(r))
				_ = counter
				PthreadRWLockUnlock(rptr(r))
			}
		}()
	}
	wg.Wait()
	if counter != M*N {
		t.Fatalf("counter = %d, want %d (writer mutual exclusion violated)", counter, M*N)
	}
}
