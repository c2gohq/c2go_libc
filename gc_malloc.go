// gc_malloc.go — the typed GC allocation entry.
//
// c2go's managed model splits allocation in two: ordinary malloc/calloc/
// realloc/free (malloc.go — make([]byte)-backed handle table) return
// GC-UNTRACKED bytes with C semantics, while GC-TRACKED objects are allocated
// EXPLICITLY through gc_malloc (C surface `gc_malloc(type_info, n)` in
// <c2go.h>, reached as this package's GCMalloc). The GC scans a gc_malloc'd
// block per its *runtime._type so managed pointers inside it stay live.
//
// This is c2go glue, not a musl function — it has no C source; it lives here
// because it must call runtime.mallocgc directly. Kept separate from malloc.go,
// which deliberately never routes through mallocgc.

package libc

import (
	"unsafe"
	_ "unsafe" // for go:linkname
)

// runtimeMallocgc is the one-sided linkname pull of runtime.mallocgc.
// runtime/malloc.go carries a `//go:linkname mallocgc` push and the comment
// "Do not remove or change the type signature" (go.dev/issue/67401), so this
// symbol is part of Go's backward-compat linkname surface for Go 1.22–1.25.
// `typ` is a *runtime._type; we type it as unsafe.Pointer because c2go-libc
// never inspects its layout — it only forwards the pointer the caller (a
// c2go_typeinfo() RTTI var) hands in. A null typ is the noscan signal
// mallocgc itself recognises.
//
//go:linkname runtimeMallocgc runtime.mallocgc
func runtimeMallocgc(size uintptr, typ unsafe.Pointer, needzero bool) unsafe.Pointer

// GCMalloc is the typed GC allocation entry (C: gc_malloc). It allocates `n`
// zeroed bytes on the Go heap, tracked per `typeInfo` (a *runtime._type; null
// = noscan blob). needzero is always true so the GC never scans stale heap
// garbage before the caller's first store.
//
// NOT //go:nosplit: runtime.mallocgc grows the stack and may trigger a GC, so
// GCMalloc must be an ordinary preemptible, stack-checked Go function.
//
//go:linkname GCMalloc
func GCMalloc(typeInfo unsafe.Pointer, n uint64) unsafe.Pointer {
	if n == 0 {
		return nil
	}
	if n > ^uint64(0)-8 {
		return nil // the +8 pad below would wrap; such a size never allocates anyway
	}
	if n > 1<<46 {
		// #661: runtime.mallocgc beyond maxAlloc is an unrecoverable fatal
		// throw, not a catchable panic (the #651 recover trick does not apply)
		// -- an explicit compare is the only guard. 1<<46 is far above any
		// real allocation and safely below every target's maxAlloc.
		setErrno(errENOMEM)
		return nil
	}
	// #588: over-allocate by one pointer word. C code routinely holds
	// one-past-the-end pointers (the canonical `p < base+n` loop-end idiom)
	// across safepoints; Go's precise GC resolves a past-the-end address as a
	// reference to the NEXT heap object ("marked free object" fatal if that
	// object is free). The pad keeps base+n inside this allocation. A non-
	// multiple-of-type size is fine: growslice's roundupsize does the same,
	// and the zeroed tail carries no heap bits, so the GC never scans it.
	return runtimeMallocgc(uintptr(n)+8, typeInfo, true)
}
