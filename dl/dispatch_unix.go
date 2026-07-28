//go:build darwin || freebsd || linux || netbsd

package dl

import (
	"runtime"
	"unsafe"
)

// syscallArgs mirrors purego.syscallArgs (purego/syscall.go) field-for-field.
// This private ABI is pinned to the PureGo version in go.mod and guarded by the
// dl tests and clean-checkout cross-build matrix.
type syscallArgs struct {
	fn, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15                uintptr
	a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32 uintptr
	f1, f2, f3, f4, f5, f6, f7, f8                                                      uintptr
	arm64_r8                                                                            uintptr
}

// puregoSyscallN is PureGo's class-separated native dispatcher. It reads fixed
// 32-word integer and 8-word floating-point banks.
//
//go:linkname puregoSyscallN github.com/ebitengine/purego.syscall_SyscallN
func puregoSyscallN(fn uintptr, sysargs []uintptr, floats []uintptr, r8 uintptr) *syscallArgs

func numIntArgRegs() int {
	if runtime.GOARCH == "amd64" {
		return 6
	}
	return 8
}

func callNative(fn uintptr, pieces []Piece, sret uintptr, retKind uint8, retWords []RetWord, retDst unsafe.Pointer) (intRet, floatRet uintptr) {
	var sysargs [32]uintptr
	var floats [32]uintptr
	numInts, numFloats, numStack := 0, 0, 0
	intRegs := numIntArgRegs()

	addStack := func(x uintptr) {
		if intRegs+numStack >= len(sysargs) {
			panic("c2go/dl: too many native stack argument words")
		}
		sysargs[intRegs+numStack] = x
		numStack++
	}
	addInt := func(x uintptr) {
		if numInts >= intRegs {
			addStack(x)
		} else {
			sysargs[numInts] = x
			numInts++
		}
	}
	addFloat := func(x uintptr) {
		if numFloats < 8 {
			floats[numFloats] = x
			numFloats++
		} else {
			addStack(x)
		}
	}

	var r8 uintptr
	if sret != 0 {
		if runtime.GOARCH == "amd64" {
			addInt(sret)
		} else {
			r8 = sret
		}
	}

	darwinArm64 := runtime.GOOS == "darwin" && runtime.GOARCH == "arm64"
	for _, p := range pieces {
		switch p.Class {
		case PInt:
			addInt(p.Val)
		case PFloat:
			addFloat(p.Val)
		case PStack:
			addStack(p.Val)
		case PVararg:
			if darwinArm64 {
				addStack(p.Val)
			} else {
				addInt(p.Val)
			}
		default:
			panic("c2go/dl: invalid argument piece class")
		}
	}

	s := puregoSyscallN(fn, sysargs[:], floats[:], r8)
	switch retKind {
	case RVoid:
		return 0, 0
	case RInt:
		return s.a1, 0
	case RFloat:
		return 0, s.f1
	case RStruct:
		if retDst == nil {
			panic("c2go/dl: nil struct return destination")
		}
		fregs := [4]uintptr{s.f1, s.f2, s.f3, s.f4}
		iregs := [2]uintptr{s.a1, s.a2}
		ni, nf := 0, 0
		for _, word := range retWords {
			var src uintptr
			if word.Float {
				if nf >= len(fregs) {
					panic("c2go/dl: too many floating-point return words")
				}
				src = fregs[nf]
				nf++
			} else {
				if ni >= len(iregs) {
					panic("c2go/dl: too many integer return words")
				}
				src = iregs[ni]
				ni++
			}
			copy(unsafe.Slice((*byte)(unsafe.Add(retDst, word.Off)), word.Size),
				unsafe.Slice((*byte)(unsafe.Pointer(&src)), word.Size))
		}
		return 0, 0
	default:
		panic("c2go/dl: invalid return kind")
	}
}
