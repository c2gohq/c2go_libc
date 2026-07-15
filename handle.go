// handle.go — a dense id->object handle table: the shared rooting + identity
// substrate for the goroutine-backed POSIX sync objects (pthread mutex / cond /
// rwlock and the semaphore).
//
// Each such object is plain unmanaged C memory whose only meaningful field is a
// 64-bit `_id` (0 = not yet initialized, so PTHREAD_MUTEX_INITIALIZER's all-zero
// bytes mean "uninitialized"). The Go-side state that actually parks goroutines
// (a sync.Mutex / sync.Cond / channel) lives HERE, rooted by the registry so the
// GC keeps it alive as long as the C object references it. This is the same
// rooting idea as malloc.go's handle table, specialized to one *T per id rather
// than one []byte per allocation — and it lets the C struct hold no managed
// pointer at all (fully unmanaged, memcpy-tolerant, statically initializable).
//
// id = generation<<32 | (registry index + 1), so id 0 stays reserved for
// "uninitialized" and a recycled slot retires every stale copy of its old id
// (#658 M6). A freelist recycles indices, bounding the registry to the
// high-water count of concurrently-live objects. Approach (a): every operation is under one global
// RWMutex — the hot lock/unlock path only RESOLVES (get), so it takes the read
// lock and resolves in parallel, while init/destroy take the write lock. A
// RWMutex (not the plain Mutex malloc.go uses) is the right fit precisely
// because this table — unlike malloc's write-only one — is read-dominated in
// steady state: the real st.Lock() runs outside this lock, so after init nearly
// every table access is a get(). Measured ~15-30% faster than a Mutex at 4-8
// cores for that pattern; the read-lock's shared reader count still bounces
// between cores, so at high core counts this converges toward the plain Mutex
// and the real fix becomes a chunked lock-free table — which can replace this
// without changing the API.
//
// Values are stored as *T, never T by value: the registry slice is reallocated
// when it grows, which copies its elements — and Go forbids copying a used
// sync.Mutex — so only the pointers ever move, never the states they point at.
package libc

import (
	"sync"
	"sync/atomic"
)

// handleTable is a dense id->*T registry with index recycling. The zero value
// is ready to use. Each POSIX-sync primitive instantiates its own table over
// its own state type.
type handleTable[T any] struct {
	mu       sync.RWMutex
	registry []*T     // index -> live state (nil = free slot)
	gens     []uint32 // index -> generation, bumped on free (#658 M6: ABA guard)
	freelist []int    // recycled indices (LIFO)
}

// ids are generation-stamped (#658 M6): gen<<32 | (index+1). A stale id whose
// slot has been recycled no longer resolves (get -> nil) and no longer frees
// (free -> no-op), so a double close can never hit an innocent new occupant.
func (t *handleTable[T]) idOf(idx int) uint64 {
	return uint64(t.gens[idx])<<32 | uint64(idx+1)
}

// slotOf validates id against the current generation; -1 if stale/invalid.
// Caller holds at least the read lock.
func (t *handleTable[T]) slotOf(id uint64) int {
	idx := int(uint32(id)) - 1
	if idx < 0 || idx >= len(t.registry) || t.gens[idx] != uint32(id>>32) {
		return -1
	}
	return idx
}

// allocLocked roots st at a fresh or recycled index and returns that index.
// Caller holds the WRITE lock (it mutates registry/freelist).
func (t *handleTable[T]) allocLocked(st *T) int {
	if k := len(t.freelist); k > 0 {
		idx := t.freelist[k-1]
		t.freelist = t.freelist[:k-1]
		t.registry[idx] = st
		return idx
	}
	idx := len(t.registry)
	t.registry = append(t.registry, st)
	t.gens = append(t.gens, 0)
	return idx
}

// getLocked resolves id to its state, or nil if id is 0 or out of range.
// Caller holds at least the READ lock (it only reads registry).
func (t *handleTable[T]) getLocked(id uint64) *T {
	if id == 0 {
		return nil
	}
	idx := t.slotOf(id)
	if idx < 0 {
		return nil
	}
	return t.registry[idx]
}

// alloc roots st and returns its generation-stamped id (low half always >= 1).
func (t *handleTable[T]) alloc(st *T) uint64 {
	t.mu.Lock()
	idx := t.allocLocked(st)
	id := t.idOf(idx)
	t.mu.Unlock()
	return id
}

// get returns the state for id, or nil if id is 0 or has been freed. This is
// the hot path (one per lock/unlock), so it takes the read lock: many
// resolvers proceed in parallel.
func (t *handleTable[T]) get(id uint64) *T {
	t.mu.RLock()
	st := t.getLocked(id)
	t.mu.RUnlock()
	return st
}

// free drops the root for id (slot -> nil) and recycles the index. A double
// free or an unknown id is a no-op. After free the id is stale; reusing it is
// caller error (POSIX UB for a destroyed object), the same latitude malloc.go
// takes for use-after-free.
func (t *handleTable[T]) free(id uint64) {
	if id == 0 {
		return
	}
	t.mu.Lock()
	if idx := t.slotOf(id); idx >= 0 && t.registry[idx] != nil {
		t.registry[idx] = nil
		t.gens[idx]++ // retire every outstanding copy of this id
		t.freelist = append(t.freelist, idx)
	}
	t.mu.Unlock()
}

// lazyInit resolves the state for an object whose C memory holds *idp (its _id
// field). On first use (*idp == 0) it constructs a state with mk, roots it, and
// publishes the id; concurrent first-users are serialized by t.mu so exactly
// one state is ever created. This is the core call every primitive's
// lock/wait/post makes.
//
// idp must point at stable memory: a mutex shared across goroutines lives in a
// C global or on the C/malloc heap, never on a moving goroutine stack, so its
// address — and thus the atomic below — is stable.
func (t *handleTable[T]) lazyInit(idp *uint64, mk func() *T) *T {
	if id := atomic.LoadUint64(idp); id != 0 {
		return t.get(id)
	}
	t.mu.Lock()
	// Re-check under the table lock: the 0 -> nonzero publish happens here, so
	// at most one goroutine runs mk and appends.
	if id := atomic.LoadUint64(idp); id != 0 {
		st := t.getLocked(id)
		t.mu.Unlock()
		return st
	}
	st := mk()
	idx := t.allocLocked(st)
	atomic.StoreUint64(idp, t.idOf(idx))
	t.mu.Unlock()
	return st
}
