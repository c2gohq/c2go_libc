// SPDX-License-Identifier: AGPL-3.0-only

package mlib

import (
	"runtime"
	"sync/atomic"
	"testing"
	"time"
	"unsafe"
)

// semCarrier mirrors c2go/mlib/semaphore.h. The real C carrier gets its GC
// pointer mask from C2Go; this Go mirror lets the state algorithm and direct
// pointer ownership be tested without a generated C fixture.
type semCarrier struct {
	state unsafe.Pointer
}

func TestSemaphoreStoresStateDirectly(t *testing.T) {
	var sem semCarrier
	if got := SemInit(unsafe.Pointer(&sem), 0, 1); got != 0 {
		t.Fatalf("SemInit = %d", got)
	}
	if semSlot(unsafe.Pointer(&sem)).Load() == nil {
		t.Fatal("SemInit did not store a direct state pointer in the carrier")
	}
	if got := SemTrywait(unsafe.Pointer(&sem)); got != 0 {
		t.Fatalf("first SemTrywait = %d", got)
	}
	if got := SemTrywait(unsafe.Pointer(&sem)); got != errEAGAIN {
		t.Fatalf("empty SemTrywait = %d, want %d", got, errEAGAIN)
	}
	if got := SemPost(unsafe.Pointer(&sem)); got != 0 {
		t.Fatalf("SemPost = %d", got)
	}
	var value int32
	if got := SemGetvalue(unsafe.Pointer(&sem), unsafe.Pointer(&value)); got != 0 || value != 1 {
		t.Fatalf("SemGetvalue = (%d, %d), want (0, 1)", got, value)
	}
	if got := SemDestroy(unsafe.Pointer(&sem)); got != 0 {
		t.Fatalf("SemDestroy = %d", got)
	}
	if semSlot(unsafe.Pointer(&sem)).Load() != nil {
		t.Fatal("SemDestroy did not clear the direct state pointer")
	}
	if got := SemPost(unsafe.Pointer(&sem)); got != errEINVAL {
		t.Fatalf("post after destroy = %d, want %d", got, errEINVAL)
	}
}

func TestSemaphoreValueRange(t *testing.T) {
	const max = uint32(1<<31 - 1)
	var sem semCarrier
	if got := SemInit(unsafe.Pointer(&sem), 0, max+1); got != errEINVAL {
		t.Fatalf("SemInit(SEM_VALUE_MAX+1) = %d, want %d", got, errEINVAL)
	}
	if semSlot(unsafe.Pointer(&sem)).Load() != nil {
		t.Fatal("failed SemInit published managed state")
	}
	if got := SemInit(unsafe.Pointer(&sem), 0, max); got != 0 {
		t.Fatalf("SemInit(SEM_VALUE_MAX) = %d", got)
	}
	var value int32
	if got := SemGetvalue(unsafe.Pointer(&sem), unsafe.Pointer(&value)); got != 0 || value != int32(max) {
		t.Fatalf("SemGetvalue = (%d, %d), want (0, %d)", got, value, max)
	}
	if got := SemPost(unsafe.Pointer(&sem)); got != errEOVERFLOW {
		t.Fatalf("SemPost at SEM_VALUE_MAX = %d, want %d", got, errEOVERFLOW)
	}
	if got := SemGetvalue(unsafe.Pointer(&sem), unsafe.Pointer(&value)); got != 0 || value != int32(max) {
		t.Fatalf("SemGetvalue after overflow = (%d, %d), want (0, %d)", got, value, max)
	}
	SemDestroy(unsafe.Pointer(&sem))
}

func TestSemaphoreDirectPointerSurvivesGCAndHandoff(t *testing.T) {
	var sem semCarrier
	if got := SemInit(unsafe.Pointer(&sem), 0, 0); got != 0 {
		t.Fatalf("SemInit = %d", got)
	}

	var woke atomic.Bool
	started := make(chan struct{})
	done := make(chan struct{})
	go func() {
		close(started)
		if got := SemWait(unsafe.Pointer(&sem)); got != 0 {
			t.Errorf("SemWait = %d", got)
		}
		woke.Store(true)
		close(done)
	}()
	<-started

	for i := 0; i < 8; i++ {
		runtime.GC()
	}
	if got := SemPost(unsafe.Pointer(&sem)); got != 0 {
		t.Fatalf("SemPost = %d", got)
	}
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("waiter did not wake after GC stress")
	}
	if !woke.Load() {
		t.Fatal("waiter completion was not observed")
	}
	SemDestroy(unsafe.Pointer(&sem))
}
