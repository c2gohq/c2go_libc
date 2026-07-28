//go:build darwin || freebsd || linux || netbsd

// ABI0-exposed cgo dispatch entry for clang-generated .s wrappers.
//
// In the .s-wrapper model, clang emits, for each unmanaged_extern a TU uses, a
// Plan 9 ABI0 wrapper that marshals the ABI0-frame arguments into a syscallArgs
// block (the native C-ABI register layout it already computes) and then invokes
// the C function. The actual cgo transition (g0 switch + native call) is purego
// machinery reached through runtime.cgocall, which is awkward to call from ABI0
// asm directly. So we expose a tiny Go entry, c2goExternCall, under a stable
// flat symbol via //go:linkname; the .s wrapper just `CALL c2goExternCall(SB)`
// with a pointer to the marshaled block, exactly like a normal ABI0 call.

package dl

import "unsafe"

// runtime_cgocall is the runtime's cgo-call entry (made to work under
// CGO_ENABLED=0 by purego's fakecgo, pulled in transitively).
//
//go:linkname runtime_cgocall runtime.cgocall
func runtime_cgocall(fn uintptr, arg unsafe.Pointer) int32

// purego_syscallXABI0 holds the address of purego's syscallX asm trampoline,
// which loads a1..a8 into the integer registers, f1..f8 into the float
// registers, calls fn, and writes the results back into the block.
//
//go:linkname puregoSyscallXABI0 github.com/ebitengine/purego.syscallXABI0
var puregoSyscallXABI0 uintptr

// c2goExternCall runs the C function described by the pre-marshaled syscallArgs
// block s (built by a clang-generated .s wrapper) through purego's syscallX
// trampoline. It is published under the flat symbol `c2goExternCall` so the
// Plan 9 .s can call it as an ordinary ABI0 function.
//
//go:linkname c2goExternCall c2goExternCall
//go:nosplit
func c2goExternCall(s *syscallArgs) {
	runtime_cgocall(puregoSyscallXABI0, unsafe.Pointer(s))
}
