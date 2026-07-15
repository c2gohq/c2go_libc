// #653 setjmp/longjmp support (amd64). Design contract in sjlj.go and
// include/setjmp.h. All TEXTs are NOSPLIT with zero frame: no prologue, no
// morestack, so no stack move can interleave with the snapshot or the resume.
//
// amd64 facts this relies on (probed): CALL pushes the resume pc at 0(SP), so
// the caller's SP is SP+8; c2go codegen addresses locals through BP (frame
// base), so BP is saved and delta-restored; R14 is the g register (reserved
// by the c2go backend, maintained by the Go register ABI) — the init check in
// sjlj.go verifies this fail-closed at startup.

#include "textflag.h"

// func setjmp(env unsafe.Pointer) int32
TEXT ·setjmp(SB), NOSPLIT, $0-12
	MOVQ	env+0(FP), AX
	MOVQ	R14, 0(AX)	// e.g
	MOVQ	8(R14), CX	// g.stack.hi
	MOVQ	CX, 8(AX)	// e.stackhi
	LEAQ	8(SP), CX	// caller SP (what post-return code expects)
	MOVQ	CX, 16(AX)	// e.sp
	MOVQ	BP, 24(AX)	// e.bp — c2go frame base
	MOVQ	0(SP), DX	// resume pc (pushed by CALL)
	MOVQ	DX, 32(AX)	// e.pc
	LEAQ	ret+8(FP), DX
	SUBQ	CX, DX
	MOVQ	DX, 40(AX)	// e.retoff = &ret - caller SP
	MOVL	$0, ret+8(FP)	// first return: 0
	RET

// func longjmp(env unsafe.Pointer, val int32)
// ABI0 entry for the C world (identical arg layout): tail-jump to the Go
// implementation; this asm reference also makes the compiler emit longjmpGo's
// ABI0 wrapper.
TEXT ·longjmp(SB), NOSPLIT, $0-12
	JMP	·longjmpGo(SB)

// func sjG() uintptr
TEXT ·sjG(SB), NOSPLIT, $0-8
	MOVQ	R14, ret+0(FP)
	RET

// func sjStackLo() uintptr
TEXT ·sjStackLo(SB), NOSPLIT, $0-8
	MOVQ	0(R14), AX
	MOVQ	AX, ret+0(FP)
	RET

// func sjStackHi() uintptr
TEXT ·sjStackHi(SB), NOSPLIT, $0-8
	MOVQ	8(R14), AX
	MOVQ	AX, ret+0(FP)
	RET

// func sjResume(usp, ubp, upc, uret uintptr, val int32)
// The result-slot write lands ABOVE the restored SP (retoff > 0). SP is set
// immediately before the branch so no profiler/traceback window sees a
// half-restored frame.
TEXT ·sjResume(SB), NOSPLIT, $0-36
	MOVQ	usp+0(FP), AX
	MOVQ	ubp+8(FP), BX
	MOVQ	upc+16(FP), CX
	MOVQ	uret+24(FP), DX
	MOVL	val+32(FP), SI
	MOVL	SI, (DX)	// setjmp's second-return value
	MOVQ	BX, BP
	MOVQ	AX, SP
	JMP	CX
