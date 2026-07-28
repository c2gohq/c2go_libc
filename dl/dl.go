package dl

import "github.com/ebitengine/purego"

// SyscallN invokes the C function pointer fn with up to 32 integer- or
// pointer-sized arguments. It returns the primary result register, secondary
// result register, and errno captured immediately after the call.
//
// Obtain fn from a //go:cgo_import_dynamic binding or Dlsym. Calling SyscallN
// with a zero function address panics. For mixed integer/floating-point or
// aggregate signatures, use Call with c2go-bind's Clang-derived ABI pieces.
func SyscallN(fn uintptr, args ...uintptr) (r1, r2, errno uintptr) {
	return purego.SyscallN(fn, args...)
}
