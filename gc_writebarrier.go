// gc_writebarrier.go — package-local slow path for generated write barriers.
//
// Managed C stores are lowered to an inline runtime.writeBarrier check. The
// uncommon enabled path calls this ordinary Go function so the Go compiler,
// rather than generated assembly, emits the runtime's current barrier sequence.

package libc

import "unsafe"

//go:nosplit
func _c2go_writePtr(slot *unsafe.Pointer, value unsafe.Pointer) {
	*slot = value
}
