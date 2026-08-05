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
// symbol is part of Go's backward-compat linkname surface for Go 1.22–1.26.
// `typ` is a *runtime._type; we type it as unsafe.Pointer because c2go-libc
// never inspects its layout — it only forwards the pointer the caller (a
// c2go_typeinfo() RTTI var) hands in. A null typ is the noscan signal
// mallocgc itself recognises.
//
//go:linkname runtimeMallocgc runtime.mallocgc
func runtimeMallocgc(size uintptr, typ unsafe.Pointer, needzero bool) unsafe.Pointer

// GCMalloc is the typed GC allocation entry (C: gc_malloc). It allocates at
// least `n` zeroed bytes on the Go heap, tracked per `typeInfo` (a
// *runtime._type; null = noscan blob). needzero is always true so the GC never
// scans stale heap garbage before the caller's first store.
//
// NOT //go:nosplit: runtime.mallocgc grows the stack and may trigger a GC, so
// GCMalloc must be an ordinary preemptible, stack-checked Go function.
//
//go:linkname GCMalloc
func GCMalloc(typeInfo unsafe.Pointer, n uint64) unsafe.Pointer {
	if n == 0 {
		return nil
	}
	const maxManagedAllocation = uint64(1) << 46
	if n > maxManagedAllocation-8 {
		// #661: runtime.mallocgc beyond maxAlloc is an unrecoverable fatal
		// throw, not a catchable panic (the #651 recover trick does not apply)
		// -- an explicit compare is the only guard. 1<<46 is far above any
		// real allocation and safely below every target's maxAlloc.
		setErrno(errENOMEM)
		return nil
	}
	allocationSize := n + 8
	if typeInfo != nil {
		// runtime.mallocgc repeats typ's pointer bitmap over the requested byte
		// range. That range must be an exact multiple of typ.Size_. Go 1.26's
		// size-specialized allocator relies on this invariant when installing
		// span heap bits; violating it can set bits in the following object.
		//
		// Keep the one-past-end guarantee below by rounding n+8 up to a whole
		// extra typed element rather than appending an untyped pointer word.
		typeSize := uint64(*(*uintptr)(typeInfo)) // runtime._type.Size_ at offset 0
		if typeSize == 0 {
			// A zero-sized type cannot contain pointers. Passing it with a
			// non-zero allocation would make bitmap repetition ill-defined.
			typeInfo = nil
		} else if remainder := allocationSize % typeSize; remainder != 0 {
			padding := typeSize - remainder
			if padding > maxManagedAllocation-allocationSize {
				setErrno(errENOMEM)
				return nil
			}
			allocationSize += padding
		}
	}
	// #588: over-allocate by one pointer word. C code routinely holds
	// one-past-the-end pointers (the canonical `p < base+n` loop-end idiom)
	// across safepoints; Go's precise GC resolves a past-the-end address as a
	// reference to the NEXT heap object ("marked free object" fatal if that
	// object is free). The pad keeps base+n inside this allocation. For typed
	// allocations allocationSize is rounded to a whole number of type values,
	// which preserves runtime.mallocgc's bitmap-repetition contract.
	return runtimeMallocgc(uintptr(allocationSize), typeInfo, true)
}
