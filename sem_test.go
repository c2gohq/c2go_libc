//go:build unix

package libc

import (
	"sync"
	"syscall"
	"testing"
	"time"
	"unsafe"
)

// cSem mirrors sem_t { size_t _id; unsigned long _pad[3]; } — 4 words.
type cSem [4]uint64

func sptr(s *cSem) unsafe.Pointer { return unsafe.Pointer(s) }

// TestSemMutualExclusion uses a binary semaphore (init 1) as a lock: M
// goroutines each wait/increment/post N times; the count must land at M*N.
func TestSemMutualExclusion(t *testing.T) {
	s := new(cSem)
	if r := SemInit(sptr(s), 0, 1); r != 0 {
		t.Fatalf("sem_init = %d", r)
	}
	const M, N = 16, 3000
	counter := 0
	var wg sync.WaitGroup
	for i := 0; i < M; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < N; j++ {
				if r := SemWait(sptr(s)); r != 0 {
					t.Errorf("wait = %d", r)
					return
				}
				counter++
				if r := SemPost(sptr(s)); r != 0 {
					t.Errorf("post = %d", r)
					return
				}
			}
		}()
	}
	wg.Wait()
	if counter != M*N {
		t.Fatalf("counter = %d, want %d", counter, M*N)
	}
	SemDestroy(sptr(s))
}

// TestSemTrywaitGetvalue checks the count arithmetic and the non-blocking path.
func TestSemTrywaitGetvalue(t *testing.T) {
	s := new(cSem)
	SemInit(sptr(s), 0, 0)
	var val int32
	if SemGetvalue(sptr(s), unsafe.Pointer(&val)); val != 0 {
		t.Fatalf("getvalue after init(0) = %d, want 0", val)
	}
	if r := SemTrywait(sptr(s)); r != int32(syscall.EAGAIN) {
		t.Fatalf("trywait on empty = %d, want EAGAIN(%d)", r, syscall.EAGAIN)
	}
	SemPost(sptr(s))
	if SemGetvalue(sptr(s), unsafe.Pointer(&val)); val != 1 {
		t.Fatalf("getvalue after post = %d, want 1", val)
	}
	if r := SemTrywait(sptr(s)); r != 0 {
		t.Fatalf("trywait after post = %d, want 0", r)
	}
	if SemGetvalue(sptr(s), unsafe.Pointer(&val)); val != 0 {
		t.Fatalf("getvalue after trywait = %d, want 0", val)
	}
	SemDestroy(sptr(s))
}

// TestSemBlockingHandoff: a consumer blocks in sem_wait until a producer posts.
func TestSemBlockingHandoff(t *testing.T) {
	s := new(cSem)
	SemInit(sptr(s), 0, 0)
	done := make(chan struct{})
	go func() {
		SemWait(sptr(s)) // blocks until the post below
		close(done)
	}()
	time.Sleep(15 * time.Millisecond)
	SemPost(sptr(s))
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("sem_wait not released by post (deadlock)")
	}
	SemDestroy(sptr(s))
}

// TestSemTimedwait: an empty semaphore times out; after a post it succeeds.
func TestSemTimedwait(t *testing.T) {
	s := new(cSem)
	SemInit(sptr(s), 0, 0)
	if r := SemTimedwait(sptr(s), absTS(20*time.Millisecond)); r != int32(syscall.ETIMEDOUT) {
		t.Fatalf("timedwait on empty = %d, want ETIMEDOUT(%d)", r, syscall.ETIMEDOUT)
	}
	SemPost(sptr(s))
	if r := SemTimedwait(sptr(s), absTS(time.Second)); r != 0 {
		t.Fatalf("timedwait after post = %d, want 0", r)
	}
	SemDestroy(sptr(s))
}

// TestSemUninitialized: wait/post on a never-initialized sem is EINVAL.
func TestSemUninitialized(t *testing.T) {
	s := new(cSem) // _id stays 0
	if r := SemWait(sptr(s)); r != int32(syscall.EINVAL) {
		t.Fatalf("wait on uninitialized = %d, want EINVAL(%d)", r, syscall.EINVAL)
	}
	if r := SemPost(sptr(s)); r != int32(syscall.EINVAL) {
		t.Fatalf("post on uninitialized = %d, want EINVAL(%d)", r, syscall.EINVAL)
	}
}
