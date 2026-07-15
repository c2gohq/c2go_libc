// #653 setjmp/longjmp support (arm64). Design contract in sjlj.go and
// include/setjmp.h. All TEXTs are NOSPLIT|NOFRAME: no prologue, no morestack,
// so no stack move can interleave with the snapshot or the resume.
//
// arm64 facts this relies on (probed): BL does not move RSP, so RSP at entry
// == the caller's RSP at the call == what the caller's post-call code expects;
// LR holds the resume pc; c2go codegen is pure RSP-relative (zero R29 uses in
// the whole generated libc), so no frame base needs saving.

#include "textflag.h"

// func setjmp(env unsafe.Pointer) int32
TEXT ·setjmp(SB), NOSPLIT|NOFRAME, $0-12
	MOVD	env+0(FP), R8
	MOVD	g, R9
	MOVD	R9, 0(R8)	// e.g
	MOVD	8(g), R9	// g.stack.hi (offset anchored by init in sjlj.go)
	MOVD	R9, 8(R8)	// e.stackhi
	MOVD	RSP, R10
	MOVD	R10, 16(R8)	// e.sp
	MOVD	ZR, R9
	MOVD	R9, 24(R8)	// e.bp — unused on arm64
	MOVD	LR, R9
	MOVD	R9, 32(R8)	// e.pc
	MOVD	$ret+8(FP), R9
	SUB	R10, R9, R9
	MOVD	R9, 40(R8)	// e.retoff = &ret - sp
	MOVW	ZR, ret+8(FP)	// first return: 0
	RET

// func longjmp(env unsafe.Pointer, val int32)
// ABI0 entry for the C world (identical arg layout): tail-jump to the Go
// implementation; this asm reference also makes the compiler emit longjmpGo's
// ABI0 wrapper.
TEXT ·longjmp(SB), NOSPLIT|NOFRAME, $0-12
	JMP	·longjmpGo(SB)

// func sjG() uintptr
TEXT ·sjG(SB), NOSPLIT|NOFRAME, $0-8
	MOVD	g, R8
	MOVD	R8, ret+0(FP)
	RET

// func sjStackLo() uintptr
TEXT ·sjStackLo(SB), NOSPLIT|NOFRAME, $0-8
	MOVD	0(g), R8
	MOVD	R8, ret+0(FP)
	RET

// func sjStackHi() uintptr
TEXT ·sjStackHi(SB), NOSPLIT|NOFRAME, $0-8
	MOVD	8(g), R8
	MOVD	R8, ret+0(FP)
	RET

// func sjResume(usp, ubp, upc, uret uintptr, val int32)
// The result-slot write lands ABOVE the restored SP (retoff > 0), i.e. in the
// live region of the resumed frame. SP is set immediately before the branch
// so no profiler/traceback window sees a half-restored frame.
TEXT ·sjResume(SB), NOSPLIT|NOFRAME, $0-36
	MOVD	usp+0(FP), R8
	MOVD	upc+16(FP), R9
	MOVD	uret+24(FP), R10
	MOVW	val+32(FP), R11
	MOVW	R11, (R10)	// setjmp's second-return value
	MOVD	R8, RSP
	B	(R9)
