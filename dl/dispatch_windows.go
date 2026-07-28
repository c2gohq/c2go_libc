//go:build windows

package dl

import (
	"syscall"
	"unsafe"
)

// Win64 uses positional argument slots. syscall.SyscallN duplicates the first
// four words into the XMM registers and reports an XMM0 return in r2.
func callNative(fn uintptr, pieces []Piece, sret uintptr, retKind uint8, _ []RetWord, _ unsafe.Pointer) (intRet, floatRet uintptr) {
	if retKind == RStruct || sret != 0 {
		panic("c2go/dl: windows struct extern not implemented")
	}
	var args [32]uintptr
	n := 0
	for _, piece := range pieces {
		if piece.Class == PStack {
			panic("c2go/dl: windows struct-by-value extern not implemented")
		}
		if piece.Class > PVararg {
			panic("c2go/dl: invalid argument piece class")
		}
		if n == len(args) {
			panic("c2go/dl: too many native argument words")
		}
		args[n] = piece.Val
		n++
	}
	r1, r2, _ := syscall.SyscallN(fn, args[:n]...)
	switch retKind {
	case RVoid:
		return 0, 0
	case RInt:
		return r1, 0
	case RFloat:
		return 0, r2
	default:
		panic("c2go/dl: invalid return kind")
	}
}
