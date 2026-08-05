// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"runtime"
	"sync"
	"testing"
	"time"
	"unsafe"
)

type pthreadCarrier struct {
	state unsafe.Pointer
}

func pthreadCarrierPtr(carrier *pthreadCarrier) unsafe.Pointer {
	return unsafe.Pointer(carrier)
}

func pthreadAbsTimespec(after time.Duration) unsafe.Pointer {
	deadline := time.Now().Add(after)
	value := new([2]int64)
	value[0] = deadline.Unix()
	value[1] = int64(deadline.Nanosecond())
	return unsafe.Pointer(value)
}

func TestPthreadMutexDirectPointerAndMutualExclusion(t *testing.T) {
	mutex := new(pthreadCarrier)
	const goroutines, iterations = 12, 2000
	var counter int
	var wait sync.WaitGroup
	start := make(chan struct{})
	for i := 0; i < goroutines; i++ {
		wait.Add(1)
		go func() {
			defer wait.Done()
			<-start
			for j := 0; j < iterations; j++ {
				if result := PthreadMutexLock(pthreadCarrierPtr(mutex)); result != 0 {
					t.Errorf("lock = %d", result)
					return
				}
				counter++
				if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
					t.Errorf("unlock = %d", result)
					return
				}
			}
		}()
	}
	close(start)
	wait.Wait()
	if counter != goroutines*iterations {
		t.Fatalf("counter = %d, want %d", counter, goroutines*iterations)
	}
	if mutexSlot(pthreadCarrierPtr(mutex)).Load() == nil {
		t.Fatal("lazy mutex initialization did not publish a direct pointer")
	}
	for i := 0; i < 4; i++ {
		runtime.GC()
	}
	if result := PthreadMutexTryLock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("trylock after GC = %d", result)
	}
	if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("unlock after GC = %d", result)
	}
	PthreadMutexDestroy(pthreadCarrierPtr(mutex))
	if mutexSlot(pthreadCarrierPtr(mutex)).Load() != nil {
		t.Fatal("destroy did not clear the direct mutex pointer")
	}
}

func TestPthreadMutexKinds(t *testing.T) {
	var attr [24]byte
	if result := PthreadMutexAttrInit(unsafe.Pointer(&attr)); result != 0 {
		t.Fatalf("attr init = %d", result)
	}
	if result := PthreadMutexAttrSetType(unsafe.Pointer(&attr), mutexRecursive); result != 0 {
		t.Fatalf("attr set recursive = %d", result)
	}
	mutex := new(pthreadCarrier)
	if result := PthreadMutexInit(pthreadCarrierPtr(mutex), unsafe.Pointer(&attr)); result != 0 {
		t.Fatalf("recursive init = %d", result)
	}
	if result := PthreadMutexLock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("recursive lock 1 = %d", result)
	}
	if result := PthreadMutexLock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("recursive lock 2 = %d", result)
	}
	if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("recursive unlock 1 = %d", result)
	}
	if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("recursive unlock 2 = %d", result)
	}
	PthreadMutexDestroy(pthreadCarrierPtr(mutex))

	PthreadMutexAttrSetType(unsafe.Pointer(&attr), mutexErrorcheck)
	if result := PthreadMutexInit(pthreadCarrierPtr(mutex), unsafe.Pointer(&attr)); result != 0 {
		t.Fatalf("errorcheck init = %d", result)
	}
	if result := PthreadMutexLock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("errorcheck lock = %d", result)
	}
	if result := PthreadMutexLock(pthreadCarrierPtr(mutex)); result != errEDEADLK {
		t.Fatalf("errorcheck relock = %d, want %d", result, errEDEADLK)
	}
	if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("errorcheck unlock = %d", result)
	}
}

func TestPthreadCondDirectPointerSignalAndTimeout(t *testing.T) {
	mutex := new(pthreadCarrier)
	cond := new(pthreadCarrier)
	ready := false
	done := make(chan int32, 1)
	go func() {
		PthreadMutexLock(pthreadCarrierPtr(mutex))
		for !ready {
			if result := PthreadCondWait(pthreadCarrierPtr(cond), pthreadCarrierPtr(mutex)); result != 0 {
				done <- result
				return
			}
		}
		done <- PthreadMutexUnlock(pthreadCarrierPtr(mutex))
	}()
	time.Sleep(15 * time.Millisecond)
	PthreadMutexLock(pthreadCarrierPtr(mutex))
	ready = true
	PthreadCondSignal(pthreadCarrierPtr(cond))
	PthreadMutexUnlock(pthreadCarrierPtr(mutex))
	select {
	case result := <-done:
		if result != 0 {
			t.Fatalf("condition waiter = %d", result)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("condition waiter did not wake")
	}
	if condSlot(pthreadCarrierPtr(cond)).Load() == nil {
		t.Fatal("condition variable did not store a direct state pointer")
	}

	PthreadMutexLock(pthreadCarrierPtr(mutex))
	result := PthreadCondTimedwait(
		pthreadCarrierPtr(cond), pthreadCarrierPtr(mutex), pthreadAbsTimespec(15*time.Millisecond),
	)
	if result != errETIMEDOUT {
		t.Fatalf("timedwait = %d, want %d", result, errETIMEDOUT)
	}
	if result := PthreadMutexUnlock(pthreadCarrierPtr(mutex)); result != 0 {
		t.Fatalf("mutex was not reacquired after timeout: %d", result)
	}
}

func TestPthreadRWLockDirectPointer(t *testing.T) {
	rwlock := new(pthreadCarrier)
	if result := PthreadRWLockWrlock(pthreadCarrierPtr(rwlock)); result != 0 {
		t.Fatalf("wrlock = %d", result)
	}
	tryResult := make(chan int32, 1)
	go func() { tryResult <- PthreadRwlockTryrdlock(pthreadCarrierPtr(rwlock)) }()
	if result := <-tryResult; result != errEBUSY {
		t.Fatalf("tryrdlock while write-held = %d, want %d", result, errEBUSY)
	}
	if result := PthreadRWLockUnlock(pthreadCarrierPtr(rwlock)); result != 0 {
		t.Fatalf("write unlock = %d", result)
	}
	if result := PthreadRWLockRdlock(pthreadCarrierPtr(rwlock)); result != 0 {
		t.Fatalf("rdlock = %d", result)
	}
	if result := PthreadRWLockUnlock(pthreadCarrierPtr(rwlock)); result != 0 {
		t.Fatalf("read unlock = %d", result)
	}
	if rwlockSlot(pthreadCarrierPtr(rwlock)).Load() == nil {
		t.Fatal("rwlock did not store a direct state pointer")
	}
	PthreadRWLockDestroy(pthreadCarrierPtr(rwlock))
}
