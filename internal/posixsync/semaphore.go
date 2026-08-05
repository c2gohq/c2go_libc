// SPDX-License-Identifier: AGPL-3.0-only

// Package posixsync contains state algorithms shared by the unmanaged libc
// carriers and the managed mlib carriers. It deliberately knows nothing about
// IDs, handle tables, or C carrier layouts.
package posixsync

import (
	"sync"
	"time"
	"unsafe"
)

type SemResult uint8

const (
	SemOK SemResult = iota
	SemInvalid
	SemWouldBlock
	SemTimedOut
	SemOverflow
)

// SemaphoreValueMax is POSIX SEM_VALUE_MAX. Keeping the invariant in the
// shared state prevents Value's int32 result from wrapping in either carrier.
const SemaphoreValueMax uint32 = 1<<31 - 1

// Semaphore is the Go-owned state behind either kind of C carrier.
type Semaphore struct {
	mu      sync.Mutex
	count   int
	waiters []chan struct{}
}

func NewSemaphore(value uint32) (*Semaphore, SemResult) {
	if value > SemaphoreValueMax {
		return nil, SemInvalid
	}
	return &Semaphore{count: int(value)}, SemOK
}

func (s *Semaphore) Wait(abstime unsafe.Pointer) SemResult {
	if abstime != nil && !TimespecValid(abstime) {
		return SemInvalid
	}
	s.mu.Lock()
	if s.count > 0 {
		s.count--
		s.mu.Unlock()
		return SemOK
	}
	w := make(chan struct{}, 1)
	s.waiters = append(s.waiters, w)
	s.mu.Unlock()

	if abstime == nil {
		<-w
		return SemOK
	}
	if WaitWall(w, abstime) {
		return SemOK
	}
	s.mu.Lock()
	if i := WaiterIndex(s.waiters, w); i >= 0 {
		s.waiters = append(s.waiters[:i], s.waiters[i+1:]...)
		s.mu.Unlock()
		return SemTimedOut
	}
	s.mu.Unlock()
	<-w
	return SemOK
}

func (s *Semaphore) TryWait() SemResult {
	s.mu.Lock()
	if s.count > 0 {
		s.count--
		s.mu.Unlock()
		return SemOK
	}
	s.mu.Unlock()
	return SemWouldBlock
}

func (s *Semaphore) Post() SemResult {
	s.mu.Lock()
	if len(s.waiters) > 0 {
		w := s.waiters[0]
		s.waiters = s.waiters[1:]
		w <- struct{}{}
	} else {
		if s.count >= int(SemaphoreValueMax) {
			s.mu.Unlock()
			return SemOverflow
		}
		s.count++
	}
	s.mu.Unlock()
	return SemOK
}

func (s *Semaphore) Value() int32 {
	s.mu.Lock()
	value := int32(s.count)
	s.mu.Unlock()
	return value
}

// TimespecValid applies musl's __timedwait tv_nsec validation.
func TimespecValid(ts unsafe.Pointer) bool {
	if ts == nil {
		return false
	}
	nsec := *(*int64)(unsafe.Add(ts, 8))
	return nsec >= 0 && nsec < 1_000_000_000
}

// WaitWall waits until the CLOCK_REALTIME deadline, re-deriving the remaining
// duration in bounded slices so a wall-clock step is observed.
func WaitWall(w chan struct{}, ts unsafe.Pointer) bool {
	const slice = 250 * time.Millisecond
	sec := *(*int64)(ts)
	nsec := *(*int64)(unsafe.Add(ts, 8))
	deadline := time.Unix(sec, nsec)
	t := time.NewTimer(slice)
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

// WaiterIndex finds target in a FIFO waiter slice, or returns -1.
func WaiterIndex(waiters []chan struct{}, target chan struct{}) int {
	for i, waiter := range waiters {
		if waiter == target {
			return i
		}
	}
	return -1
}
