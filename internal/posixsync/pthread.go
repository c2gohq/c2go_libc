// SPDX-License-Identifier: AGPL-3.0-only

package posixsync

import (
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/timandy/routine"
)

// SyncResult is independent of platform errno numbering. The root libc and
// mlib carrier layers translate it to their target's POSIX constants.
type SyncResult uint8

const (
	SyncOK SyncResult = iota
	SyncInvalid
	SyncBusy
	SyncPermission
	SyncDeadlock
	SyncTimedOut
)

// MutexKind matches the public PTHREAD_MUTEX_* values.
type MutexKind int32

const (
	MutexNormal MutexKind = iota
	MutexRecursive
	MutexErrorcheck
)

func ValidMutexKind(kind MutexKind) bool {
	return kind >= MutexNormal && kind <= MutexErrorcheck
}

// Mutex is the Go-owned state behind either an unmanaged pthread mutex ID or
// an mlib carrier's direct managed pointer.
type Mutex struct {
	mu    sync.Mutex
	kind  MutexKind
	owner atomic.Uint64
	count int32
}

func NewMutex(kind MutexKind) *Mutex { return &Mutex{kind: kind} }

func (m *Mutex) Lock() SyncResult {
	if m.kind == MutexNormal {
		m.mu.Lock()
		return SyncOK
	}
	self := uint64(routine.Goid())
	if m.owner.Load() == self {
		if m.kind == MutexErrorcheck {
			return SyncDeadlock
		}
		m.count++
		return SyncOK
	}
	m.mu.Lock()
	m.owner.Store(self)
	m.count = 1
	return SyncOK
}

func (m *Mutex) TryLock() SyncResult {
	if m.kind == MutexNormal {
		if m.mu.TryLock() {
			return SyncOK
		}
		return SyncBusy
	}
	self := uint64(routine.Goid())
	if m.owner.Load() == self {
		if m.kind == MutexRecursive {
			m.count++
			return SyncOK
		}
		return SyncBusy
	}
	if m.mu.TryLock() {
		m.owner.Store(self)
		m.count = 1
		return SyncOK
	}
	return SyncBusy
}

func (m *Mutex) TimedLock(abstime unsafe.Pointer) SyncResult {
	if abstime == nil || !TimespecValid(abstime) {
		return SyncInvalid
	}
	if result := m.TryLock(); result != SyncBusy {
		return result
	}
	sec := *(*int64)(abstime)
	nsec := *(*int64)(unsafe.Add(abstime, 8))
	deadline := time.Unix(sec, nsec)
	for {
		if time.Until(deadline) <= 0 {
			return SyncTimedOut
		}
		time.Sleep(10 * time.Millisecond)
		if result := m.TryLock(); result != SyncBusy {
			return result
		}
	}
}

func (m *Mutex) Unlock() SyncResult {
	if m.kind == MutexNormal {
		m.mu.Unlock()
		return SyncOK
	}
	if m.owner.Load() != uint64(routine.Goid()) {
		return SyncPermission
	}
	if m.kind == MutexRecursive {
		m.count--
		if m.count > 0 {
			return SyncOK
		}
	}
	m.owner.Store(0)
	m.mu.Unlock()
	return SyncOK
}

// RWLock is the Go-owned state behind either rwlock carrier.
type RWLock struct {
	rw        sync.RWMutex
	writeHeld atomic.Bool
}

func NewRWLock() *RWLock { return new(RWLock) }

func (rw *RWLock) ReadLock() SyncResult {
	rw.rw.RLock()
	return SyncOK
}

func (rw *RWLock) WriteLock() SyncResult {
	rw.rw.Lock()
	rw.writeHeld.Store(true)
	return SyncOK
}

func (rw *RWLock) TryReadLock() SyncResult {
	if rw.rw.TryRLock() {
		return SyncOK
	}
	return SyncBusy
}

func (rw *RWLock) TryWriteLock() SyncResult {
	if rw.rw.TryLock() {
		rw.writeHeld.Store(true)
		return SyncOK
	}
	return SyncBusy
}

func (rw *RWLock) Unlock() SyncResult {
	if rw.writeHeld.Load() {
		rw.writeHeld.Store(false)
		rw.rw.Unlock()
	} else {
		rw.rw.RUnlock()
	}
	return SyncOK
}

// Cond is a FIFO of one-shot waiter channels. Wait handles the associated
// Mutex's owner/recursion bookkeeping inside this package so both carrier
// layers exercise exactly the same release/reacquire behavior.
type Cond struct {
	mu      sync.Mutex
	waiters []chan struct{}
}

func NewCond() *Cond { return new(Cond) }

func (c *Cond) Wait(m *Mutex, abstime unsafe.Pointer) SyncResult {
	if abstime != nil && !TimespecValid(abstime) {
		return SyncInvalid
	}
	w := make(chan struct{}, 1)
	c.mu.Lock()
	c.waiters = append(c.waiters, w)
	c.mu.Unlock()

	savedCount := int32(0)
	if m.kind != MutexNormal {
		savedCount = m.count
		m.count = 0
		m.owner.Store(0)
	}
	m.mu.Unlock()

	result := SyncOK
	if abstime == nil {
		<-w
	} else if !WaitWall(w, abstime) {
		c.mu.Lock()
		if i := WaiterIndex(c.waiters, w); i >= 0 {
			c.waiters = append(c.waiters[:i], c.waiters[i+1:]...)
			result = SyncTimedOut
		} else {
			<-w
		}
		c.mu.Unlock()
	}

	m.mu.Lock()
	if m.kind != MutexNormal {
		m.owner.Store(uint64(routine.Goid()))
		m.count = savedCount
	}
	return result
}

func (c *Cond) Signal() {
	c.mu.Lock()
	if len(c.waiters) > 0 {
		waiter := c.waiters[0]
		c.waiters = c.waiters[1:]
		waiter <- struct{}{}
	}
	c.mu.Unlock()
}

func (c *Cond) Broadcast() {
	c.mu.Lock()
	waiters := c.waiters
	c.waiters = nil
	c.mu.Unlock()
	for _, waiter := range waiters {
		waiter <- struct{}{}
	}
}
