//go:build darwin || freebsd || linux || netbsd

#include "textflag.h"

// cbinvoke(converter, frame uintptr): GoABI0-call converter(frame). The clang
// converter is a Go ABI0 function taking the frame pointer; place it in the
// ABI0 stack-arg slot (caller_sp+8 on arm64) and call indirectly.
TEXT ·cbinvoke(SB), NOSPLIT, $16-16
	MOVD frame+8(FP), R0
	MOVD R0, 8(RSP)
	MOVD converter+0(FP), R9
	CALL (R9)
	RET
