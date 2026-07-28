// Float- and aggregate-aware native dispatch for the `unmanaged extern`
// boundary.
//
// The plain SyscallN above duplicates every argument into BOTH the integer and
// the floating-point register banks (purego's convenience trick). That works on
// Win64 (positional ABI) but mis-calls anything that mixes register classes,
// passes a struct by value, or returns a float / struct on SysV (amd64) and
// AAPCS64 (arm64).
//
// purego has the class-separated primitive internally: syscall_SyscallN takes
// the integer-class words and the float-class words in two separate slices plus
// the indirect-result pointer (AAPCS64 x8), runs the syscallX asm trampoline,
// and returns the integer (a1/a2/errno) and float (f1..f4) result registers. We
// reach it by //go:linkname.
//
// Unlike purego — which only has the Go reflect.Type and must re-derive the C
// ABI classification at run time (the eightbyte / HFA logic in its struct_*.go)
// — c2go has clang, which computes the exact per-target ABI lowering while
// compiling the extern declaration. c2go-bind emits, per argument and per
// return, a list of register/stack PIECES already classified by clang, and the
// generated wrapper fills them in. So this dispatcher carries NO ABI knowledge:
// it just sequences the pieces purego's primitive expects.

package dl

import (
	"math"
	"unsafe"
)

// Piece classes — where one machine word of an argument goes, as decided by
// clang's per-target ABI lowering.
const (
	PInt    = iota // next integer argument register, overflowing to the stack
	PFloat         // next floating-point argument register, overflowing to the stack
	PStack         // a stack slot (SysV "memory"-class byval struct word)
	PVararg        // a variadic (trailing `...`) integer word: register on SysV /
	// AAPCS64, but the Apple arm64 variadic ABI passes ALL
	// variadic args on the STACK regardless of type.
)

// Piece is one machine word of an argument together with its register class.
// c2go-bind reads the word out of the scalar / struct image and tags it with
// the class clang assigned.
type Piece struct {
	Class uint8
	Val   uintptr
}

// Return kinds.
const (
	RVoid = iota
	RInt
	RFloat
	RStruct
)

// RetWord describes one machine word of a register-returned struct: which
// result-register class it came from and where in the struct image to store it.
// c2go-bind emits these from clang's return coercion.
type RetWord struct {
	Float bool
	Off   uintptr
	Size  uintptr
}

// Call dispatches the C function fn.
//
//   - pieces are every argument's register/stack words in source order, already
//     classified by clang (PInt/PFloat/PStack).
//   - sret is the indirect-result pointer for a "memory"-class struct return
//     (caller-allocated); 0 otherwise. It is placed where the target ABI wants
//     it (amd64: a hidden leading integer arg; arm64: x8).
//   - retKind selects how the result comes back; for RStruct (register-
//     returned), retWords map the result registers into retDst, and for a
//     memory-class struct return (sret != 0) retDst was written by the callee.
//
// For scalar returns the caller reads the returned intRet (int/pointer, RInt)
// or floatRet (float, RFloat).
//
// POINTER LIFETIME: pointer words (PInt holding uintptr(p), or a pointer to a
// copied byref struct) are invisible to the GC inside pieces; the generated
// wrapper MUST runtime.KeepAlive the originating typed pointers across the call.
func Call(fn uintptr, pieces []Piece, sret uintptr, retKind uint8, retWords []RetWord, retDst unsafe.Pointer) (intRet, floatRet uintptr) {
	if fn == 0 {
		panic("c2go/dl: Call fn is nil")
	}
	return callNative(fn, pieces, sret, retKind, retWords, retDst)
}

// F32bits / F64bits return a float argument's IEEE-754 bit pattern in a uintptr
// (the float-register word Call expects), without taking the address of the
// argument — scalar float args are passed by value, and taking their address in
// a wrapper that needs an ABI0 trampoline confuses the Go ABI-wrapper generator.
func F32bits(v float32) uintptr { return uintptr(math.Float32bits(v)) }
func F64bits(v float64) uintptr { return uintptr(math.Float64bits(v)) }

// Word reads sz (1/2/4/8, or an odd struct-tail size) bytes at byte offset off
// of p, zero-extended into a uintptr — one machine word of an argument image
// for Call's Piece list. The generated wrapper takes the address of each
// argument and pulls its register words out with Word.
func Word(p unsafe.Pointer, off, sz uintptr) uintptr {
	switch sz {
	case 8:
		return *(*uintptr)(unsafe.Add(p, off))
	case 4:
		return uintptr(*(*uint32)(unsafe.Add(p, off)))
	case 2:
		return uintptr(*(*uint16)(unsafe.Add(p, off)))
	case 1:
		return uintptr(*(*uint8)(unsafe.Add(p, off)))
	}
	var w uintptr
	for i := uintptr(0); i < sz; i++ {
		w |= uintptr(*(*byte)(unsafe.Add(p, off+i))) << (i * 8)
	}
	return w
}

// WordSext is Word but sign-extends a signed sub-word scalar into the register
// (some ABIs, e.g. Apple arm64, require the caller to extend char/short args).
func WordSext(p unsafe.Pointer, off, sz uintptr) uintptr {
	switch sz {
	case 8:
		return *(*uintptr)(unsafe.Add(p, off))
	case 4:
		return uintptr(int64(*(*int32)(unsafe.Add(p, off))))
	case 2:
		return uintptr(int64(*(*int16)(unsafe.Add(p, off))))
	case 1:
		return uintptr(int64(*(*int8)(unsafe.Add(p, off))))
	}
	return Word(p, off, sz)
}
