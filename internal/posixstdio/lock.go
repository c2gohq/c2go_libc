// SPDX-License-Identifier: AGPL-3.0-only

// Package posixstdio contains state shared by the unmanaged and managed FILE
// carriers. Carrier ownership stays in the public packages: root libc stores a
// generation-stamped handle, while mlib stores a direct GC-visible pointer.
package posixstdio

import (
	"sync"
	"sync/atomic"

	"github.com/timandy/routine"
)

// Lock is the recursive, goroutine-owned lock behind a FILE. It mirrors musl's
// recursive flockfile behavior while allowing one stream to block without
// stopping unrelated streams.
type Lock struct {
	mu    sync.Mutex
	owner atomic.Uint64 // goroutine id of the holder, 0 when unowned
	count int32         // recursion depth, guarded by ownership
}

func (l *Lock) Lock() {
	self := uint64(routine.Goid())
	if l.owner.Load() == self {
		l.count++
		return
	}
	l.mu.Lock()
	l.owner.Store(self)
	l.count = 1
}

// TryLock returns true after acquiring the lock and false when another
// goroutine owns it. Recursive acquisition by the owner succeeds.
func (l *Lock) TryLock() bool {
	self := uint64(routine.Goid())
	if l.owner.Load() == self {
		l.count++
		return true
	}
	if !l.mu.TryLock() {
		return false
	}
	l.owner.Store(self)
	l.count = 1
	return true
}

func (l *Lock) Unlock() {
	if l.count--; l.count == 0 {
		l.owner.Store(0)
		l.mu.Unlock()
	}
}
