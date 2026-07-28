//go:build darwin || freebsd || linux || netbsd

// Flat linker symbols referenced directly by clang-synthesized GoABI0 .s
// wrappers (the .s-wrapper model). A clang wrapper for an unmanaged_extern
// loads the purego syscallX trampoline PC out of c2go_syscallX and the target
// C function address out of a per-symbol c2go_fn_<name> var (emitted by
// c2go-bind), builds the syscallArgs block on its own frame, and
// `CALL runtime·cgocall(SB)` directly. Exposing the trampoline as a flat
// self-linknamed DATA symbol lets the cross-"package" .s reference it as
// `c2go_syscallX(SB)` without re-pulling the unexported purego symbol from the
// generated code.

package dl

import _ "unsafe" // for go:linkname

// c2go_syscallX holds purego.syscallXABI0 — the PC of purego's syscallX asm
// trampoline (loads a1..a8 -> int regs, f1..f8 -> float regs, a9.. -> stack,
// arm64_r8 -> indirect-result reg, calls fn, writes results back). It is
// initialized once from purego_syscallXABI0 (set by purego's package init,
// which runs before ours). The two-arg self-linkname gives it the flat linker
// name `c2go_syscallX` so a clang .s wrapper resolves `c2go_syscallX(SB)`.
//
//go:linkname c2go_syscallX c2go_syscallX
var c2go_syscallX uintptr

func init() {
	c2go_syscallX = puregoSyscallXABI0
}

// SyscallXTrampoline returns the purego syscallX trampoline PC. clang-generated
// wrappers reference a package-LOCAL `c2go_syscallX` var (the Plan 9 streamer
// renders a dotless global as a current-package `·c2go_syscallX`), so c2go-bind
// emits, once per generated package, `var c2go_syscallX uintptr` initialized
// from this getter. (A single cross-package flat symbol would need the .s to
// reference `c2go_syscallX(SB)` without the `·`, which the streamer cannot
// emit for a dotless name.)
func SyscallXTrampoline() uintptr { return puregoSyscallXABI0 }
