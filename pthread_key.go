// pthread_key.go — thread-specific data: pthread_key_create / delete /
// getspecific / setspecific, with POSIX destructor-on-thread-exit.
//
// A pthread_key_t (void* in <pthread.h>) is the address of a heap pthreadKey
// descriptor carrying the key's destructor and a deleted flag. The descriptor is
// rooted in keyRoots (below): the pthread_key_t itself lives in UNMANAGED C
// memory, which the GC does not scan, so without a Go-side root the descriptor
// would be collectible between key_create and the first setspecific (or after
// every holder clears), leaving C with a dangling key.
//
// A stored value is a POSIX void* held as an unsafe.Pointer, so a value that is
// a real managed pointer is kept alive while set. Like every other pthread void*
// payload in c2go, a value must be a real pointer or NULL — a fabricated integer
// (`pthread_setspecific(k, (void*)n)`) would land a non-pointer in a GC-scanned
// slot and trip precise GC; that idiom is unsupported (see the thread.go note).
//
// Destructors run on the exiting goroutine: promptly for a pthread_create thread
// (thread.go's finish defer calls runKeyDestructors), and best-effort for any
// other goroutine via a runtime.AddCleanup armed on first setspecific.

package libc

import (
	"runtime"
	"sync"
	"sync/atomic"
	"unsafe"
)

// pthreadKey is the descriptor a pthread_key_t (void*) points at.
type pthreadKey struct {
	destructor unsafe.Pointer // C void(*)(void*), or nil
	deleted    atomic.Bool
}

// keyRoots keeps every live pthreadKey descriptor reachable from the Go heap
// (the C-side pthread_key_t is unmanaged and does not root it). Entries are
// removed on pthread_key_delete.
var keyRoots = struct {
	mu sync.Mutex
	m  map[*pthreadKey]struct{}
}{m: map[*pthreadKey]struct{}{}}

// keyID is the C-facing pthread_key_t value: a *pthreadKey as a void*.
type keyID = unsafe.Pointer

func keyDesc(key keyID) *pthreadKey { return (*pthreadKey)(key) }

// PTHREAD_DESTRUCTOR_ITERATIONS: POSIX bound on destructor rounds when a
// destructor itself sets new TLS values.
const pthreadDestructorIterations = 4

//go:linkname PthreadKeyCreate
func PthreadKeyCreate(key *keyID, destructor unsafe.Pointer) int32 {
	if key == nil {
		return errEINVAL
	}
	k := &pthreadKey{destructor: destructor}
	keyRoots.mu.Lock()
	keyRoots.m[k] = struct{}{} // root before publishing to C
	keyRoots.mu.Unlock()
	*key = unsafe.Pointer(k)
	return 0
}

//go:linkname PthreadKeyDelete
func PthreadKeyDelete(key keyID) int32 {
	if k := keyDesc(key); k != nil {
		// Mark deleted: subsequent get/set see it and the destructor stops
		// running. Other goroutines' stored values cannot be reached to erase
		// (that would race their get/set), but the flag makes them inert and
		// they are reclaimed when their goroutines die / clear them.
		k.deleted.Store(true)
		keyRoots.mu.Lock()
		delete(keyRoots.m, k) // the key is gone; drop the root
		keyRoots.mu.Unlock()
	}
	return 0
}

//go:linkname PthreadGetSpecific
func PthreadGetSpecific(key keyID) unsafe.Pointer {
	k := keyDesc(key)
	if k == nil || k.deleted.Load() {
		return nil
	}
	if ts := glsLocal.Get(); ts != nil && ts.tls != nil {
		return ts.tls[k]
	}
	return nil
}

//go:linkname PthreadSetSpecific
func PthreadSetSpecific(key keyID, value unsafe.Pointer) int32 {
	k := keyDesc(key)
	if k == nil || k.deleted.Load() {
		return errEINVAL
	}
	ts := glsLookup()
	if ts.tls == nil {
		ts.tls = make(map[*pthreadKey]unsafe.Pointer)
		// Arm best-effort destructor-on-death for a goroutine that is NOT a
		// pthread_create thread (those run destructors promptly from thread.go's
		// finish defer). keyDeath is a distinct allocation from the map arg so
		// the map stays unreachable-from-obj and the cleanup can fire; a later
		// prompt run leaves all slots nil, so this second pass no-ops.
		ts.keyDeath = new(byte)
		runtime.AddCleanup(ts.keyDeath, runKeyDestructorsMap, ts.tls)
	}
	ts.tls[k] = value
	return 0
}

// runKeyDestructors runs the current goroutine's TSD destructors as it exits.
// Called from the pthread finish defer (thread.go); a no-op if no keys were set.
func runKeyDestructors() {
	if ts := glsLocal.Get(); ts != nil {
		runKeyDestructorsMap(ts.tls)
	}
}

// runKeyDestructorsMap invokes destructors for non-nil values in a goroutine's
// TSD map, clearing each slot as it runs, looping up to PTHREAD_DESTRUCTOR_-
// ITERATIONS times in case a destructor sets new values. Safe to call twice
// (a second pass finds all slots nil): the prompt pthread-exit path and the
// GC-cleanup path can both reach it. Map keys are live *pthreadKey, so each
// descriptor is valid here.
func runKeyDestructorsMap(m map[*pthreadKey]unsafe.Pointer) {
	if m == nil {
		return
	}
	for round := 0; round < pthreadDestructorIterations; round++ {
		moreWork := false
		for k, val := range m {
			if val == nil || k.destructor == nil || k.deleted.Load() {
				continue
			}
			m[k] = nil
			C2goPthreadRunDtor(uintptr(k.destructor), val) // call destructor(val) in C
			moreWork = true
		}
		if !moreWork {
			return
		}
	}
}
