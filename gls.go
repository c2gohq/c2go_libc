// gls.go — goroutine-local storage for c2go-libc, backed by
// github.com/timandy/routine.
//
// Per-goroutine c2go-libc state lives in a single *tlsState held in a
// routine.ThreadLocal. timandy stores it in the goroutine's g.labels slot
// (profiler-safe: its `thread` struct leads with a labelMap) and transparently
// recreates it when a recycled g reports a new goid. NON-inheritable: a new
// pthread (a goroutine spawned by pthread_create) starts with fresh state,
// matching POSIX — errno and __thread do not inherit across pthread_create.
//
// The field set grows as support lands: errno, plus pthread_key thread-specific
// data (tls / keyDeath, see pthread_key.go).
package libc

import (
	"unsafe"

	"github.com/timandy/routine"
)

// tlsState holds per-goroutine c2go-libc state.
type tlsState struct {
	errno int32 // C errno value

	// pthread_key thread-specific data: value per key descriptor, populated
	// lazily by pthread_setspecific. keyDeath is a distinct allocation whose
	// GC-cleanup runs destructors if a non-pthread goroutine dies holding
	// values (pthread_create threads run them promptly on exit instead).
	tls      map[*pthreadKey]unsafe.Pointer
	keyDeath *byte

	// threadDeath is the sentinel for reaping a pthread_self() record allocated on
	// a goroutine that was NOT created by pthread_create (see thread.go's
	// PthreadSelf). Held only here (in g.labels), so the goroutine's death makes it
	// unreachable and fires the AddCleanup that frees the threadTab slot.
	threadDeath *byte
}

// glsLocal holds each goroutine's *tlsState. NON-inheritable (pthread TLS
// semantics: no copy to child goroutines).
var glsLocal = routine.NewThreadLocal[*tlsState]()

// glsLookup returns the current goroutine's tlsState, creating it on first
// access.
func glsLookup() *tlsState {
	if ts := glsLocal.Get(); ts != nil {
		return ts
	}
	ts := &tlsState{}
	glsLocal.Set(ts)
	return ts
}

// glsRegisterIfAbsent is retained for source compatibility with existing
// callers; identical to glsLookup now that timandy handles storage and
// g-recycle detection.
func glsRegisterIfAbsent() *tlsState { return glsLookup() }
