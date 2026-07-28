//go:build darwin || freebsd || linux || netbsd

#include "textflag.h"

// cbinvoke(converter, frame uintptr): GoABI0-call converter(frame). amd64 Go
// ABI0 places the first arg at 0(SP) (the CALL pushes the return address).
TEXT ·cbinvoke(SB), NOSPLIT, $16-16
	MOVQ frame+8(FP), AX
	MOVQ AX, 0(SP)
	MOVQ converter+0(FP), DX
	CALL DX
	RET
