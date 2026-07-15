// Copyright 2026 c2go authors.
//
// c2go §B4 — c2go-libc GC-mask registry.
//
// The registry-consumer route ("§B4 phase 6") was REJECTED in #646: aliased
// global-pointer writes cannot be statically enumerated, so GC-root coverage
// for pointer-carrying globals is provided by CEDING their storage to
// Go-owned variables (zero-init aggregates included) plus the widened write-
// barrier pass — NOT by a runtime hook over this registry. Do not revive
// phase 6. The generated `RegisterGCMaskLookup` init calls still run (every
// generated package registers its `_c2goLookupGCMask`), so the registry stays
// as a DIAGNOSTIC surface only (GCMaskLookupCount).

package libc

import (
	"sync"
	"sync/atomic"
)

// GCMaskLookup is the signature of a c2go-bind-generated package's
// `_c2goLookupGCMask` function. Given a data-segment address, it
// returns the pointer bitmap, its pop-count, and ok=true on hit.
// A miss returns (nil, 0, false); callers fall through to the next
// registered lookup or to the no-mask path.
type GCMaskLookup func(addr uintptr) (mask []byte, ptrBits int, ok bool)

// gcMaskLookups holds the registered per-package lookup functions.
// Writers (init-time RegisterGCMaskLookup) serialise through
// gcMaskLookupsMu; readers do a single atomic load. The only reader is
// GCMaskLookupCount (diagnostic) — see the header: no runtime consumer
// will be added (#646 decision).
var (
	gcMaskLookupsMu sync.Mutex
	gcMaskLookups   atomic.Pointer[[]GCMaskLookup]
)

// RegisterGCMaskLookup is called from a c2go-bind-generated package's
// `init()` to plug its per-package `_c2goLookupGCMask` into the
// process-wide registry. Multiple packages can register independently.
func RegisterGCMaskLookup(fn GCMaskLookup) {
	if fn == nil {
		return
	}
	gcMaskLookupsMu.Lock()
	defer gcMaskLookupsMu.Unlock()
	old := gcMaskLookups.Load()
	var next []GCMaskLookup
	if old != nil {
		next = make([]GCMaskLookup, len(*old), len(*old)+1)
		copy(next, *old)
	}
	next = append(next, fn)
	gcMaskLookups.Store(&next)
}

// GCMaskLookupCount returns the number of c2go-bind-generated packages
// that have registered a lookup. Useful for sanity-checking init
// ordering — should match the number of c2go-bind packages linked.
func GCMaskLookupCount() int {
	snapshot := gcMaskLookups.Load()
	if snapshot == nil {
		return 0
	}
	return len(*snapshot)
}
