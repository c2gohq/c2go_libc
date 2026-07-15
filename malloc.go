// malloc.go — the libc malloc/free/calloc/realloc/aligned_alloc family,
// implemented in Go over per-allocation make([]byte) slices.
//
// Go's runtime allocator IS the malloc engine: every malloc() is one
// make([]byte) (size-classed, per-P cached, zeroed by the runtime). The only
// job left here is ROOTING — C code stores the returned pointer where the GC
// cannot see it (inside other malloc'd noscan bytes), so each live
// allocation's backing slice must be referenced from a Go-visible root until
// free(). That root is a dense handle table ([][]byte plus a free-index
// list). The handle index and the payload size live in a 16-byte header
// immediately before the returned pointer, which also guarantees max_align_t
// (16-byte) alignment and gives realloc its copy size without a table lookup.
//
// free() just drops the root (registry slot -> nil): the bytes become
// unreachable and the next GC cycle reclaims them — ordinary Go object
// lifetime, pacer-accounted. Deliberately NOT runtime.mallocgc with nil typ:
// a bare noscan block with no Go-side reference is reclaimed while C still
// holds pointers to it. GC-TRACKED allocation stays explicit via GCMalloc
// (gc_malloc.go).
//
// The C callers reach these through the c2go_linkname declarations in
// <stdlib.h> (github.com/c2gohq/c2go_libc.Malloc etc., GoABI0) — the same
// C-calls-Go pattern as gc_malloc/GCMalloc.
package libc

import (
	"sync"
	"unsafe"
	_ "unsafe" // for go:linkname
)

const (
	mallocHdr   = 16               // {idx uint64, size uint64} right before the payload
	mallocAlign = 16               // max_align_t
	mallocPad   = 8                // #588: keep one-past-the-end pointers inside the object
	mallocMaxSz = uint64(^uint(0) >> 1) // beyond this a make() would panic, not fail
)

var (
	mallocMu       sync.Mutex
	mallocRegistry [][]byte // handle table: index -> backing slice (nil = free slot)
	mallocFreeIdx  []int    // recycled registry indices
)

// mallocMake allocates the backing slice, converting the runtime's
// "makeslice: len out of range" panic — thrown for any total above the
// runtime's maxAlloc (~1<<48 on 64-bit, far BELOW the caller's MaxInt guard)
// — into a nil return, so malloc fails POSIX-style (NULL + ENOMEM) instead of
// panicking the process (#651). A true out-of-memory below maxAlloc is a
// fatal runtime throw, not a recoverable panic — a documented limit of
// running on the Go heap.
func mallocMake(total uint64) (raw []byte) {
	defer func() {
		if recover() != nil {
			raw = nil
		}
	}()
	return make([]byte, total)
}

// mallocAligned is the shared allocation core: one make([]byte) rooted in the
// handle table, {idx,size} header at p-16, payload aligned to `align` (a
// power of two >= 16).
func mallocAligned(n, align uint64) unsafe.Pointer {
	if n == 0 {
		return nil
	}
	total := n + mallocHdr + (align - 1) + mallocPad
	if total < n || total > mallocMaxSz {
		setErrno(errENOMEM) // size overflow: fail like malloc, don't panic
		return nil
	}
	raw := mallocMake(total)
	if raw == nil {
		setErrno(errENOMEM) // above the runtime's maxAlloc
		return nil
	}
	base := unsafe.Pointer(&raw[0])
	delta := 0
	if rem := (uintptr(base) + mallocHdr) & uintptr(align-1); rem != 0 {
		delta = int(uintptr(align) - rem)
	}
	p := unsafe.Add(base, mallocHdr+delta)

	mallocMu.Lock()
	var idx int
	if k := len(mallocFreeIdx); k > 0 {
		idx = mallocFreeIdx[k-1]
		mallocFreeIdx = mallocFreeIdx[:k-1]
		mallocRegistry[idx] = raw
	} else {
		idx = len(mallocRegistry)
		mallocRegistry = append(mallocRegistry, raw)
	}
	mallocMu.Unlock()

	*(*uint64)(unsafe.Add(p, -16)) = uint64(idx)
	*(*uint64)(unsafe.Add(p, -8)) = n
	return p
}

// mallocLookup validates that p is a live pointer previously returned by this
// allocator, via its header index. The containment check makes a stale or
// foreign pointer a no-op instead of corrupting an innocent registry slot.
// Caller holds mallocMu.
func mallocLookup(p unsafe.Pointer) (int, bool) {
	idx := int(*(*uint64)(unsafe.Add(p, -16)))
	if idx < 0 || idx >= len(mallocRegistry) {
		return 0, false
	}
	raw := mallocRegistry[idx]
	if raw == nil {
		return 0, false
	}
	b := uintptr(unsafe.Pointer(&raw[0]))
	q := uintptr(p)
	if q < b+mallocHdr || q > b+uintptr(len(raw)) {
		return 0, false
	}
	return idx, true
}

//go:linkname Malloc
func Malloc(nbytes uint64) unsafe.Pointer {
	if nbytes == 0 {
		nbytes = 1 // musl: malloc(0) returns a UNIQUE pointer, not NULL (#661)
	}
	return mallocAligned(nbytes, mallocAlign)
}

//go:linkname Calloc
func Calloc(nmemb uint64, size uint64) unsafe.Pointer {
	if nmemb != 0 && size != 0 && nmemb > ^uint64(0)/size {
		setErrno(errENOMEM) // musl calloc.c: overflow is ENOMEM (#657)
		return nil
	}
	total := nmemb * size
	if total == 0 {
		total = 1 // musl: calloc(0,·) is a unique pointer too (#661)
	}
	return mallocAligned(total, mallocAlign) // make([]byte) memory is zeroed
}

//go:linkname Realloc
func Realloc(ap unsafe.Pointer, nbytes uint64) unsafe.Pointer {
	if ap == nil {
		return Malloc(nbytes)
	}
	if nbytes == 0 {
		Free(ap)
		return Malloc(0) // musl: realloc(p,0) yields a fresh minimal allocation (#661)
	}
	mallocMu.Lock()
	_, ok := mallocLookup(ap)
	mallocMu.Unlock()
	if !ok {
		return nil
	}
	oldSize := *(*uint64)(unsafe.Add(ap, -8))
	np := Malloc(nbytes)
	if np == nil {
		return nil
	}
	copyN := oldSize
	if nbytes < copyN {
		copyN = nbytes
	}
	copy(unsafe.Slice((*byte)(np), copyN), unsafe.Slice((*byte)(ap), copyN))
	Free(ap)
	return np
}

//go:linkname Free
func Free(ap unsafe.Pointer) {
	if ap == nil {
		return
	}
	mallocMu.Lock()
	if idx, ok := mallocLookup(ap); ok {
		mallocRegistry[idx] = nil
		mallocFreeIdx = append(mallocFreeIdx, idx)
	}
	mallocMu.Unlock()
}

//go:linkname AlignedAlloc
func AlignedAlloc(align uint64, size uint64) unsafe.Pointer {
	if align == 0 || align&(align-1) != 0 {
		setErrno(errEINVAL) // musl aligned_alloc.c: bad alignment is EINVAL (#657)
		return nil
	}
	if align < mallocAlign {
		align = mallocAlign
	}
	return mallocAligned(size, align)
}
